// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/fidlcat/lib/syscall_decoder.h"

#include <zircon/system/public/zircon/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "src/developer/debug/zxdb/client/breakpoint.h"
#include "src/developer/debug/zxdb/client/frame.h"
#include "src/developer/debug/zxdb/client/memory_dump.h"
#include "src/developer/debug/zxdb/client/process.h"
#include "src/developer/debug/zxdb/client/step_thread_controller.h"
#include "src/developer/debug/zxdb/client/thread.h"
#include "src/developer/debug/zxdb/symbols/symbol.h"
#include "src/lib/fxl/logging.h"

// TODO(fidlcat): Look into this.  Removing the hack that led to this (in
// debug_ipc/helper/message_loop.h) seems to work, except it breaks SDK builds
// on CQ in a way I can't repro locally.
#undef __TA_REQUIRES

#include "tools/fidlcat/lib/interception_workflow.h"
#include "tools/fidlcat/lib/type_decoder.h"

namespace fidlcat {

constexpr int kBitsPerByte = 8;

// Helper function to convert a vector of bytes to a T.
template <typename T>
T GetValueFromBytes(const std::vector<uint8_t>& bytes, size_t offset) {
  T ret = 0;
  for (size_t i = 0; (i < sizeof(ret)) && (offset < bytes.size()); i++) {
    ret |= static_cast<uint64_t>(bytes[offset++]) << (i * kBitsPerByte);
  }
  return ret;
}

uint64_t GetRegisterValue(const std::vector<debug_ipc::Register>& general_registers,
                          const debug_ipc::RegisterID register_id) {
  for (const auto& reg : general_registers) {
    if (reg.id == register_id) {
      return GetValueFromBytes<uint64_t>(reg.data, 0);
    }
  }
  return 0;
}

void MemoryDumpToVector(const zxdb::MemoryDump& dump, std::vector<uint8_t>* output_vector) {
  output_vector->reserve(dump.size());
  for (const debug_ipc::MemoryBlock& block : dump.blocks()) {
    FXL_DCHECK(block.valid);
    for (size_t offset = 0; offset < block.size; ++offset) {
      output_vector->push_back(block.data[offset]);
    }
  }
}

void SyscallUse::SyscallInputsDecoded(SyscallDecoder* decoder) {}

void SyscallUse::SyscallOutputsDecoded(SyscallDecoder* decoder) { decoder->Destroy(); }

void SyscallUse::SyscallDecodingError(const DecoderError& error, SyscallDecoder* decoder) {
  FXL_LOG(ERROR) << error.message();
  decoder->Destroy();
}

void SyscallDecoder::DisplayHandle(const zx_handle_info_t& handle_info,
                                   const fidl_codec::Colors& colors, std::ostream& os) {
  fidl_codec::DisplayHandle(colors, handle_info, os);
  const fidl_codec::semantic::HandleDescription* known_handle =
      dispatcher_->inference().GetHandleDescription(process_id_, handle_info.handle);
  if (known_handle != nullptr) {
    os << '(';
    known_handle->Display(colors, os);
    os << ')';
  }
}

void SyscallDecoder::LoadMemory(uint64_t address, size_t size, std::vector<uint8_t>* destination) {
  if (address == 0) {
    // Null pointer => don't load anything.
    return;
  }
  zxdb::Thread* thread = get_thread();
  if (thread == nullptr) {
    aborted_ = true;
    Destroy();
  }
  ++pending_request_count_;
  thread->GetProcess()->ReadMemory(
      address, size,
      [this, address, size, destination](const zxdb::Err& err, zxdb::MemoryDump dump) {
        --pending_request_count_;
        if (aborted()) {
          Destroy();
        } else {
          if (!err.ok()) {
            Error(DecoderError::Type::kCantReadMemory)
                << "Can't load memory at " << address << ": " << err.msg();
          } else if ((dump.size() != size) || !dump.AllValid()) {
            Error(DecoderError::Type::kCantReadMemory)
                << "Can't load memory at " << address << ": not enough data";
          } else {
            MemoryDumpToVector(dump, destination);
          }
          if (input_arguments_loaded_) {
            LoadOutputs();
          } else {
            LoadInputs();
          }
        }
      });
}

void SyscallDecoder::LoadArgument(Stage stage, int argument_index, size_t size) {
  if (decoded_arguments_[argument_index].loading(stage)) {
    return;
  }
  decoded_arguments_[argument_index].set_loading(stage);
  LoadMemory(ArgumentValue(argument_index), size,
             &decoded_arguments_[argument_index].loaded_values(stage));
}

void SyscallDecoder::LoadBuffer(Stage stage, uint64_t address, size_t size) {
  if (address == 0) {
    return;
  }
  SyscallDecoderBuffer& buffer = buffers_[std::make_pair(stage, address)];
  if (buffer.loading()) {
    return;
  }
  buffer.set_loading();
  LoadMemory(address, size, &buffer.loaded_values());
}

void SyscallDecoder::Decode() {
  zxdb::Thread* thread = weak_thread_.get();
  if (aborted_ || (thread == nullptr) || (thread->GetStack().size() == 0)) {
    aborted_ = true;
    Destroy();
    return;
  }
  if (dispatcher_->decode_options().stack_level >= kFullStack) {
    thread->GetStack().SyncFrames([this](const zxdb::Err& /*err*/) { DoDecode(); });
  } else {
    DoDecode();
  }
}

void SyscallDecoder::DoDecode() {
  zxdb::Thread* thread = weak_thread_.get();
  if (aborted_ || (thread == nullptr) || (thread->GetStack().size() == 0)) {
    aborted_ = true;
    Destroy();
    return;
  }
  const zxdb::Stack& stack = thread->GetStack();
  // Don't keep the inner frame which is the syscall and is not useful.
  for (size_t i = stack.size() - 1; i > 0; --i) {
    const zxdb::Frame* caller = stack[i];
    caller_locations_.push_back(caller->GetLocation());
  }
  const std::vector<debug_ipc::Register>* general_registers =
      thread->GetStack()[0]->GetRegisterCategorySync(debug_ipc::RegisterCategory::kGeneral);
  FXL_DCHECK(general_registers);  // General registers should always be available synchronously.

  // The order of parameters in the System V AMD64 ABI we use, according to
  // Wikipedia:
  static std::vector<debug_ipc::RegisterID> amd64_abi = {
      debug_ipc::RegisterID::kX64_rdi, debug_ipc::RegisterID::kX64_rsi,
      debug_ipc::RegisterID::kX64_rdx, debug_ipc::RegisterID::kX64_rcx,
      debug_ipc::RegisterID::kX64_r8,  debug_ipc::RegisterID::kX64_r9};

  // The order of parameters in the System V AArch64 ABI we use, according to
  // Wikipedia:
  static std::vector<debug_ipc::RegisterID> aarch64_abi = {
      debug_ipc::RegisterID::kARMv8_x0, debug_ipc::RegisterID::kARMv8_x1,
      debug_ipc::RegisterID::kARMv8_x2, debug_ipc::RegisterID::kARMv8_x3,
      debug_ipc::RegisterID::kARMv8_x4, debug_ipc::RegisterID::kARMv8_x5,
      debug_ipc::RegisterID::kARMv8_x6, debug_ipc::RegisterID::kARMv8_x7};

  const std::vector<debug_ipc::RegisterID>* abi;
  if (arch_ == debug_ipc::Arch::kX64) {
    abi = &amd64_abi;
    entry_sp_ = GetRegisterValue(*general_registers, debug_ipc::RegisterID::kX64_rsp);
  } else if (arch_ == debug_ipc::Arch::kArm64) {
    abi = &aarch64_abi;
    entry_sp_ = GetRegisterValue(*general_registers, debug_ipc::RegisterID::kARMv8_sp);
    return_address_ = GetRegisterValue(*general_registers, debug_ipc::RegisterID::kARMv8_lr);
  } else {
    Error(DecoderError::Type::kUnknownArchitecture) << "Unknown architecture";
    if (pending_request_count_ == 0) {
      use_->SyscallDecodingError(error_, this);
    }
    return;
  }

  size_t argument_count = syscall_->arguments().size();
  decoded_arguments_.reserve(argument_count);
  size_t register_count = std::min(argument_count, abi->size());
  for (size_t i = 0; i < register_count; i++) {
    decoded_arguments_.emplace_back(GetRegisterValue(*general_registers, (*abi)[i]));
  }

  LoadStack();
}

void SyscallDecoder::LoadStack() {
  zxdb::Thread* thread = weak_thread_.get();
  if (aborted_ || (thread == nullptr) || (thread->GetStack().size() == 0)) {
    aborted_ = true;
    Destroy();
    return;
  }
  size_t stack_size = (syscall_->arguments().size() - decoded_arguments_.size()) * sizeof(uint64_t);
  if (arch_ == debug_ipc::Arch::kX64) {
    stack_size += sizeof(uint64_t);
  }
  if (stack_size == 0) {
    LoadInputs();
    return;
  }
  uint64_t address = entry_sp_;
  ++pending_request_count_;
  thread->GetProcess()->ReadMemory(
      address, stack_size,
      [this, address, stack_size](const zxdb::Err& err, zxdb::MemoryDump dump) {
        --pending_request_count_;
        if (aborted()) {
          Destroy();
        } else {
          if (!err.ok()) {
            Error(DecoderError::Type::kCantReadMemory)
                << "Can't load stack at " << address << '/' << stack_size << ": " << err.msg();
          } else if ((dump.size() != stack_size) || !dump.AllValid()) {
            Error(DecoderError::Type::kCantReadMemory)
                << "Can't load stack at " << address << '/' << stack_size << ": not enough data";
          } else {
            std::vector<uint8_t> data;
            MemoryDumpToVector(dump, &data);
            size_t offset = 0;
            if (arch_ == debug_ipc::Arch::kX64) {
              return_address_ = GetValueFromBytes<uint64_t>(data, 0);
              offset += sizeof(uint64_t);
            }
            while (offset < data.size()) {
              decoded_arguments_.emplace_back(GetValueFromBytes<uint64_t>(data, offset));
              offset += sizeof(uint64_t);
            }
          }
          LoadInputs();
        }
      });
}

void SyscallDecoder::LoadInputs() {
  if (error_.type() != DecoderError::Type::kNone) {
    if (pending_request_count_ == 0) {
      use_->SyscallDecodingError(error_, this);
    }
    return;
  }
  for (const auto& input : syscall_->inputs()) {
    if (input->ConditionsAreTrue(this, Stage::kEntry)) {
      input->Load(this, Stage::kEntry);
    }
  }
  if (pending_request_count_ > 0) {
    return;
  }
  input_arguments_loaded_ = true;
  if (error_.type() != DecoderError::Type::kNone) {
    use_->SyscallDecodingError(error_, this);
  } else {
    StepToReturnAddress();
  }
}

void SyscallDecoder::StepToReturnAddress() {
  zxdb::Thread* thread = weak_thread_.get();
  if (aborted_ || (thread == nullptr) || (thread->GetStack().size() == 0)) {
    aborted_ = true;
    Destroy();
    return;
  }
  // Eventually calls the code before displaying the input (which may invalidate
  // the display).
  if ((syscall_->inputs_decoded_action() == nullptr) ||
      (dispatcher_->*(syscall_->inputs_decoded_action()))(this)) {
    use_->SyscallInputsDecoded(this);
  }

  if (syscall_->return_type() == SyscallReturnType::kNoReturn) {
    // We don't expect the syscall to return and it doesn't have any output.
    use_->SyscallOutputsDecoded(this);
    return;
  }

  thread_observer_->Register(thread_id(), this);
  thread_observer_->AddExitBreakpoint(thread, syscall_->name(), return_address_);

  // Restarts the stopped thread. When the breakpoint will be reached (at the
  // end of the syscall), LoadSyscallReturnValue will be called.
  thread->Continue();
}

void SyscallDecoder::LoadSyscallReturnValue() {
  zxdb::Thread* thread = weak_thread_.get();
  if (aborted_ || (thread == nullptr) || (thread->GetStack().size() == 0)) {
    aborted_ = true;
    Destroy();
    return;
  }
  const std::vector<debug_ipc::Register>* general_registers =
      thread->GetStack()[0]->GetRegisterCategorySync(debug_ipc::RegisterCategory::kGeneral);
  FXL_DCHECK(general_registers);  // General registers should always be available synchronously.

  debug_ipc::RegisterID result_register = (arch_ == debug_ipc::Arch::kX64)
                                              ? debug_ipc::RegisterID::kX64_rax
                                              : debug_ipc::RegisterID::kARMv8_x0;
  syscall_return_value_ = GetRegisterValue(*general_registers, result_register);

  LoadOutputs();
}

void SyscallDecoder::LoadOutputs() {
  if (error_.type() != DecoderError::Type::kNone) {
    if (pending_request_count_ == 0) {
      use_->SyscallDecodingError(error_, this);
    }
    return;
  }
  for (const auto& output : syscall_->outputs()) {
    if ((output->error_code() == static_cast<zx_status_t>(syscall_return_value_)) &&
        output->ConditionsAreTrue(this, Stage::kExit)) {
      output->Load(this, Stage::kExit);
    }
  }
  if (pending_request_count_ > 0) {
    return;
  }
  if (error_.type() != DecoderError::Type::kNone) {
    use_->SyscallDecodingError(error_, this);
  } else {
    DecodeAndDisplay();
  }
}

void SyscallDecoder::DecodeAndDisplay() {
  if (pending_request_count_ > 0) {
    return;
  }
  use_->SyscallOutputsDecoded(this);
}

void SyscallDecoder::Destroy() {
  if (pending_request_count_ == 0) {
    if (syscall_->displayed_action() != nullptr) {
      // Calls the action associated with the syscall. This is used to infer semantic about handles.
      (dispatcher_->*(syscall_->displayed_action()))(this);
    }

    dispatcher_->DeleteDecoder(this);
  }
}

void SyscallDisplay::SyscallInputsDecoded(SyscallDecoder* decoder) {
  const fidl_codec::Colors& colors = dispatcher_->colors();
  line_header_ = decoder->process_name() + ' ' + colors.red +
                 std::to_string(decoder->process_id()) + colors.reset + ':' + colors.red +
                 std::to_string(decoder->thread_id()) + colors.reset + ' ';

  if (dispatcher_->with_process_info()) {
    os_ << line_header_ << '\n';
  } else {
    os_ << '\n';
  }

  if (dispatcher_->decode_options().stack_level != kNoStack) {
    // Display caller locations.
    DisplayStackFrame(dispatcher_->colors(), line_header_, decoder->caller_locations(), os_);
  }

  if (decoder->syscall()->fidl_codec_values_ready()) {
    // We are able to create values from the syscall => create the values and then print them.
    //
    // The long term goal is that zxdb gives the timestamp. Currently we only create one when we
    // print the syscall.
    int64_t timestamp = time(nullptr);
    const Thread* thread = dispatcher_->SearchThread(decoder->thread_id());
    if (thread == nullptr) {
      const Process* process = dispatcher_->SearchProcess(decoder->process_id());
      if (process == nullptr) {
        process = dispatcher_->CreateProcess(decoder->process_name(), decoder->process_id());
      }
      thread = dispatcher_->CreateThread(decoder->thread_id(), process);
    }
    InvokedEvent* invoked_event = decoder->set_invoked_event(
        std::make_unique<InvokedEvent>(timestamp << 32, thread, decoder->syscall()));
    auto inline_member = decoder->syscall()->input_inline_members().begin();
    auto outline_member = decoder->syscall()->input_outline_members().begin();
    for (const auto& input : decoder->syscall()->inputs()) {
      if (input->InlineValue()) {
        if (input->ConditionsAreTrue(decoder, Stage::kEntry)) {
          FXL_DCHECK(inline_member != decoder->syscall()->input_inline_members().end());
          std::unique_ptr<fidl_codec::Value> value = input->GenerateValue(decoder, Stage::kEntry);
          FXL_DCHECK(value != nullptr);
          invoked_event->AddInlineField(inline_member->get(), std::move(value));
        }
        ++inline_member;
      } else {
        if (input->ConditionsAreTrue(decoder, Stage::kEntry)) {
          FXL_DCHECK(outline_member != decoder->syscall()->input_outline_members().end());
          std::unique_ptr<fidl_codec::Value> value = input->GenerateValue(decoder, Stage::kEntry);
          FXL_DCHECK(value != nullptr);
          invoked_event->AddOutlineField(outline_member->get(), std::move(value));
        }
        ++outline_member;
      }
    }
    const fidl_codec::Colors& colors = dispatcher_->colors();
    std::string line_header = invoked_event->thread()->process()->name() + ' ' + colors.red +
                              std::to_string(invoked_event->thread()->process()->koid()) +
                              colors.reset + ':' + colors.red +
                              std::to_string(invoked_event->thread()->koid()) + colors.reset + ' ';
    FidlcatPrinter printer(decoder, dispatcher_->dump_messages(),
                           dispatcher_->message_decoder_dispatcher().display_options().pretty_print,
                           os_, dispatcher_->colors(), line_header, dispatcher_->columns(),
                           dispatcher_->with_process_info());
    invoked_event->PrettyPrint(printer);

    if (!dispatcher_->with_process_info()) {
      line_header_ = "";
    }
  } else {
    // This code will be deleted when we will be able to have the two step printing for all the
    // syscalls.
    //
    // Displays the header and the inline input arguments.
    os_ << line_header_ << decoder->syscall()->name() << '(';
    const char* separator = "";
    for (const auto& input : decoder->syscall()->inputs()) {
      if (input->ConditionsAreTrue(decoder, Stage::kEntry)) {
        separator = input->DisplayInline(dispatcher_, decoder, Stage::kEntry, separator, os_);
      }
    }
    os_ << ")\n";

    if (!dispatcher_->with_process_info()) {
      line_header_ = "";
    }

    // Displays the outline input arguments.
    for (const auto& input : decoder->syscall()->inputs()) {
      if (input->ConditionsAreTrue(decoder, Stage::kEntry)) {
        input->DisplayOutline(dispatcher_, decoder, Stage::kEntry, line_header_, /*tabs=*/1, os_);
      }
    }
  }
  dispatcher_->set_last_displayed_syscall(this);
}

void SyscallDisplay::SyscallOutputsDecoded(SyscallDecoder* decoder) {
  if (decoder->syscall()->return_type() != SyscallReturnType::kNoReturn) {
    const fidl_codec::Colors& colors = dispatcher_->colors();
    if (decoder->syscall()->fidl_codec_values_ready()) {
      // We are able to create values from the syscall => create the values and then print them.
      //
      // The long term goal is that zxdb gives the timestamp. Currently we only create one when we
      // print the syscall.
      int64_t timestamp = time(nullptr);
      const Thread* thread = dispatcher_->SearchThread(decoder->thread_id());
      if (thread == nullptr) {
        const Process* process = dispatcher_->SearchProcess(decoder->process_id());
        if (process == nullptr) {
          process = dispatcher_->CreateProcess(decoder->process_name(), decoder->process_id());
        }
        thread = dispatcher_->CreateThread(decoder->thread_id(), process);
      }
      OutputEvent* output_event = decoder->set_output_event(std::make_unique<OutputEvent>(
          timestamp << 32, thread, decoder->syscall(), decoder->syscall_return_value()));
      auto inline_member = decoder->syscall()->output_inline_members().begin();
      auto outline_member = decoder->syscall()->output_outline_members().begin();
      for (const auto& output : decoder->syscall()->outputs()) {
        if (output->InlineValue()) {
          if ((output->error_code() == static_cast<zx_status_t>(decoder->syscall_return_value())) &&
              (output->ConditionsAreTrue(decoder, Stage::kExit))) {
            FXL_DCHECK(inline_member != decoder->syscall()->output_inline_members().end());
            std::unique_ptr<fidl_codec::Value> value = output->GenerateValue(decoder, Stage::kExit);
            FXL_DCHECK(value != nullptr);
            output_event->AddInlineField(inline_member->get(), std::move(value));
          }
          ++inline_member;
        } else {
          if ((output->error_code() == static_cast<zx_status_t>(decoder->syscall_return_value())) &&
              (output->ConditionsAreTrue(decoder, Stage::kExit))) {
            FXL_DCHECK(outline_member != decoder->syscall()->output_outline_members().end());
            std::unique_ptr<fidl_codec::Value> value = output->GenerateValue(decoder, Stage::kExit);
            FXL_DCHECK(value != nullptr);
            output_event->AddOutlineField(outline_member->get(), std::move(value));
          }
          ++outline_member;
        }
      }
      std::string line_header;
      if (dispatcher_->with_process_info() || (dispatcher_->last_displayed_syscall() != this)) {
        line_header = output_event->thread()->process()->name() + ' ' + colors.red +
                      std::to_string(output_event->thread()->process()->koid()) + colors.reset +
                      ':' + colors.red + std::to_string(output_event->thread()->koid()) +
                      colors.reset + ' ';
      }
      if (dispatcher_->last_displayed_syscall() != this) {
        // Add a blank line to tell the user that this display is not linked to the
        // previous displayed lines.
        os_ << "\n";
      }
      FidlcatPrinter printer(
          decoder, dispatcher_->dump_messages(),
          dispatcher_->message_decoder_dispatcher().display_options().pretty_print, os_,
          dispatcher_->colors(), line_header, dispatcher_->columns(),
          dispatcher_->with_process_info());
      output_event->PrettyPrint(printer);
    } else {
      // This code will be deleted when we will be able to have the two step printing for all the
      // syscalls.
      //
      // Displays the returned value.
      if (dispatcher_->last_displayed_syscall() != this) {
        // Add a blank line to tell the user that this display is not linked to the
        // previous displayed lines.
        os_ << "\n";
        // Then always display the process info to be able able to know for which thread
        // we are displaying the output.
        std::string first_line_header = decoder->process_name() + ' ' + colors.red +
                                        std::to_string(decoder->process_id()) + colors.reset + ':' +
                                        colors.red + std::to_string(decoder->thread_id()) +
                                        colors.reset + ' ';
        os_ << first_line_header << "  -> ";
      } else {
        os_ << line_header_ << "  -> ";
      }
      switch (decoder->syscall()->return_type()) {
        case SyscallReturnType::kNoReturn:
        case SyscallReturnType::kVoid:
          break;
        case SyscallReturnType::kStatus:
          StatusName(colors, static_cast<zx_status_t>(decoder->syscall_return_value()), os_);
          break;
        case SyscallReturnType::kTicks:
          os_ << colors.green << "ticks" << colors.reset << ": " << colors.blue
              << static_cast<uint64_t>(decoder->syscall_return_value()) << colors.reset;
          break;
        case SyscallReturnType::kTime:
          os_ << colors.green << "time" << colors.reset << ": "
              << DisplayTime(colors, static_cast<zx_time_t>(decoder->syscall_return_value()));
          break;
        case SyscallReturnType::kUint32:
          os_ << colors.blue << static_cast<uint32_t>(decoder->syscall_return_value())
              << colors.reset;
          break;
        case SyscallReturnType::kUint64:
          os_ << colors.blue << static_cast<uint64_t>(decoder->syscall_return_value())
              << colors.reset;
          break;
      }
      // And the inline output arguments (if any).
      const char* separator = " (";
      for (const auto& output : decoder->syscall()->outputs()) {
        if ((output->error_code() == static_cast<zx_status_t>(decoder->syscall_return_value())) &&
            output->ConditionsAreTrue(decoder, Stage::kExit)) {
          separator = output->DisplayInline(dispatcher_, decoder, Stage::kExit, separator, os_);
        }
      }
      if (std::string(" (") != separator) {
        os_ << ')';
      }
      os_ << '\n';
      // Displays the outline output arguments.
      for (const auto& output : decoder->syscall()->outputs()) {
        if ((output->error_code() == static_cast<zx_status_t>(decoder->syscall_return_value())) &&
            output->ConditionsAreTrue(decoder, Stage::kExit)) {
          output->DisplayOutline(dispatcher_, decoder, Stage::kExit, line_header_, /*tabs=*/2, os_);
        }
      }
    }

    dispatcher_->set_last_displayed_syscall(this);
  }

  // Now our job is done, we can destroy the object.
  decoder->Destroy();
}

void SyscallDisplay::SyscallDecodingError(const DecoderError& error, SyscallDecoder* decoder) {
  std::string message = error.message();
  size_t pos = 0;
  for (;;) {
    size_t end = message.find('\n', pos);
    const fidl_codec::Colors& colors = dispatcher_->colors();
    os_ << decoder->process_name() << ' ' << colors.red << decoder->process_id() << colors.reset
        << ':' << colors.red << decoder->thread_id() << colors.reset << ' '
        << decoder->syscall()->name() << ": " << colors.red << error.message().substr(pos, end)
        << colors.reset << '\n';
    if (end == std::string::npos) {
      break;
    }
    pos = end + 1;
  }
  os_ << '\n';
  decoder->Destroy();
}

void SyscallCompare::SyscallInputsDecoded(SyscallDecoder* decoder) {
  os_.clear();
  os_.str("");
  SyscallDisplay::SyscallInputsDecoded(decoder);
  comparator_->CompareInput(os_.str(), decoder->process_name(), decoder->process_id(),
                            decoder->thread_id());
}

void SyscallCompare::SyscallOutputsDecoded(SyscallDecoder* decoder) {
  os_.clear();
  os_.str("");
  SyscallDisplay::SyscallOutputsDecoded(decoder);
  if (decoder->syscall()->return_type() != SyscallReturnType::kNoReturn) {
    comparator_->CompareOutput(os_.str(), decoder->process_name(), decoder->process_id(),
                               decoder->thread_id());
  }
}

void SyscallCompare::SyscallDecodingError(const DecoderError& error, SyscallDecoder* decoder) {
  os_.clear();
  os_.str("");
  SyscallDisplay::SyscallDecodingError(error, decoder);
  comparator_->DecodingError(os_.str());
}

}  // namespace fidlcat

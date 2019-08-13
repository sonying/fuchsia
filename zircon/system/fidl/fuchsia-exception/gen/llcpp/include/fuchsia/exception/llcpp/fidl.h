// WARNING: This file is machine generated by fidlgen.

#pragma once

#include <lib/fidl/internal.h>
#include <lib/fidl/cpp/vector_view.h>
#include <lib/fidl/cpp/string_view.h>
#include <lib/fidl/llcpp/array.h>
#include <lib/fidl/llcpp/coding.h>
#include <lib/fidl/llcpp/sync_call.h>
#include <lib/fidl/llcpp/traits.h>
#include <lib/fidl/llcpp/transaction.h>
#include <lib/fit/function.h>
#include <lib/zx/channel.h>
#include <lib/zx/exception.h>
#include <zircon/fidl.h>

namespace llcpp {

namespace fuchsia {
namespace exception {

enum class ExceptionType : uint32_t {
  GENERAL = 8u,
  FATAL_PAGE_FAULT = 264u,
  UNDEFINED_INSTRUCTION = 520u,
  SW_BREAKPOINT = 776u,
  HW_BREAKPOINT = 1032u,
  UNALIGNED_ACCESS = 1288u,
  THREAD_STARTING = 32776u,
  THREAD_EXITING = 33032u,
  POLICY_ERROR = 33288u,
  PROCESS_STARTING = 33544u,
};


struct ExceptionInfo;
class Handler;

extern "C" const fidl_type_t fuchsia_exception_ExceptionInfoTable;

// Basic exception information associated with a particular exception.
// Maps to `zx_exception_info_t`.
struct ExceptionInfo {
  static constexpr const fidl_type_t* Type = &fuchsia_exception_ExceptionInfoTable;
  static constexpr uint32_t MaxNumHandles = 0;
  static constexpr uint32_t PrimarySize = 24;
  [[maybe_unused]]
  static constexpr uint32_t MaxOutOfLine = 0;

  uint64_t process_koid = {};

  uint64_t thread_koid = {};

  ExceptionType type = {};
};

extern "C" const fidl_type_t fuchsia_exception_HandlerOnExceptionRequestTable;

// Protocol meant for clients interested in handling exceptions for a
// particular service.
class Handler final {
  Handler() = delete;
 public:
  static constexpr char Name[] = "fuchsia.exception.Handler";

  using OnExceptionResponse = ::fidl::AnyZeroArgMessage;
  struct OnExceptionRequest final {
    FIDL_ALIGNDECL
    fidl_message_header_t _hdr;
    ::zx::exception exception;
    ExceptionInfo info;

    static constexpr const fidl_type_t* Type = &fuchsia_exception_HandlerOnExceptionRequestTable;
    static constexpr uint32_t MaxNumHandles = 1;
    static constexpr uint32_t PrimarySize = 48;
    static constexpr uint32_t MaxOutOfLine = 0;
    static constexpr bool HasFlexibleEnvelope = false;
    static constexpr ::fidl::internal::TransactionalMessageKind MessageKind =
        ::fidl::internal::TransactionalMessageKind::kRequest;
  };


  // Collection of return types of FIDL calls in this interface.
  class ResultOf final {
    ResultOf() = delete;
   private:
    template <typename ResponseType>
    class OnException_Impl final : private ::fidl::internal::OwnedSyncCallBase<ResponseType> {
      using Super = ::fidl::internal::OwnedSyncCallBase<ResponseType>;
     public:
      OnException_Impl(zx::unowned_channel _client_end, ::zx::exception exception, ExceptionInfo info);
      ~OnException_Impl() = default;
      OnException_Impl(OnException_Impl&& other) = default;
      OnException_Impl& operator=(OnException_Impl&& other) = default;
      using Super::status;
      using Super::error;
      using Super::ok;
      using Super::Unwrap;
      using Super::value;
      using Super::operator->;
      using Super::operator*;
    };

   public:
    using OnException = OnException_Impl<OnExceptionResponse>;
  };

  // Collection of return types of FIDL calls in this interface,
  // when the caller-allocate flavor or in-place call is used.
  class UnownedResultOf final {
    UnownedResultOf() = delete;
   private:
    template <typename ResponseType>
    class OnException_Impl final : private ::fidl::internal::UnownedSyncCallBase<ResponseType> {
      using Super = ::fidl::internal::UnownedSyncCallBase<ResponseType>;
     public:
      OnException_Impl(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::zx::exception exception, ExceptionInfo info, ::fidl::BytePart _response_buffer);
      ~OnException_Impl() = default;
      OnException_Impl(OnException_Impl&& other) = default;
      OnException_Impl& operator=(OnException_Impl&& other) = default;
      using Super::status;
      using Super::error;
      using Super::ok;
      using Super::Unwrap;
      using Super::value;
      using Super::operator->;
      using Super::operator*;
    };

   public:
    using OnException = OnException_Impl<OnExceptionResponse>;
  };

  class SyncClient final {
   public:
    explicit SyncClient(::zx::channel channel) : channel_(std::move(channel)) {}
    ~SyncClient() = default;
    SyncClient(SyncClient&&) = default;
    SyncClient& operator=(SyncClient&&) = default;

    const ::zx::channel& channel() const { return channel_; }

    ::zx::channel* mutable_channel() { return &channel_; }

    // This exception mirrors closely the information provided by exception
    // channels. The design is to have clients of this API behave as closely as
    // possible to native exception handlers that are listening to an exception
    // channel.
    //
    // `exception` is an exception handle, which controls the exception's
    // lifetime. See exception zircon docs for more information.
    //
    // `info` represents basic exception information as provided by the
    // exception channel.
    // Allocates 64 bytes of message buffer on the stack. No heap allocation necessary.
    ResultOf::OnException OnException(::zx::exception exception, ExceptionInfo info);

    // This exception mirrors closely the information provided by exception
    // channels. The design is to have clients of this API behave as closely as
    // possible to native exception handlers that are listening to an exception
    // channel.
    //
    // `exception` is an exception handle, which controls the exception's
    // lifetime. See exception zircon docs for more information.
    //
    // `info` represents basic exception information as provided by the
    // exception channel.
    // Caller provides the backing storage for FIDL message via request and response buffers.
    UnownedResultOf::OnException OnException(::fidl::BytePart _request_buffer, ::zx::exception exception, ExceptionInfo info, ::fidl::BytePart _response_buffer);

    // This exception mirrors closely the information provided by exception
    // channels. The design is to have clients of this API behave as closely as
    // possible to native exception handlers that are listening to an exception
    // channel.
    //
    // `exception` is an exception handle, which controls the exception's
    // lifetime. See exception zircon docs for more information.
    //
    // `info` represents basic exception information as provided by the
    // exception channel.
    zx_status_t OnException_Deprecated(::zx::exception exception, ExceptionInfo info);

    // This exception mirrors closely the information provided by exception
    // channels. The design is to have clients of this API behave as closely as
    // possible to native exception handlers that are listening to an exception
    // channel.
    //
    // `exception` is an exception handle, which controls the exception's
    // lifetime. See exception zircon docs for more information.
    //
    // `info` represents basic exception information as provided by the
    // exception channel.
    // Caller provides the backing storage for FIDL message via request and response buffers.
    // The lifetime of handles in the response, unless moved, is tied to the returned RAII object.
    ::fidl::DecodeResult<OnExceptionResponse> OnException_Deprecated(::fidl::BytePart _request_buffer, ::zx::exception exception, ExceptionInfo info);

   private:
    ::zx::channel channel_;
  };

  // Methods to make a sync FIDL call directly on an unowned channel, avoiding setting up a client.
  class Call final {
    Call() = delete;
   public:

    // This exception mirrors closely the information provided by exception
    // channels. The design is to have clients of this API behave as closely as
    // possible to native exception handlers that are listening to an exception
    // channel.
    //
    // `exception` is an exception handle, which controls the exception's
    // lifetime. See exception zircon docs for more information.
    //
    // `info` represents basic exception information as provided by the
    // exception channel.
    // Allocates 64 bytes of message buffer on the stack. No heap allocation necessary.
    static ResultOf::OnException OnException(zx::unowned_channel _client_end, ::zx::exception exception, ExceptionInfo info);

    // This exception mirrors closely the information provided by exception
    // channels. The design is to have clients of this API behave as closely as
    // possible to native exception handlers that are listening to an exception
    // channel.
    //
    // `exception` is an exception handle, which controls the exception's
    // lifetime. See exception zircon docs for more information.
    //
    // `info` represents basic exception information as provided by the
    // exception channel.
    // Caller provides the backing storage for FIDL message via request and response buffers.
    static UnownedResultOf::OnException OnException(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::zx::exception exception, ExceptionInfo info, ::fidl::BytePart _response_buffer);

    // This exception mirrors closely the information provided by exception
    // channels. The design is to have clients of this API behave as closely as
    // possible to native exception handlers that are listening to an exception
    // channel.
    //
    // `exception` is an exception handle, which controls the exception's
    // lifetime. See exception zircon docs for more information.
    //
    // `info` represents basic exception information as provided by the
    // exception channel.
    static zx_status_t OnException_Deprecated(zx::unowned_channel _client_end, ::zx::exception exception, ExceptionInfo info);

    // This exception mirrors closely the information provided by exception
    // channels. The design is to have clients of this API behave as closely as
    // possible to native exception handlers that are listening to an exception
    // channel.
    //
    // `exception` is an exception handle, which controls the exception's
    // lifetime. See exception zircon docs for more information.
    //
    // `info` represents basic exception information as provided by the
    // exception channel.
    // Caller provides the backing storage for FIDL message via request and response buffers.
    // The lifetime of handles in the response, unless moved, is tied to the returned RAII object.
    static ::fidl::DecodeResult<OnExceptionResponse> OnException_Deprecated(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::zx::exception exception, ExceptionInfo info);

  };

  // Messages are encoded and decoded in-place when these methods are used.
  // Additionally, requests must be already laid-out according to the FIDL wire-format.
  class InPlace final {
    InPlace() = delete;
   public:

    // This exception mirrors closely the information provided by exception
    // channels. The design is to have clients of this API behave as closely as
    // possible to native exception handlers that are listening to an exception
    // channel.
    //
    // `exception` is an exception handle, which controls the exception's
    // lifetime. See exception zircon docs for more information.
    //
    // `info` represents basic exception information as provided by the
    // exception channel.
    static ::fidl::DecodeResult<OnExceptionResponse> OnException(zx::unowned_channel _client_end, ::fidl::DecodedMessage<OnExceptionRequest> params, ::fidl::BytePart response_buffer);

  };

  // Pure-virtual interface to be implemented by a server.
  class Interface {
   public:
    Interface() = default;
    virtual ~Interface() = default;
    using _Outer = Handler;
    using _Base = ::fidl::CompleterBase;

    class OnExceptionCompleterBase : public _Base {
     public:
      void Reply();

     protected:
      using ::fidl::CompleterBase::CompleterBase;
    };

    using OnExceptionCompleter = ::fidl::Completer<OnExceptionCompleterBase>;

    virtual void OnException(::zx::exception exception, ExceptionInfo info, OnExceptionCompleter::Sync _completer) = 0;

  };

  // Attempts to dispatch the incoming message to a handler function in the server implementation.
  // If there is no matching handler, it returns false, leaving the message and transaction intact.
  // In all other cases, it consumes the message and returns true.
  // It is possible to chain multiple TryDispatch functions in this manner.
  static bool TryDispatch(Interface* impl, fidl_msg_t* msg, ::fidl::Transaction* txn);

  // Dispatches the incoming message to one of the handlers functions in the interface.
  // If there is no matching handler, it closes all the handles in |msg| and closes the channel with
  // a |ZX_ERR_NOT_SUPPORTED| epitaph, before returning false. The message should then be discarded.
  static bool Dispatch(Interface* impl, fidl_msg_t* msg, ::fidl::Transaction* txn);

  // Same as |Dispatch|, but takes a |void*| instead of |Interface*|. Only used with |fidl::Bind|
  // to reduce template expansion.
  // Do not call this method manually. Use |Dispatch| instead.
  static bool TypeErasedDispatch(void* impl, fidl_msg_t* msg, ::fidl::Transaction* txn) {
    return Dispatch(static_cast<Interface*>(impl), msg, txn);
  }

};

}  // namespace exception
}  // namespace fuchsia
}  // namespace llcpp

namespace fidl {

template <>
struct IsFidlType<::llcpp::fuchsia::exception::ExceptionInfo> : public std::true_type {};
static_assert(std::is_standard_layout_v<::llcpp::fuchsia::exception::ExceptionInfo>);
static_assert(offsetof(::llcpp::fuchsia::exception::ExceptionInfo, process_koid) == 0);
static_assert(offsetof(::llcpp::fuchsia::exception::ExceptionInfo, thread_koid) == 8);
static_assert(offsetof(::llcpp::fuchsia::exception::ExceptionInfo, type) == 16);
static_assert(sizeof(::llcpp::fuchsia::exception::ExceptionInfo) == ::llcpp::fuchsia::exception::ExceptionInfo::PrimarySize);

template <>
struct IsFidlType<::llcpp::fuchsia::exception::Handler::OnExceptionRequest> : public std::true_type {};
template <>
struct IsFidlMessage<::llcpp::fuchsia::exception::Handler::OnExceptionRequest> : public std::true_type {};
static_assert(sizeof(::llcpp::fuchsia::exception::Handler::OnExceptionRequest)
    == ::llcpp::fuchsia::exception::Handler::OnExceptionRequest::PrimarySize);
static_assert(offsetof(::llcpp::fuchsia::exception::Handler::OnExceptionRequest, exception) == 16);
static_assert(offsetof(::llcpp::fuchsia::exception::Handler::OnExceptionRequest, info) == 24);

}  // namespace fidl

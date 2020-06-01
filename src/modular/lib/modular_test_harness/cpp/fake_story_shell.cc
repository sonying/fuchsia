// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/modular/lib/modular_test_harness/cpp/fake_story_shell.h"

#include <fuchsia/modular/cpp/fidl.h>

namespace modular_testing {

FakeStoryShell::FakeStoryShell(FakeComponent::Args args) : FakeComponent(std::move(args)) {}
FakeStoryShell::~FakeStoryShell() = default;

// static
std::unique_ptr<FakeStoryShell> FakeStoryShell::CreateWithDefaultOptions() {
  return std::make_unique<FakeStoryShell>(modular_testing::FakeComponent::Args{
      .url = modular_testing::TestHarnessBuilder::GenerateFakeUrl(),
      .sandbox_services = FakeStoryShell::GetDefaultSandboxServices()});
}

// static
std::vector<std::string> FakeStoryShell::GetDefaultSandboxServices() { return {}; }

void FakeStoryShell::OnCreate(fuchsia::sys::StartupInfo startup_info) {
  component_context()->outgoing()->AddPublicService(bindings_.GetHandler(this));
}

void FakeStoryShell::OnDestroy() {
  if (on_destroy_)
    on_destroy_();
}

fidl::InterfaceRequestHandler<fuchsia::modular::StoryShell> FakeStoryShell::GetHandler() {
  return bindings_.GetHandler(this);
}

void FakeStoryShell::Initialize(
    fidl::InterfaceHandle<fuchsia::modular::StoryShellContext> story_shell_context) {
  story_shell_context_ = story_shell_context.Bind();
}

void FakeStoryShell::AddSurface(fuchsia::modular::ViewConnection view_connection,
                                fuchsia::modular::SurfaceInfo surface_info) {
  if (on_add_surface_) {
    on_add_surface_(std::move(view_connection), std::move(surface_info));
  }
}

void FakeStoryShell::AddSurface3(fuchsia::modular::ViewConnection view_connection,
                                 fuchsia::modular::SurfaceInfo2 surface_info2) {
  if (on_add_surface_) {
    fuchsia::modular::SurfaceInfo surface_info;
    surface_info.set_parent_id(surface_info2.parent_id());
    surface_info.set_surface_relation(*surface_info2.mutable_surface_relation());
    surface_info.set_module_manifest(std::move(*surface_info2.mutable_module_manifest()));
    surface_info.set_module_source(surface_info2.module_source());
    on_add_surface_(std::move(view_connection), std::move(surface_info));
  }
}

}  // namespace modular_testing

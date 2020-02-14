// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/modular/bin/sessionmgr/agent_runner/agent_context_impl.h"

#include <dirent.h>
#include <fuchsia/intl/cpp/fidl.h>
#include <fuchsia/modular/cpp/fidl.h>

#include <memory>

#include "lib/fdio/directory.h"
#include "src/modular/bin/sessionmgr/agent_runner/agent_runner.h"
#include "src/modular/lib/common/teardown.h"

namespace modular {

constexpr char kAppStoragePath[] = "/data/APP_DATA";

namespace {

// A stopgap solution to map an agent's url to a directory name where the
// agent's /data is mapped. We need three properties here - (1) two module urls
// that are the same get mapped to the same hash, (2) two modules urls that are
// different don't get the same name (with very high probability) and (3) the
// name is visually inspectable.
std::string HashAgentUrl(const std::string& agent_url) {
  std::size_t found = agent_url.find_last_of('/');
  auto last_part = found == agent_url.length() - 1 ? "" : agent_url.substr(found + 1);
  return std::to_string(std::hash<std::string>{}(agent_url)) + last_part;
}

std::set<std::string> ReadDirHandleContents(zx_handle_t dir_handle) {
  int dir_fd;
  FXL_CHECK(fdio_fd_create(dir_handle, &dir_fd) == ZX_OK);
  FXL_CHECK(dir_fd >= 0) << dir_fd;
  DIR* dir = fdopendir(dir_fd);
  FXL_CHECK(dir != nullptr);
  struct dirent* de;
  errno = 0;
  std::set<std::string> out;
  while ((de = readdir(dir)) != nullptr) {
    out.insert(de->d_name);
  }
  closedir(dir);
  return out;
}

};  // namespace

class AgentContextImpl::InitializeCall : public Operation<> {
 public:
  InitializeCall(AgentContextImpl* const agent_context_impl, fuchsia::sys::Launcher* const launcher,
                 fuchsia::modular::AppConfig agent_config)
      : Operation(
            "AgentContextImpl::InitializeCall", [] {}, agent_context_impl->url_),
        agent_context_impl_(agent_context_impl),
        launcher_(launcher),
        agent_config_(std::move(agent_config)) {}

 private:
  void Run() override {
    FXL_CHECK(agent_context_impl_->state_ == State::INITIALIZING);

    FlowToken flow{this};

    // No agent services factory is available during testing. We want to
    // keep going without it.
    if (!agent_context_impl_->agent_services_factory_) {
      auto service_list = fuchsia::sys::ServiceList::New();
      Continue(std::move(service_list), flow);
      return;
    }

    auto agent_service_list = agent_context_impl_->agent_services_factory_->GetServicesForAgent(
        agent_context_impl_->url_);
    auto service_list = fuchsia::sys::ServiceList::New();
    service_list->names = std::move(agent_service_list.names);
    agent_context_impl_->service_provider_impl_.SetDefaultServiceProvider(
        agent_service_list.provider.Bind());
    Continue(std::move(service_list), flow);
  }

  void Continue(fuchsia::sys::ServiceListPtr service_list, FlowToken flow) {
    service_list->names.push_back(fuchsia::modular::ComponentContext::Name_);
    service_list->names.push_back(fuchsia::modular::AgentContext::Name_);
    for (const auto& service_name : agent_context_impl_->agent_runner_->GetAgentServices()) {
      service_list->names.push_back(service_name);
    }
    agent_context_impl_->service_provider_impl_.AddBinding(service_list->provider.NewRequest());
    agent_context_impl_->app_client_ = std::make_unique<AppClient<fuchsia::modular::Lifecycle>>(
        launcher_, std::move(agent_config_),
        std::string(kAppStoragePath) + HashAgentUrl(agent_context_impl_->url_),
        std::move(service_list));

    agent_context_impl_->app_client_->services().ConnectToService(
        agent_context_impl_->agent_.NewRequest());

    // Enumerate the services that the agent has published in its outgoing directory.
    agent_context_impl_->agent_outgoing_services_ = ReadDirHandleContents(
        fdio_service_clone(agent_context_impl_->app_client_->services().directory().get()));

    // We only want to use fuchsia::modular::Lifecycle if it exists.
    agent_context_impl_->app_client_->primary_service().set_error_handler(
        [agent_context_impl = agent_context_impl_](zx_status_t status) {
          agent_context_impl->app_client_->primary_service().Unbind();
        });

    // When the agent process dies, we remove it.
    agent_context_impl_->app_client_->SetAppErrorHandler(
        [agent_context_impl = agent_context_impl_] {
          agent_context_impl->agent_runner_->RemoveAgent(agent_context_impl->url_);
        });

    // When all the |fuchsia::modular::AgentController| bindings go away maybe
    // stop the agent.
    agent_context_impl_->agent_controller_bindings_.set_empty_set_handler(
        [agent_context_impl = agent_context_impl_] { agent_context_impl->StopAgentIfIdle(); });

    agent_context_impl_->state_ = State::RUNNING;
  }

  AgentContextImpl* const agent_context_impl_;
  fuchsia::sys::Launcher* const launcher_;
  fuchsia::modular::AppConfig agent_config_;
};

// If |terminating| is set to true, the agent will be torn down irrespective
// of whether there is an open-connection. Returns |true| if the
// agent was stopped, false otherwise.
class AgentContextImpl::StopCall : public Operation<bool> {
 public:
  StopCall(const bool terminating, AgentContextImpl* const agent_context_impl,
           ResultCall result_call)
      : Operation("AgentContextImpl::StopCall", std::move(result_call), agent_context_impl->url_),
        agent_context_impl_(agent_context_impl),
        terminating_(terminating) {}

 private:
  void Run() override {
    FlowToken flow{this, &stopped_};

    if (agent_context_impl_->state_ == State::TERMINATING) {
      return;
    }

    if (terminating_ || agent_context_impl_->agent_controller_bindings_.size() == 0) {
      Stop(flow);
    }
  }

  void Stop(FlowToken flow) {
    agent_context_impl_->state_ = State::TERMINATING;
    // Calling Teardown() below will branch |flow| into normal and timeout
    // paths. |flow| must go out of scope when either of the paths finishes.
    //
    // TODO(mesch): AppClient/AsyncHolder should implement this. See also
    // StoryProviderImpl::StopStoryShellCall.
    FlowTokenHolder branch{flow};
    agent_context_impl_->app_client_->Teardown(kBasicTimeout, [this, branch] {
      std::unique_ptr<FlowToken> cont = branch.Continue();
      if (cont) {
        Kill(*cont);
      }
    });
  }

  void Kill(FlowToken flow) {
    stopped_ = true;
    agent_context_impl_->agent_.Unbind();
    agent_context_impl_->agent_context_bindings_.CloseAll();
    agent_context_impl_->token_manager_bindings_.CloseAll();
  }

  bool stopped_ = false;
  AgentContextImpl* const agent_context_impl_;
  const bool terminating_;  // is the agent runner terminating?
};

AgentContextImpl::AgentContextImpl(const AgentContextInfo& info,
                                   fuchsia::modular::AppConfig agent_config,
                                   inspect::Node agent_node)
    : url_(agent_config.url),
      component_context_impl_(info.component_context_info, kAgentComponentNamespace, url_, url_),
      agent_runner_(info.component_context_info.agent_runner),
      token_manager_(info.token_manager),
      entity_provider_runner_(info.component_context_info.entity_provider_runner),
      agent_services_factory_(info.agent_services_factory),
      agent_node_(std::move(agent_node)) {
  agent_runner_->PublishAgentServices(url_, &service_provider_impl_);
  service_provider_impl_.AddService<fuchsia::modular::ComponentContext>(
      [this](fidl::InterfaceRequest<fuchsia::modular::ComponentContext> request) {
        component_context_impl_.Connect(std::move(request));
      });
  service_provider_impl_.AddService<fuchsia::modular::AgentContext>(
      [this](fidl::InterfaceRequest<fuchsia::modular::AgentContext> request) {
        agent_context_bindings_.AddBinding(this, std::move(request));
      });
  if (info.sessionmgr_context != nullptr) {
    service_provider_impl_.AddService<fuchsia::intl::PropertyProvider>(
        [info](fidl::InterfaceRequest<fuchsia::intl::PropertyProvider> request) {
          info.sessionmgr_context->svc()->Connect<fuchsia::intl::PropertyProvider>(
              std::move(request));
        });
  }
  operation_queue_.Add(
      std::make_unique<InitializeCall>(this, info.launcher, std::move(agent_config)));
}

AgentContextImpl::~AgentContextImpl() = default;

void AgentContextImpl::ConnectToService(
    std::string requestor_url,
    fidl::InterfaceRequest<fuchsia::modular::AgentController> agent_controller_request,
    std::string service_name, ::zx::channel channel) {
  // Run this task on the operation queue to ensure that all member variables are
  // fully initialized before we query their state.
  operation_queue_.Add(std::make_unique<SyncCall>(
      [this, requestor_url, agent_controller_request = std::move(agent_controller_request),
       service_name, channel = std::move(channel)]() mutable {
        FXL_CHECK(state_ == State::RUNNING);

        if (agent_outgoing_services_.count(service_name) > 0) {
          app_client_->services().ConnectToService(std::move(channel), service_name);
        } else {
          fuchsia::sys::ServiceProviderPtr agent_services;
          agent_->Connect(requestor_url, agent_services.NewRequest());
          agent_services->ConnectToService(service_name, std::move(channel));
        }

        // Add a binding to the |controller|. When all the bindings go away,
        // the agent will stop.
        agent_controller_bindings_.AddBinding(this, std::move(agent_controller_request));
      }));
}

void AgentContextImpl::NewAgentConnection(
    const std::string& requestor_url,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services_request,
    fidl::InterfaceRequest<fuchsia::modular::AgentController> agent_controller_request) {
  // Queue adding the connection
  operation_queue_.Add(std::make_unique<SyncCall>(
      [this, requestor_url, incoming_services_request = std::move(incoming_services_request),
       agent_controller_request = std::move(agent_controller_request)]() mutable {
        FXL_CHECK(state_ == State::RUNNING);

        agent_->Connect(requestor_url, std::move(incoming_services_request));

        // Add a binding to the |controller|. When all the bindings go away,
        // the agent will stop.
        agent_controller_bindings_.AddBinding(this, std::move(agent_controller_request));
      }));
}

void AgentContextImpl::NewEntityProviderConnection(
    fidl::InterfaceRequest<fuchsia::modular::EntityProvider> entity_provider_request,
    fidl::InterfaceRequest<fuchsia::modular::AgentController> agent_controller_request) {
  operation_queue_.Add(std::make_unique<SyncCall>(
      [this, entity_provider_request = std::move(entity_provider_request),
       agent_controller_request = std::move(agent_controller_request)]() mutable {
        FXL_CHECK(state_ == State::RUNNING);
        app_client_->services().ConnectToService(std::move(entity_provider_request));
        agent_controller_bindings_.AddBinding(this, std::move(agent_controller_request));
      }));
}

void AgentContextImpl::GetComponentContext(
    fidl::InterfaceRequest<fuchsia::modular::ComponentContext> request) {
  component_context_impl_.Connect(std::move(request));
}

void AgentContextImpl::GetTokenManager(
    fidl::InterfaceRequest<fuchsia::auth::TokenManager> request) {
  token_manager_bindings_.AddBinding(this, std::move(request));
}

void AgentContextImpl::GetEntityReferenceFactory(
    fidl::InterfaceRequest<fuchsia::modular::EntityReferenceFactory> request) {
  entity_provider_runner_->ConnectEntityReferenceFactory(url_, std::move(request));
}

void AgentContextImpl::Authorize(
    fuchsia::auth::AppConfig app_config,
    fidl::InterfaceHandle<fuchsia::auth::AuthenticationUIContext> auth_ui_context,
    std::vector<::std::string> app_scopes, fidl::StringPtr user_profile_id,
    fidl::StringPtr auth_code, AuthorizeCallback callback) {
  FXL_LOG(ERROR) << "AgentContextImpl::Authorize() not supported from agent "
                 << "context";
  callback(fuchsia::auth::Status::INVALID_REQUEST, nullptr);
}

void AgentContextImpl::GetAccessToken(fuchsia::auth::AppConfig app_config,
                                      std::string user_profile_id,
                                      std::vector<::std::string> app_scopes,
                                      GetAccessTokenCallback callback) {
  FXL_CHECK(token_manager_);

  FXL_DLOG(INFO) << "AgentContextImpl::GetAccessToken() invoked for user:" << user_profile_id;
  token_manager_->GetAccessToken(std::move(app_config), std::move(user_profile_id),
                                 std::move(app_scopes), std::move(callback));
}

void AgentContextImpl::GetIdToken(fuchsia::auth::AppConfig app_config, std::string user_profile_id,
                                  fidl::StringPtr audience, GetIdTokenCallback callback) {
  FXL_CHECK(token_manager_);

  FXL_DLOG(INFO) << "AgentContextImpl::GetIdToken() invoked for user:" << user_profile_id;
  token_manager_->GetIdToken(std::move(app_config), std::move(user_profile_id), std::move(audience),
                             std::move(callback));
}

void AgentContextImpl::DeleteAllTokens(fuchsia::auth::AppConfig app_config,
                                       std::string user_profile_id, bool force,
                                       DeleteAllTokensCallback callback) {
  FXL_LOG(ERROR) << "AgentContextImpl::DeleteAllTokens() not supported from "
                 << "agent context";
  callback(fuchsia::auth::Status::INVALID_REQUEST);
}

void AgentContextImpl::ListProfileIds(fuchsia::auth::AppConfig app_config,
                                      ListProfileIdsCallback callback) {
  FXL_CHECK(token_manager_);

  token_manager_->ListProfileIds(std::move(app_config), std::move(callback));
}

void AgentContextImpl::StopAgentIfIdle() {
  // See if this agent is in the agent service index. If so, and to facilitate components
  // with connections to the agent made through the environment and without associated
  // AgentControllers, short-circuit the usual idle cleanup and leave us running.
  if (agent_runner_->AgentInServiceIndex(url_)) {
    return;
  }

  operation_queue_.Add(std::make_unique<StopCall>(false /* is agent runner terminating? */, this,
                                                  [this](bool stopped) {
                                                    if (stopped) {
                                                      agent_runner_->RemoveAgent(url_);
                                                      // |this| is no longer valid at this
                                                      // point.
                                                    }
                                                  }));
}

void AgentContextImpl::StopForTeardown(fit::function<void()> callback) {
  FXL_DLOG(INFO) << "AgentContextImpl::StopForTeardown() " << url_;
  operation_queue_.Add(
      std::make_unique<StopCall>(true /* is agent runner terminating? */, this,
                                 [this, callback = std::move(callback)](bool stopped) {
                                   FXL_DCHECK(stopped);
                                   agent_runner_->RemoveAgent(url_);
                                   callback();
                                   // |this| is no longer valid at this
                                   // point.
                                 }));
}

}  // namespace modular

#include "guidexos/appmodel/application.hpp"

#include "platform/platform_backend.hpp"
#include "runtime.hpp"

#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {

Application::Application(std::string applicationId, ShutdownMode shutdownMode)
    : state_(std::make_shared<detail::ApplicationState>(std::move(applicationId),
                                                         shutdownMode)) {
    if (state_->id.empty()) {
        throw std::invalid_argument("Application identity must not be empty");
    }
    state_->backend = detail::CreatePlatformBackend(state_);
    if (!state_->backend) {
        throw std::runtime_error("No platform backend is available");
    }
}

Application::~Application() {
    if (state_ && !state_->shutdown) {
        state_->backend->Shutdown();
        state_->shutdown = true;
    }
}

const std::string& Application::GetId() const noexcept {
    return state_->id;
}

ShutdownMode Application::GetShutdownMode() const noexcept {
    return state_->shutdownMode;
}

std::size_t Application::GetLiveWindowCount() const noexcept {
    return detail::GetLiveWindowCount(state_);
}

int Application::Run() {
    return detail::RunApplication(state_);
}

void Application::Quit(int exitCode) noexcept {
    detail::RequestQuit(state_, exitCode);
}

} // namespace guidexos::appmodel

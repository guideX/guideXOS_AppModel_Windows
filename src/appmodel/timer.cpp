#include "guidexos/appmodel/timer.hpp"

#include "guidexos/appmodel/application.hpp"
#include "platform/platform_backend.hpp"
#include "runtime.hpp"

#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {
namespace {

void ValidateInterval(std::chrono::milliseconds interval) {
    if (interval.count() <= 0) {
        throw std::invalid_argument("Timer interval must be positive");
    }
}

std::shared_ptr<detail::ApplicationState> RequireApplication(
    const std::shared_ptr<detail::TimerState>& state) {
    if (!state) throw std::logic_error("Timer is invalid");
    const auto application = state->application.lock();
    if (!application || application->shutdown || !application->backend) {
        throw std::logic_error("Timer Application is unavailable");
    }
    return application;
}

} // namespace

Timer::Timer(Application& application, std::chrono::milliseconds interval)
    : state_(std::make_shared<detail::TimerState>(application.state_, interval)) {
    ValidateInterval(interval);
    if (!application.state_) throw std::logic_error("Application is invalid");
}

Timer::~Timer() {
    Stop();
    if (state_) state_->onTick = {};
}

void Timer::SetInterval(std::chrono::milliseconds interval) {
    ValidateInterval(interval);
    if (!state_) throw std::logic_error("Timer is invalid");
    if (state_->interval == interval) return;

    const bool wasRunning = state_->running;
    std::shared_ptr<detail::ApplicationState> application;
    if (wasRunning) {
        application = RequireApplication(state_);
        state_->running = false;
        application->backend->StopTimer(state_);
    }

    state_->interval = interval;
    if (wasRunning) {
        if (!application->backend->StartTimer(state_)) {
            throw std::runtime_error("Unable to restart App Model Timer");
        }
        state_->running = true;
    }
}

std::chrono::milliseconds Timer::GetInterval() const noexcept {
    return state_ ? state_->interval : std::chrono::milliseconds::zero();
}

void Timer::Start() {
    if (!state_) throw std::logic_error("Timer is invalid");
    if (state_->running) return;

    const auto application = RequireApplication(state_);
    if (!application->backend->StartTimer(state_)) {
        throw std::runtime_error(
            "Unable to start App Model Timer; the interval may be too large");
    }
    state_->running = true;
}

void Timer::Stop() noexcept {
    if (!state_ || !state_->running) return;
    state_->running = false;
    if (const auto application = state_->application.lock()) {
        if (application->backend && !application->shutdown) {
            application->backend->StopTimer(state_);
        }
    }
}

bool Timer::IsRunning() const noexcept {
    return state_ && state_->running;
}

void Timer::OnTick(std::function<void()> callback) {
    if (!state_) throw std::logic_error("Timer is invalid");
    state_->onTick = std::move(callback);
}

} // namespace guidexos::appmodel

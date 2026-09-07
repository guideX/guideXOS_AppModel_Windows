#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace guidexos::appmodel {

class Application;

namespace detail {
struct TimerState;
}

// A repeating application timer. Tick callbacks run synchronously on the
// Application event-loop thread; the native scheduling mechanism is private
// to the selected backend.
class Timer final {
public:
    explicit Timer(
        Application& application,
        std::chrono::milliseconds interval = std::chrono::seconds(1));
    ~Timer();

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    void SetInterval(std::chrono::milliseconds interval);
    std::chrono::milliseconds GetInterval() const noexcept;

    // Start and Stop are idempotent. Start requires a live Application
    // backend and schedules a repeating tick; Stop also suppresses any
    // already-queued native tick before its callback is dispatched.
    void Start();
    void Stop() noexcept;
    bool IsRunning() const noexcept;

    // Replaces the current callback. Passing an empty callback unsubscribes.
    // The callback is copied before each tick, so it may stop or reconfigure
    // this timer safely.
    void OnTick(std::function<void()> callback);

private:
    std::shared_ptr<detail::TimerState> state_;
};

} // namespace guidexos::appmodel

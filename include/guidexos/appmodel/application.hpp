#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace guidexos::appmodel {

enum class ShutdownMode {
    // The event loop ends when the last shown App Model window closes.
    WhenLastWindowCloses,
    // Closing windows does not end the event loop; call Quit() explicitly.
    Explicit,
};

namespace detail {
struct ApplicationState;
}

class Application final {
public:
    explicit Application(
        std::string applicationId = "com.guidexos.app",
        ShutdownMode shutdownMode = ShutdownMode::WhenLastWindowCloses);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    const std::string& GetId() const noexcept;
    ShutdownMode GetShutdownMode() const noexcept;

    // Returns the number of currently shown App Model windows. A closed
    // window is not counted and may be shown again before shutdown.
    std::size_t GetLiveWindowCount() const noexcept;

    // Runs the platform event loop until the configured shutdown policy is
    // satisfied or Quit() is called. Run() must be called on the creating
    // thread and is not a background dispatcher.
    int Run();

    // Requests orderly event-loop termination regardless of the number of
    // live windows. The request is safe to make from a UI callback;
    // cross-thread use is not part of this milestone.
    void Quit(int exitCode = 0) noexcept;

private:
    std::shared_ptr<detail::ApplicationState> state_;

    friend class Window;
};

} // namespace guidexos::appmodel

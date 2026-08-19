#pragma once

#include <memory>
#include <string>

namespace guidexos::appmodel {

namespace detail {
struct StatusBarState;
}

// A single-text window-chrome status bar. StatusBar is a shared model handle,
// but its state can be attached to only one Window at a time.
class StatusBar final {
public:
    explicit StatusBar(std::string text = {});
    ~StatusBar() = default;

    StatusBar(const StatusBar&) = default;
    StatusBar& operator=(const StatusBar&) = default;
    StatusBar(StatusBar&&) noexcept = default;
    StatusBar& operator=(StatusBar&&) noexcept = default;

    void SetText(std::string text);
    std::string GetText() const;

private:
    std::shared_ptr<detail::StatusBarState> state_;

    friend class Window;
};

} // namespace guidexos::appmodel

#include "guidexos/appmodel/window.hpp"

#include "guidexos/appmodel/application.hpp"
#include "guidexos/appmodel/controls.hpp"
#include "guidexos/appmodel/layout.hpp"
#include "guidexos/appmodel/menu.hpp"
#include "guidexos/appmodel/status_bar.hpp"
#include "runtime.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {

Window::Window(Application& application)
    : state_(std::make_shared<detail::WindowState>(application.state_)) {
    detail::RegisterWindow(application.state_, state_);
}

Window::~Window() {
    detail::UnregisterWindow(state_);
}

void Window::SetTitle(std::string title) {
    state_->title = std::move(title);
    detail::NotifyWindowChanged(state_);
}

void Window::SetSize(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Window dimensions must be positive");
    }
    state_->width = width;
    state_->height = height;
    detail::NotifyWindowSizeChanged(state_);
}

void Window::SetContent(const Layout& layout) {
    const auto application = state_->application.lock();
    if (!layout.state_) throw std::logic_error("Window content Layout is invalid");
    if (auto parent = layout.state_->parent.lock()) {
        (void)parent;
        throw std::logic_error("A child Layout cannot be used as window content");
    }
    if (auto owner = layout.state_->contentWindow.lock(); owner && owner != state_) {
        throw std::logic_error("A Layout cannot be content for multiple Windows");
    }
    detail::BindLayoutToApplication(layout.state_, application);
    if (state_->content && state_->content != layout.state_) {
        state_->content->contentWindow.reset();
    }
    layout.state_->contentWindow = state_;
    state_->content = layout.state_;
    detail::NotifyWindowChanged(state_);
}

void Window::SetMenuBar(const MenuBar& menuBar) {
    if (!menuBar.state_) throw std::logic_error("Window MenuBar is invalid");
    const auto application = state_->application.lock();
    detail::BindMenuBarToApplication(menuBar.state_, application, state_);
    if (state_->menuBar == menuBar.state_) {
        detail::NotifyWindowChanged(state_);
        return;
    }

    if (state_->menuBar) {
        detail::DetachMenuBarFromWindow(state_->menuBar);
    }
    state_->menuBar = menuBar.state_;
    detail::NotifyWindowChanged(state_);
}

void Window::ClearMenuBar() {
    if (!state_->menuBar) return;
    detail::DetachMenuBarFromWindow(state_->menuBar);
    state_->menuBar.reset();
    detail::NotifyWindowChanged(state_);
}

void Window::SetStatusBar(const StatusBar& statusBar) {
    if (!statusBar.state_) throw std::logic_error("Window StatusBar is invalid");
    const auto application = state_->application.lock();
    detail::BindStatusBarToApplication(statusBar.state_, application, state_);
    if (state_->statusBar == statusBar.state_) {
        detail::NotifyWindowChanged(state_);
        return;
    }

    if (state_->statusBar) {
        detail::DetachStatusBarFromWindow(state_->statusBar);
    }
    state_->statusBar = statusBar.state_;
    detail::NotifyWindowChanged(state_);
}

void Window::ClearStatusBar() {
    if (!state_->statusBar) return;
    detail::DetachStatusBarFromWindow(state_->statusBar);
    state_->statusBar.reset();
    detail::NotifyWindowChanged(state_);
}

bool Window::Show() {
    return detail::ShowWindow(state_);
}

ControlRef Window::GetFocusedControl() const noexcept {
    return ControlRef{detail::GetFocusedControl(state_)};
}

void Window::Close() noexcept {
    detail::RequestWindowClose(state_);
}

void Window::OnClosing(std::function<void(WindowClosingEvent&)> callback) {
    state_->onClosing = std::move(callback);
}

void Window::OnFilesDropped(
    std::function<void(const FileDropEvent&)> callback) {
    state_->onFilesDropped = std::move(callback);
    detail::NotifyWindowChanged(state_);
}

bool Window::IsShown() const noexcept {
    return detail::IsWindowShown(state_);
}

} // namespace guidexos::appmodel

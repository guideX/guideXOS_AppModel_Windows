#pragma once

#include "file_drop.hpp"

#include <functional>
#include <memory>
#include <string>

namespace guidexos::appmodel {

class Application;
class ControlRef;
class Layout;
class MenuBar;
class MessageDialog;
class OpenFileDialog;
class SaveFileDialog;
class StatusBar;

class WindowClosingEvent final {
public:
    WindowClosingEvent() noexcept = default;

    void Cancel() noexcept { canceled_ = true; }
    bool IsCanceled() const noexcept { return canceled_; }

private:
    bool canceled_ = false;
};

namespace detail {
struct WindowState;
#if defined(GUIDEXOS_FILE_DROP_MODEL_TEST_ACCESS)
struct WindowStateTestAccess;
#endif
}

class Window final {
public:
    explicit Window(Application& application);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    void SetTitle(std::string title);
    void SetSize(int width, int height);
    void SetContent(const Layout& layout);
    void SetMenuBar(const MenuBar& menuBar);
    void ClearMenuBar();
    void SetStatusBar(const StatusBar& statusBar);
    void ClearStatusBar();

    // Creates and shows the native top-level window. The native handle and
    // message details remain private to the selected platform backend.
    bool Show();

    // Returns the logical control that currently owns keyboard focus in this
    // Window. The invalid default ControlRef means that this window is
    // hidden, inactive, modal-blocked, or has no focused child.
    ControlRef GetFocusedControl() const noexcept;

    // Requests closure of this window. If OnClosing cancels the request, the
    // native realization remains alive. Otherwise it is destroyed. It is
    // harmless to call on a window that is already closed. The App Model
    // object remains valid and Show() may realize it again before its
    // Application shuts down.
    void Close() noexcept;

    // Replaces the close-request callback. Passing an empty callback clears
    // it. The callback runs synchronously for both Window::Close() and a
    // native platform close request. Calling event.Cancel() keeps the
    // window open; otherwise native destruction is committed after the
    // callback returns. A request made while this callback is executing is
    // ignored for this window.
    void OnClosing(std::function<void(WindowClosingEvent&)> callback);

    // Replaces the shell file-drop callback. Passing an empty callback clears
    // it and disables native file-drop acceptance for this realized Window.
    // The callback runs synchronously on the application event-loop thread,
    // after all accepted UTF-8 paths have been copied into an immutable event.
    // The callback remains registered across Close()/Show() and may update
    // controls, show dialogs, read files, replace itself, or close windows.
    void OnFilesDropped(std::function<void(const FileDropEvent&)> callback);

    bool IsShown() const noexcept;

private:
    std::shared_ptr<detail::WindowState> state_;

    friend class MessageDialog;
    friend class OpenFileDialog;
    friend class SaveFileDialog;
#if defined(GUIDEXOS_FILE_DROP_MODEL_TEST_ACCESS)
    friend struct detail::WindowStateTestAccess;
#endif
};

} // namespace guidexos::appmodel

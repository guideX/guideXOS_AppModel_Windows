#pragma once

#include "guidexos/appmodel/dialogs.hpp"

#include <memory>
#include <optional>
#include <string>

namespace guidexos::appmodel::detail {

struct ApplicationState;
struct ControlState;
struct LayoutState;
struct WindowState;

class PlatformBackend {
public:
    virtual ~PlatformBackend() = default;

    virtual bool ShowWindow(const std::shared_ptr<WindowState>& window) = 0;
    virtual bool FocusControl(const std::shared_ptr<WindowState>& window,
                              const std::shared_ptr<ControlState>& control) = 0;
    virtual void RefreshWindow(const std::shared_ptr<WindowState>& window) = 0;
    virtual void RefreshMenuBar(const std::shared_ptr<WindowState>& window) = 0;
    virtual void ResizeWindow(const std::shared_ptr<WindowState>& window) = 0;
    virtual void CloseWindow(const std::shared_ptr<WindowState>& window) noexcept = 0;
    virtual int Run() = 0;
    virtual void RequestQuit(int exitCode) noexcept = 0;
    virtual void Shutdown() noexcept = 0;

    virtual MessageDialogResult ShowMessageDialog(
        const std::shared_ptr<WindowState>& owner,
        const std::string& message, const std::string& title,
        MessageDialogButtons buttons, MessageDialogIcon icon) = 0;
    virtual std::optional<std::string> ShowOpenFileDialog(
        const std::shared_ptr<WindowState>& owner,
        const std::string& title,
        const std::optional<std::string>& initialDirectory,
        const std::vector<FileDialogFilter>& filters) = 0;
    virtual std::optional<std::string> ShowSaveFileDialog(
        const std::shared_ptr<WindowState>& owner,
        const std::string& title, const std::string& suggestedFileName,
        const std::optional<std::string>& initialDirectory,
        const std::vector<FileDialogFilter>& filters) = 0;
};

std::unique_ptr<PlatformBackend> CreatePlatformBackend(
    const std::shared_ptr<ApplicationState>& application);

} // namespace guidexos::appmodel::detail

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "platform/platform_backend.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace guidexos::appmodel::detail {

class WindowsBackend final : public PlatformBackend {
public:
    explicit WindowsBackend(std::shared_ptr<ApplicationState> application);
    ~WindowsBackend() override;

    bool ShowWindow(const std::shared_ptr<WindowState>& window) override;
    bool FocusControl(const std::shared_ptr<WindowState>& window,
                      const std::shared_ptr<ControlState>& control) override;
    void RefreshWindow(const std::shared_ptr<WindowState>& window) override;
    void RefreshMenuBar(const std::shared_ptr<WindowState>& window) override;
    void ResizeWindow(const std::shared_ptr<WindowState>& window) override;
    void CloseWindow(const std::shared_ptr<WindowState>& window) noexcept override;
    bool StartTimer(const std::shared_ptr<TimerState>& timer) override;
    void StopTimer(const std::shared_ptr<TimerState>& timer) noexcept override;
    int Run() override;
    void RequestQuit(int exitCode) noexcept override;
    void Shutdown() noexcept override;
    MessageDialogResult ShowMessageDialog(
        const std::shared_ptr<WindowState>& owner,
        const std::string& message, const std::string& title,
        MessageDialogButtons buttons, MessageDialogIcon icon) override;
    std::optional<std::string> ShowOpenFileDialog(
        const std::shared_ptr<WindowState>& owner,
        const std::string& title,
        const std::optional<std::string>& initialDirectory,
        const std::vector<FileDialogFilter>& filters) override;
    std::optional<std::string> ShowSaveFileDialog(
        const std::shared_ptr<WindowState>& owner,
        const std::string& title, const std::string& suggestedFileName,
        const std::optional<std::string>& initialDirectory,
        const std::vector<FileDialogFilter>& filters) override;

private:
    struct ChildBinding;
    struct ToolTipBinding;
    struct WindowBinding;

    bool RegisterWindowClass();
    bool RegisterImageWindowClass();
    bool RegisterScrollViewClass();
    WindowBinding* FindWindowBinding(HWND hwnd) noexcept;
    WindowBinding* FindWindowBinding(const std::shared_ptr<WindowState>& window) noexcept;
    void RebuildControls(WindowBinding& binding);
    void DestroyToolTips(WindowBinding& binding) noexcept;
    void RebuildToolTips(WindowBinding& binding);
    void RebuildStatusBar(WindowBinding& binding);
    void SynchronizeStatusBar(WindowBinding& binding);
    void DestroyStatusBar(WindowBinding& binding) noexcept;
    void RebuildMenuBar(WindowBinding& binding);
    void DestroyMenuBar(WindowBinding& binding) noexcept;
    UINT AllocateMenuCommandId();
    void SynchronizeCheckBox(ChildBinding& binding,
                             const ControlState& control);
    void SynchronizeTextEdit(ChildBinding& binding,
                             const ControlState& control);
    void SynchronizeTextAreaProperties(ChildBinding& binding,
                                       const ControlState& control);
    void SynchronizeListBox(ChildBinding& binding, const ControlState& control);
    void SynchronizeComboBox(ChildBinding& binding, const ControlState& control);
    void SynchronizeProgressBar(ChildBinding& binding,
                                const ControlState& control);
    void SynchronizeSlider(ChildBinding& binding,
                           const ControlState& control);
    void SynchronizeTabView(ChildBinding& binding,
                            const ControlState& control);
    void SynchronizeImage(ChildBinding& binding,
                          const ControlState& control);
    void SynchronizeRadioButton(ChildBinding& binding,
                                const ControlState& control);
    void LayoutControls(WindowBinding& binding);
    int GetStatusBarHeight(const WindowBinding& binding) const noexcept;
    void HandleTimer(UINT_PTR timerId) noexcept;
    UINT_PTR FindTimerId(const std::shared_ptr<TimerState>& timer) const noexcept;
    void HandleNativeDestroyed(HWND hwnd) noexcept;
    void HandleScrollViewMessage(HWND hwnd, UINT message, WPARAM wParam,
                                 LPARAM lParam) noexcept;
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam,
                          LPARAM lParam) noexcept;

    static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam) noexcept;
    static LRESULT CALLBACK ImageWindowProcedure(HWND hwnd, UINT message,
                                                 WPARAM wParam,
                                                 LPARAM lParam) noexcept;
    static LRESULT CALLBACK ScrollViewWindowProcedure(HWND hwnd, UINT message,
                                                      WPARAM wParam,
                                                      LPARAM lParam) noexcept;

    struct ComInitialization;
    struct NativeImageDecoder;

    std::weak_ptr<ApplicationState> application_;
    HINSTANCE instance_{};
    ATOM classAtom_{};
    ATOM imageClassAtom_{};
    ATOM scrollViewClassAtom_{};
    bool registeredClass_{false};
    bool imageClassRegistered_{false};
    bool scrollViewClassRegistered_{false};
    bool shutdown_{false};
    int nextControlId_{1000};
    std::uint32_t nextMenuCommandId_{0x4000};
    UINT_PTR nextTimerId_{1};
    bool commonControlsReady_{false};
    std::map<HWND, std::unique_ptr<WindowBinding>> windows_;
    std::unordered_map<UINT_PTR, std::weak_ptr<TimerState>> timers_;
    std::unique_ptr<ComInitialization> comInitialization_;
    std::unique_ptr<NativeImageDecoder> imageDecoder_;
};

} // namespace guidexos::appmodel::detail

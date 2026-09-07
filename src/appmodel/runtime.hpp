#pragma once

#include "guidexos/appmodel/application.hpp"
#include "guidexos/appmodel/controls.hpp"
#include "guidexos/appmodel/dialogs.hpp"
#include "guidexos/appmodel/file_drop.hpp"
#include "guidexos/appmodel/layout.hpp"
#include "guidexos/appmodel/menu.hpp"
#include "platform/platform_backend.hpp"

#include <functional>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace guidexos::appmodel {

class WindowClosingEvent;

namespace detail {

enum class ControlKind {
    Label,
    Button,
    CheckBox,
    TextBox,
    TextArea,
    ListBox,
    ComboBox,
    ProgressBar,
    RadioButton,
};

struct ApplicationState;
struct ControlState;
struct RadioGroupState;
struct LayoutState;
struct WindowState;
struct MenuBarState;
struct MenuState;
struct MenuItemState;
struct StatusBarState;

enum class WindowCloseState {
    Closed,
    Open,
    CloseRequested,
    Closing,
};

struct LayoutMeasurement {
    LayoutSize natural;
    LayoutSize minimum;
};

// A backend can provide native information for a realized control without
// exposing native types through the App Model. Geometry remains usable with
// the neutral fallback when no provider is supplied.
class LayoutMeasurementProvider {
public:
    virtual ~LayoutMeasurementProvider() = default;

    virtual LayoutMeasurement Measure(const ControlState& control) const = 0;
};

struct ControlState {
    explicit ControlState(ControlKind controlKind, std::string initialText)
        : kind(controlKind), text(std::move(initialText)) {}

    ControlKind kind;
    std::string text;
    std::string toolTip;
    std::vector<std::string> items;
    std::optional<std::size_t> selectedIndex;
    bool checked = false;
    bool selected = false;
    int minimum = 0;
    int maximum = 100;
    int value = 0;
    bool indeterminate = false;
    bool enabled = true;
    bool readOnly = false;
    bool wordWrap = true;
    std::size_t caretIndex = 0;
    TextRange selection{};
    std::weak_ptr<LayoutState> layoutParent;
    std::function<void()> onClick;
    std::function<void(const std::string&)> onTextChanged;
    std::function<void(std::optional<std::size_t>)> onSelectionChanged;
    std::function<void(bool)> onCheckedChanged;
    std::function<void(bool)> onSelectedChanged;
    std::weak_ptr<RadioGroupState> radioGroup;
    std::weak_ptr<ApplicationState> application;
    std::weak_ptr<WindowState> focusedWindow;
};

struct RadioGroupState {
    std::weak_ptr<ApplicationState> application;
    std::vector<std::weak_ptr<ControlState>> members;
    std::weak_ptr<ControlState> selected;
};

struct LayoutItem {
    std::shared_ptr<ControlState> control;
    std::shared_ptr<LayoutState> layout;
    LayoutSizing sizing = LayoutSizing::Natural;
    bool spacer = false;
};

struct LayoutState {
    LayoutState(Orientation layoutOrientation, int layoutPadding, int layoutSpacing)
        : padding(layoutPadding), spacing(layoutSpacing), orientation(layoutOrientation) {}

    int padding;
    int spacing;
    Orientation orientation;
    std::vector<LayoutItem> children;
    std::weak_ptr<ApplicationState> application;
    std::weak_ptr<LayoutState> parent;
    std::weak_ptr<WindowState> contentWindow;
};

struct MenuItemState {
    explicit MenuItemState(std::string initialText)
        : text(std::move(initialText)) {}

    std::string text;
    bool enabled = true;
    bool checked = false;
    std::optional<KeyShortcut> shortcut;
    std::function<void()> onInvoked;
    std::weak_ptr<MenuState> parent;
    std::weak_ptr<MenuBarState> menuBar;
    std::weak_ptr<ApplicationState> application;
};

struct MenuEntry {
    std::shared_ptr<MenuState> menu;
    std::shared_ptr<MenuItemState> item;
    bool separator = false;
};

struct MenuState {
    explicit MenuState(std::string initialText)
        : text(std::move(initialText)) {}

    std::string text;
    std::vector<MenuEntry> entries;
    std::function<void()> onOpening;
    std::weak_ptr<MenuState> parent;
    std::weak_ptr<MenuBarState> menuBar;
    std::weak_ptr<ApplicationState> application;
};

struct MenuBarState {
    std::vector<std::shared_ptr<MenuState>> menus;
    std::weak_ptr<WindowState> window;
    std::weak_ptr<ApplicationState> application;
};

struct StatusBarState {
    std::string text;
    std::weak_ptr<WindowState> window;
    std::weak_ptr<ApplicationState> application;
};

struct TimerState {
    TimerState(std::shared_ptr<ApplicationState> owner,
               std::chrono::milliseconds initialInterval)
        : application(std::move(owner)), interval(initialInterval) {}

    std::weak_ptr<ApplicationState> application;
    std::chrono::milliseconds interval;
    bool running = false;
    std::function<void()> onTick;
};

struct WindowState {
    explicit WindowState(std::shared_ptr<ApplicationState> owner)
        : application(owner) {}

    std::weak_ptr<ApplicationState> application;
    std::string title = "guideXOS App Model Window";
    int width = 800;
    int height = 500;
    std::shared_ptr<LayoutState> content;
    std::shared_ptr<MenuBarState> menuBar;
    std::shared_ptr<StatusBarState> statusBar;
    bool shown = false;
    WindowCloseState closeState = WindowCloseState::Closed;
    std::function<void(WindowClosingEvent&)> onClosing;
    std::function<void(const FileDropEvent&)> onFilesDropped;
    std::weak_ptr<ControlState> focusedControl;
};

struct ApplicationState {
    explicit ApplicationState(std::string applicationId, ShutdownMode mode)
        : id(std::move(applicationId)), shutdownMode(mode) {}

    ~ApplicationState();

    std::string id;
    ShutdownMode shutdownMode;
    std::unique_ptr<PlatformBackend> backend;
    std::vector<std::weak_ptr<WindowState>> windows;
    bool running = false;
    bool shutdown = false;
    int exitCode = 0;
};

void RegisterWindow(const std::shared_ptr<ApplicationState>& application,
                    const std::shared_ptr<WindowState>& window);
void UnregisterWindow(const std::shared_ptr<WindowState>& window) noexcept;

void NotifyWindowChanged(const std::shared_ptr<WindowState>& window);
void NotifyWindowSizeChanged(const std::shared_ptr<WindowState>& window);
void NotifyLayoutChanged(const std::shared_ptr<LayoutState>& layout);
void NotifyControlChanged(const std::shared_ptr<ControlState>& control);
void NotifyMenuChanged(const std::shared_ptr<MenuState>& menu);
void NotifyMenuItemChanged(const std::shared_ptr<MenuItemState>& item);
void NotifyStatusBarChanged(const std::shared_ptr<StatusBarState>& statusBar);
void BindLayoutToApplication(const std::shared_ptr<LayoutState>& layout,
                             const std::shared_ptr<ApplicationState>& application);
void BindRadioGroupToApplication(
    const std::shared_ptr<RadioGroupState>& group,
    const std::shared_ptr<ApplicationState>& application);
void BindMenuBarToApplication(
    const std::shared_ptr<MenuBarState>& menuBar,
    const std::shared_ptr<ApplicationState>& application,
    const std::shared_ptr<WindowState>& window);
void DetachMenuBarFromWindow(
    const std::shared_ptr<MenuBarState>& menuBar) noexcept;
void BindStatusBarToApplication(
    const std::shared_ptr<StatusBarState>& statusBar,
    const std::shared_ptr<ApplicationState>& application,
    const std::shared_ptr<WindowState>& window);
void DetachStatusBarFromWindow(
    const std::shared_ptr<StatusBarState>& statusBar) noexcept;

struct ControlPlacement {
    std::shared_ptr<ControlState> control;
    LayoutRect bounds;
};

std::vector<LayoutRect> CalculateDirectLayoutGeometry(
    const std::shared_ptr<LayoutState>& layout, LayoutRect bounds,
    const LayoutMeasurementProvider* provider = nullptr);
std::vector<ControlPlacement> CalculateControlPlacements(
    const std::shared_ptr<LayoutState>& layout, LayoutRect bounds,
    const LayoutMeasurementProvider* provider = nullptr);
LayoutMeasurement GetControlMeasurement(
    const std::shared_ptr<ControlState>& control,
    const LayoutMeasurementProvider* provider = nullptr);
LayoutMeasurement GetNeutralControlMeasurement(const ControlState& control);
LayoutMeasurement GetLayoutMeasurement(
    const std::shared_ptr<LayoutState>& layout,
    const LayoutMeasurementProvider* provider = nullptr);
void CollectControls(const std::shared_ptr<LayoutState>& layout,
                     std::vector<std::shared_ptr<ControlState>>& controls);

bool ShowWindow(const std::shared_ptr<WindowState>& window);
void RequestWindowClose(const std::shared_ptr<WindowState>& window) noexcept;
void CloseWindow(const std::shared_ptr<WindowState>& window) noexcept;
bool IsWindowShown(const std::shared_ptr<WindowState>& window) noexcept;
std::shared_ptr<ControlState> GetFocusedControl(
    const std::shared_ptr<WindowState>& window) noexcept;
bool IsControlFocused(const std::shared_ptr<ControlState>& control) noexcept;
bool FocusControl(const std::shared_ptr<ControlState>& control) noexcept;
void SetNativeFocus(const std::shared_ptr<WindowState>& window,
                    const std::shared_ptr<ControlState>& control) noexcept;
void ClearNativeFocus(const std::shared_ptr<WindowState>& window,
                      const std::shared_ptr<ControlState>& control) noexcept;
std::size_t GetLiveWindowCount(
    const std::shared_ptr<ApplicationState>& application) noexcept;
bool ShouldQuitAfterWindowClosed(
    const std::shared_ptr<ApplicationState>& application) noexcept;
void RequestQuit(const std::shared_ptr<ApplicationState>& application,
                 int exitCode) noexcept;
int RunApplication(const std::shared_ptr<ApplicationState>& application);

MessageDialogResult ShowMessageDialog(
    const std::shared_ptr<WindowState>& owner, const std::string& message,
    const std::string& title, MessageDialogButtons buttons,
    MessageDialogIcon icon);
std::optional<std::string> ShowOpenFileDialog(
    const std::shared_ptr<WindowState>& owner, const std::string& title,
    const std::optional<std::string>& initialDirectory,
    const std::vector<FileDialogFilter>& filters);
std::optional<std::string> ShowSaveFileDialog(
    const std::shared_ptr<WindowState>& owner, const std::string& title,
    const std::string& suggestedFileName,
    const std::optional<std::string>& initialDirectory,
    const std::vector<FileDialogFilter>& filters);

void DispatchButtonClick(const std::shared_ptr<ControlState>& control);
void DispatchTextChanged(const std::shared_ptr<ControlState>& control,
                         std::string text);
void DispatchSelectionChanged(const std::shared_ptr<ControlState>& control,
                              std::optional<std::size_t> index);
void DispatchSelectionCallback(const std::shared_ptr<ControlState>& control);
void DispatchCheckedChanged(const std::shared_ptr<ControlState>& control,
                            bool checked);
void DispatchRadioSelection(const std::shared_ptr<ControlState>& control);
void DispatchRadioSelectionChanged(const std::shared_ptr<ControlState>& control,
                                   bool selected);
void DispatchMenuItemInvocation(
    const std::shared_ptr<MenuItemState>& item);
void DispatchMenuItemAcceleratorInvocation(
    const std::shared_ptr<MenuItemState>& item);
void DispatchMenuOpening(const std::shared_ptr<MenuState>& menu);
void DispatchFilesDropped(const std::shared_ptr<WindowState>& window,
                          std::vector<std::string> paths);
void DispatchTimerTick(const std::shared_ptr<TimerState>& timer);
void RemoveRadioButtonFromGroup(const std::shared_ptr<ControlState>& control,
                                bool dispatchCallback);
void DestroyRadioGroup(const std::shared_ptr<RadioGroupState>& group) noexcept;
bool RadioGroupContains(const std::shared_ptr<RadioGroupState>& group,
                        const std::shared_ptr<ControlState>& control) noexcept;
std::size_t RadioGroupMemberCount(
    const std::shared_ptr<RadioGroupState>& group) noexcept;
std::optional<std::size_t> GetRadioGroupSelectedIndex(
    const std::shared_ptr<RadioGroupState>& group) noexcept;
void AddRadioButtonToGroup(const std::shared_ptr<RadioGroupState>& group,
                           const std::shared_ptr<ControlState>& control);
void SelectRadioButton(const std::shared_ptr<RadioGroupState>& group,
                       const std::shared_ptr<ControlState>& control);
void ClearRadioGroupSelection(const std::shared_ptr<RadioGroupState>& group);

} // namespace detail
} // namespace guidexos::appmodel

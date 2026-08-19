#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>
#include <optional>
#include <string>

using namespace guidexos::appmodel;

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (false)

namespace {

HWND FindChild(HWND parent, const wchar_t* title) {
    return FindWindowExW(parent, nullptr, L"Button", title);
}

int ControlId(HWND control) {
    return static_cast<int>(GetDlgCtrlID(control));
}

void NativeClick(HWND window, HWND control) {
    SendMessageW(window, WM_COMMAND,
                 MAKEWPARAM(ControlId(control), BN_CLICKED),
                 reinterpret_cast<LPARAM>(control));
}

} // namespace

int main() {
    Application app("com.guidexos.tests.choice-lifecycle");
    Window primary(app);
    Window secondary(app);
    primary.SetTitle("guideXOS Choice Routing Primary");
    secondary.SetTitle("guideXOS Choice Routing Secondary");
    primary.SetSize(640, 700);
    secondary.SetSize(640, 700);

    Label primaryStatus("primary ready");
    TextBox primaryMirror;
    CheckBox primaryNotifications("Primary notifications", true);
    CheckBox primaryStartup("Primary startup");
    RadioGroup primaryTheme;
    RadioButton primarySystem("System");
    RadioButton primaryLight("Light");
    RadioButton primaryDark("Dark");
    primaryTheme.Add(primarySystem);
    primaryTheme.Add(primaryLight);
    primaryTheme.Add(primaryDark);

    Label secondaryStatus("secondary ready");
    CheckBox secondaryNotifications("Secondary notifications");
    RadioGroup secondaryTheme;
    RadioButton secondarySystem("System");
    RadioButton secondaryDark("Dark");
    secondaryTheme.Add(secondarySystem);
    secondaryTheme.Add(secondaryDark);

    int primaryCheckEvents = 0;
    int secondaryCheckEvents = 0;
    bool closeOnNextPrimaryCheck = false;
    primaryNotifications.OnCheckedChanged([&](bool checked) {
        ++primaryCheckEvents;
        primaryStatus.SetText(checked ? "primary enabled" : "primary disabled");
        primaryMirror.SetText(checked ? "primary:on" : "primary:off");
        if (closeOnNextPrimaryCheck) {
            primary.Close();
            secondary.Close();
        }
    });
    primaryStartup.OnCheckedChanged([&](bool checked) {
        primaryStatus.SetText(checked ? "startup enabled" : "startup disabled");
    });
    secondaryNotifications.OnCheckedChanged([&](bool checked) {
        ++secondaryCheckEvents;
        secondaryStatus.SetText(checked ? "secondary enabled" : "secondary disabled");
    });
    primaryDark.OnSelectedChanged([&](bool selected) {
        if (selected) primaryStatus.SetText("primary dark selected");
    });
    secondaryDark.OnSelectedChanged([&](bool selected) {
        if (selected) secondaryStatus.SetText("secondary dark selected");
    });

    Layout primaryLayout;
    primaryLayout.Add(primaryStatus);
    primaryLayout.Add(primaryMirror);
    primaryLayout.Add(primaryNotifications);
    primaryLayout.Add(primaryStartup);
    primaryLayout.Add(primarySystem);
    primaryLayout.Add(primaryLight);
    primaryLayout.Add(primaryDark);
    primary.SetContent(primaryLayout);

    Layout secondaryLayout;
    secondaryLayout.Add(secondaryStatus);
    secondaryLayout.Add(secondaryNotifications);
    secondaryLayout.Add(secondarySystem);
    secondaryLayout.Add(secondaryDark);
    secondary.SetContent(secondaryLayout);

    assert(primary.Show());
    assert(secondary.Show());
    assert(app.GetLiveWindowCount() == 2);
    assert(primaryCheckEvents == 0);
    assert(secondaryCheckEvents == 0);

    const HWND primaryNative = FindWindowW(nullptr, L"guideXOS Choice Routing Primary");
    const HWND secondaryNative = FindWindowW(nullptr, L"guideXOS Choice Routing Secondary");
    const HWND primaryCheck = FindChild(primaryNative, L"Primary notifications");
    const HWND secondaryCheck = FindChild(secondaryNative, L"Secondary notifications");
    const HWND primaryDarkNative = FindChild(primaryNative, L"Dark");
    const HWND primaryLightNative = FindChild(primaryNative, L"Light");
    const HWND secondaryDarkNative = FindChild(secondaryNative, L"Dark");
    assert(primaryNative != nullptr);
    assert(secondaryNative != nullptr);
    assert(primaryCheck != nullptr);
    assert(secondaryCheck != nullptr);
    assert(primaryDarkNative != nullptr);
    assert(primaryLightNative != nullptr);
    assert(secondaryDarkNative != nullptr);
    assert(primaryNotifications.IsChecked());
    assert(!secondaryNotifications.IsChecked());
    assert(!primaryTheme.GetSelectedIndex());
    assert(!secondaryTheme.GetSelectedIndex());

    // A real child notification is routed through the owning top-level window.
    SendMessageW(primaryCheck, BM_SETCHECK, BST_UNCHECKED, 0);
    NativeClick(primaryNative, primaryCheck);
    assert(primaryNotifications.IsChecked() == false);
    assert(primaryCheckEvents == 1);
    assert(primaryStatus.GetText() == "primary disabled");
    assert(primaryMirror.GetText() == "primary:off");
    assert(!secondaryNotifications.IsChecked());
    assert(secondaryCheckEvents == 0);

    // Programmatic changes are still legal after native detachment and while
    // a control is disabled.
    primaryNotifications.SetEnabled(false);
    assert(!primaryNotifications.IsEnabled());
    SendMessageW(primaryCheck, BM_SETCHECK, BST_CHECKED, 0);
    NativeClick(primaryNative, primaryCheck);
    assert(!primaryNotifications.IsChecked());
    primaryNotifications.SetChecked(true);
    assert(primaryNotifications.IsChecked());
    assert(primaryCheckEvents == 2);
    assert(IsWindowEnabled(primaryCheck) == FALSE);
    primaryNotifications.SetEnabled(true);

    // Native radio selection updates the explicit group and preserves
    // independent group state in the other window.
    SendMessageW(primaryLightNative, BM_SETCHECK, BST_CHECKED, 0);
    NativeClick(primaryNative, primaryLightNative);
    assert(primaryTheme.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(!primarySystem.IsSelected());
    assert(primaryLight.IsSelected());
    assert(primaryDark.IsSelected() == false);
    assert(!secondaryTheme.GetSelectedIndex());
    assert(secondaryStatus.GetText() == "secondary ready");

    primaryTheme.Select(primaryDark);
    assert(primaryTheme.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(SendMessageW(primaryDarkNative, BM_GETCHECK, 0, 0) == BST_CHECKED);
    assert(SendMessageW(primaryLightNative, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    primaryDark.SetEnabled(false);
    assert(IsWindowEnabled(primaryDarkNative) == FALSE);
    SendMessageW(primaryDarkNative, BM_SETCHECK, BST_UNCHECKED, 0);
    NativeClick(primaryNative, primaryDarkNative);
    assert(primaryTheme.GetSelectedIndex() == std::optional<std::size_t>(2));
    primaryTheme.Select(primaryLight);
    assert(primaryTheme.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(primaryLight.IsSelected());
    primaryDark.SetEnabled(true);

    secondaryTheme.Select(secondaryDark);
    assert(secondaryTheme.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(primaryTheme.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(secondaryStatus.GetText() == "secondary dark selected");

    // Logical state survives closing and native recreation without an
    // initial callback or cross-window event.
    primary.Close();
    assert(!primary.IsShown());
    assert(secondary.IsShown());
    assert(app.GetLiveWindowCount() == 1);
    primaryNotifications.SetChecked(false);
    assert(primaryCheckEvents == 3);
    assert(primary.Show());
    assert(primary.IsShown());
    assert(primaryNotifications.IsChecked() == false);
    assert(primaryTheme.GetSelectedIndex() == std::optional<std::size_t>(1));
    const HWND reopenedPrimary = FindWindowW(
        nullptr, L"guideXOS Choice Routing Primary");
    const HWND reopenedCheck = FindChild(reopenedPrimary, L"Primary notifications");
    assert(reopenedPrimary != nullptr);
    assert(reopenedCheck != nullptr);
    assert(SendMessageW(reopenedCheck, BM_GETCHECK, 0, 0) == BST_UNCHECKED);

    // A callback may close both windows; the retained model is still safe to
    // query and can be realized again before Run() shuts the backend down.
    closeOnNextPrimaryCheck = true;
    primaryNotifications.SetChecked(true);
    assert(!primary.IsShown());
    assert(!secondary.IsShown());
    assert(app.GetLiveWindowCount() == 0);
    assert(primary.Show());
    assert(secondary.Show());
    assert(primaryNotifications.IsChecked());
    assert(secondaryTheme.GetSelectedIndex() == std::optional<std::size_t>(1));
    primary.Close();
    assert(secondary.IsShown());
    secondaryNotifications.SetChecked(true);
    assert(secondaryCheckEvents == 1);
    secondary.Close();
    assert(app.GetLiveWindowCount() == 0);
    assert(app.Run() == 0);
    return 0;
}

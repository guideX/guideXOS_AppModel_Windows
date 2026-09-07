#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>
#include <string>
#include <vector>

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

HWND FindTrackbar(HWND parent, HWND previous = nullptr) {
    return FindWindowExW(parent, previous, TRACKBAR_CLASSW, nullptr);
}

int TrackbarPosition(HWND trackbar) {
    return static_cast<int>(SendMessageW(trackbar, TBM_GETPOS, 0, 0));
}

void NotifySlider(HWND window, HWND trackbar, WORD notification) {
    SendMessageW(window, WM_HSCROLL,
                 MAKEWPARAM(notification, 0),
                 reinterpret_cast<LPARAM>(trackbar));
}

} // namespace

int main() {
    Application app("com.guidexos.tests.slider-lifecycle",
                    ShutdownMode::Explicit);
    Window primary(app);
    Window secondary(app);
    primary.SetTitle("guideXOS Slider Routing Primary");
    secondary.SetTitle("guideXOS Slider Routing Secondary");
    primary.SetSize(640, 360);
    secondary.SetSize(640, 360);

    Label primaryValue("primary ready");
    Slider primarySlider;
    primarySlider.SetMinimum(-100);
    primarySlider.SetMaximum(100);
    primarySlider.SetValue(-25);
    Label secondaryValue("secondary ready");
    Slider secondarySlider;
    secondarySlider.SetMinimum(0);
    secondarySlider.SetMaximum(100);
    secondarySlider.SetValue(75);

    int primaryEvents = 0;
    int secondaryEvents = 0;
    std::vector<int> primaryValues;
    primarySlider.OnChanged([&]() {
        ++primaryEvents;
        primaryValues.push_back(primarySlider.GetValue());
        primaryValue.SetText("primary " +
                            std::to_string(primarySlider.GetValue()));
    });
    secondarySlider.OnChanged([&]() {
        ++secondaryEvents;
        secondaryValue.SetText("secondary " +
                              std::to_string(secondarySlider.GetValue()));
    });

    Layout primaryLayout;
    primaryLayout.Add(primaryValue);
    primaryLayout.Add(primarySlider, LayoutSizing::Expand);
    primary.SetContent(primaryLayout);

    Layout secondaryLayout;
    secondaryLayout.Add(secondaryValue);
    secondaryLayout.Add(secondarySlider, LayoutSizing::Expand);
    secondary.SetContent(secondaryLayout);

    assert(primary.Show());
    assert(secondary.Show());
    const HWND primaryNative = FindWindowW(
        nullptr, L"guideXOS Slider Routing Primary");
    const HWND secondaryNative = FindWindowW(
        nullptr, L"guideXOS Slider Routing Secondary");
    assert(primaryNative != nullptr);
    assert(secondaryNative != nullptr);
    const HWND nativePrimarySlider = FindTrackbar(primaryNative);
    const HWND nativeSecondarySlider = FindTrackbar(secondaryNative);
    assert(nativePrimarySlider != nullptr);
    assert(nativeSecondarySlider != nullptr);
    assert(nativePrimarySlider != nativeSecondarySlider);

    RECT primaryRect{};
    assert(GetWindowRect(nativePrimarySlider, &primaryRect));
    assert(primaryRect.right - primaryRect.left >= 96);
    assert(primaryRect.bottom - primaryRect.top >= 24);
    assert((GetWindowLongPtrW(nativePrimarySlider, GWL_STYLE) & WS_TABSTOP) != 0);
    assert((GetWindowLongPtrW(nativePrimarySlider, GWL_STYLE) & TBS_VERT) == 0);

    assert(SendMessageW(nativePrimarySlider, TBM_GETRANGEMIN, 0, 0) == -100);
    assert(SendMessageW(nativePrimarySlider, TBM_GETRANGEMAX, 0, 0) == 100);
    assert(TrackbarPosition(nativePrimarySlider) == -25);
    assert(SendMessageW(nativeSecondarySlider, TBM_GETRANGEMIN, 0, 0) == 0);
    assert(SendMessageW(nativeSecondarySlider, TBM_GETRANGEMAX, 0, 0) == 100);
    assert(TrackbarPosition(nativeSecondarySlider) == 75);

    assert(primarySlider.Focus());
    assert(primarySlider.GetControlRef().HasFocus());

    primarySlider.SetValue(40);
    assert(TrackbarPosition(nativePrimarySlider) == 40);
    assert(primaryEvents == 1);
    assert(primaryValues.back() == 40);
    primarySlider.SetValue(40);
    assert(primaryEvents == 1);

    // A native position change is routed through the owning parent window.
    SendMessageW(nativePrimarySlider, TBM_SETPOS, TRUE, 55);
    NotifySlider(primaryNative, nativePrimarySlider, TB_THUMBTRACK);
    assert(primarySlider.GetValue() == 55);
    assert(primaryEvents == 2);
    NotifySlider(primaryNative, nativePrimarySlider, TB_THUMBPOSITION);
    NotifySlider(primaryNative, nativePrimarySlider, TB_ENDTRACK);
    assert(primaryEvents == 2);

    // Line/page and Home/End notifications use the native bounded position;
    // duplicate notifications remain one logical event.
    SendMessageW(nativePrimarySlider, TBM_SETPOS, TRUE, -10);
    NotifySlider(primaryNative, nativePrimarySlider, TB_LINEUP);
    assert(primarySlider.GetValue() == -10);
    assert(primaryEvents == 3);
    NotifySlider(primaryNative, nativePrimarySlider, TB_LINEUP);
    assert(primaryEvents == 3);
    SendMessageW(nativePrimarySlider, TBM_SETPOS, TRUE, 100);
    NotifySlider(primaryNative, nativePrimarySlider, TB_BOTTOM);
    assert(primarySlider.GetValue() == 100);
    assert(primaryEvents == 4);
    SendMessageW(nativePrimarySlider, TBM_SETPOS, TRUE, -100);
    NotifySlider(primaryNative, nativePrimarySlider, TB_TOP);
    assert(primarySlider.GetValue() == -100);
    assert(primaryEvents == 5);

    // Notification routing is independent across windows.
    SendMessageW(nativeSecondarySlider, TBM_SETPOS, TRUE, 12);
    NotifySlider(secondaryNative, nativeSecondarySlider, TB_THUMBTRACK);
    assert(secondarySlider.GetValue() == 12);
    assert(secondaryEvents == 1);
    assert(primarySlider.GetValue() == -100);
    assert(primaryEvents == 5);

    secondarySlider.SetEnabled(false);
    assert(!IsWindowEnabled(nativeSecondarySlider));
    SendMessageW(nativeSecondarySlider, TBM_SETPOS, TRUE, 88);
    NotifySlider(secondaryNative, nativeSecondarySlider, TB_THUMBTRACK);
    assert(secondarySlider.GetValue() == 12);
    assert(secondaryEvents == 1);
    secondarySlider.SetValue(88);
    assert(secondarySlider.GetValue() == 88);
    assert(TrackbarPosition(nativeSecondarySlider) == 88);
    secondarySlider.SetEnabled(true);
    assert(IsWindowEnabled(nativeSecondarySlider));

    // A callback can disable the control without invalidating backend state.
    primarySlider.OnChanged([&]() { primarySlider.SetEnabled(false); });
    primarySlider.SetValue(0);
    assert(primarySlider.GetValue() == 0);
    assert(!primarySlider.IsEnabled());
    assert(!IsWindowEnabled(nativePrimarySlider));
    primarySlider.SetEnabled(true);

    // Native children are destroyed and recreated while logical state and
    // callbacks remain retained.
    const HWND oldPrimarySlider = nativePrimarySlider;
    primary.Close();
    assert(!primary.IsShown());
    assert(!IsWindow(oldPrimarySlider));
    primarySlider.SetValue(25);
    assert(primarySlider.GetValue() == 25);
    primarySlider.SetEnabled(true);
    primarySlider.OnChanged([&]() {
        ++primaryEvents;
        primary.Close();
    });
    assert(primary.Show());
    const HWND reopenedPrimary = FindWindowW(
        nullptr, L"guideXOS Slider Routing Primary");
    const HWND reopenedSlider = FindTrackbar(reopenedPrimary);
    assert(reopenedPrimary != nullptr);
    assert(reopenedSlider != nullptr);
    assert(reopenedSlider != oldPrimarySlider);
    assert(TrackbarPosition(reopenedSlider) == 25);
    assert(primarySlider.IsEnabled());
    assert(IsWindowEnabled(reopenedSlider));

    // The callback may close its containing window during native dispatch.
    SendMessageW(reopenedSlider, TBM_SETPOS, TRUE, 26);
    NotifySlider(reopenedPrimary, reopenedSlider, TB_THUMBTRACK);
    assert(primarySlider.GetValue() == 26);
    assert(primaryEvents == 6);
    assert(!primary.IsShown());
    assert(secondary.IsShown());

    secondary.Close();
    assert(app.GetLiveWindowCount() == 0);
    app.Quit();
    assert(app.Run() == 0);
    assert(primarySlider.GetValue() == 26);
    assert(secondarySlider.GetValue() == 88);
    return 0;
}

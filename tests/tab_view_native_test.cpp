#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>

#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>
#include <iterator>
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

HWND FindChildByClass(HWND parent, const wchar_t* className,
                      HWND previous = nullptr) {
    return FindWindowExW(parent, previous, className, nullptr);
}

HWND FindChildByText(HWND parent, const wchar_t* className,
                     const wchar_t* text) {
    HWND current = nullptr;
    while ((current = FindChildByClass(parent, className, current)) != nullptr) {
        wchar_t buffer[256]{};
        GetWindowTextW(current, buffer, static_cast<int>(std::size(buffer)));
        if (std::wstring(buffer) == text) return current;
    }
    return nullptr;
}

std::wstring TabTitle(HWND tab, int index) {
    wchar_t buffer[256]{};
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = buffer;
    item.cchTextMax = static_cast<int>(std::size(buffer));
    if (!TabCtrl_GetItem(tab, index, &item)) return {};
    return buffer;
}

void NotifyTabSelection(HWND parent, HWND tab) {
    NMHDR header{tab, static_cast<UINT_PTR>(GetDlgCtrlID(tab)), TCN_SELCHANGE};
    SendMessageW(parent, WM_NOTIFY, static_cast<WPARAM>(header.idFrom),
                 reinterpret_cast<LPARAM>(&header));
}

bool RectContains(const RECT& outer, const RECT& inner) {
    return inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom;
}

} // namespace

int main() {
    Application app("com.guidexos.tests.tab-view-native", ShutdownMode::Explicit);
    Window primary(app);
    Window secondary(app);
    primary.SetTitle("guideXOS TabView Native Primary");
    secondary.SetTitle("guideXOS TabView Native Secondary");
    primary.SetSize(760, 520);
    secondary.SetSize(520, 320);

    TabView tabs;
    auto general = tabs.AddTab("General");
    auto network = tabs.AddTab("N\xC3\xA9twork \xF0\x9F\x8C\x90");
    auto diagnostics = tabs.AddTab("Diagnostics");
    TextBox generalBox("Alice");
    Slider generalSlider;
    generalSlider.SetValue(73);
    ProgressBar generalProgress;
    generalProgress.SetValue(73);
    Layout generalLayout = general.GetLayout();
    generalLayout.Add(generalBox);
    generalLayout.Add(generalSlider);
    generalLayout.Add(generalProgress);

    TextBox networkBox("server.test");
    Button connect("Connect");
    Layout networkLayout = network.GetLayout();
    networkLayout.Add(networkBox);
    networkLayout.Add(connect);

    TextArea diagnosticsLog("diagnostic state");
    Button diagnosticsButton("Start Test");
    Layout diagnosticsLayout = diagnostics.GetLayout();
    diagnosticsLayout.Add(diagnosticsButton);
    diagnosticsLayout.Add(diagnosticsLog, LayoutSizing::Expand);

    int selectionEvents = 0;
    std::optional<std::size_t> lastSelection;
    tabs.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++selectionEvents;
        lastSelection = index;
    });
    Layout primaryContent;
    primaryContent.Add(tabs, LayoutSizing::Expand);
    primary.SetContent(primaryContent);

    TabView otherTabs;
    auto otherPage = otherTabs.AddTab("Other");
    TextBox otherBox("secondary");
    otherPage.GetLayout().Add(otherBox);
    Layout secondaryContent;
    secondaryContent.Add(otherTabs, LayoutSizing::Expand);
    secondary.SetContent(secondaryContent);

    assert(primary.Show());
    assert(secondary.Show());
    const HWND primaryNative = FindWindowW(nullptr,
                                           L"guideXOS TabView Native Primary");
    const HWND secondaryNative = FindWindowW(
        nullptr, L"guideXOS TabView Native Secondary");
    assert(primaryNative != nullptr);
    assert(secondaryNative != nullptr);
    HWND nativeTab = FindChildByClass(primaryNative, WC_TABCONTROLW);
    const HWND otherNativeTab = FindChildByClass(secondaryNative, WC_TABCONTROLW);
    assert(nativeTab != nullptr);
    assert(otherNativeTab != nullptr);
    assert(TabCtrl_GetItemCount(nativeTab) == 3);
    assert(TabCtrl_GetItemCount(otherNativeTab) == 1);
    const auto title0 = TabTitle(nativeTab, 0);
    const auto title1 = TabTitle(nativeTab, 1);
    const auto title2 = TabTitle(nativeTab, 2);
    assert(title0 == L"General");
    assert(TabTitle(nativeTab, 1) == L"N\x00E9twork \xD83C\xDF10");
    assert(TabTitle(nativeTab, 2) == L"Diagnostics");
    assert(TabCtrl_GetCurSel(nativeTab) == 0);
    assert(TabCtrl_GetCurSel(otherNativeTab) == 0);

    const HWND nativeGeneralBox = FindChildByText(primaryNative, L"Edit", L"Alice");
    const HWND nativeNetworkBox = FindChildByText(
        primaryNative, L"Edit", L"server.test");
    assert(nativeGeneralBox != nullptr);
    assert(nativeNetworkBox != nullptr);
    assert(IsWindowVisible(nativeGeneralBox) != FALSE);
    assert(IsWindowVisible(nativeNetworkBox) == FALSE);

    RECT tabRect{};
    RECT contentRect{0, 0, 0, 0};
    assert(GetWindowRect(nativeTab, &tabRect));
    assert(GetClientRect(nativeTab, &contentRect));
    TabCtrl_AdjustRect(nativeTab, FALSE, &contentRect);
    POINT contentTopLeft{contentRect.left, contentRect.top};
    POINT contentBottomRight{contentRect.right, contentRect.bottom};
    assert(ClientToScreen(nativeTab, &contentTopLeft));
    assert(ClientToScreen(nativeTab, &contentBottomRight));
    contentRect = {contentTopLeft.x, contentTopLeft.y,
                   contentBottomRight.x, contentBottomRight.y};
    RECT generalBoxRect{};
    assert(GetWindowRect(nativeGeneralBox, &generalBoxRect));
    assert(RectContains(contentRect, generalBoxRect));

    assert(generalBox.Focus());
    assert(primary.GetFocusedControl() == generalBox.GetControlRef());
    tabs.SetSelectedIndex(1);
    assert(TabCtrl_GetCurSel(nativeTab) == 1);
    assert(tabs.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(selectionEvents == 1);
    assert(lastSelection == std::optional<std::size_t>(1));
    assert(IsWindowVisible(nativeGeneralBox) == FALSE);
    assert(IsWindowVisible(nativeNetworkBox) != FALSE);
    assert(primary.GetFocusedControl().GetType() == ControlType::TabView);
    assert(networkBox.Focus());
    networkBox.SetText("server.changed");
    tabs.SetSelectedIndex(0);
    assert(generalBox.GetText() == "Alice");
    assert(generalSlider.GetValue() == 73);
    assert(networkBox.GetText() == "server.changed");
    assert(selectionEvents == 2);
    tabs.SetSelectedIndex(0);
    assert(selectionEvents == 2);

    // Native user selection is routed through the parent WM_NOTIFY boundary.
    TabCtrl_SetCurSel(nativeTab, 2);
    NotifyTabSelection(primaryNative, nativeTab);
    assert(tabs.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(selectionEvents == 3);
    assert(lastSelection == std::optional<std::size_t>(2));
    assert(IsWindowVisible(nativeNetworkBox) == FALSE);

    const HWND oldPrimaryNative = primaryNative;
    auto runtime = tabs.AddTab("Runtime");
    assert(tabs.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(TabCtrl_GetItemCount(nativeTab) == 4);
    TextBox runtimeBox("runtime");
    runtime.GetLayout().Add(runtimeBox);
    nativeTab = FindChildByClass(primaryNative, WC_TABCONTROLW);
    assert(nativeTab != nullptr);
    tabs.SetSelectedIndex(3);
    assert(TabCtrl_GetCurSel(nativeTab) == 3);
    assert(runtimeBox.Focus());
    assert(primary.GetFocusedControl() == runtimeBox.GetControlRef());
    runtime.SetTitle("Runtime Updated");
    assert(TabTitle(nativeTab, 3) == L"Runtime Updated");

    // Independent native routing remains isolated across windows.
    otherTabs.SetSelectedIndex(0);
    assert(TabCtrl_GetCurSel(otherNativeTab) == 0);
    assert(otherBox.GetText() == "secondary");

    // Close/recreate retains the logical page and child state.
    primary.Close();
    assert(!primary.IsShown());
    assert(!IsWindow(oldPrimaryNative));
    assert(tabs.GetSelectedIndex() == std::optional<std::size_t>(3));
    assert(runtimeBox.GetText() == "runtime");
    assert(primary.Show());
    const HWND reopened = FindWindowW(nullptr,
                                      L"guideXOS TabView Native Primary");
    assert(reopened != nullptr && reopened != oldPrimaryNative);
    const HWND reopenedTab = FindChildByClass(reopened, WC_TABCONTROLW);
    assert(reopenedTab != nullptr);
    assert(TabCtrl_GetItemCount(reopenedTab) == 4);
    assert(TabCtrl_GetCurSel(reopenedTab) == 3);
    primary.Close();
    secondary.Close();
    app.Quit();
    assert(app.Run() == 0);
    return 0;
}

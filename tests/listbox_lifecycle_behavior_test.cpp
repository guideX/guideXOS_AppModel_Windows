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

int main() {
    Application app("com.guidexos.tests.listbox-lifecycle");
    Window primary(app);
    Window secondary(app);
    primary.SetTitle("guideXOS ListBox Routing Primary");
    secondary.SetTitle("guideXOS ListBox Routing Secondary");

    Label primaryStatus("primary ready");
    TextBox primaryMirror;
    ListBox primaryList;
    ListBox primaryOtherList;
    Label secondaryStatus("secondary ready");
    ListBox secondaryList;

    primaryList.AddItem("primary-0");
    primaryList.AddItem("primary-1");
    primaryList.AddItem("primary-2");
    primaryOtherList.AddItem("other-0");
    primaryOtherList.AddItem("other-1");
    secondaryList.AddItem("secondary-0");
    secondaryList.AddItem("secondary-1");

    int primaryEvents = 0;
    int secondaryEvents = 0;
    bool closeOnThirdPrimarySelection = false;
    primaryList.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++primaryEvents;
        if (index) {
            primaryStatus.SetText("primary selected " + std::to_string(*index));
            primaryMirror.SetText(primaryList.GetItem(*index));
            if (*index == 0) secondaryList.SetSelectedIndex(1);
            if (*index == 2 && closeOnThirdPrimarySelection) {
                primary.Close();
                secondary.Close();
            }
        } else {
            primaryStatus.SetText("primary cleared");
            primaryMirror.SetText({});
        }
    });
    primaryOtherList.OnSelectionChanged([&](std::optional<std::size_t> index) {
        if (index) primaryStatus.SetText("other selected " + std::to_string(*index));
    });
    secondaryList.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++secondaryEvents;
        if (index) secondaryStatus.SetText("secondary selected " +
                                           std::to_string(*index));
    });

    Layout primaryLayout;
    primaryLayout.Add(primaryStatus);
    primaryLayout.Add(primaryMirror);
    primaryLayout.Add(primaryList);
    primaryLayout.Add(primaryOtherList);
    primary.SetContent(primaryLayout);

    Layout secondaryLayout;
    secondaryLayout.Add(secondaryStatus);
    secondaryLayout.Add(secondaryList);
    secondary.SetContent(secondaryLayout);

    assert(primary.Show());
    assert(secondary.Show());
    assert(primary.IsShown());
    assert(secondary.IsShown());
    assert(app.GetLiveWindowCount() == 2);
    assert(primaryEvents == 0);
    assert(secondaryEvents == 0);

    // Exercise the actual native notification path against both top-level
    // windows. The backend must resolve each child through its owning window.
    const HWND primaryNative = FindWindowW(
        nullptr, L"guideXOS ListBox Routing Primary");
    const HWND secondaryNative = FindWindowW(
        nullptr, L"guideXOS ListBox Routing Secondary");
    const HWND primaryListNative = FindWindowExW(
        primaryNative, nullptr, L"ListBox", nullptr);
    const HWND secondaryListNative = FindWindowExW(
        secondaryNative, nullptr, L"ListBox", nullptr);
    assert(primaryNative != nullptr);
    assert(secondaryNative != nullptr);
    assert(primaryListNative != nullptr);
    assert(secondaryListNative != nullptr);
    SendMessageW(primaryListNative, LB_SETCURSEL, 0, 0);
    SendMessageW(primaryNative, WM_COMMAND,
                 static_cast<WPARAM>(LBN_SELCHANGE) << 16,
                 reinterpret_cast<LPARAM>(primaryListNative));
    assert(primaryList.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(primaryStatus.GetText() == "primary selected 0");
    assert(primaryMirror.GetText() == "primary-0");
    assert(secondaryList.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(secondaryStatus.GetText() == "secondary selected 1");
    assert(primaryOtherList.GetSelectedIndex() == std::nullopt);
    assert(primaryEvents == 1);
    assert(secondaryEvents == 1);

    primaryOtherList.SetSelectedIndex(1);
    assert(primaryOtherList.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(primaryList.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(primaryEvents == 1);

    primaryList.InsertItem(0, "inserted before selected");
    assert(primaryList.GetSelectedIndex() == std::optional<std::size_t>(1));
    primaryList.RemoveItem(0);
    assert(primaryList.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(primaryEvents == 1);

    // The model and retained native realization can be detached and recreated
    // without losing items, selection, or callbacks.
    primary.Close();
    assert(!primary.IsShown());
    assert(secondary.IsShown());
    assert(app.GetLiveWindowCount() == 1);
    primaryList.SetSelectedIndex(1);
    assert(primaryEvents == 2);
    assert(primaryList.GetItem(1) == "primary-1");
    assert(primary.Show());
    assert(primary.IsShown());
    assert(primaryList.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(primaryList.GetItemCount() == 3);

    // A callback may close both its own and another window. Dispatch retains
    // only model state across that user-code boundary.
    closeOnThirdPrimarySelection = true;
    primaryList.SetSelectedIndex(2);
    assert(!primary.IsShown());
    assert(!secondary.IsShown());
    assert(app.GetLiveWindowCount() == 0);

    // Reopening realizes the retained selection without an initial event.
    assert(primary.Show());
    assert(secondary.Show());
    assert(primaryList.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(secondaryList.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(primaryEvents == 3);
    assert(secondaryEvents == 1);

    secondaryList.SetSelectedIndex(0);
    assert(secondaryEvents == 2);
    assert(secondaryStatus.GetText() == "secondary selected 0");
    primary.Close();
    assert(!primary.IsShown());
    assert(secondary.IsShown());
    secondaryList.SetSelectedIndex(1);
    assert(secondaryEvents == 3);
    secondary.Close();
    assert(app.GetLiveWindowCount() == 0);
    assert(app.Run() == 0);
    return 0;
}

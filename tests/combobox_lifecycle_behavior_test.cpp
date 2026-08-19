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
    Application app("com.guidexos.tests.combobox-lifecycle");
    Window primary(app);
    Window secondary(app);
    primary.SetTitle("guideXOS ComboBox Routing Primary");
    secondary.SetTitle("guideXOS ComboBox Routing Secondary");

    Label primaryStatus("primary ready");
    ComboBox primaryMode;
    ComboBox primaryOtherMode;
    Label secondaryStatus("secondary ready");
    ComboBox secondaryMode;

    primaryMode.AddItem("primary-0");
    primaryMode.AddItem("primary-1");
    primaryMode.AddItem("primary-2");
    primaryOtherMode.AddItem("other-0");
    primaryOtherMode.AddItem("other-1");
    secondaryMode.AddItem("secondary-0");
    secondaryMode.AddItem("secondary-1");

    int primaryEvents = 0;
    int secondaryEvents = 0;
    bool closeOnThirdPrimarySelection = false;
    primaryMode.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++primaryEvents;
        if (index) {
            primaryStatus.SetText("primary selected " + std::to_string(*index));
            if (*index == 0) secondaryMode.SetSelectedIndex(1);
            if (*index == 2 && closeOnThirdPrimarySelection) {
                primary.Close();
                secondary.Close();
            }
        } else {
            primaryStatus.SetText("primary cleared");
        }
    });
    primaryOtherMode.OnSelectionChanged([&](std::optional<std::size_t> index) {
        if (index) primaryStatus.SetText("other selected " +
                                         std::to_string(*index));
    });
    secondaryMode.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++secondaryEvents;
        if (index) secondaryStatus.SetText("secondary selected " +
                                           std::to_string(*index));
    });

    Layout primaryLayout;
    primaryLayout.Add(primaryStatus);
    primaryLayout.Add(primaryMode);
    primaryLayout.Add(primaryOtherMode);
    primary.SetContent(primaryLayout);

    Layout secondaryLayout;
    secondaryLayout.Add(secondaryStatus);
    secondaryLayout.Add(secondaryMode);
    secondary.SetContent(secondaryLayout);

    assert(primary.Show());
    assert(secondary.Show());
    assert(primary.IsShown());
    assert(secondary.IsShown());
    assert(app.GetLiveWindowCount() == 2);
    assert(primaryEvents == 0);
    assert(secondaryEvents == 0);

    const HWND primaryNative = FindWindowW(
        nullptr, L"guideXOS ComboBox Routing Primary");
    const HWND secondaryNative = FindWindowW(
        nullptr, L"guideXOS ComboBox Routing Secondary");
    const HWND primaryComboNative = FindWindowExW(
        primaryNative, nullptr, L"ComboBox", nullptr);
    const HWND secondaryComboNative = FindWindowExW(
        secondaryNative, nullptr, L"ComboBox", nullptr);
    assert(primaryNative != nullptr);
    assert(secondaryNative != nullptr);
    assert(primaryComboNative != nullptr);
    assert(secondaryComboNative != nullptr);

    // This is the same child-origin native notification path used by an
    // actual CBN_SELCHANGE. The child handle must resolve only in its owner.
    SendMessageW(primaryComboNative, CB_SETCURSEL, 0, 0);
    SendMessageW(primaryNative, WM_COMMAND,
                static_cast<WPARAM>(CBN_SELCHANGE) << 16,
                reinterpret_cast<LPARAM>(primaryComboNative));
    assert(primaryMode.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(primaryStatus.GetText() == "primary selected 0");
    assert(secondaryMode.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(secondaryStatus.GetText() == "secondary selected 1");
    assert(primaryOtherMode.GetSelectedIndex() == std::nullopt);
    assert(primaryEvents == 1);
    assert(secondaryEvents == 1);

    primaryOtherMode.SetSelectedIndex(1);
    assert(primaryOtherMode.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(primaryMode.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(primaryEvents == 1);

    primaryMode.InsertItem(0, "inserted before selected");
    assert(primaryMode.GetSelectedIndex() == std::optional<std::size_t>(1));
    primaryMode.RemoveItem(0);
    assert(primaryMode.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(primaryEvents == 1);

    primary.Close();
    assert(!primary.IsShown());
    assert(secondary.IsShown());
    assert(app.GetLiveWindowCount() == 1);
    primaryMode.SetSelectedIndex(1);
    assert(primaryEvents == 2);
    assert(primaryMode.GetItem(1) == "primary-1");
    assert(primary.Show());
    assert(primary.IsShown());
    assert(primaryMode.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(primaryMode.GetItemCount() == 3);

    closeOnThirdPrimarySelection = true;
    primaryMode.SetSelectedIndex(2);
    assert(!primary.IsShown());
    assert(!secondary.IsShown());
    assert(app.GetLiveWindowCount() == 0);

    assert(primary.Show());
    assert(secondary.Show());
    assert(primaryMode.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(secondaryMode.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(primaryEvents == 3);
    assert(secondaryEvents == 1);

    secondaryMode.SetSelectedIndex(0);
    assert(secondaryEvents == 2);
    assert(secondaryStatus.GetText() == "secondary selected 0");
    primary.Close();
    assert(!primary.IsShown());
    assert(secondary.IsShown());
    secondaryMode.SetSelectedIndex(1);
    assert(secondaryEvents == 3);
    secondary.Close();
    assert(app.GetLiveWindowCount() == 0);
    assert(app.Run() == 0);
    return 0;
}

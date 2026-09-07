#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <cstdio>
#include <functional>
#include <optional>
#include <stdexcept>
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

bool ThrowsOutOfRange(const std::function<void()>& action) {
    try {
        action();
    } catch (const std::out_of_range&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    TabView empty;
    assert(empty.GetTabCount() == 0);
    assert(!empty.GetSelectedIndex());
    assert(empty.IsEnabled());
    assert(empty.GetControlRef().GetType() == ControlType::TabView);
    const auto tabViewRef = empty.GetControlRef().AsTabView();
    assert(tabViewRef && tabViewRef->IsValid());
    assert(tabViewRef->GetTabCount() == 0);
    assert(!tabViewRef->GetSelectedIndex());

    int selectionEvents = 0;
    std::optional<std::size_t> lastSelection;
    empty.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++selectionEvents;
        lastSelection = index;
    });

    const std::string unicodeTitle = "N\xC3\xA9twork \xF0\x9F\x8C\x90";
    auto general = empty.AddTab("General");
    auto network = empty.AddTab(unicodeTitle);
    auto diagnostics = empty.AddTab("Diagnostics");
    assert(empty.GetTabCount() == 3);
    assert(empty.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(selectionEvents == 1);
    assert(lastSelection == std::optional<std::size_t>(0));
    assert(general.GetTitle() == "General");
    assert(network.GetTitle() == unicodeTitle);
    assert(network.GetTabPageRef().IsValid());
    assert(network.GetTabPageRef().GetTitle() == unicodeTitle);
    assert(network.GetIndex() == std::optional<std::size_t>(1));
    assert(empty.GetTab(1).GetTabPageRef().GetIndex() ==
           std::optional<std::size_t>(1));
    assert(ThrowsOutOfRange([&]() { empty.GetTab(99); }));

    Label nameLabel("Name");
    TextBox nameBox("guideXOS user");
    CheckBox notifications("Enable notifications", true);
    Slider volume;
    volume.SetValue(73);
    ProgressBar progress;
    progress.SetValue(73);
    Button save("Save");
    Layout generalLayout = general.GetLayout();
    generalLayout.Add(nameLabel);
    generalLayout.Add(nameBox);
    generalLayout.Add(notifications);
    generalLayout.Add(volume);
    generalLayout.Add(progress);
    generalLayout.Add(save);

    TextBox server("example.com");
    TextArea networkNotes("server state");
    Layout networkLayout = network.GetLayout();
    networkLayout.Add(server);
    networkLayout.Add(networkNotes, LayoutSizing::Expand);

    TextArea log("diagnostic log");
    Button start("Start Test");
    Layout diagnosticsLayout = diagnostics.GetLayout();
    diagnosticsLayout.Add(start);
    diagnosticsLayout.Add(log, LayoutSizing::Expand);
    assert(generalLayout.ChildCount() == 6);
    assert(networkLayout.ChildCount() == 2);
    assert(diagnosticsLayout.ChildCount() == 2);
    assert(general.GetLayout().GetMinimumSize().height > 0);

    empty.SetSelectedIndex(1);
    assert(empty.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(selectionEvents == 2);
    assert(lastSelection == std::optional<std::size_t>(1));
    empty.SetSelectedIndex(1);
    assert(selectionEvents == 2);
    assert(ThrowsOutOfRange([&]() { empty.SetSelectedIndex(100); }));

    nameBox.SetText("Alice");
    volume.SetValue(73);
    server.SetText("server.test");
    networkNotes.SetText("network state");
    empty.SetSelectedIndex(2);
    empty.SetSelectedIndex(0);
    assert(nameBox.GetText() == "Alice");
    assert(volume.GetValue() == 73);
    empty.SetSelectedIndex(1);
    assert(server.GetText() == "server.test");
    assert(networkNotes.GetText() == "network state");

    empty.SetSelectedIndex(std::nullopt);
    assert(!empty.GetSelectedIndex());
    assert(selectionEvents == 6);
    empty.SetSelectedIndex(std::nullopt);
    assert(selectionEvents == 6);

    int replacementEvents = 0;
    empty.OnSelectionChanged([&](std::optional<std::size_t>) {
        ++replacementEvents;
    });
    empty.SetSelectedIndex(0);
    assert(replacementEvents == 1);
    empty.OnSelectionChanged({});
    empty.SetSelectedIndex(1);
    assert(replacementEvents == 1);

    int reentrantEvents = 0;
    empty.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++reentrantEvents;
        if (index == std::optional<std::size_t>(0)) empty.SetSelectedIndex(2);
    });
    empty.SetSelectedIndex(0);
    assert(reentrantEvents == 2);
    assert(empty.GetSelectedIndex() == std::optional<std::size_t>(2));

    TabView other;
    auto otherPage = other.AddTab("Other");
    assert(other.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(otherPage.GetIndex() == std::optional<std::size_t>(0));
    assert(other.GetControlRef() != empty.GetControlRef());
    other.SetEnabled(false);
    assert(!other.IsEnabled());
    assert(!other.GetControlRef().IsEnabled());
    other.SetEnabled(true);

    Application app("com.guidexos.tests.tab-view-model",
                    ShutdownMode::Explicit);
    Window window(app);
    Layout root;
    root.Add(empty, LayoutSizing::Expand);
    window.SetContent(root);
    assert(window.Show());
    empty.OnSelectionChanged({});
    empty.SetSelectedIndex(0);
    assert(nameBox.Focus());
    assert(window.GetFocusedControl() == nameBox.GetControlRef());
    empty.SetSelectedIndex(1);
    assert(window.GetFocusedControl().GetType() == ControlType::TabView);
    window.Close();
    assert(!window.IsShown());
    assert(nameBox.GetText() == "Alice");
    assert(volume.GetValue() == 73);
    assert(server.GetText() == "server.test");
    assert(window.Show());
    empty.SetSelectedIndex(0);
    assert(nameBox.Focus());
    empty.SetSelectedIndex(1);
    assert(window.GetFocusedControl().GetType() == ControlType::TabView);
    window.Close();

    Window callbackWindow(app);
    callbackWindow.SetTitle("guideXOS TabView Selection Close");
    TabView callbackTabs;
    callbackTabs.AddTab("One");
    callbackTabs.AddTab("Two");
    Layout callbackContent;
    callbackContent.Add(callbackTabs, LayoutSizing::Expand);
    callbackWindow.SetContent(callbackContent);
    callbackTabs.OnSelectionChanged([&](std::optional<std::size_t>) {
        callbackWindow.Close();
    });
    assert(callbackWindow.Show());
    callbackTabs.SetSelectedIndex(1);
    assert(!callbackWindow.IsShown());

    app.Quit();
    assert(app.Run() == 0);
    return 0;
}

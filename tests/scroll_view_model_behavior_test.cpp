#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>
#include <memory>
#include <optional>
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

int main() {
    ScrollView empty;
    assert(empty.GetVerticalOffset() == 0);
    assert(empty.GetMaximumVerticalOffset() == 0);
    assert(empty.GetContentSize().height >= 0);

    ScrollView scroll;
    Label title("Scrollable title");
    TextArea notes("state survives scrolling");
    Layout nested(Orientation::Horizontal, 0, 8);
    Button nestedButton("Nested");
    nested.Add(nestedButton);
    scroll.ContentLayout().Add(title);
    scroll.ContentLayout().Add(nested);
    scroll.ContentLayout().Add(notes);
    std::vector<std::unique_ptr<Label>> rows;
    for (int index = 0; index < 12; ++index) {
        rows.push_back(std::make_unique<Label>(
            "Row " + std::to_string(index)));
        scroll.ContentLayout().Add(*rows.back());
    }

    // The pre-realization value is retained until the first viewport is
    // known, then clamped against the measured content extent.
    scroll.SetVerticalOffset(120);
    assert(scroll.GetVerticalOffset() == 120);
    assert(scroll.GetControlRef().GetType() == ControlType::ScrollView);
    assert(scroll.GetControlRef().AsScrollView()->IsValid());
    assert(!title.GetControlRef().AsScrollView().has_value());

    Application app("com.guidexos.tests.scroll-view-model",
                     ShutdownMode::Explicit);
    Window window(app);
    window.SetTitle("ScrollView model");
    window.SetSize(560, 300);
    Layout root;
    root.Add(scroll, LayoutSizing::Expand);
    window.SetContent(root);
    assert(window.Show());
    assert(scroll.GetViewportSize().height > 0);
    assert(scroll.GetContentSize().height > scroll.GetViewportSize().height);
    assert(scroll.GetMaximumVerticalOffset() > 0);
    assert(scroll.GetVerticalOffset() <= scroll.GetMaximumVerticalOffset());

    scroll.SetVerticalOffset(1'000'000);
    assert(scroll.GetVerticalOffset() == scroll.GetMaximumVerticalOffset());
    scroll.SetVerticalOffset(-50);
    assert(scroll.GetVerticalOffset() == 0);
    scroll.SetVerticalOffset(90);
    assert(scroll.GetVerticalOffset() == 90);
    const int oldMaximum = scroll.GetMaximumVerticalOffset();

    window.SetSize(560, 760);
    assert(scroll.GetMaximumVerticalOffset() == 0);
    assert(scroll.GetVerticalOffset() == 0);
    window.SetSize(560, 300);
    assert(scroll.GetMaximumVerticalOffset() > oldMaximum - 20);
    scroll.SetVerticalOffset(scroll.GetMaximumVerticalOffset());

    TabView tabs;
    auto first = tabs.AddTab("First");
    auto second = tabs.AddTab("Second");
    Label secondLabel("Second page");
    ScrollView tabScroll;
    std::vector<std::unique_ptr<Label>> tabRows;
    for (int index = 0; index < 14; ++index) {
        tabRows.push_back(std::make_unique<Label>(
            "Tab row " + std::to_string(index)));
        tabScroll.ContentLayout().Add(*tabRows.back());
    }
    first.GetLayout().Add(tabScroll, LayoutSizing::Expand);
    second.GetLayout().Add(secondLabel);
    Layout tabRoot;
    tabRoot.Add(tabs, LayoutSizing::Expand);
    Window tabWindow(app);
    tabWindow.SetTitle("ScrollView tab model");
    tabWindow.SetSize(560, 300);
    tabWindow.SetContent(tabRoot);
    assert(tabWindow.Show());
    assert(tabScroll.GetMaximumVerticalOffset() > 0);
    tabScroll.SetVerticalOffset(tabScroll.GetMaximumVerticalOffset());
    const int retainedOffset = tabScroll.GetVerticalOffset();
    tabs.SetSelectedIndex(1);
    tabs.SetSelectedIndex(0);
    assert(tabScroll.GetVerticalOffset() == retainedOffset);

    tabWindow.Close();
    window.Close();
    assert(tabScroll.GetVerticalOffset() == retainedOffset);
    assert(tabWindow.Show());
    assert(tabScroll.GetVerticalOffset() == retainedOffset);
    tabWindow.Close();
    window.Close();
    app.Quit();
    assert(app.Run() == 0);

    ScrollViewRef expired;
    {
        ScrollView temporary;
        expired = *temporary.GetControlRef().AsScrollView();
        assert(expired.IsValid());
    }
    assert(!expired.IsValid());
    return 0;
}

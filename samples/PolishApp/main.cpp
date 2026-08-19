#include <guidexos/appmodel/appmodel.hpp>

#include <optional>
#include <string>

using namespace guidexos::appmodel;

#ifdef GUIDEXOS_STATUS_TEST_HELPER
void ReportStatusForTest(const std::string& text);
void ResetToolTipsForTest();
void ReportToolTipForTest(const std::string& text);
#endif

int main() {
    Application app("com.guidexos.samples.polish");
    Window window(app);
    window.SetTitle("Application Polish Demo");
    window.SetSize(640, 480);

    Label heading("Application Polish Demo");
    Label nameLabel("Name");
    TextBox name;
    Button apply("Apply");
    Button clear("Clear");
    Label optionsLabel("Options");
    CheckBox feature("Enable feature");
    Label listLabel("List");
    ListBox list;
    list.AddItem("First item");
    list.AddItem("Second item");
    list.AddItem("Third item");

    name.SetToolTip("Enter a display name");
    apply.SetToolTip("Apply the current values");
    clear.SetToolTip("Clear the form");
    feature.SetToolTip("Enable or disable the feature");
    list.SetToolTip("Select an item");

    StatusBar status("Ready");
#ifdef GUIDEXOS_STATUS_TEST_HELPER
    ReportStatusForTest(status.GetText());
    const auto reportToolTips = [&]() {
        ResetToolTipsForTest();
        ReportToolTipForTest(name.GetToolTip());
        ReportToolTipForTest(apply.GetToolTip());
        ReportToolTipForTest(clear.GetToolTip());
        ReportToolTipForTest(feature.GetToolTip());
        ReportToolTipForTest(list.GetToolTip());
    };
    reportToolTips();
#endif

    Layout content(Orientation::Vertical, 24, 8);
    Layout nameRow(Orientation::Horizontal, 0, 8);
    nameRow.Add(nameLabel);
    nameRow.Add(name, LayoutSizing::Expand);
    Layout actions(Orientation::Horizontal, 0, 8);
    actions.Add(apply);
    actions.Add(clear);
    content.Add(heading);
    content.Add(nameRow);
    content.Add(optionsLabel);
    content.Add(feature);
    content.Add(listLabel);
    content.Add(list, LayoutSizing::Expand);
    content.Add(actions);
    window.SetContent(content);
    window.SetStatusBar(status);

    apply.OnClick([&]() {
        name.SetToolTip("Display name applied");
        status.SetText("Applied");
#ifdef GUIDEXOS_STATUS_TEST_HELPER
        ReportStatusForTest(status.GetText());
        reportToolTips();
#endif
    });
    clear.OnClick([&]() {
        name.SetText({});
        name.SetToolTip({});
        feature.SetChecked(false);
        list.SetSelectedIndex(std::nullopt);
        status.SetText("Cleared");
#ifdef GUIDEXOS_STATUS_TEST_HELPER
        ReportStatusForTest(status.GetText());
        reportToolTips();
#endif
    });
    feature.OnCheckedChanged([&](bool enabled) {
        feature.SetToolTip(enabled ? "Feature is enabled"
                                   : "Enable or disable the feature");
        status.SetText(enabled ? "Feature enabled" : "Feature disabled");
#ifdef GUIDEXOS_STATUS_TEST_HELPER
        ReportStatusForTest(status.GetText());
        reportToolTips();
#endif
    });
    list.OnSelectionChanged([&](std::optional<std::size_t> index) {
        if (index) {
            status.SetText("Selected: " + list.GetItem(*index));
        } else {
            status.SetText("Ready");
        }
#ifdef GUIDEXOS_STATUS_TEST_HELPER
        ReportStatusForTest(status.GetText());
        reportToolTips();
#endif
    });

    if (!window.Show()) return 1;
    return app.Run();
}

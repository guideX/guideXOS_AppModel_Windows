#include <guidexos/appmodel/appmodel.hpp>

#include <string>

using namespace guidexos::appmodel;

namespace {

const char* ControlName(ControlType type) {
    switch (type) {
    case ControlType::Label: return "Label";
    case ControlType::Button: return "Button";
    case ControlType::TextBox: return "TextBox";
    case ControlType::ListBox: return "ListBox";
    case ControlType::CheckBox: return "CheckBox";
    case ControlType::RadioButton: return "RadioButton";
    case ControlType::ComboBox: return "ComboBox";
    case ControlType::None: return "None";
    }
    return "None";
}

} // namespace

int main() {
    Application app("com.guidexos.samples.focus-command",
                    ShutdownMode::Explicit);
    Window window(app);
    Window lifecycle(app);
    window.SetTitle("guideXOS Focus Command Demo");
    window.SetSize(760, 560);

    Label title("Focus and Edit Command Routing");
    Label nameLabel("Name");
    TextBox name("Alpha");
    Label descriptionLabel("Description");
    TextBox description("Beta");
    Button focusName("Focus Name");
    Button focusDescription("Focus Description");
    Button focusButton("Focus Button");
    ListBox list;
    list.AddItem("List item");
    CheckBox check("Check");
    RadioButton radio("Radio");
    ComboBox combo;
    combo.AddItem("Combo item");
    Label focusStatus("Focused: None");
    Label commandStatus("Command: Ready");
    Button closeWindow("Close Focus Window");
    Button reopenWindow("Reopen Focus Window");
    Button quitApp("Quit App");
    Label lifecycleTitle("Focus Window Lifecycle");

    MenuBar menuBar;
    Menu edit("&Edit");
    MenuItem cut("&Cut");
    MenuItem copy("&Copy");
    MenuItem paste("&Paste");
    MenuItem deleteSelection("&Delete");
    MenuItem selectAll("Select &All");
    cut.SetShortcut(KeyShortcut::Ctrl('X'));
    copy.SetShortcut(KeyShortcut::Ctrl('C'));
    paste.SetShortcut(KeyShortcut::Ctrl('V'));
    selectAll.SetShortcut(KeyShortcut::Ctrl('A'));
    edit.Add(cut);
    edit.Add(copy);
    edit.Add(paste);
    edit.Add(deleteSelection);
    edit.AddSeparator();
    edit.Add(selectAll);
    menuBar.Add(edit);

    const auto updateFocusStatus = [&]() {
        focusStatus.SetText(std::string("Focused: ") +
                            ControlName(window.GetFocusedControl().GetType()));
    };
    const auto updateEditState = [&]() {
        const auto target = window.GetFocusedControl().AsTextBox();
        const bool selection = target && target->HasSelection();
        bool clipboardText = false;
        try {
            clipboardText = Clipboard::HasText();
        } catch (const ClipboardError&) {
            clipboardText = false;
        }
        cut.SetEnabled(selection);
        copy.SetEnabled(selection);
        paste.SetEnabled(target.has_value() && clipboardText);
        deleteSelection.SetEnabled(selection);
        selectAll.SetEnabled(target && target->HasText());
        updateFocusStatus();
    };
    const auto runEdit = [&](const char* name,
                             void (TextBoxRef::*command)() const) {
        const auto target = window.GetFocusedControl().AsTextBox();
        if (target) {
            try {
                (target.value().*command)();
                commandStatus.SetText(std::string("Command: ") + name);
            } catch (const ClipboardError& error) {
                commandStatus.SetText(std::string("Command: ") + error.what());
            }
        } else {
            commandStatus.SetText("Command: no focused TextBox");
        }
        updateFocusStatus();
    };

    edit.OnOpening(updateEditState);
    cut.OnInvoked([&]() { runEdit("Cut", &TextBoxRef::Cut); });
    copy.OnInvoked([&]() { runEdit("Copy", &TextBoxRef::Copy); });
    paste.OnInvoked([&]() { runEdit("Paste", &TextBoxRef::Paste); });
    deleteSelection.OnInvoked(
        [&]() { runEdit("Delete", &TextBoxRef::DeleteSelection); });
    selectAll.OnInvoked([&]() { runEdit("Select All", &TextBoxRef::SelectAll); });

    focusName.OnClick([&]() { name.Focus(); updateFocusStatus(); });
    focusDescription.OnClick(
        [&]() { description.Focus(); updateFocusStatus(); });
    focusButton.OnClick([&]() { focusButton.Focus(); updateFocusStatus(); });

    Layout nameRow(Orientation::Horizontal, 0, 8);
    nameRow.Add(nameLabel);
    nameRow.Add(name, LayoutSizing::Expand);
    Layout descriptionRow(Orientation::Horizontal, 0, 8);
    descriptionRow.Add(descriptionLabel);
    descriptionRow.Add(description, LayoutSizing::Expand);
    Layout focusRow(Orientation::Horizontal, 0, 8);
    focusRow.Add(focusName);
    focusRow.Add(focusDescription);
    focusRow.Add(focusButton);
    focusRow.AddSpacer();

    Layout content(Orientation::Vertical, 20, 8);
    content.Add(title);
    content.Add(nameRow);
    content.Add(descriptionRow);
    content.Add(focusRow);
    content.Add(list);
    content.Add(check);
    content.Add(radio);
    content.Add(combo);
    content.Add(focusStatus);
    content.Add(commandStatus);
    window.SetContent(content);
    window.SetMenuBar(menuBar);

    Layout lifecycleContent(Orientation::Vertical, 16, 8);
    lifecycleContent.Add(lifecycleTitle);
    lifecycleContent.Add(closeWindow);
    lifecycleContent.Add(reopenWindow);
    lifecycleContent.Add(quitApp);
    lifecycle.SetTitle("Focus Command Lifecycle");
    lifecycle.SetSize(300, 240);
    lifecycle.SetContent(lifecycleContent);

    closeWindow.OnClick([&]() { window.Close(); });
    reopenWindow.OnClick([&]() { window.Show(); });
    quitApp.OnClick([&]() { app.Quit(); });

    updateFocusStatus();
    updateEditState();
    if (!window.Show() || !lifecycle.Show()) return 1;
    return app.Run();
}

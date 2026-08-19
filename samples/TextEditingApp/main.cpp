#include <guidexos/appmodel/appmodel.hpp>

#include <functional>
#include <string>

using namespace guidexos::appmodel;

namespace {

const std::string kUnicodeText = "A caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x9A\x80";

} // namespace

int main() {
    Application app("com.guidexos.samples.texteditingapp",
                    ShutdownMode::Explicit);
    Window window(app);
    Window lifecycle(app);
    window.SetTitle("guideXOS Text Editing Demo");
    window.SetSize(980, 700);

    Label title("Text Editing Demo");
    Label sourceLabel("Source");
    TextBox source(kUnicodeText);
    Label destinationLabel("Destination");
    TextBox destination("Paste here");
    Label selectionStatus;
    Label caretStatus;
    Label selectedTextStatus;
    Label targetStatus;
    Label operationStatus("Ready");

    Button sourceTarget("Target: Source");
    Button destinationTarget("Target: Destination");
    Button selectAll("Select All");
    Button clearSelection("Clear Selection");
    Button setCaret("Set Caret: 1");
    Button selectEmoji("Select Emoji");
    Button copy("Copy");
    Button cut("Cut");
    Button paste("Paste");
    Button deleteSelection("Delete Selection");
    Button closeDemo("Close Demo Window");
    Button reopenDemo("Reopen Demo Window");
    Button quitApp("Quit App");
    Label lifecycleTitle("Text Editing Lifecycle");

    MenuBar menuBar;
    Menu edit("Edit");
    MenuItem selectAllMenu("Select All");
    MenuItem copyMenu("Copy");
    MenuItem cutMenu("Cut");
    MenuItem pasteMenu("Paste");
    MenuItem deleteMenu("Delete Selection");
    selectAllMenu.SetShortcut(KeyShortcut::Ctrl('A'));
    copyMenu.SetShortcut(KeyShortcut::Ctrl('C'));
    cutMenu.SetShortcut(KeyShortcut::Ctrl('X'));
    pasteMenu.SetShortcut(KeyShortcut::Ctrl('V'));
    edit.Add(selectAllMenu);
    edit.Add(copyMenu);
    edit.Add(cutMenu);
    edit.Add(pasteMenu);
    edit.Add(deleteMenu);
    menuBar.Add(edit);
    window.SetMenuBar(menuBar);

    const auto getFocusedTextBox = [&]() -> TextBox* {
        const ControlRef focused = window.GetFocusedControl();
        if (focused == source.GetControlRef()) return &source;
        if (focused == destination.GetControlRef()) return &destination;
        return nullptr;
    };
    const auto refreshStatus = [&]() {
        TextBox* focused = getFocusedTextBox();
        if (!focused) {
            selectionStatus.SetText("Selection: none");
            caretStatus.SetText("Caret: none");
            selectedTextStatus.SetText("Selected Text: <none>");
            targetStatus.SetText("Target: None");
            return;
        }
        const TextRange range = focused->GetSelection();
        selectionStatus.SetText("Selection: start=" +
                                std::to_string(range.start) +
                                " length=" + std::to_string(range.length));
        caretStatus.SetText("Caret: " +
                            std::to_string(focused->GetCaretIndex()));
        const std::string selected = focused->GetSelectedText();
        selectedTextStatus.SetText("Selected Text: " +
                                   (selected.empty() ? "<none>" : selected));
        targetStatus.SetText(std::string("Target: ") +
                             (focused == &source ? "Source" : "Destination"));
    };
    edit.OnOpening([&]() { refreshStatus(); });
    const auto runCommand = [&](const char* description,
                                const std::function<void(TextBox&)>& command) {
        TextBox* focused = getFocusedTextBox();
        if (!focused) {
            operationStatus.SetText("Focus a TextBox before editing.");
            refreshStatus();
            return;
        }
        try {
            command(*focused);
            operationStatus.SetText(description);
            refreshStatus();
        } catch (const ClipboardError& error) {
            operationStatus.SetText("Clipboard error: " +
                                    std::string(error.what()));
        }
    };

    source.OnTextChanged([&](const std::string&) { refreshStatus(); });
    destination.OnTextChanged([&](const std::string&) { refreshStatus(); });

    sourceTarget.OnClick([&]() {
        source.Focus();
        operationStatus.SetText("Source focused as command target.");
        refreshStatus();
    });
    destinationTarget.OnClick([&]() {
        destination.Focus();
        operationStatus.SetText("Destination focused as command target.");
        refreshStatus();
    });
    selectAll.OnClick([&]() {
        runCommand("Select All completed.", [](TextBox& textBox) {
            textBox.SelectAll();
        });
    });
    clearSelection.OnClick([&]() {
        source.Focus();
        source.ClearSelection();
        operationStatus.SetText("Clear Selection completed.");
        refreshStatus();
    });
    setCaret.OnClick([&]() {
        source.Focus();
        source.SetCaretIndex(1);
        operationStatus.SetText("Set Caret completed.");
        refreshStatus();
    });
    selectEmoji.OnClick([&]() {
        source.Focus();
        source.SetSelection(TextRange{10, 1});
        operationStatus.SetText("Select Emoji completed.");
        refreshStatus();
    });
    copy.OnClick([&]() {
        runCommand("Copy completed.", [](TextBox& textBox) { textBox.Copy(); });
    });
    cut.OnClick([&]() {
        runCommand("Cut completed.", [](TextBox& textBox) { textBox.Cut(); });
    });
    paste.OnClick([&]() {
        runCommand("Paste completed.", [](TextBox& textBox) { textBox.Paste(); });
    });
    deleteSelection.OnClick([&]() {
        runCommand("Delete Selection completed.", [](TextBox& textBox) {
            textBox.DeleteSelection();
        });
    });

    selectAllMenu.OnInvoked([&]() {
        runCommand("Select All completed.", [](TextBox& textBox) {
            textBox.SelectAll();
        });
    });
    copyMenu.OnInvoked([&]() {
        runCommand("Copy completed.", [](TextBox& textBox) { textBox.Copy(); });
    });
    cutMenu.OnInvoked([&]() {
        runCommand("Cut completed.", [](TextBox& textBox) { textBox.Cut(); });
    });
    pasteMenu.OnInvoked([&]() {
        runCommand("Paste completed.", [](TextBox& textBox) { textBox.Paste(); });
    });
    deleteMenu.OnInvoked([&]() {
        runCommand("Delete Selection completed.", [](TextBox& textBox) {
            textBox.DeleteSelection();
        });
    });

    Layout targetRow(Orientation::Horizontal, 0, 8);
    targetRow.Add(sourceTarget);
    targetRow.Add(destinationTarget);
    targetRow.AddSpacer();

    Layout positionRow(Orientation::Horizontal, 0, 8);
    positionRow.Add(setCaret);
    positionRow.Add(selectEmoji);
    positionRow.AddSpacer();

    Layout selectionRow(Orientation::Horizontal, 0, 8);
    selectionRow.Add(selectAll);
    selectionRow.Add(clearSelection);
    selectionRow.Add(copy);
    selectionRow.Add(cut);
    selectionRow.Add(paste);
    selectionRow.Add(deleteSelection);
    selectionRow.AddSpacer();

    Layout content(Orientation::Vertical, 20, 8);
    content.Add(title);
    content.Add(sourceLabel);
    content.Add(source, LayoutSizing::Expand);
    content.Add(destinationLabel);
    content.Add(destination, LayoutSizing::Expand);
    content.Add(targetRow);
    content.Add(selectionStatus);
    content.Add(caretStatus);
    content.Add(selectedTextStatus);
    content.Add(targetStatus);
    content.Add(positionRow);
    content.Add(selectionRow);
    content.Add(operationStatus);
    window.SetContent(content);

    closeDemo.OnClick([&]() { window.Close(); });
    reopenDemo.OnClick([&]() { window.Show(); });
    quitApp.OnClick([&]() { app.Quit(); });
    Layout lifecycleContent(Orientation::Vertical, 16, 8);
    lifecycleContent.Add(lifecycleTitle);
    lifecycleContent.Add(closeDemo);
    lifecycleContent.Add(reopenDemo);
    lifecycleContent.Add(quitApp);
    lifecycle.SetContent(lifecycleContent);
    lifecycle.SetTitle("Text Editing Lifecycle");
    lifecycle.SetSize(300, 260);

    refreshStatus();
    if (!window.Show() || !lifecycle.Show()) return 1;
    return app.Run();
}

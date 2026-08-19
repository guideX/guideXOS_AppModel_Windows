#include <guidexos/appmodel/appmodel.hpp>

#include <string>

using namespace guidexos::appmodel;

namespace {

class ClipboardWindow final {
public:
    explicit ClipboardWindow(Application& application)
        : window_(application), edit_("Edit"), copyMenu_("Copy Input"),
          pasteMenu_("Paste To Input"), readMenu_("Read Clipboard"),
          clearMenu_("Clear Clipboard"), input_("Initial text"),
          clipboardStatus_("Clipboard: no supported text") {
        edit_.Add(copyMenu_);
        edit_.Add(pasteMenu_);
        edit_.Add(readMenu_);
        edit_.Add(clearMenu_);
        menuBar_.Add(edit_);
        window_.SetMenuBar(menuBar_);

        copyButton_.OnClick([this]() { CopyInput(); });
        pasteButton_.OnClick([this]() { PasteToInput(); });
        clearButton_.OnClick([this]() { ClearClipboard(); });
        readButton_.OnClick([this]() { ReadClipboard(); });
        copyMenu_.OnInvoked([this]() { CopyInput(); });
        pasteMenu_.OnInvoked([this]() { PasteToInput(); });
        readMenu_.OnInvoked([this]() { ReadClipboard(); });
        clearMenu_.OnInvoked([this]() { ClearClipboard(); });

        window_.OnClosing([this](WindowClosingEvent&) {
            // Exercise the same process-wide service during lifecycle
            // dispatch without making close depend on clipboard contents.
            try {
                (void)Clipboard::HasText();
            } catch (const ClipboardError& error) {
                operationStatus_.SetText("Close clipboard check failed: " +
                                         std::string(error.what()));
            }
        });

        Layout inputRow(Orientation::Horizontal, 0, 8);
        inputRow.Add(inputLabel_);
        inputRow.Add(input_, LayoutSizing::Expand);

        Layout actions(Orientation::Horizontal, 0, 8);
        actions.Add(copyButton_);
        actions.Add(pasteButton_);
        actions.Add(clearButton_);
        actions.Add(readButton_);
        actions.AddSpacer();

        Layout content(Orientation::Vertical, 20, 10);
        content.Add(title_);
        content.Add(inputRow);
        content.Add(clipboardLabel_);
        content.Add(clipboardStatus_);
        content.Add(operationStatus_);
        content.Add(actions);
        window_.SetContent(content);

        RefreshClipboardState();
        window_.SetTitle("guideXOS Clipboard Demo");
        window_.SetSize(900, 520);
    }

    bool Show() { return window_.Show(); }

private:
    void SetClipboardStatus(const std::string& text) {
        clipboardStatus_.SetText("Clipboard: " +
                                 (text.empty() ? "<empty>" : text));
    }

    void SetNoClipboardText() {
        clipboardStatus_.SetText("Clipboard: no supported text");
        pasteButton_.SetEnabled(false);
        readButton_.SetEnabled(false);
        clearButton_.SetEnabled(false);
        pasteMenu_.SetEnabled(false);
        readMenu_.SetEnabled(false);
        clearMenu_.SetEnabled(false);
    }

    void SetClipboardError(const ClipboardError& error) {
        clipboardStatus_.SetText("Clipboard error: " +
                                 std::string(error.what()));
        pasteButton_.SetEnabled(false);
        readButton_.SetEnabled(false);
        clearButton_.SetEnabled(false);
        pasteMenu_.SetEnabled(false);
        readMenu_.SetEnabled(false);
        clearMenu_.SetEnabled(false);
    }

    void RefreshClipboardState() {
        try {
            if (!Clipboard::HasText()) {
                SetNoClipboardText();
                return;
            }
            const std::string text = Clipboard::GetText();
            SetClipboardStatus(text);
            pasteButton_.SetEnabled(true);
            readButton_.SetEnabled(true);
            clearButton_.SetEnabled(true);
            pasteMenu_.SetEnabled(true);
            readMenu_.SetEnabled(true);
            clearMenu_.SetEnabled(true);
        } catch (const ClipboardError& error) {
            SetClipboardError(error);
        }
    }

    void CopyInput() {
        try {
            Clipboard::SetText(input_.GetText());
            operationStatus_.SetText("Copied input to the clipboard.");
            RefreshClipboardState();
        } catch (const ClipboardError& error) {
            SetClipboardError(error);
        }
    }

    void PasteToInput() {
        try {
            if (!Clipboard::HasText()) {
                RefreshClipboardState();
                return;
            }
            input_.SetText(Clipboard::GetText());
            operationStatus_.SetText("Pasted clipboard text into Input.");
            RefreshClipboardState();
        } catch (const ClipboardError& error) {
            SetClipboardError(error);
        }
    }

    void ReadClipboard() {
        try {
            if (!Clipboard::HasText()) {
                RefreshClipboardState();
                return;
            }
            const std::string text = Clipboard::GetText();
            operationStatus_.SetText(text.empty() ? "Read empty clipboard text."
                                                  : "Read clipboard text.");
            SetClipboardStatus(text);
        } catch (const ClipboardError& error) {
            SetClipboardError(error);
        }
    }

    void ClearClipboard() {
        try {
            Clipboard::Clear();
            operationStatus_.SetText("Cleared all clipboard formats.");
            RefreshClipboardState();
        } catch (const ClipboardError& error) {
            SetClipboardError(error);
        }
    }

    Window window_;
    MenuBar menuBar_;
    Menu edit_;
    MenuItem copyMenu_;
    MenuItem pasteMenu_;
    MenuItem readMenu_;
    MenuItem clearMenu_;
    Label title_{"Clipboard Demo"};
    Label inputLabel_{"Input"};
    TextBox input_;
    Label clipboardLabel_{"Clipboard"};
    Label clipboardStatus_;
    Label operationStatus_{"Ready"};
    Button copyButton_{"Copy Input"};
    Button pasteButton_{"Paste To Input"};
    Button clearButton_{"Clear Clipboard"};
    Button readButton_{"Read Clipboard"};
};

} // namespace

int main() {
    Application app("com.guidexos.samples.clipboardapp");
    ClipboardWindow window(app);
    if (!window.Show()) return 1;
    return app.Run();
}

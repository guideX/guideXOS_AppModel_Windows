#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>

using namespace guidexos::appmodel;

namespace {

const std::string kUnicodeText =
    "A caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x9A\x80\xF0\x9F\xA7\xAA";

class ClipboardBackup final {
public:
    ClipboardBackup() {
        if (Clipboard::HasText()) original_ = Clipboard::GetText();
    }

    ~ClipboardBackup() {
        try {
            if (original_) {
                Clipboard::SetText(*original_);
            } else {
                Clipboard::Clear();
            }
        } catch (...) {
        }
    }

    ClipboardBackup(const ClipboardBackup&) = delete;
    ClipboardBackup& operator=(const ClipboardBackup&) = delete;

private:
    std::optional<std::string> original_;
};

template <typename Error, typename Callback>
void ExpectThrow(Callback&& callback) {
    bool thrown = false;
    try {
        callback();
    } catch (const Error&) {
        thrown = true;
    }
    assert(thrown);
}

void TestScalarIndexingAndSelection() {
    TextBox text(kUnicodeText);
    assert(text.GetCaretIndex() == 12);
    assert((text.GetSelection() == TextRange{12, 0}));

    text.SetCaretIndex(10);
    assert(text.GetCaretIndex() == 10);
    assert((text.GetSelection() == TextRange{10, 0}));
    text.SetSelection(TextRange{10, 2});
    assert(text.GetCaretIndex() == 12);
    assert(text.GetSelectedText() == "\xF0\x9F\x9A\x80\xF0\x9F\xA7\xAA");

    text.ClearSelection();
    assert(text.GetCaretIndex() == 12);
    assert((text.GetSelection() == TextRange{12, 0}));
    text.SelectAll();
    assert((text.GetSelection() == TextRange{0, 12}));
    assert(text.GetSelectedText() == kUnicodeText);

    ExpectThrow<std::out_of_range>([&] { text.SetCaretIndex(13); });
    ExpectThrow<std::out_of_range>([&] {
        text.SetSelection(TextRange{11, 2});
    });

    int events = 0;
    text.OnTextChanged([&](const std::string& value) {
        (void)value;
        ++events;
        assert(value == "reset");
        assert(text.GetCaretIndex() == 5);
        assert((text.GetSelection() == TextRange{5, 0}));
    });
    text.SetText("reset");
    assert(events == 1);
    assert(text.GetCaretIndex() == 5);
    assert((text.GetSelection() == TextRange{5, 0}));

    // SetText resets even when the text value is unchanged, without creating
    // a redundant TextChanged event.
    text.SetSelection(TextRange{1, 2});
    text.SetText("reset");
    assert(events == 1);
    assert(text.GetCaretIndex() == 5);
    assert((text.GetSelection() == TextRange{5, 0}));
}

void TestClipboardCommandsAndEvents() {
    ClipboardBackup backup;
    TextBox text("hello");
    int events = 0;
    text.OnTextChanged([&](const std::string&) { ++events; });

    Clipboard::SetText("sentinel");
    text.Copy();
    assert(Clipboard::GetText() == "sentinel");

    text.SetSelection(TextRange{1, 3});
    assert(text.GetSelectedText() == "ell");
    text.Copy();
    assert(Clipboard::GetText() == "ell");

    text.Cut();
    assert(text.GetText() == "ho");
    assert(text.GetCaretIndex() == 1);
    assert((text.GetSelection() == TextRange{1, 0}));
    assert(events == 1);
    assert(Clipboard::GetText() == "ell");

    Clipboard::SetText("\xF0\x9F\x9A\x80");
    text.Paste();
    assert(text.GetText() == "h\xF0\x9F\x9A\x80o");
    assert(text.GetCaretIndex() == 2);
    assert((text.GetSelection() == TextRange{2, 0}));
    assert(events == 2);

    text.SetSelection(TextRange{1, 1});
    Clipboard::SetText("\xE6\x97\xA5\xE6\x9C\xAC");
    text.Paste();
    assert(text.GetText() == "h\xE6\x97\xA5\xE6\x9C\xACo");
    assert(text.GetCaretIndex() == 3);
    assert(events == 3);

    Clipboard::Clear();
    text.Paste();
    assert(text.GetText() == "h\xE6\x97\xA5\xE6\x9C\xACo");
    assert(events == 3);

    text.SetSelection(TextRange{1, 2});
    text.DeleteSelection();
    assert(text.GetText() == "ho");
    assert(text.GetCaretIndex() == 1);
    assert((text.GetSelection() == TextRange{1, 0}));
    assert(events == 4);
    assert(!Clipboard::HasText());
}

void TestReentrantCallbacks() {
    ClipboardBackup backup;
    Clipboard::SetText("outer clipboard");
    TextBox first("abc");
    TextBox second("xyz");
    int firstEvents = 0;
    bool nested = false;

    first.OnTextChanged([&](const std::string& value) {
        (void)value;
        ++firstEvents;
        assert(first.GetText() == value);
        assert((first.GetSelection() == TextRange{value.size(), 0}));
        first.SelectAll();
        first.Copy();
        second.SetSelection(TextRange{0, 1});
        second.Cut();
        second.Paste();
        first.SetCaretIndex(0);
        first.SetSelection(TextRange{0, 1});
        if (!nested) {
            nested = true;
            first.SetText("callback normalized");
        }
    });

    first.SetText("outer");
    assert(firstEvents == 2);
    assert(first.GetText() == "callback normalized");
    assert((first.GetSelection() == TextRange{0, 1}));
    assert(second.GetText() == "xyz");
}

void TestCloseReopenAndMultiWindowClipboard() {
    ClipboardBackup backup;
    Application app("com.guidexos.tests.text-box-lifecycle");
    Window sourceWindow(app);
    Window destinationWindow(app);
    TextBox source("source \xF0\x9F\x9A\x80");
    TextBox destination("destination");
    Layout sourceLayout;
    Layout destinationLayout;
    sourceLayout.Add(source);
    destinationLayout.Add(destination);
    sourceWindow.SetContent(sourceLayout);
    destinationWindow.SetContent(destinationLayout);

    assert(sourceWindow.Show());
    assert(destinationWindow.Show());
    source.SetSelection(TextRange{0, 8});
    source.Copy();
    assert(Clipboard::GetText() == "source \xF0\x9F\x9A\x80");
    sourceWindow.Close();
    assert(!sourceWindow.IsShown());

    destination.SelectAll();
    destination.Paste();
    assert(destination.GetText() == "source \xF0\x9F\x9A\x80");
    assert(destination.GetCaretIndex() == 8);

    sourceWindow.Show();
    assert((source.GetSelection() == TextRange{0, 8}));
    assert(source.GetSelectedText() == "source \xF0\x9F\x9A\x80");
    source.SetCaretIndex(1);
    sourceWindow.Close();
    assert(source.GetCaretIndex() == 1);
    destinationWindow.Close();
    assert(app.Run() == 0);
}

} // namespace

int main() {
    TestScalarIndexingAndSelection();
    TestClipboardCommandsAndEvents();
    TestReentrantCallbacks();
    TestCloseReopenAndMultiWindowClipboard();
    return 0;
}

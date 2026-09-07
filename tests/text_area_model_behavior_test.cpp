#include <guidexos/appmodel/appmodel.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cassert>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace guidexos::appmodel;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

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

HWND FindWindowForCurrentProcess(const wchar_t* title) {
    struct Search {
        DWORD processId = GetCurrentProcessId();
        const wchar_t* title = nullptr;
        HWND result = nullptr;
    } search{GetCurrentProcessId(), title, nullptr};

    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL {
            auto& search = *reinterpret_cast<Search*>(parameter);
            DWORD processId = 0;
            wchar_t candidateTitle[256]{};
            GetWindowThreadProcessId(window, &processId);
            GetWindowTextW(window, candidateTitle,
                           static_cast<int>(std::size(candidateTitle)));
            if (processId == search.processId &&
                std::wstring(candidateTitle) == search.title) {
                search.result = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.result;
}

std::vector<HWND> FindEditChildren(HWND parent) {
    struct Search {
        std::vector<HWND> result;
    } search;
    EnumChildWindows(
        parent,
        [](HWND child, LPARAM parameter) -> BOOL {
            auto& search = *reinterpret_cast<Search*>(parameter);
            wchar_t className[32]{};
            GetClassNameW(child, className,
                          static_cast<int>(std::size(className)));
            if (std::wstring(className) == L"Edit") search.result.push_back(child);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.result;
}

void TestModelSemantics() {
    TextArea empty;
    assert(empty.GetText().empty());
    assert(empty.GetCaretIndex() == 0);
    assert((empty.GetSelection() == TextRange{0, 0}));
    assert(!empty.IsReadOnly());
    assert(empty.IsWordWrap());
    assert(empty.GetControlRef().GetType() == ControlType::TextArea);
    assert(!empty.GetControlRef().AsTextBox());
    assert(empty.GetControlRef().AsTextArea());

    TextArea area("first\r\nsecond\rthird\nfourth");
    assert(area.GetText() == "first\nsecond\nthird\nfourth");
    assert(area.GetCaretIndex() == 25);
    assert((area.GetSelection() == TextRange{25, 0}));

    int events = 0;
    area.OnTextChanged([&](const std::string& value) {
        ++events;
        (void)value;
        assert(value == area.GetText());
        assert((area.GetSelection() == TextRange{area.GetCaretIndex(), 0}));
    });
    area.SetText("one\r\ntwo\rthree");
    assert(area.GetText() == "one\ntwo\nthree");
    assert(area.GetCaretIndex() == 13);
    assert(events == 1);

    area.SetText("one\ntwo\nthree");
    assert(events == 1);
    assert(area.GetCaretIndex() == 13);
    area.SetSelection({4, 3});
    assert(area.GetSelectedText() == "two");
    area.ClearSelection();
    assert(area.GetCaretIndex() == 7);
    area.SelectAll();
    assert(area.GetSelectedText() == "one\ntwo\nthree");

    area.SetWordWrap(false);
    assert(!area.IsWordWrap());
    area.SetWordWrap(true);
    assert(area.IsWordWrap());
    area.SetReadOnly(true);
    assert(area.IsReadOnly());
    area.SetReadOnly(false);
}

void TestNativeEditingAndLifecycle() {
    ClipboardBackup backup;
    Application app("com.guidexos.tests.text-area-lifecycle");
    Window window(app);
    window.SetTitle("guideXOS Text Area Test");
    window.SetSize(800, 520);

    TextArea first("one");
    TextArea second("independent");
    TextBox singleLine("single");
    Layout content(Orientation::Vertical, 12, 8);
    content.Add(first, LayoutSizing::Expand);
    content.Add(second, LayoutSizing::Expand);
    content.Add(singleLine);
    window.SetContent(content);

    int firstEvents = 0;
    first.OnTextChanged([&](const std::string&) { ++firstEvents; });
    Require(window.Show(), "TextArea test window did not show");
    const HWND nativeWindow = FindWindowForCurrentProcess(
        L"guideXOS Text Area Test");
    assert(nativeWindow != nullptr);
    const auto editors = FindEditChildren(nativeWindow);
    assert(editors.size() == 3);
    const HWND firstNative = editors[0];
    const HWND singleNative = editors[2];

    Require(first.Focus(), "TextArea did not receive focus");
    assert(first.GetControlRef().HasFocus());
    SendMessageW(firstNative, WM_CHAR, static_cast<WPARAM>(L'!'), 0);
    SendMessageW(firstNative, WM_CHAR, static_cast<WPARAM>(VK_RETURN), 0);
    SendMessageW(firstNative, WM_CHAR, static_cast<WPARAM>(L'n'), 0);
    SendMessageW(firstNative, WM_CHAR, static_cast<WPARAM>(L'e'), 0);
    SendMessageW(firstNative, WM_CHAR, static_cast<WPARAM>(L'x'), 0);
    SendMessageW(firstNative, WM_CHAR, static_cast<WPARAM>(L't'), 0);
    assert(first.GetText() == "one!\nnext");
    assert(firstEvents == 6);

    first.SetSelection({0, 1});
    SendMessageW(firstNative, WM_KEYDOWN, VK_DELETE, 0);
    SendMessageW(firstNative, WM_KEYUP, VK_DELETE, 0);
    assert(first.GetText() == "ne!\nnext");
    assert(firstEvents == 7);
    first.SetSelection({1, 2});
    assert(first.GetSelectedText() == "e!");

    Clipboard::SetText("paste\nblock");
    first.Paste();
    assert(first.GetText() == "npaste\nblock\nnext");
    assert(first.GetCaretIndex() == 12);
    first.SelectAll();
    first.Copy();
    assert(Clipboard::GetText() == "npaste\nblock\nnext");
    first.Cut();
    assert(first.GetText().empty());
    assert(first.GetCaretIndex() == 0);
    first.Paste();
    assert(first.GetText() == "npaste\nblock\nnext");

    first.SetSelection({0, 1});
    Clipboard::SetText("sentinel");
    first.SetReadOnly(true);
    first.Copy();
    assert(Clipboard::GetText() == "n");
    first.Cut();
    first.Paste();
    first.DeleteSelection();
    assert(first.GetText() == "npaste\nblock\nnext");
    SendMessageW(firstNative, WM_CHAR, static_cast<WPARAM>(L'X'), 0);
    assert(first.GetText() == "npaste\nblock\nnext");
    first.SetReadOnly(false);

    Require(singleLine.Focus(), "TextBox did not receive focus");
    assert(singleLine.GetControlRef().HasFocus());
    assert(!first.GetControlRef().HasFocus());
    SendMessageW(singleNative, WM_CHAR, static_cast<WPARAM>(VK_RETURN), 0);
    assert(singleLine.GetText() == "single");

    second.SetText("second\neditor");
    assert(second.GetText() == "second\neditor");
    second.SetWordWrap(false);
    assert(!second.IsWordWrap());
    second.SetWordWrap(true);
    assert(second.IsWordWrap());

    window.Close();
    assert(!window.IsShown());
    assert(first.GetText() == "npaste\nblock\nnext");
    assert((first.GetSelection() == TextRange{0, 1}));
    assert(first.Focus() == false);
    Require(window.Show(), "TextArea test window did not reopen");
    assert(first.GetText() == "npaste\nblock\nnext");
    assert((first.GetSelection() == TextRange{0, 1}));
    window.Close();
    Require(app.Run() == 0, "TextArea test application did not shut down cleanly");
}

} // namespace

int main() {
    TestModelSemantics();
    TestNativeEditingAndLifecycle();
    return 0;
}

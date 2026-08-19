#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <optional>
#include <string>
#include <vector>

using namespace guidexos::appmodel;

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, \
                         __LINE__); \
            throw std::runtime_error("clipboard test assertion failed"); \
        } \
    } while (false)

namespace {

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

class ClipboardBackup final {
public:
    ClipboardBackup() {
        if (Clipboard::HasText()) originalText_ = Clipboard::GetText();
    }

    ~ClipboardBackup() {
        try {
            if (originalText_) {
                Clipboard::SetText(*originalText_);
            } else {
                Clipboard::Clear();
            }
        } catch (...) {
            // Test cleanup must not hide the assertion that failed first.
        }
    }

    ClipboardBackup(const ClipboardBackup&) = delete;
    ClipboardBackup& operator=(const ClipboardBackup&) = delete;

private:
    std::optional<std::string> originalText_;
};

void TestRoundTripsAndContract() {
    ClipboardBackup backup;

    Clipboard::Clear();
    assert(!Clipboard::HasText());
    ExpectThrow<ClipboardNoTextError>([] { (void)Clipboard::GetText(); });

    const std::string accented = "caf\xC3\xA9";
    const std::string nonLatin = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    const std::string emoji = "rocket \xF0\x9F\x9A\x80";
    for (const std::string& value : {std::string("ASCII"), accented,
                                     nonLatin, emoji, std::string("long ") +
                                         std::string(128U * 1024U, 'x')}) {
        Clipboard::SetText(value);
        assert(Clipboard::HasText());
        assert(Clipboard::GetText() == value);
    }

    Clipboard::SetText({});
    assert(Clipboard::HasText());
    assert(Clipboard::GetText().empty());

    Clipboard::SetText("first");
    Clipboard::SetText("second");
    assert(Clipboard::GetText() == "second");
    Clipboard::Clear();
    assert(!Clipboard::HasText());
}

void TestValidationAndBounds() {
    ClipboardBackup backup;

    ExpectThrow<ClipboardInvalidUtf8Error>([] {
        Clipboard::SetText(std::string("\xF0\x28\x8C\xBC", 4));
    });
    ExpectThrow<ClipboardEmbeddedNulError>([] {
        Clipboard::SetText(std::string("prefix\0suffix", 13));
    });
    ExpectThrow<ClipboardTooLargeError>([] {
        Clipboard::SetText(std::string(kMaximumClipboardTextBytes + 1U, 'x'));
    });
}

void TestMalformedNativeText() {
    ClipboardBackup backup;

    // An unpaired high surrogate is invalid UTF-16 even though it is
    // NUL-terminated. The strict Windows conversion must reject it.
    if (OpenClipboard(nullptr) == FALSE) {
        throw std::runtime_error("Unable to prepare malformed surrogate text");
    }
    if (EmptyClipboard() == FALSE) {
        CloseClipboard();
        throw std::runtime_error("Unable to prepare malformed surrogate text");
    }
    HGLOBAL surrogate = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * 2U);
    if (!surrogate) {
        CloseClipboard();
        throw std::runtime_error("Unable to allocate malformed surrogate text");
    }
    auto* surrogateText = static_cast<wchar_t*>(GlobalLock(surrogate));
    if (!surrogateText) {
        GlobalFree(surrogate);
        CloseClipboard();
        throw std::runtime_error("Unable to lock malformed surrogate text");
    }
    surrogateText[0] = static_cast<wchar_t>(0xD800);
    surrogateText[1] = L'\0';
    SetLastError(ERROR_SUCCESS);
    if (GlobalUnlock(surrogate) != FALSE && GetLastError() != ERROR_SUCCESS) {
        GlobalFree(surrogate);
        CloseClipboard();
        throw std::runtime_error("Unable to unlock malformed surrogate text");
    }
    if (!SetClipboardData(CF_UNICODETEXT, surrogate)) {
        GlobalFree(surrogate);
        CloseClipboard();
        throw std::runtime_error("Unable to publish malformed surrogate text");
    }
    CloseClipboard();
    ExpectThrow<ClipboardMalformedTextError>([] { (void)Clipboard::GetText(); });
}

void TestCallbacksAndMultiWindowVisibility() {
    ClipboardBackup backup;
    Clipboard::SetText("initial");

    Application app("com.guidexos.tests.clipboard-callbacks");
    Window first(app);
    Window second(app);
    Button button("button");
    CheckBox check("check");
    ComboBox combo;
    combo.AddItem("combo");
    MenuBar menuBar;
    Menu edit("Edit");
    MenuItem menuItem("Copy");
    edit.Add(menuItem);
    menuBar.Add(edit);
    Layout content;
    content.Add(button);
    content.Add(check);
    content.Add(combo);
    first.SetContent(content);
    first.SetMenuBar(menuBar);

    button.OnClick([] { Clipboard::SetText("button callback"); });
    menuItem.OnInvoked([] { Clipboard::SetText("menu callback"); });
    check.OnCheckedChanged([](bool) { Clipboard::SetText("check callback"); });
    combo.OnSelectionChanged(
        [](std::optional<std::size_t>) { Clipboard::SetText("combo callback"); });

    std::string closingText;
    second.OnClosing([&](WindowClosingEvent&) {
        closingText = Clipboard::GetText();
    });

    assert(first.Show());
    assert(second.Show());
    button.Click();
    assert(Clipboard::GetText() == "button callback");
    menuItem.Invoke();
    assert(Clipboard::GetText() == "menu callback");
    check.SetChecked(true);
    assert(Clipboard::GetText() == "check callback");
    combo.SetSelectedIndex(0);
    assert(Clipboard::GetText() == "combo callback");

    Clipboard::SetText("window A writes");
    assert(Clipboard::GetText() == "window A writes");
    first.Close();
    assert(second.IsShown());
    assert(Clipboard::GetText() == "window A writes");
    assert(first.Show());
    assert(Clipboard::GetText() == "window A writes");

    Clipboard::SetText("closing callback");
    second.Close();
    assert(closingText == "closing callback");
    first.Close();
}

void TestBoundedContention() {
    ClipboardBackup backup;

    std::vector<wchar_t> modulePath(32768, L'\0');
    const DWORD moduleLength = GetModuleFileNameW(
        nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (moduleLength == 0 || moduleLength >= modulePath.size()) {
        std::puts("Clipboard contention probe skipped: test executable path was unavailable.");
        return;
    }

    const std::wstring eventName =
        L"Local\\guideXOS-clipboard-contention-" +
        std::to_wstring(GetCurrentProcessId());
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
    if (!ready) {
        std::puts("Clipboard contention probe skipped: readiness event was unavailable.");
        return;
    }

    std::wstring commandLine = L"\"" +
        std::wstring(modulePath.data(), moduleLength) +
        L"\" --hold-clipboard " + std::to_wstring(GetCurrentProcessId());
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(
        nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr,
        nullptr, &startup, &process);
    if (!created) {
        CloseHandle(ready);
        std::puts("Clipboard contention probe skipped: helper process was unavailable.");
        return;
    }
    CloseHandle(process.hThread);

    const DWORD signaled = WaitForSingleObject(ready, 2000);
    CloseHandle(ready);
    if (signaled != WAIT_OBJECT_0) {
        WaitForSingleObject(process.hProcess, 2000);
        CloseHandle(process.hProcess);
        throw std::runtime_error("Clipboard contention helper did not signal");
    }
    DWORD helperExitCode = STILL_ACTIVE;
    Sleep(100);
    GetExitCodeProcess(process.hProcess, &helperExitCode);
    if (GetClipboardOwner() == nullptr) {
        WaitForSingleObject(process.hProcess, 2000);
        CloseHandle(process.hProcess);
        std::puts("Clipboard contention probe skipped: helper did not become clipboard owner.");
        return;
    }
    if (helperExitCode != STILL_ACTIVE) {
        WaitForSingleObject(process.hProcess, 2000);
        CloseHandle(process.hProcess);
        std::puts("Clipboard contention probe skipped: helper could not own the clipboard.");
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    bool unavailable = false;
    try {
        Clipboard::SetText("contention");
    } catch (const ClipboardUnavailableError&) {
        unavailable = true;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (!unavailable) {
        WaitForSingleObject(process.hProcess, 2000);
        CloseHandle(process.hProcess);
        throw std::runtime_error("Clipboard contention was not reported");
    }
    if (elapsed >= std::chrono::seconds(1)) {
        WaitForSingleObject(process.hProcess, 2000);
        CloseHandle(process.hProcess);
        throw std::runtime_error("Clipboard contention was not bounded");
    }

    WaitForSingleObject(process.hProcess, 2000);
    CloseHandle(process.hProcess);
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc >= 3 && std::wstring(argv[1]) == L"--hold-clipboard") {
        const std::wstring eventName =
            std::wstring(L"Local\\guideXOS-clipboard-contention-") + argv[2];
        HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
        if (!ready) return 2;
        const HWND owner = GetConsoleWindow();
        if (!owner) {
            SetEvent(ready);
            CloseHandle(ready);
            return 4;
        }
        const BOOL opened = OpenClipboard(owner);
        const BOOL prepared = opened != FALSE && EmptyClipboard() != FALSE;
        SetEvent(ready);
        CloseHandle(ready);
        if (!prepared) {
            if (opened != FALSE) CloseClipboard();
            return 3;
        }
        Sleep(1000);
        CloseClipboard();
        return 0;
    }

    try {
        TestRoundTripsAndContract();
        TestValidationAndBounds();
        TestMalformedNativeText();
        TestCallbacksAndMultiWindowVisibility();
        TestBoundedContention();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Clipboard test failed: %s\n", error.what());
        return 1;
    }
}

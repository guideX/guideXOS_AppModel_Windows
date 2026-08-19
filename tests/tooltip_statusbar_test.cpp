#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <guidexos/appmodel/appmodel.hpp>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <iterator>
#include <stdexcept>
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

namespace {

std::wstring WindowClass(HWND window) {
    wchar_t value[256]{};
    GetClassNameW(window, value, static_cast<int>(std::size(value)));
    return value;
}

std::vector<HWND> FindTopLevelWindows(const std::wstring& className,
                                      DWORD processId) {
    struct Search final {
        std::wstring className;
        DWORD processId;
        std::vector<HWND> windows;
    } search{className, processId, {}};
    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL {
            auto& search = *reinterpret_cast<Search*>(parameter);
            DWORD processId = 0;
            GetWindowThreadProcessId(window, &processId);
            if (processId == search.processId &&
                WindowClass(window) == search.className) {
                search.windows.push_back(window);
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.windows;
}

HWND FindWindowForCurrentProcess(const std::wstring& title) {
    struct Search final {
        std::wstring title;
        HWND result = nullptr;
    } search{title};
    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL {
            auto& search = *reinterpret_cast<Search*>(parameter);
            wchar_t title[512]{};
            GetWindowTextW(window, title, static_cast<int>(std::size(title)));
            DWORD processId = 0;
            GetWindowThreadProcessId(window, &processId);
            if (processId == GetCurrentProcessId() &&
                title == search.title) {
                search.result = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.result;
}

std::vector<std::wstring> ToolTipTexts(DWORD processId,
                                       HWND owner = nullptr) {
    std::vector<std::wstring> result;
    for (const HWND toolTip : FindTopLevelWindows(TOOLTIPS_CLASSW, processId)) {
        const int count = static_cast<int>(SendMessageW(
            toolTip, TTM_GETTOOLCOUNT, 0, 0));
        for (int index = 0; index < count; ++index) {
            TOOLINFOW info{};
            info.cbSize = TTTOOLINFOW_V1_SIZE;
            if (SendMessageW(toolTip, TTM_ENUMTOOLSW,
                             static_cast<WPARAM>(index),
                             reinterpret_cast<LPARAM>(&info)) == FALSE) {
                continue;
            }
            if (owner && info.hwnd != owner) continue;
            wchar_t text[8192]{};
            info.lpszText = text;
            SendMessageW(toolTip, TTM_GETTEXTW, 0,
                         reinterpret_cast<LPARAM>(&info));
            result.emplace_back(text);
        }
    }
    return result;
}

HWND FindStatusBar(HWND window) {
    return FindWindowExW(window, nullptr, STATUSCLASSNAMEW, nullptr);
}

std::wstring StatusText(HWND statusBar) {
    wchar_t text[8192]{};
    SendMessageW(statusBar, SB_GETTEXTW, 0,
                 reinterpret_cast<LPARAM>(text));
    return text;
}

RECT ChildRect(HWND parent, HWND child) {
    RECT result{};
    GetWindowRect(child, &result);
    POINT topLeft{result.left, result.top};
    POINT bottomRight{result.right, result.bottom};
    ScreenToClient(parent, &topLeft);
    ScreenToClient(parent, &bottomRight);
    return RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

struct NativeDropFiles {
    DWORD pFiles;
    POINT point;
    BOOL fNC;
    BOOL fWide;
};

HDROP MakeNativeDrop(const std::vector<std::wstring>& paths) {
    std::vector<wchar_t> payload;
    for (const auto& path : paths) {
        payload.insert(payload.end(), path.begin(), path.end());
        payload.push_back(L'\0');
    }
    payload.push_back(L'\0');

    const SIZE_T bytes = sizeof(NativeDropFiles) +
                         payload.size() * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GHND, bytes);
    if (!memory) return nullptr;
    auto* drop = static_cast<NativeDropFiles*>(GlobalLock(memory));
    if (!drop) {
        GlobalFree(memory);
        return nullptr;
    }
    drop->pFiles = sizeof(NativeDropFiles);
    drop->fWide = TRUE;
    std::memcpy(reinterpret_cast<char*>(drop) + sizeof(NativeDropFiles),
                payload.data(), payload.size() * sizeof(wchar_t));
    GlobalUnlock(memory);
    return reinterpret_cast<HDROP>(memory);
}

void SendNativeDrop(HWND window, const std::vector<std::wstring>& paths) {
    const HDROP drop = MakeNativeDrop(paths);
    if (!drop) return;
    SendMessageW(window, WM_DROPFILES, reinterpret_cast<WPARAM>(drop), 0);
}

bool ContainsText(const std::vector<std::wstring>& values,
                  const std::wstring& expected) {
    return std::find(values.begin(), values.end(), expected) != values.end();
}

} // namespace

int main() {
    Button button;
    TextBox textBox;
    ListBox list;
    CheckBox check;
    RadioButton radio;
    ComboBox combo;
    assert(button.GetToolTip().empty());
    assert(textBox.GetToolTip().empty());
    assert(list.GetToolTip().empty());
    assert(check.GetToolTip().empty());
    assert(radio.GetToolTip().empty());
    assert(combo.GetToolTip().empty());

    const std::string unicode = "Cr" "\xC3\xA9" "er \xF0\x9F\x9A\x80";
    button.SetToolTip(unicode);
    button.SetToolTip("replace");
    assert(button.GetToolTip() == "replace");
    button.SetToolTip({});
    assert(button.GetToolTip().empty());
    textBox.SetToolTip("Enter \xE2\x9C\x93");
    list.SetToolTip("Choose an item");
    check.SetToolTip("Enable the option");
    radio.SetToolTip("Choose the mode");
    combo.SetToolTip("Choose a value");

    bool rejected = false;
    try {
        button.SetToolTip(std::string("\x80", 1));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        button.SetToolTip(std::string(8193, 'x'));
    } catch (const std::length_error&) {
        rejected = true;
    }
    assert(rejected);
    button.SetToolTip("Create A");

    {
        Application simpleApplication("com.guidexos.tests.tooltip-statusbar-simple",
                                      ShutdownMode::Explicit);
        Window simpleWindow(simpleApplication);
        const std::string simpleTitle = "Tooltip StatusBar Simple " +
            std::to_string(GetCurrentProcessId());
        const std::wstring simpleWideTitle = L"Tooltip StatusBar Simple " +
            std::to_wstring(GetCurrentProcessId());
        simpleWindow.SetTitle(simpleTitle);
        Label simpleLabel("Simple");
        Layout simpleContent;
        simpleContent.Add(simpleLabel);
        simpleWindow.SetContent(simpleContent);
        StatusBar simpleStatus("Ready");
        simpleWindow.SetStatusBar(simpleStatus);
        assert(simpleWindow.Show());
        const HWND simpleNative = FindWindowForCurrentProcess(simpleWideTitle);
        assert(simpleNative != nullptr);
        assert(StatusText(FindStatusBar(simpleNative)) == L"Ready");
        simpleWindow.Close();
    }

    StatusBar emptyStatus;
    assert(emptyStatus.GetText().empty());
    StatusBar status("Ready A");
    status.SetText(unicode);
    assert(status.GetText() == unicode);
    rejected = false;
    try {
        status.SetText(std::string("\xC0\xAF", 2));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    Application application("com.guidexos.tests.tooltip-statusbar",
                             ShutdownMode::Explicit);
    Window first(application);
    Window second(application);
    const std::string title =
        "Tooltip StatusBar " + std::to_string(GetCurrentProcessId());
    const std::wstring wideTitle =
        L"Tooltip StatusBar " + std::to_wstring(GetCurrentProcessId());
    first.SetTitle(title);
    second.SetTitle("Tooltip StatusBar Second " +
                    std::to_string(GetCurrentProcessId()));
    first.SetSize(640, 360);
    second.SetSize(500, 300);

    Layout firstContent;
    firstContent.Add(button);
    firstContent.Add(textBox);
    firstContent.Add(list);
    firstContent.Add(check);
    firstContent.Add(radio);
    firstContent.Add(combo);
    first.SetContent(firstContent);
    Layout secondContent;
    Button secondButton("Second");
    secondButton.SetToolTip("Second window");
    secondContent.Add(secondButton);
    second.SetContent(secondContent);

    MenuBar menuBar;
    Menu file("&File");
    MenuItem menuItem("Ready");
    file.Add(menuItem);
    menuBar.Add(file);
    first.SetMenuBar(menuBar);

    StatusBar firstStatus("Ready A");
    StatusBar secondStatus("Ready B");
    first.SetStatusBar(firstStatus);
    second.SetStatusBar(secondStatus);
    rejected = false;
    try {
        second.SetStatusBar(firstStatus);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);
    first.ClearStatusBar();
    second.SetStatusBar(firstStatus);
    second.ClearStatusBar();
    first.SetStatusBar(firstStatus);
    second.SetStatusBar(secondStatus);

    Application otherApplication("com.guidexos.tests.tooltip-statusbar-other",
                                 ShutdownMode::Explicit);
    Window otherWindow(otherApplication);
    rejected = false;
    try {
        otherWindow.SetStatusBar(firstStatus);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);

    button.OnClick([&]() {
        firstStatus.SetText("Clicked A");
        button.SetToolTip("Changed A");
    });
    textBox.OnTextChanged([&](const std::string&) {
        firstStatus.SetText("Text changed");
    });
    menuItem.OnInvoked([&]() { firstStatus.SetText("Menu invoked"); });
    assert(first.Show());
    assert(second.Show());

    const DWORD processId = GetCurrentProcessId();
    const HWND firstNative = FindWindowForCurrentProcess(wideTitle);
    assert(firstNative != nullptr);
    const HWND firstStatusNative = FindStatusBar(firstNative);
    assert(firstStatusNative != nullptr);
    assert(StatusText(firstStatusNative) == L"Ready A");
    RECT client{};
    GetClientRect(firstNative, &client);
    const RECT statusRect = ChildRect(firstNative, firstStatusNative);
    assert(statusRect.left == 0);
    assert(statusRect.right == client.right);
    assert(statusRect.bottom == client.bottom);
    assert(statusRect.top < statusRect.bottom);
    const HWND buttonNative = FindWindowExW(firstNative, nullptr, L"BUTTON", nullptr);
    assert(buttonNative != nullptr);
    const RECT buttonRect = ChildRect(firstNative, buttonNative);
    assert(buttonRect.bottom <= statusRect.top);
    assert(client.right == 640);
    assert(client.bottom - (statusRect.bottom - statusRect.top) == 360);

    const auto initialToolTips = ToolTipTexts(processId);
    assert(ContainsText(initialToolTips, L"Create A"));
    assert(ContainsText(initialToolTips, L"Enter \x2713"));
    assert(ContainsText(initialToolTips, L"Choose an item"));
    assert(ContainsText(initialToolTips, L"Enable the option"));
    assert(ContainsText(initialToolTips, L"Choose the mode"));
    assert(ContainsText(initialToolTips, L"Choose a value"));
    assert(ContainsText(initialToolTips, L"Second window"));

    button.Click();
    assert(firstStatus.GetText() == "Clicked A");
    assert(StatusText(firstStatusNative) == L"Clicked A");
    assert(ContainsText(ToolTipTexts(processId), L"Changed A"));
    textBox.SetText("changed");
    assert(StatusText(firstStatusNative) == L"Text changed");
    menuItem.Invoke();
    assert(StatusText(firstStatusNative) == L"Menu invoked");
    first.OnFilesDropped([&](const FileDropEvent& event) {
        firstStatus.SetText("Dropped: " + event.GetFiles().front());
    });
    SendNativeDrop(firstNative, {L"C:\\drop\\status.txt"});
    assert(StatusText(firstStatusNative) == L"Dropped: C:\\drop\\status.txt");
    first.OnClosing([&](WindowClosingEvent& event) {
        firstStatus.SetText("Close canceled");
        event.Cancel();
    });
    first.Close();
    assert(first.IsShown());
    assert(StatusText(firstStatusNative) == L"Close canceled");
    first.OnClosing({});
    button.SetToolTip({});
    assert(!ContainsText(ToolTipTexts(processId), L"Changed A"));
    button.SetToolTip("Reassigned A");
    assert(ContainsText(ToolTipTexts(processId), L"Reassigned A"));

    first.Close();
    assert(!first.IsShown());
    assert(second.IsShown());
    const auto secondToolTips = ToolTipTexts(processId);
    assert(ContainsText(secondToolTips, L"Second window"));
    assert(!ContainsText(secondToolTips, L"Reassigned A"));
    assert(StatusText(FindStatusBar(
               FindTopLevelWindows(L"guideXOS.AppModel.Windows.Foundation",
                                    processId)[0])) == L"Ready B");

    Layout detached;
    first.SetContent(detached);
    assert(first.Show());
    const HWND reopened = FindWindowForCurrentProcess(wideTitle);
    assert(reopened != nullptr);
    assert(ToolTipTexts(processId, reopened).empty());
    first.SetContent(firstContent);
    assert(ContainsText(ToolTipTexts(processId, reopened), L"Reassigned A"));
    assert(StatusText(FindStatusBar(reopened)) == L"Close canceled");
    firstStatus.SetText("Reopened A");
    assert(StatusText(FindStatusBar(reopened)) == L"Reopened A");

    {
        Button transient("Transient");
        transient.SetToolTip("Transient");
        ControlRef identity = transient.GetControlRef();
        assert(identity.IsValid());
    }

    first.Close();
    second.Close();
    otherWindow.Close();
    return 0;
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <string>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.tests.listbox-textbox-sync");
    Window window(app);
    window.SetTitle("guideXOS ListBox TextBox Sync");
    ListBox list;
    list.AddItem("first");
    list.AddItem("second");
    TextBox mirror("initial");
    list.OnSelectionChanged([&](std::optional<std::size_t> index) {
        if (index) mirror.SetText(list.GetItem(*index));
    });
    Layout content;
    content.Add(list);
    content.Add(mirror);
    window.SetContent(content);
    assert(window.Show());

    const HWND nativeWindow = FindWindowW(nullptr, L"guideXOS ListBox TextBox Sync");
    const HWND nativeList = FindWindowExW(nativeWindow, nullptr, L"ListBox", nullptr);
    const HWND nativeEdit = FindWindowExW(nativeWindow, nullptr, L"Edit", nullptr);
    assert(nativeWindow != nullptr);
    assert(nativeList != nullptr);
    assert(nativeEdit != nullptr);

    SendMessageW(nativeList, LB_SETCURSEL, 1, 0);
    SendMessageW(nativeWindow, WM_COMMAND,
                 static_cast<WPARAM>(LBN_SELCHANGE) << 16,
                 reinterpret_cast<LPARAM>(nativeList));

    wchar_t text[64]{};
    GetWindowTextW(nativeEdit, text, static_cast<int>(std::size(text)));
    assert(mirror.GetText() == "second");
    assert(std::wstring(text) == L"second");

    window.Close();
    return 0;
}

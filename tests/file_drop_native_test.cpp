#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

using namespace guidexos::appmodel;

namespace {

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
    assert(memory != nullptr);
    auto* drop = static_cast<NativeDropFiles*>(GlobalLock(memory));
    assert(drop != nullptr);
    drop->pFiles = sizeof(NativeDropFiles);
    drop->fWide = TRUE;
    std::memcpy(reinterpret_cast<char*>(drop) + sizeof(NativeDropFiles),
                payload.data(), payload.size() * sizeof(wchar_t));
    assert(GlobalUnlock(memory) == FALSE || GetLastError() == NO_ERROR);
    return reinterpret_cast<HDROP>(memory);
}

HWND FindWindowForCurrentProcess(const std::wstring& title) {
    HWND window = FindWindowW(nullptr, title.c_str());
    if (!window) return nullptr;
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    return processId == GetCurrentProcessId() ? window : nullptr;
}

void SendNativeDrop(HWND window, const std::vector<std::wstring>& paths) {
    const HDROP drop = MakeNativeDrop(paths);
    SendMessageW(window, WM_DROPFILES, reinterpret_cast<WPARAM>(drop), 0);
}

void TestNativeRoutingAndReopen() {
    Application application("com.guidexos.tests.file-drop-native",
                             ShutdownMode::Explicit);
    Window first(application);
    Window second(application);
    const std::wstring firstTitle =
        L"File Drop Native Test First " + std::to_wstring(GetCurrentProcessId());
    const std::wstring secondTitle =
        L"File Drop Native Test Second " + std::to_wstring(GetCurrentProcessId());
    first.SetTitle("File Drop Native Test First " +
                   std::to_string(GetCurrentProcessId()));
    second.SetTitle("File Drop Native Test Second " +
                    std::to_string(GetCurrentProcessId()));

    std::vector<std::vector<std::string>> firstDrops;
    std::vector<std::vector<std::string>> secondDrops;
    first.OnFilesDropped([&](const FileDropEvent& event) {
        firstDrops.push_back(event.GetFiles());
    });
    second.OnFilesDropped([&](const FileDropEvent& event) {
        secondDrops.push_back(event.GetFiles());
    });
    assert(first.Show());
    assert(second.Show());

    HWND firstNative = FindWindowForCurrentProcess(firstTitle);
    HWND secondNative = FindWindowForCurrentProcess(secondTitle);
    assert(firstNative != nullptr);
    assert(secondNative != nullptr);

    const std::string unicodePath =
        "C:\\drop\\" "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" " "
        "\xF0\x9F\x9A\x80.txt";
    SendNativeDrop(firstNative,
                   {L"C:\\drop folder\\alpha.txt",
                    L"C:\\drop\\\x65E5\x672C\x8A9E \xD83D\xDE80.txt"});
    assert(firstDrops.size() == 1);
    assert(firstDrops[0].size() == 2);
    assert(firstDrops[0][0] == "C:\\drop folder\\alpha.txt");
    assert(firstDrops[0][1] == unicodePath);
    assert(secondDrops.empty());

    SendNativeDrop(secondNative, {L"C:\\drop folder\\alpha.txt"});
    assert(secondDrops.size() == 1);
    assert(firstDrops.size() == 1);

    bool closedInCallback = false;
    first.OnFilesDropped([&](const FileDropEvent&) {
        closedInCallback = true;
        first.Close();
    });
    SendNativeDrop(firstNative, {L"C:\\drop\\close.txt"});
    assert(closedInCallback);
    assert(!first.IsShown());
    assert(second.IsShown());

    // The logical callback survives native detach and the next Show creates
    // a fresh association with DragAcceptFiles.
    std::vector<std::string> reopened;
    first.OnFilesDropped([&](const FileDropEvent& event) {
        reopened = event.GetFiles();
    });
    assert(first.Show());
    firstNative = FindWindowForCurrentProcess(firstTitle);
    assert(firstNative != nullptr);
    SendNativeDrop(firstNative, {L"C:\\drop\\reopened.txt"});
    assert(reopened == std::vector<std::string>{"C:\\drop\\reopened.txt"});
}

} // namespace

int main() {
    TestNativeRoutingAndReopen();
    return 0;
}

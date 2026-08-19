#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

struct NativeDropFiles {
    DWORD pFiles;
    POINT point;
    BOOL fNC;
    BOOL fWide;
};

std::wstring Environment(const wchar_t* name) {
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (length == 0) return {};
    std::wstring value(static_cast<std::size_t>(length), L'\0');
    const DWORD copied = GetEnvironmentVariableW(name, value.data(), length);
    if (copied == 0 || copied >= length) return {};
    value.resize(copied);
    return value;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    if (value.size() > static_cast<std::size_t>(INT_MAX)) return {};
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), length) !=
        length) {
        return {};
    }
    return result;
}

HWND FindCurrentProcessWindow(const std::wstring& title) {
    HWND window = FindWindowW(nullptr, title.c_str());
    if (!window) return nullptr;
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    return processId == GetCurrentProcessId() ? window : nullptr;
}

struct WindowSearch final {
    HWND result = nullptr;
};

BOOL CALLBACK FindAnyCurrentProcessWindowProc(HWND window, LPARAM parameter) {
    auto* search = reinterpret_cast<WindowSearch*>(parameter);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != GetCurrentProcessId() ||
        IsWindowVisible(window) == FALSE) {
        return TRUE;
    }
    wchar_t className[64]{};
    GetClassNameW(window, className, static_cast<int>(std::size(className)));
    if (std::wstring_view(className) == L"#32770") return TRUE;
    search->result = window;
    return FALSE;
}

HWND FindAnyCurrentProcessWindow() {
    WindowSearch search;
    EnumWindows(&FindAnyCurrentProcessWindowProc,
                reinterpret_cast<LPARAM>(&search));
    return search.result;
}

void SendDrop(HWND window, const std::vector<std::wstring>& paths) {
    std::vector<wchar_t> payload;
    for (const auto& path : paths) {
        payload.insert(payload.end(), path.begin(), path.end());
        payload.push_back(L'\0');
    }
    payload.push_back(L'\0');

    const SIZE_T bytes = sizeof(NativeDropFiles) +
                         payload.size() * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GHND, bytes);
    if (!memory) return;
    auto* drop = static_cast<NativeDropFiles*>(GlobalLock(memory));
    if (!drop) {
        GlobalFree(memory);
        return;
    }
    drop->pFiles = sizeof(NativeDropFiles);
    drop->fWide = TRUE;
    std::memcpy(reinterpret_cast<char*>(drop) + sizeof(NativeDropFiles),
                payload.data(), payload.size() * sizeof(wchar_t));
    GlobalUnlock(memory);
    SendMessageW(window, WM_DROPFILES, reinterpret_cast<WPARAM>(memory), 0);
}

std::vector<std::string> ReadCommand(const std::string& command) {
    std::vector<std::string> paths;
    std::size_t cursor = 0;
    const std::size_t lineEnd = command.find('\n', cursor);
    if (lineEnd == std::string::npos) return paths;

    unsigned long count = 0;
    const auto parsed = std::from_chars(command.data(), command.data() + lineEnd,
                                         count);
    if (parsed.ec != std::errc{} || parsed.ptr != command.data() + lineEnd ||
        count > 1024U) {
        return paths;
    }
    cursor = lineEnd + 1U;
    paths.reserve(static_cast<std::size_t>(count));
    for (unsigned long index = 0; index < count; ++index) {
        const std::size_t end = command.find('\n', cursor);
        if (end == std::string::npos) return {};
        paths.emplace_back(command.data() + cursor, end - cursor);
        cursor = end + 1U;
    }
    return paths;
}

void RunDropAgent(std::wstring pipeName, std::wstring windowTitle) {
    const std::wstring fullPipeName = L"\\\\.\\pipe\\" + pipeName;
    for (;;) {
        HANDLE pipe = CreateNamedPipeW(
            fullPipeName.c_str(), PIPE_ACCESS_INBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 0, 64U * 1024U,
            5000, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) return;

        const BOOL connected = ConnectNamedPipe(pipe, nullptr)
            ? TRUE
            : (GetLastError() == ERROR_PIPE_CONNECTED ? TRUE : FALSE);
        if (!connected) {
            CloseHandle(pipe);
            continue;
        }

        std::string command;
        char buffer[4096];
        DWORD read = 0;
        while (ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr) != FALSE &&
               read != 0) {
            command.append(buffer, read);
            if (command.size() > 4U * 1024U * 1024U) break;
        }
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);

        const auto paths = ReadCommand(command);
        std::vector<std::wstring> nativePaths;
        nativePaths.reserve(paths.size());
        for (const auto& path : paths) {
            const std::wstring wide = Utf8ToWide(path);
            if (wide.empty()) {
                nativePaths.clear();
                break;
            }
            nativePaths.push_back(wide);
        }
        if (nativePaths.empty()) continue;

        HWND window = nullptr;
        for (int attempt = 0; attempt != 200 && !window; ++attempt) {
            window = FindCurrentProcessWindow(windowTitle);
            if (!window) window = FindAnyCurrentProcessWindow();
            if (!window) Sleep(25);
        }
        if (window) SendDrop(window, nativePaths);
    }
}

} // namespace

void StartFileDropTestAgent() {
    static std::once_flag started;
    std::call_once(started, [] {
        const std::wstring pipe = Environment(L"GUIDEXOS_TEST_FILE_DROP_PIPE");
        const std::wstring title = Environment(L"GUIDEXOS_TEST_FILE_DROP_TITLE");
        if (!pipe.empty() && !title.empty()) {
            std::thread(RunDropAgent, pipe, title).detach();
        }
    });
}

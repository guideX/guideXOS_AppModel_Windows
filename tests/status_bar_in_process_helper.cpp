#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace {

std::wstring Environment(const wchar_t* name) {
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (length == 0) return {};
    std::wstring value(static_cast<std::size_t>(length), L'\0');
    const DWORD copied = GetEnvironmentVariableW(name, value.data(), length);
    if (copied == 0 || copied >= length) return {};
    value.resize(copied);
    return value;
}

} // namespace

void ReportStatusForTest(const std::string& text) {
    const std::wstring path = Environment(L"GUIDEXOS_TEST_STATUS_FILE");
    if (path.empty()) return;

    HANDLE file = CreateFileW(
        path.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    DWORD written = 0;
    WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written,
              nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
}

void ResetToolTipsForTest() {
    const std::wstring path = Environment(L"GUIDEXOS_TEST_TOOLTIP_FILE");
    if (path.empty()) return;
    HANDLE file = CreateFileW(
        path.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
}

void ReportToolTipForTest(const std::string& text) {
    const std::wstring path = Environment(L"GUIDEXOS_TEST_TOOLTIP_FILE");
    if (path.empty()) return;
    HANDLE file = CreateFileW(
        path.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    const std::string line = text + "\n";
    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written,
              nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
}

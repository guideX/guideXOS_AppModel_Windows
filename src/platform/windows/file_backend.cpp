#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "guidexos/appmodel/file.hpp"
#include "../file_backend.hpp"

#include <algorithm>
#include <atomic>
#include <limits>
#include <string>
#include <vector>

namespace guidexos::appmodel::detail {
namespace {

constexpr DWORD kIoChunkBytes = 64U * 1024U;
std::atomic<unsigned long> gTemporaryFileSequence{0};

std::wstring Utf8ToWidePath(const std::string& path) {
    if (path.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("File path is too long");
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
        nullptr, 0);
    if (length <= 0) throw std::invalid_argument("File path is not valid UTF-8");

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(),
                            static_cast<int>(path.size()), result.data(), length) !=
        length) {
        throw std::invalid_argument("File path could not be converted to UTF-16");
    }
    return result;
}

std::wstring AbsolutePath(const std::wstring& path) {
    std::vector<wchar_t> buffer(512, L'\0');
    for (;;) {
        const DWORD length = GetFullPathNameW(path.c_str(),
                                               static_cast<DWORD>(buffer.size()),
                                               buffer.data(), nullptr);
        if (length == 0) {
            throw guidexos::appmodel::FileAccessError(
                "Unable to resolve file path");
        }
        if (length < buffer.size()) {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(static_cast<std::size_t>(length) + 1, L'\0');
    }
}

std::string PathContext(const std::string& path) {
    return " '" + path + "'";
}

[[noreturn]] void ThrowOpenFailure(const std::string& path, DWORD error) {
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
        throw guidexos::appmodel::FileNotFoundError(
            "File was not found" + PathContext(path));
    }
    if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION ||
        error == ERROR_LOCK_VIOLATION) {
        throw guidexos::appmodel::FileAccessError(
            "File could not be opened because access was denied" +
            PathContext(path));
    }
    throw guidexos::appmodel::FileAccessError(
        "File could not be opened" + PathContext(path));
}

[[noreturn]] void ThrowReadFailure(const std::string& path) {
    throw guidexos::appmodel::FileAccessError(
        "File could not be read" + PathContext(path));
}

[[noreturn]] void ThrowWriteFailure(const std::string& path, DWORD error) {
    if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION ||
        error == ERROR_LOCK_VIOLATION) {
        throw guidexos::appmodel::FileAccessError(
            "File could not be opened for writing because access was denied" +
            PathContext(path));
    }
    throw guidexos::appmodel::FileWriteError(
        "File write failed" + PathContext(path));
}

class ScopedHandle final {
public:
    explicit ScopedHandle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept
        : handle_(handle) {}
    ~ScopedHandle() {
        if (IsValid()) CloseHandle(handle_);
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    HANDLE Get() const noexcept { return handle_; }
    bool IsValid() const noexcept { return handle_ != INVALID_HANDLE_VALUE; }

    HANDLE Release() noexcept {
        const HANDLE result = handle_;
        handle_ = INVALID_HANDLE_VALUE;
        return result;
    }

private:
    HANDLE handle_;
};

std::wstring TemporaryPath(const std::wstring& destinationDirectory) {
    const unsigned long sequence = ++gTemporaryFileSequence;
    std::wstring path = destinationDirectory;
    if (path.empty() || (path.back() != L'\\' && path.back() != L'/')) {
        path.push_back(L'\\');
    }
    path += L".guidexos-tmp-" + std::to_wstring(GetCurrentProcessId()) +
            L"-" + std::to_wstring(sequence) + L".tmp";
    return path;
}

ScopedHandle CreateTemporaryFile(const std::wstring& directory,
                                 const std::string& destination,
                                 std::wstring& path) {
    for (int attempt = 0; attempt != 100; ++attempt) {
        path = TemporaryPath(directory);
        HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                                    CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (handle != INVALID_HANDLE_VALUE) return ScopedHandle(handle);
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            ThrowWriteFailure(destination, error);
        }
    }
    throw guidexos::appmodel::FileWriteError(
        "Unable to create a temporary file for" + PathContext(destination));
}

void WriteBytes(HANDLE handle, const std::string& path,
                const std::string& contents) {
    std::size_t offset = 0;
    while (offset < contents.size()) {
        const DWORD count = static_cast<DWORD>(std::min<std::size_t>(
            kIoChunkBytes, contents.size() - offset));
        DWORD written = 0;
        if (WriteFile(handle, contents.data() + offset, count, &written, nullptr) ==
                FALSE ||
            written != count) {
            ThrowWriteFailure(path, GetLastError());
        }
        offset += written;
    }
    if (FlushFileBuffers(handle) == FALSE) {
        throw guidexos::appmodel::FileWriteError(
            "File flush failed" + PathContext(path));
    }
}

} // namespace

std::string ReadAllBytes(const std::string& path, std::size_t maximumBytes) {
    const std::wstring nativePath = Utf8ToWidePath(path);
    ScopedHandle file(CreateFileW(nativePath.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE |
                                      FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                  nullptr));
    if (!file.IsValid()) ThrowOpenFailure(path, GetLastError());

    LARGE_INTEGER reportedSize{};
    if (GetFileSizeEx(file.Get(), &reportedSize) == FALSE ||
        reportedSize.QuadPart < 0) {
        ThrowReadFailure(path);
    }
    if (static_cast<unsigned long long>(reportedSize.QuadPart) > maximumBytes) {
        throw guidexos::appmodel::FileTooLargeError(
            "File is larger than the App Model text-file limit" +
            PathContext(path));
    }

    std::string contents;
    contents.reserve(static_cast<std::size_t>(reportedSize.QuadPart));
    std::vector<char> buffer(kIoChunkBytes);
    for (;;) {
        DWORD read = 0;
        if (ReadFile(file.Get(), buffer.data(), kIoChunkBytes, &read, nullptr) ==
            FALSE) {
            ThrowReadFailure(path);
        }
        if (read == 0) break;
        if (contents.size() > maximumBytes - read) {
            throw guidexos::appmodel::FileTooLargeError(
                "File is larger than the App Model text-file limit" +
                PathContext(path));
        }
        contents.append(buffer.data(), read);
    }
    return contents;
}

void WriteAllBytesAtomically(const std::string& path,
                             const std::string& contents) {
    const std::wstring destination = AbsolutePath(Utf8ToWidePath(path));
    const std::size_t separator = destination.find_last_of(L"\\/");
    const std::wstring directory = separator == std::wstring::npos
        ? L"."
        : destination.substr(0, separator);

    std::wstring temporary;
    ScopedHandle file = CreateTemporaryFile(directory, path, temporary);
    bool temporaryExists = true;
    try {
        WriteBytes(file.Get(), path, contents);
        if (!CloseHandle(file.Release())) {
            throw guidexos::appmodel::FileWriteError(
                "Temporary file close failed" + PathContext(path));
        }
        if (MoveFileExW(temporary.c_str(), destination.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ==
            FALSE) {
            throw guidexos::appmodel::FileReplacementError(
                "Could not replace destination file" + PathContext(path));
        }
        temporaryExists = false;
    } catch (...) {
        if (file.IsValid()) CloseHandle(file.Release());
        if (temporaryExists) DeleteFileW(temporary.c_str());
        throw;
    }
}

} // namespace guidexos::appmodel::detail

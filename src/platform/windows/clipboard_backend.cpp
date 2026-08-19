#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../clipboard_backend.hpp"
#include "../../appmodel/text_validation.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace guidexos::appmodel::detail {
namespace {

constexpr int kOpenClipboardAttempts = 3;
constexpr DWORD kRetryDelayMilliseconds = 1;
constexpr SIZE_T kMaximumNativeClipboardBytes =
    static_cast<SIZE_T>(kMaximumClipboardTextBytes) * 2U +
    sizeof(wchar_t);

std::wstring Utf8ToUtf16(const std::string& text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw ClipboardTooLargeError("Clipboard text is too long to convert");
    }

    const int inputLength = static_cast<int>(text.size());
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), inputLength, nullptr, 0);
    if (length <= 0) {
        throw ClipboardInvalidUtf8Error(
            "Clipboard text could not be converted to UTF-16");
    }

    try {
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                inputLength, result.data(), length) != length) {
            throw ClipboardInvalidUtf8Error(
                "Clipboard text could not be converted to UTF-16");
        }
        return result;
    } catch (const std::bad_alloc&) {
        throw ClipboardAllocationError(
            "Unable to allocate UTF-16 clipboard text");
    }
}

std::string Utf16ToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw ClipboardTooLargeError("Native clipboard text is too long to convert");
    }

    // Do not rely solely on the platform conversion flag: validate surrogate
    // pairing explicitly so malformed native clipboard data is deterministic
    // across Windows SDK/runtime versions.
    for (std::size_t index = 0; index < text.size(); ++index) {
        const unsigned int value = static_cast<unsigned int>(text[index]);
        if (value >= 0xD800U && value <= 0xDBFFU) {
            if (index + 1U >= text.size()) {
                throw ClipboardMalformedTextError(
                    "Native clipboard text is not valid UTF-16");
            }
            const unsigned int next =
                static_cast<unsigned int>(text[index + 1U]);
            if (next < 0xDC00U || next > 0xDFFFU) {
                throw ClipboardMalformedTextError(
                    "Native clipboard text is not valid UTF-16");
            }
            ++index;
        } else if (value >= 0xDC00U && value <= 0xDFFFU) {
            throw ClipboardMalformedTextError(
                "Native clipboard text is not valid UTF-16");
        }
    }

    const int inputLength = static_cast<int>(text.size());
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), inputLength, nullptr, 0,
        nullptr, nullptr);
    if (length <= 0) {
        throw ClipboardMalformedTextError(
            "Native clipboard text is not valid UTF-16");
    }
    if (static_cast<std::size_t>(length) > kMaximumClipboardTextBytes) {
        throw ClipboardTooLargeError(
            "Native clipboard text is larger than the App Model limit");
    }

    try {
        std::string result(static_cast<std::size_t>(length), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                                inputLength, result.data(), length, nullptr,
                                nullptr) != length) {
            throw ClipboardMalformedTextError(
                "Native clipboard text could not be converted to UTF-8");
        }
        return result;
    } catch (const std::bad_alloc&) {
        throw ClipboardAllocationError(
            "Unable to allocate UTF-8 clipboard text");
    }
}

class ScopedClipboard final {
public:
    ScopedClipboard() {
        for (int attempt = 0; attempt < kOpenClipboardAttempts; ++attempt) {
            if (OpenClipboard(nullptr) != FALSE) {
                opened_ = true;
                return;
            }
            if (attempt + 1 < kOpenClipboardAttempts) {
                Sleep(kRetryDelayMilliseconds);
            }
        }
        throw ClipboardUnavailableError(
            "Windows clipboard is temporarily unavailable");
    }

    ~ScopedClipboard() {
        if (opened_) CloseClipboard();
    }

    ScopedClipboard(const ScopedClipboard&) = delete;
    ScopedClipboard& operator=(const ScopedClipboard&) = delete;

private:
    bool opened_ = false;
};

class ScopedGlobalMemory final {
public:
    explicit ScopedGlobalMemory(HGLOBAL memory = nullptr) noexcept
        : memory_(memory) {}

    ~ScopedGlobalMemory() {
        if (memory_) GlobalFree(memory_);
    }

    ScopedGlobalMemory(const ScopedGlobalMemory&) = delete;
    ScopedGlobalMemory& operator=(const ScopedGlobalMemory&) = delete;

    HGLOBAL Get() const noexcept { return memory_; }

    HGLOBAL Release() noexcept {
        const HGLOBAL result = memory_;
        memory_ = nullptr;
        return result;
    }

private:
    HGLOBAL memory_ = nullptr;
};

class ScopedGlobalLock final {
public:
    explicit ScopedGlobalLock(HGLOBAL memory) : memory_(memory) {
        data_ = GlobalLock(memory_);
        if (!data_) {
            throw ClipboardOperationError(
                "Native clipboard text could not be locked");
        }
    }

    ~ScopedGlobalLock() {
        if (data_) GlobalUnlock(memory_);
    }

    ScopedGlobalLock(const ScopedGlobalLock&) = delete;
    ScopedGlobalLock& operator=(const ScopedGlobalLock&) = delete;

    void* Get() noexcept { return data_; }
    const void* Get() const noexcept { return data_; }

private:
    HGLOBAL memory_ = nullptr;
    void* data_ = nullptr;
};

std::optional<std::string> ReadNativeText() {
    ScopedClipboard clipboard;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT) == FALSE) return std::nullopt;

    HGLOBAL memory = static_cast<HGLOBAL>(GetClipboardData(CF_UNICODETEXT));
    if (!memory) {
        throw ClipboardOperationError(
            "Unicode clipboard text could not be retrieved");
    }

    const SIZE_T bytes = GlobalSize(memory);
    if (bytes == 0 || bytes > kMaximumNativeClipboardBytes) {
        throw ClipboardTooLargeError(
            "Native clipboard text is larger than the App Model limit");
    }
    if (bytes % sizeof(wchar_t) != 0) {
        throw ClipboardMalformedTextError(
            "Native clipboard text has an invalid UTF-16 block size");
    }
    const SIZE_T characterCapacity = bytes / sizeof(wchar_t);
    if (characterCapacity == 0) {
        throw ClipboardMalformedTextError(
            "Native clipboard text has no UTF-16 terminator space");
    }

    std::wstring wide;
    {
        ScopedGlobalLock lock(memory);
        const auto* characters = static_cast<const wchar_t*>(lock.Get());
        SIZE_T length = 0;
        while (length < characterCapacity && characters[length] != L'\0') {
            ++length;
        }
        if (length == characterCapacity) {
            throw ClipboardMalformedTextError(
                "Native clipboard text is not NUL terminated");
        }
        try {
            wide.assign(characters, static_cast<std::size_t>(length));
        } catch (const std::bad_alloc&) {
            throw ClipboardAllocationError(
                "Unable to copy native clipboard text");
        }
    }

    return Utf16ToUtf8(wide);
}

class WindowsClipboardBackend final : public ClipboardBackend {
public:
    bool HasText() override {
        return ReadNativeText().has_value();
    }

    std::string GetText() override {
        auto value = ReadNativeText();
        if (!value) {
            throw ClipboardNoTextError(
                "Clipboard does not contain supported Unicode text");
        }
        return std::move(*value);
    }

    void SetText(const std::string& text) override {
        try {
            ValidateUtf8(text);
        } catch (const std::invalid_argument&) {
            throw ClipboardInvalidUtf8Error(
                "Clipboard text must be valid UTF-8");
        }
        if (text.find('\0') != std::string::npos) {
            throw ClipboardEmbeddedNulError(
                "Clipboard text must not contain an embedded NUL");
        }
        if (text.size() > kMaximumClipboardTextBytes) {
            throw ClipboardTooLargeError(
                "Clipboard text is larger than the App Model limit");
        }

        const std::wstring wide = Utf8ToUtf16(text);
        const std::size_t characterCount = wide.size() + 1U;
        if (characterCount >
            static_cast<std::size_t>(kMaximumNativeClipboardBytes /
                                     sizeof(wchar_t))) {
            throw ClipboardTooLargeError(
                "Clipboard text is larger than the App Model limit");
        }

        const SIZE_T bytes = static_cast<SIZE_T>(characterCount) *
                             sizeof(wchar_t);
        ScopedGlobalMemory memory(GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,
                                              bytes));
        if (!memory.Get()) {
            throw ClipboardAllocationError(
                "Unable to allocate native clipboard text");
        }

        {
            ScopedGlobalLock lock(memory.Get());
            auto* destination = static_cast<wchar_t*>(lock.Get());
            std::copy(wide.begin(), wide.end(), destination);
            destination[wide.size()] = L'\0';
        }

        ScopedClipboard clipboard;
        if (EmptyClipboard() == FALSE) {
            throw ClipboardOperationError(
                "Clipboard contents could not be replaced");
        }
        if (!SetClipboardData(CF_UNICODETEXT, memory.Get())) {
            throw ClipboardOperationError(
                "Unicode clipboard text could not be published");
        }
        memory.Release();
    }

    void Clear() override {
        ScopedClipboard clipboard;
        if (EmptyClipboard() == FALSE) {
            throw ClipboardOperationError("Clipboard could not be cleared");
        }
    }
};

} // namespace

std::unique_ptr<ClipboardBackend> CreateClipboardBackend() {
    return std::make_unique<WindowsClipboardBackend>();
}

} // namespace guidexos::appmodel::detail

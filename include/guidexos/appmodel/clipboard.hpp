#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace guidexos::appmodel {

// Clipboard text is bounded before native clipboard memory is allocated.
// The bound is measured in public UTF-8 bytes and includes neither a native
// terminator nor any native allocation padding.
inline constexpr std::size_t kMaximumClipboardTextBytes = 8U * 1024U * 1024U;

class ClipboardError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ClipboardNoTextError : public ClipboardError {
public:
    using ClipboardError::ClipboardError;
};

class ClipboardUnavailableError : public ClipboardError {
public:
    using ClipboardError::ClipboardError;
};

class ClipboardOperationError : public ClipboardError {
public:
    using ClipboardError::ClipboardError;
};

class ClipboardAllocationError : public ClipboardError {
public:
    using ClipboardError::ClipboardError;
};

class ClipboardInvalidUtf8Error : public ClipboardError {
public:
    using ClipboardError::ClipboardError;
};

class ClipboardEmbeddedNulError : public ClipboardError {
public:
    using ClipboardError::ClipboardError;
};

class ClipboardMalformedTextError : public ClipboardError {
public:
    using ClipboardError::ClipboardError;
};

class ClipboardTooLargeError : public ClipboardError {
public:
    using ClipboardError::ClipboardError;
};

class Clipboard final {
public:
    // Clipboard is a process-wide OS service. These methods do not require an
    // Application or a shown Window and use the current Windows session
    // clipboard.
    static bool HasText();
    static std::string GetText();
    static void SetText(std::string text);
    static void Clear();
};

} // namespace guidexos::appmodel

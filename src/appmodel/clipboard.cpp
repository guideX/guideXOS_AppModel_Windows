#include "guidexos/appmodel/clipboard.hpp"

#include "../platform/clipboard_backend.hpp"
#include "text_validation.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {
namespace {

detail::ClipboardBackend& Backend() {
    // The backend is independent of ApplicationState. This keeps clipboard
    // lifetime process-wide and allows use before Application construction.
    static const std::unique_ptr<detail::ClipboardBackend> backend = [] {
        auto value = detail::CreateClipboardBackend();
        if (!value) {
            throw ClipboardOperationError("No clipboard backend is available");
        }
        return value;
    }();
    return *backend;
}

void ValidatePublicText(const std::string& text) {
    if (text.size() > kMaximumClipboardTextBytes) {
        throw ClipboardTooLargeError(
            "Clipboard text is larger than the App Model limit");
    }
    if (text.find('\0') != std::string::npos) {
        throw ClipboardEmbeddedNulError(
            "Clipboard text must not contain an embedded NUL");
    }
    try {
        detail::ValidateUtf8(text);
    } catch (const std::invalid_argument&) {
        throw ClipboardInvalidUtf8Error(
            "Clipboard text must be valid UTF-8");
    }
}

} // namespace

bool Clipboard::HasText() {
    return Backend().HasText();
}

std::string Clipboard::GetText() {
    return Backend().GetText();
}

void Clipboard::SetText(std::string text) {
    ValidatePublicText(text);
    Backend().SetText(text);
}

void Clipboard::Clear() {
    Backend().Clear();
}

} // namespace guidexos::appmodel

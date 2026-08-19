#include "guidexos/appmodel/file.hpp"

#include "../platform/file_backend.hpp"
#include "text_validation.hpp"

#include <stdexcept>

namespace guidexos::appmodel {
namespace {

void ValidatePath(const std::string& path) {
    if (path.empty()) {
        throw std::invalid_argument("File path must not be empty");
    }
    detail::ValidateUtf8(path);
}

void ValidateFileText(const std::string& contents, const char* operation) {
    try {
        detail::ValidateUtf8(contents);
    } catch (const std::invalid_argument&) {
        throw InvalidUtf8Error(std::string("Cannot ") + operation +
                               " file with invalid UTF-8 text");
    }
}

bool HasUtf8Bom(const std::string& contents) noexcept {
    return contents.size() >= 3 &&
           static_cast<unsigned char>(contents[0]) == 0xEFU &&
           static_cast<unsigned char>(contents[1]) == 0xBBU &&
           static_cast<unsigned char>(contents[2]) == 0xBFU;
}

} // namespace

std::string File::ReadAllText(const std::string& path) {
    ValidatePath(path);
    std::string contents = detail::ReadAllBytes(path, kMaximumTextFileBytes);
    if (HasUtf8Bom(contents)) contents.erase(0, 3);
    ValidateFileText(contents, "read");
    return contents;
}

void File::WriteAllText(const std::string& path, const std::string& contents) {
    ValidatePath(path);
    ValidateFileText(contents, "write");
    if (contents.size() > kMaximumTextFileBytes + 3U) {
        throw FileTooLargeError("Cannot write a text file larger than the App Model limit");
    }

    // Treat a caller-provided leading signature as a signature rather than
    // document text, keeping the write contract BOM-free and symmetric with
    // ReadAllText.
    std::string plainText = contents;
    if (HasUtf8Bom(plainText)) plainText.erase(0, 3);
    if (plainText.size() > kMaximumTextFileBytes) {
        throw FileTooLargeError("Cannot write a text file larger than the App Model limit");
    }
    detail::WriteAllBytesAtomically(path, plainText);
}

} // namespace guidexos::appmodel

#include "text_index.hpp"

#include "text_validation.hpp"

#include <limits>
#include <stdexcept>

namespace guidexos::appmodel::detail {
namespace {

std::size_t Utf8SequenceLength(unsigned char first) {
    if (first <= 0x7FU) return 1;
    if (first <= 0xDFU) return 2;
    if (first <= 0xEFU) return 3;
    return 4;
}

bool IsHighSurrogate(wchar_t value) noexcept {
    const unsigned int code = static_cast<unsigned int>(value);
    return code >= 0xD800U && code <= 0xDBFFU;
}

bool IsLowSurrogate(wchar_t value) noexcept {
    const unsigned int code = static_cast<unsigned int>(value);
    return code >= 0xDC00U && code <= 0xDFFFU;
}

} // namespace

std::size_t CountUnicodeScalars(const std::string& text) {
    ValidateUtf8(text);
    std::size_t count = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        offset += Utf8SequenceLength(
            static_cast<unsigned char>(text[offset]));
        ++count;
    }
    return count;
}

std::size_t Utf8ByteOffsetForScalarIndex(const std::string& text,
                                         std::size_t scalarIndex) {
    ValidateUtf8(text);
    std::size_t scalar = 0;
    std::size_t offset = 0;
    while (offset < text.size() && scalar < scalarIndex) {
        offset += Utf8SequenceLength(
            static_cast<unsigned char>(text[offset]));
        ++scalar;
    }
    if (scalar != scalarIndex) {
        throw std::out_of_range("TextBox text index is out of range");
    }
    return offset;
}

std::size_t Utf16CodeUnitOffsetForScalarIndex(const std::wstring& text,
                                              std::size_t scalarIndex) {
    std::size_t scalar = 0;
    std::size_t offset = 0;
    while (offset < text.size() && scalar < scalarIndex) {
        if (IsHighSurrogate(text[offset])) {
            if (offset + 1U >= text.size() ||
                !IsLowSurrogate(text[offset + 1U])) {
                throw std::invalid_argument("Native text is not valid UTF-16");
            }
            offset += 2U;
        } else if (IsLowSurrogate(text[offset])) {
            throw std::invalid_argument("Native text is not valid UTF-16");
        } else {
            ++offset;
        }
        ++scalar;
    }
    if (scalar != scalarIndex) {
        throw std::out_of_range("TextBox text index is out of range");
    }
    return offset;
}

std::size_t Utf16ScalarIndexForCodeUnitOffset(const std::wstring& text,
                                              std::size_t codeUnitOffset) {
    if (codeUnitOffset > text.size()) {
        throw std::out_of_range("Native text selection is out of range");
    }

    std::size_t scalar = 0;
    std::size_t offset = 0;
    while (offset < codeUnitOffset) {
        if (IsHighSurrogate(text[offset])) {
            if (offset + 1U >= text.size() ||
                !IsLowSurrogate(text[offset + 1U])) {
                throw std::invalid_argument("Native text is not valid UTF-16");
            }
            if (offset + 2U > codeUnitOffset) {
                throw std::invalid_argument(
                    "Native selection splits a surrogate pair");
            }
            offset += 2U;
        } else if (IsLowSurrogate(text[offset])) {
            throw std::invalid_argument("Native text is not valid UTF-16");
        } else {
            ++offset;
        }
        ++scalar;
    }
    return scalar;
}

} // namespace guidexos::appmodel::detail

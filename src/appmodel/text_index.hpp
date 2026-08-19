#pragma once

#include <cstddef>
#include <string>

namespace guidexos::appmodel::detail {

// These helpers are private to the App Model implementation. Public controls
// use scalar-value positions while the Windows backend converts at its
// boundary to UTF-16 code-unit positions.
std::size_t CountUnicodeScalars(const std::string& text);
std::size_t Utf8ByteOffsetForScalarIndex(const std::string& text,
                                         std::size_t scalarIndex);
std::size_t Utf16CodeUnitOffsetForScalarIndex(const std::wstring& text,
                                              std::size_t scalarIndex);
std::size_t Utf16ScalarIndexForCodeUnitOffset(const std::wstring& text,
                                              std::size_t codeUnitOffset);

} // namespace guidexos::appmodel::detail

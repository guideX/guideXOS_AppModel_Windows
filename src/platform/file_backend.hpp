#pragma once

#include <cstddef>
#include <string>

namespace guidexos::appmodel::detail {

std::string ReadAllBytes(const std::string& path,
                         std::size_t maximumBytes);
void WriteAllBytesAtomically(const std::string& path,
                             const std::string& contents);

} // namespace guidexos::appmodel::detail

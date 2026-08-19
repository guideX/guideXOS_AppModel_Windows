#pragma once

#include "guidexos/appmodel/file_drop.hpp"

#include <string>
#include <vector>

namespace guidexos::appmodel::detail {

std::vector<std::string> NormalizeFileDropPaths(std::vector<std::string> paths);

} // namespace guidexos::appmodel::detail

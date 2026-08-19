#include "file_drop.hpp"

#include "text_validation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace guidexos::appmodel::detail {

std::vector<std::string> NormalizeFileDropPaths(std::vector<std::string> paths) {
    std::vector<std::string> accepted;
    accepted.reserve(std::min(paths.size(), kMaximumFileDropCount));
    std::size_t totalBytes = 0;
    for (auto& path : paths) {
        if (accepted.size() >= kMaximumFileDropCount) break;
        if (path.empty() || path.size() > kMaximumFileDropPathBytes) continue;
        try {
            ValidateUtf8(path);
        } catch (const std::invalid_argument&) {
            continue;
        }
        if (path.size() > kMaximumFileDropPayloadBytes - totalBytes) break;
        totalBytes += path.size();
        accepted.push_back(std::move(path));
    }
    return accepted;
}

} // namespace guidexos::appmodel::detail

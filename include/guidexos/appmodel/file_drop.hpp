#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace guidexos::appmodel {

// Shell file drops are bounded before they enter the App Model event loop.
// The limits apply to paths accepted from a native drop operation; they are
// also useful to applications when choosing how much work to do per event.
inline constexpr std::size_t kMaximumFileDropCount = 1024U;
inline constexpr std::size_t kMaximumFileDropPathBytes = 256U * 1024U;
inline constexpr std::size_t kMaximumFileDropPayloadBytes = 4U * 1024U * 1024U;

class FileDropEvent final {
public:
    explicit FileDropEvent(std::vector<std::string> files) noexcept
        : files_(std::move(files)) {}

    const std::vector<std::string>& GetFiles() const noexcept { return files_; }

private:
    std::vector<std::string> files_;
};

} // namespace guidexos::appmodel

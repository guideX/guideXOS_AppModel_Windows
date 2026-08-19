#pragma once

#include "guidexos/appmodel/clipboard.hpp"

#include <memory>
#include <string>

namespace guidexos::appmodel::detail {

class ClipboardBackend {
public:
    virtual ~ClipboardBackend() = default;

    virtual bool HasText() = 0;
    virtual std::string GetText() = 0;
    virtual void SetText(const std::string& text) = 0;
    virtual void Clear() = 0;
};

std::unique_ptr<ClipboardBackend> CreateClipboardBackend();

} // namespace guidexos::appmodel::detail

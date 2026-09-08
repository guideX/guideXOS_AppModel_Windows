#include "guidexos/appmodel/image.hpp"

#include "runtime.hpp"
#include "text_validation.hpp"

#include <stdexcept>
#include <utility>

namespace guidexos::appmodel {
namespace {

constexpr std::size_t kMaximumImagePathBytes = 32U * 1024U;
constexpr std::size_t kMaximumToolTipBytes = 8U * 1024U;

void SetImageToolTip(const std::shared_ptr<detail::ControlState>& state,
                     std::string text) {
    detail::ValidateUtf8(text);
    if (text.size() > kMaximumToolTipBytes) {
        throw std::length_error("Control ToolTip exceeds the 8 KiB limit");
    }
    if (state->toolTip == text) return;
    state->toolTip = std::move(text);
    detail::NotifyControlChanged(state);
}

} // namespace

ImageSource ImageSource::FromFile(std::string path) {
    if (path.empty()) {
        throw std::invalid_argument("Image source file path must not be empty");
    }
    detail::ValidateUtf8(path);
    if (path.size() > kMaximumImagePathBytes) {
        throw std::length_error(
            "Image source file path exceeds the 32 KiB limit");
    }
    return ImageSource{std::move(path)};
}

Image::Image()
    : state_(std::make_shared<detail::ControlState>(
          detail::ControlKind::Image, std::string{})) {}

Image::~Image() = default;

void Image::SetSource(const ImageSource& source) {
    if (!source.IsValid()) {
        throw std::invalid_argument("Image source is invalid");
    }
    if (state_->imageSource && *state_->imageSource == source &&
        state_->imageLoadStatus != ImageLoadStatus::Failed) {
        return;
    }
    state_->imageSource = source;
    state_->imageLoadStatus = ImageLoadStatus::Pending;
    state_->imageWidth = 0;
    state_->imageHeight = 0;
    state_->imageLoadError.clear();
    detail::NotifyControlChanged(state_);
}

void Image::ClearSource() {
    if (!state_->imageSource && state_->imageLoadStatus == ImageLoadStatus::Empty) {
        return;
    }
    state_->imageSource.reset();
    state_->imageLoadStatus = ImageLoadStatus::Empty;
    state_->imageWidth = 0;
    state_->imageHeight = 0;
    state_->imageLoadError.clear();
    detail::NotifyControlChanged(state_);
}

bool Image::HasSource() const noexcept {
    return state_->imageSource.has_value();
}

std::optional<ImageSource> Image::GetSource() const {
    return state_->imageSource;
}

ImageLoadStatus Image::GetLoadStatus() const noexcept {
    return state_->imageLoadStatus;
}

const std::string& Image::GetLoadError() const noexcept {
    return state_->imageLoadError;
}

int Image::GetWidth() const noexcept {
    return state_->imageWidth;
}

int Image::GetHeight() const noexcept {
    return state_->imageHeight;
}

void Image::SetScaleMode(ImageScaleMode mode) {
    if (state_->imageScaleMode == mode) return;
    state_->imageScaleMode = mode;
    detail::NotifyControlChanged(state_);
}

ImageScaleMode Image::GetScaleMode() const noexcept {
    return state_->imageScaleMode;
}

void Image::SetToolTip(std::string text) {
    SetImageToolTip(state_, std::move(text));
}

const std::string& Image::GetToolTip() const noexcept {
    return state_->toolTip;
}

ControlRef Image::GetControlRef() const noexcept {
    return ControlRef{state_};
}

void Image::SetEnabled(bool enabled) {
    if (state_->enabled == enabled) return;
    state_->enabled = enabled;
    detail::NotifyControlChanged(state_);
}

bool Image::IsEnabled() const noexcept {
    return state_->enabled;
}

} // namespace guidexos::appmodel

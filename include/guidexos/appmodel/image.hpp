#pragma once

#include "controls.hpp"

#include <memory>
#include <optional>
#include <string>

namespace guidexos::appmodel {

// An ImageSource is an immutable, platform-neutral description of a raster
// file. The path is UTF-8 and is interpreted by the selected platform when
// the Image is realized.
class ImageSource final {
public:
    ImageSource() noexcept = default;

    static ImageSource FromFile(std::string path);

    bool IsValid() const noexcept { return !filePath_.empty(); }
    const std::string& GetFilePath() const noexcept { return filePath_; }

    friend bool operator==(const ImageSource&, const ImageSource&) noexcept =
        default;

private:
    explicit ImageSource(std::string filePath) noexcept
        : filePath_(std::move(filePath)) {}

    std::string filePath_;
};

class Image final {
public:
    Image();
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&&) = delete;
    Image& operator=(Image&&) = delete;

    // Source assignment is portable and may happen before or after the
    // owning Window is realized. A source becomes Loaded or Failed when the
    // platform backend attempts decoding; failed replacement clears the
    // previously displayed pixels.
    void SetSource(const ImageSource& source);
    void ClearSource();
    bool HasSource() const noexcept;
    std::optional<ImageSource> GetSource() const;
    ImageLoadStatus GetLoadStatus() const noexcept;
    const std::string& GetLoadError() const noexcept;
    int GetWidth() const noexcept;
    int GetHeight() const noexcept;

    void SetScaleMode(ImageScaleMode mode);
    ImageScaleMode GetScaleMode() const noexcept;

    void SetToolTip(std::string text);
    const std::string& GetToolTip() const noexcept;
    ControlRef GetControlRef() const noexcept;

    void SetEnabled(bool enabled);
    bool IsEnabled() const noexcept;

private:
    std::shared_ptr<detail::ControlState> state_;

    friend class Layout;
};

} // namespace guidexos::appmodel

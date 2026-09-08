#include <guidexos/appmodel/appmodel.hpp>
#include "appmodel/runtime.hpp"

#include <cstdio>
#include <functional>
#include <stdexcept>

using namespace guidexos::appmodel;

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (false)

int main() {
    Image image;
    assert(!image.HasSource());
    assert(image.GetLoadStatus() == ImageLoadStatus::Empty);
    assert(image.GetScaleMode() == ImageScaleMode::Fit);
    assert(image.GetWidth() == 0);
    assert(image.GetHeight() == 0);
    assert(image.GetToolTip().empty());
    assert(image.IsEnabled());

    const ControlRef imageRef = image.GetControlRef();
    assert(imageRef.IsValid());
    assert(imageRef.GetType() == ControlType::Image);
    assert(!imageRef.HasFocus());
    assert(!imageRef.Focus());
    const auto imageView = imageRef.AsImage();
    assert(imageView.has_value());
    assert(imageView->IsValid());
    assert(!imageView->HasSource());
    assert(imageView->GetLoadStatus() == ImageLoadStatus::Empty);
    assert(!imageRef.AsSlider().has_value());

    Label label("not an image");
    assert(!label.GetControlRef().AsImage().has_value());

    bool emptyPathRejected = false;
    try {
        (void)ImageSource::FromFile({});
    } catch (const std::invalid_argument&) {
        emptyPathRejected = true;
    }
    assert(emptyPathRejected);

    bool invalidUtf8Rejected = false;
    try {
        (void)ImageSource::FromFile(std::string("bad\xFF"));
    } catch (const std::invalid_argument&) {
        invalidUtf8Rejected = true;
    }
    assert(invalidUtf8Rejected);

    const ImageSource first = ImageSource::FromFile("fixtures/first.png");
    const ImageSource second = ImageSource::FromFile("fixtures/second.jpg");
    assert(first.IsValid());
    assert(first.GetFilePath() == "fixtures/first.png");
    assert(first != second);

    image.SetSource(first);
    assert(image.HasSource());
    assert(image.GetSource() == std::optional<ImageSource>(first));
    assert(image.GetLoadStatus() == ImageLoadStatus::Pending);
    assert(image.GetWidth() == 0 && image.GetHeight() == 0);
    assert(imageRef.AsImage()->HasSource());
    image.SetSource(first); // effective state assignment is suppressed
    assert(image.GetLoadStatus() == ImageLoadStatus::Pending);

    image.SetScaleMode(ImageScaleMode::Stretch);
    assert(image.GetScaleMode() == ImageScaleMode::Stretch);
    assert(imageRef.AsImage()->GetScaleMode() == ImageScaleMode::Stretch);
    image.SetScaleMode(ImageScaleMode::Fill);
    assert(image.GetScaleMode() == ImageScaleMode::Fill);
    image.SetToolTip("guideXOS logo");
    assert(image.GetToolTip() == "guideXOS logo");
    image.SetEnabled(false);
    assert(!image.IsEnabled());
    assert(!imageRef.IsEnabled());
    image.SetEnabled(true);

    image.SetSource(second);
    assert(image.GetSource() == std::optional<ImageSource>(second));
    assert(image.GetLoadStatus() == ImageLoadStatus::Pending);
    image.ClearSource();
    assert(!image.HasSource());
    assert(!image.GetSource().has_value());
    assert(image.GetLoadStatus() == ImageLoadStatus::Empty);
    assert(image.GetLoadError().empty());

    Image secondImage;
    secondImage.SetSource(first);
    assert(secondImage.GetControlRef() != image.GetControlRef());
    Layout content;
    content.Add(image);
    content.Add(secondImage, LayoutSizing::Expand);
    assert(content.ChildCount() == 2);
    const LayoutSize natural = content.GetNaturalSize();
    const LayoutSize minimum = content.GetMinimumSize();
    assert(natural.width >= minimum.width);
    assert(natural.height >= minimum.height);
    const auto geometry = content.CalculateGeometry(LayoutRect{0, 0, 640, 480});
    assert(geometry.size() == 2);
    assert(geometry[0].width >= 0 && geometry[0].height >= 0);
    assert(geometry[1].width >= 0 && geometry[1].height >= 0);

    const auto fitLandscape = detail::CalculateImageRenderGeometry(
        400, 200, LayoutRect{0, 0, 300, 300}, ImageScaleMode::Fit);
    assert(fitLandscape.x == 0 && fitLandscape.y == 75);
    assert(fitLandscape.width == 300 && fitLandscape.height == 150);
    assert(fitLandscape.sourceX == 0 && fitLandscape.sourceY == 0);
    assert(fitLandscape.sourceWidth == 400 && fitLandscape.sourceHeight == 200);

    const auto fitPortrait = detail::CalculateImageRenderGeometry(
        100, 200, LayoutRect{0, 0, 300, 200}, ImageScaleMode::Fit);
    assert(fitPortrait.x == 100 && fitPortrait.y == 0);
    assert(fitPortrait.width == 100 && fitPortrait.height == 200);

    const auto stretch = detail::CalculateImageRenderGeometry(
        400, 200, LayoutRect{8, 12, 300, 300}, ImageScaleMode::Stretch);
    assert(stretch.x == 8 && stretch.y == 12);
    assert(stretch.width == 300 && stretch.height == 300);
    assert(stretch.sourceWidth == 400 && stretch.sourceHeight == 200);

    const auto fill = detail::CalculateImageRenderGeometry(
        400, 200, LayoutRect{0, 0, 300, 300}, ImageScaleMode::Fill);
    assert(fill.x == 0 && fill.y == 0);
    assert(fill.width == 300 && fill.height == 300);
    assert(fill.sourceWidth == 200 && fill.sourceHeight == 200);
    assert(fill.sourceX == 100 && fill.sourceY == 0);

    ControlRef stale;
    {
        Image temporary;
        stale = temporary.GetControlRef();
        assert(stale.IsValid());
        assert(stale.AsImage()->IsValid());
    }
    assert(!stale.IsValid());
    assert(stale.GetType() == ControlType::None);
    assert(!stale.AsImage().has_value());

    return 0;
}

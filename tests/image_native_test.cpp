#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>

#include <guidexos/appmodel/appmodel.hpp>

#include "appmodel/runtime.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace guidexos::appmodel;

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (false)

namespace {

constexpr char kPngBase64[] =
    "iVBORw0KGgoAAAANSUhEUgAAAAMAAAACCAYAAACddGYaAAAAAXNSR0IArs4c6QAA"
    "AARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAAcSURBVBhXFcix"
    "DQAACMAgTvfzqhMJIuNJBeNyAYwaCfj7yMTOAAAAAElFTkSuQmCC";

constexpr char kJpegBase64[] =
    "/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoH"
    "BwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQME"
    "BAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQU"
    "FBQUFBQUFBQUFBQUFBT/wAARCAADAAUDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEA"
    "AAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIh"
    "MUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6"
    "Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZ"
    "mqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx"
    "8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREA"
    "AgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAV"
    "YnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hp"
    "anN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPE"
    "xcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwBP"
    "B3gTQP7LH/Erg7dj/jRRRXh4nE1/bS99792aZNi8R/Z9H95L4V1Z/9k=";

int Base64Value(char value) noexcept {
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}

std::vector<unsigned char> DecodeBase64(const char* text) {
    std::vector<unsigned char> result;
    int accumulator = 0;
    int bits = 0;
    for (const char* current = text; *current; ++current) {
        if (*current == '=') break;
        const int value = Base64Value(*current);
        if (value < 0) continue;
        accumulator = (accumulator << 6) | value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<unsigned char>(
                (accumulator >> bits) & 0xFF));
        }
    }
    return result;
}

std::string WriteBytes(const char* name,
                       const std::vector<unsigned char>& bytes) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / name;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    output.close();
    return path.string();
}

std::string WriteFixture(const char* name, const char* base64) {
    return WriteBytes(name, DecodeBase64(base64));
}

HWND FindChildByClass(HWND parent, const wchar_t* className,
                      HWND previous = nullptr) {
    return FindWindowExW(parent, previous, className, nullptr);
}

bool IsMostlyRed(COLORREF color) noexcept {
    return GetRValue(color) > 180 && GetGValue(color) < 120 &&
           GetBValue(color) < 120;
}

bool IsMostlyWhite(COLORREF color) noexcept {
    return GetRValue(color) > 180 && GetGValue(color) > 180 &&
           GetBValue(color) > 180;
}

bool IsMostlyGray(COLORREF color) noexcept {
    const int red = GetRValue(color);
    const int green = GetGValue(color);
    const int blue = GetBValue(color);
    return red > 40 && red < 220 && std::abs(red - green) < 20 &&
           std::abs(red - blue) < 20;
}

COLORREF ReadImagePixel(HWND image, const detail::ImageRenderGeometry& geometry,
                        int sourceX, int sourceY) {
    const HDC dc = GetDC(image);
    if (!dc) return RGB(0, 0, 0);
    const int x = geometry.x +
        (sourceX * geometry.width + geometry.width / 2) /
            geometry.sourceWidth;
    const int y = geometry.y +
        (sourceY * geometry.height + geometry.height / 2) /
            geometry.sourceHeight;
    const COLORREF color = GetPixel(dc, x, y);
    ReleaseDC(image, dc);
    return color;
}

} // namespace

int main() {
    const std::string pngPath = WriteFixture(
        "guideXOS_Phase6_Image_Test.png", kPngBase64);
    const std::string jpegPath = WriteFixture(
        "guideXOS_Phase6_Image_Test.jpg", kJpegBase64);
    const auto pngBytes = DecodeBase64(kPngBase64);
    const auto jpegBytes = DecodeBase64(kJpegBase64);
    std::vector<unsigned char> truncatedPng = pngBytes;
    truncatedPng.resize(std::min<std::size_t>(20, truncatedPng.size()));
    std::vector<unsigned char> truncatedJpeg = jpegBytes;
    truncatedJpeg.resize(std::min<std::size_t>(32, truncatedJpeg.size()));
    const std::string emptyPath = WriteBytes(
        "guideXOS_Phase6_Image_Empty.bin", {});
    const std::string randomPath = WriteBytes(
        "guideXOS_Phase6_Image_Random.bin", {0x00, 0x11, 0x22, 0x33,
                                               0x7F, 0x80, 0xFE, 0xFF});
    const std::string truncatedPngPath = WriteBytes(
        "guideXOS_Phase6_Image_Truncated.png", truncatedPng);
    const std::string truncatedJpegPath = WriteBytes(
        "guideXOS_Phase6_Image_Truncated.jpg", truncatedJpeg);
    const std::string missingPath =
        (std::filesystem::temp_directory_path() /
         "guideXOS_Phase6_Image_Missing.png").string();
    std::error_code ignored;
    std::filesystem::remove(missingPath, ignored);

    Application app("com.guidexos.tests.image-native", ShutdownMode::Explicit);
    Window window(app);
    window.SetTitle("guideXOS Image Native");
    window.SetSize(760, 520);

    Image image;
    image.SetSource(ImageSource::FromFile(pngPath));
    image.SetScaleMode(ImageScaleMode::Fit);
    image.SetToolTip("test image");
    Image invalid;
    invalid.SetSource(ImageSource::FromFile(missingPath));
    Image empty;
    empty.SetSource(ImageSource::FromFile(emptyPath));
    Image random;
    random.SetSource(ImageSource::FromFile(randomPath));
    Image truncatedPngImage;
    truncatedPngImage.SetSource(ImageSource::FromFile(truncatedPngPath));
    Image truncatedJpegImage;
    truncatedJpegImage.SetSource(ImageSource::FromFile(truncatedJpegPath));

    TabView tabs;
    auto overview = tabs.AddTab("Overview");
    auto details = tabs.AddTab("Details");
    Label caption("Portable raster image");
    TextArea detailsText("Image remains available after tab switches.");
    overview.GetLayout().Add(image, LayoutSizing::Expand);
    overview.GetLayout().Add(caption);
    details.GetLayout().Add(detailsText, LayoutSizing::Expand);
    Layout content;
    content.Add(tabs, LayoutSizing::Expand);
    content.Add(invalid);
    content.Add(empty);
    content.Add(random);
    content.Add(truncatedPngImage);
    content.Add(truncatedJpegImage);
    window.SetContent(content);

    assert(window.Show());
    const HWND nativeWindow = FindWindowW(nullptr, L"guideXOS Image Native");
    assert(nativeWindow != nullptr);
    const HWND nativeImage = FindChildByClass(
        nativeWindow, L"guideXOS.AppModel.Windows.Image");
    assert(nativeImage != nullptr);
    assert(IsWindowVisible(nativeImage) != FALSE);
    assert(image.GetLoadStatus() == ImageLoadStatus::Loaded);
    assert(image.GetWidth() == 3 && image.GetHeight() == 2);
    assert(invalid.GetLoadStatus() == ImageLoadStatus::Failed);
    assert(!invalid.GetLoadError().empty());
    assert(empty.GetLoadStatus() == ImageLoadStatus::Failed);
    assert(random.GetLoadStatus() == ImageLoadStatus::Failed);
    assert(truncatedPngImage.GetLoadStatus() == ImageLoadStatus::Failed);
    assert(truncatedJpegImage.GetLoadStatus() == ImageLoadStatus::Failed);
    assert(image.GetControlRef().AsImage()->GetWidth() == 3);

    RECT client{};
    assert(GetClientRect(nativeImage, &client));
    const detail::ImageRenderGeometry fit =
        detail::CalculateImageRenderGeometry(
            3, 2, LayoutRect{0, 0, client.right - client.left,
                              client.bottom - client.top},
            ImageScaleMode::Fit);
    assert(fit.width > 0 && fit.height > 0);
    RedrawWindow(nativeImage, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    assert(IsMostlyRed(ReadImagePixel(nativeImage, fit, 0, 0)));
    // The second row's first source pixel is fully transparent. It must show
    // the Image control background rather than an opaque black rectangle.
    assert(IsMostlyWhite(ReadImagePixel(nativeImage, fit, 0, 1)));
    // The second row's middle pixel is half-transparent black. It must
    // compose to an intermediate gray instead of becoming opaque black.
    assert(IsMostlyGray(ReadImagePixel(nativeImage, fit, 1, 1)));

    tabs.SetSelectedIndex(1);
    assert(IsWindowVisible(nativeImage) == FALSE);
    tabs.SetSelectedIndex(0);
    assert(IsWindowVisible(nativeImage) != FALSE);
    window.SetSize(900, 600);
    assert(image.GetLoadStatus() == ImageLoadStatus::Loaded);

    image.SetScaleMode(ImageScaleMode::Stretch);
    assert(image.GetScaleMode() == ImageScaleMode::Stretch);
    image.SetSource(ImageSource::FromFile(jpegPath));
    assert(image.GetLoadStatus() == ImageLoadStatus::Loaded);
    assert(image.GetWidth() == 5 && image.GetHeight() == 3);
    image.ClearSource();
    assert(image.GetLoadStatus() == ImageLoadStatus::Empty);
    assert(!image.HasSource());
    image.SetSource(ImageSource::FromFile(pngPath));
    assert(image.GetLoadStatus() == ImageLoadStatus::Loaded);
    assert(image.GetWidth() == 3 && image.GetHeight() == 2);

    const HWND oldWindow = nativeWindow;
    window.Close();
    assert(!window.IsShown());
    assert(!IsWindow(oldWindow));
    assert(image.GetLoadStatus() == ImageLoadStatus::Loaded);
    assert(window.Show());
    const HWND reopened = FindWindowW(nullptr, L"guideXOS Image Native");
    assert(reopened != nullptr && reopened != oldWindow);
    const HWND reopenedImage = FindChildByClass(
        reopened, L"guideXOS.AppModel.Windows.Image");
    assert(reopenedImage != nullptr && IsWindowVisible(reopenedImage) != FALSE);
    assert(image.GetLoadStatus() == ImageLoadStatus::Loaded);

    window.Close();
    app.Quit();
    assert(app.Run() == 0);
    std::filesystem::remove(pngPath, ignored);
    std::filesystem::remove(jpegPath, ignored);
    std::filesystem::remove(emptyPath, ignored);
    std::filesystem::remove(randomPath, ignored);
    std::filesystem::remove(truncatedPngPath, ignored);
    std::filesystem::remove(truncatedJpegPath, ignored);
    return 0;
}

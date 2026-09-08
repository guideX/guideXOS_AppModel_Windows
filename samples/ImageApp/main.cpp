#include <guidexos/appmodel/appmodel.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace guidexos::appmodel;

namespace {

constexpr char kPngBase64[] =
    "iVBORw0KGgoAAAANSUhEUgAAAAMAAAACCAYAAACddGYaAAAAAXNSR0IArs4c6QAA"
    "AARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAAcSURBVBhXFcix"
    "DQAACMAgTvfzqhMJIuNJBeNyAYwaCfj7yMTOAAAAAElFTkSuQmCC";

constexpr char kJpegBase64[] =
    "/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoH"
    "BwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQME"
    "BAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQU"
    "FBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCAADAAUDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEA"
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

std::string WriteEmbeddedFile(const char* name, const char* base64) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / name;
    const auto bytes = DecodeBase64(base64);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    output.close();
    return path.string();
}

} // namespace

int main() {
    const std::string pngPath = WriteEmbeddedFile(
        "guideXOS_ImageApp_demo.png", kPngBase64);
    const std::string jpegPath = WriteEmbeddedFile(
        "guideXOS_ImageApp_demo.jpg", kJpegBase64);
    const ImageSource png = ImageSource::FromFile(pngPath);
    const ImageSource jpeg = ImageSource::FromFile(jpegPath);

    Application application("com.guidexos.samples.image");
    Window window(application);
    window.SetTitle("guideXOS Image Demo");
    window.SetSize(900, 640);

    Image image;
    image.SetSource(png);
    image.SetScaleMode(ImageScaleMode::Fit);
    image.SetToolTip("Decoded PNG with transparency");

    Label status("Status: PNG pending");
    auto updateStatus = [&]() {
        const char* mode = image.GetScaleMode() == ImageScaleMode::Fit
            ? "Fit" : (image.GetScaleMode() == ImageScaleMode::Fill
                ? "Fill" : "Stretch");
        if (image.GetLoadStatus() == ImageLoadStatus::Loaded) {
            const auto source = image.GetSource();
            const std::string format = source && *source == png ? "PNG" : "JPEG";
            status.SetText("Status: " + format + " " +
                           std::to_string(image.GetWidth()) + " x " +
                           std::to_string(image.GetHeight()) + " (" + mode + ")");
        } else if (image.GetLoadStatus() == ImageLoadStatus::Failed) {
            status.SetText("Status: image failed to decode");
        } else {
            status.SetText("Status: no image source");
        }
    };

    RadioButton fit("Fit");
    RadioButton fill("Fill");
    RadioButton stretch("Stretch");
    RadioGroup scaleModes;
    scaleModes.Add(fit);
    scaleModes.Add(fill);
    scaleModes.Add(stretch);
    scaleModes.Select(fit);
    fit.OnSelectedChanged([&](bool selected) {
        if (selected) {
            image.SetScaleMode(ImageScaleMode::Fit);
            updateStatus();
        }
    });
    fill.OnSelectedChanged([&](bool selected) {
        if (selected) {
            image.SetScaleMode(ImageScaleMode::Fill);
            updateStatus();
        }
    });
    stretch.OnSelectedChanged([&](bool selected) {
        if (selected) {
            image.SetScaleMode(ImageScaleMode::Stretch);
            updateStatus();
        }
    });

    Button loadPng("Load PNG");
    Button loadJpeg("Load JPEG");
    Button clear("Clear");
    loadPng.SetToolTip("Load the bundled transparent PNG");
    loadJpeg.SetToolTip("Load the bundled JPEG");
    clear.SetToolTip("Clear the current image");
    loadPng.OnClick([&]() {
        image.SetSource(png);
        updateStatus();
    });
    loadJpeg.OnClick([&]() {
        image.SetSource(jpeg);
        updateStatus();
    });
    clear.OnClick([&]() {
        image.ClearSource();
        updateStatus();
    });

    Layout scaleLayout(Orientation::Horizontal, 0, 12);
    scaleLayout.Add(fit);
    scaleLayout.Add(fill);
    scaleLayout.Add(stretch);
    Layout buttons(Orientation::Horizontal, 0, 12);
    buttons.Add(loadPng);
    buttons.Add(loadJpeg);
    buttons.Add(clear);

    Layout content;
    content.Add(image, LayoutSizing::Expand);
    content.Add(scaleLayout);
    content.Add(buttons);
    content.Add(status);
    window.SetContent(content);
    if (!window.Show()) return 1;
    updateStatus();
    const int result = application.Run();

    std::error_code ignored;
    std::filesystem::remove(pngPath, ignored);
    std::filesystem::remove(jpegPath, ignored);
    return result;
}

#include <guidexos/appmodel/appmodel.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace guidexos::appmodel;

namespace {

constexpr char kPngBase64[] =
    "iVBORw0KGgoAAAANSUhEUgAAAAMAAAACCAYAAACddGYaAAAAAXNSR0IArs4c6QAA"
    "AARnQU1BAACxjwv8YQUAAAAcSURBVBhXFcixDQAACMAgTvfzqhMJIuNJBeNyAYwaCfj7yMTOAAAAAElFTkSuQmCC";

int Base64Value(char value) noexcept {
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}

std::string WriteDemoImage() {
    std::vector<unsigned char> bytes;
    int accumulator = 0;
    int bits = 0;
    for (const char* current = kPngBase64; *current; ++current) {
        if (*current == '=') break;
        const int value = Base64Value(*current);
        if (value < 0) continue;
        accumulator = (accumulator << 6) | value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<unsigned char>(
                (accumulator >> bits) & 0xFF));
        }
    }
    const auto path = std::filesystem::temp_directory_path() /
        "guideXOS_ScrollViewApp_preview.png";
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return path.string();
}

} // namespace

int main() {
    const std::string imagePath = WriteDemoImage();

    Application application("com.guidexos.samples.scroll-view");
    Window window(application);
    window.SetTitle("guideXOS ScrollView Settings");
    window.SetSize(760, 540);

    TabView tabs;
    auto settingsPage = tabs.AddTab("Settings");
    auto aboutPage = tabs.AddTab("About");

    ScrollView scroll;
    scroll.SetToolTip("Use the wheel or the vertical scrollbar to explore settings");
    Label profileLabel("Profile");
    TextBox profile("guideXOS");
    Image preview;
    preview.SetSource(ImageSource::FromFile(imagePath));
    preview.SetScaleMode(ImageScaleMode::Stretch);
    Label optionsLabel("Options");
    CheckBox optionOne("Option 1", true);
    CheckBox optionTwo("Option 2", true);
    CheckBox optionThree("Option 3");
    Label volumeLabel("Volume");
    Slider volume;
    volume.SetValue(62);
    Label notesLabel("Notes");
    TextArea notes("This state remains available while the page is scrolled.");
    Label progressLabel("Progress");
    ProgressBar progress;
    progress.SetValue(68);
    Button advance("Advance progress");
    advance.OnClick([&] {
        progress.SetValue((progress.GetValue() + 10) % 101);
    });
    Label moreLabel("More settings");
    std::vector<std::unique_ptr<Label>> moreRows;
    for (int index = 1; index <= 10; ++index) {
        moreRows.push_back(std::make_unique<Label>(
            "Additional setting " + std::to_string(index)));
    }

    auto& content = scroll.ContentLayout();
    content.Add(profileLabel);
    content.Add(profile);
    content.Add(preview, LayoutSizing::Expand);
    content.Add(optionsLabel);
    content.Add(optionOne);
    content.Add(optionTwo);
    content.Add(optionThree);
    content.Add(volumeLabel);
    content.Add(volume);
    content.Add(notesLabel);
    content.Add(notes, LayoutSizing::Expand);
    content.Add(progressLabel);
    content.Add(progress);
    content.Add(advance);
    content.Add(moreLabel);
    for (const auto& row : moreRows) content.Add(*row);

    settingsPage.GetLayout().Add(scroll, LayoutSizing::Expand);
    Label about("ScrollView keeps this content and its control state alive while the viewport moves.");
    aboutPage.GetLayout().Add(about);

    Layout root;
    root.Add(tabs, LayoutSizing::Expand);
    window.SetContent(root);
    if (!window.Show()) return 1;
    const int result = application.Run();

    std::error_code ignored;
    std::filesystem::remove(imagePath, ignored);
    return result;
}

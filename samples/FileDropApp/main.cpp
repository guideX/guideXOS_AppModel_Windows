#include <guidexos/appmodel/appmodel.hpp>

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

using namespace guidexos::appmodel;

#ifdef GUIDEXOS_FILE_DROP_TEST_HELPER
void StartFileDropTestAgent();
#endif

namespace {

class FileDropDemo final {
public:
    explicit FileDropDemo(Application& application)
        : window_(application), count_("Count: 0") {
        window_.SetTitle("File Drop Demo");
        window_.SetSize(820, 620);

        Layout actions(Orientation::Horizontal, 0, 10);
        actions.Add(clearButton_);
        actions.Add(openButton_);
        actions.AddSpacer(LayoutSizing::Expand);

        Layout content(Orientation::Vertical, 24, 12);
        content.Add(heading_);
        content.Add(instruction_);
        content.Add(filesLabel_);
        content.Add(files_, LayoutSizing::Expand);
        content.Add(count_);
        content.Add(actions);
        content.Add(status_);
        window_.SetContent(content);

        clearButton_.OnClick([this]() { Clear(); });
        openButton_.OnClick([this]() { OpenFirstFile(); });
        window_.OnFilesDropped(
            [this](const FileDropEvent& event) { HandleDrop(event); });
        status_.SetText("Status: Ready for shell file drops");
    }

    bool Show() { return window_.Show(); }

private:
    void HandleDrop(const FileDropEvent& event) {
        for (const auto& path : event.GetFiles()) files_.AddItem(path);
        UpdateCount();
        status_.SetText("Status: Added " +
                        std::to_string(event.GetFiles().size()) +
                        " dropped path(s)");
    }

    void Clear() {
        files_.ClearItems();
        UpdateCount();
        status_.SetText("Status: List cleared");
    }

    void OpenFirstFile() {
        if (files_.GetItemCount() == 0) {
            status_.SetText("Status: No dropped path is available");
            return;
        }

        const std::string path = files_.GetItem(0);
        try {
            const std::string text = File::ReadAllText(path);
            constexpr std::size_t kPreviewBytes = 1024U;
            const std::size_t previewLength =
                std::min(text.size(), kPreviewBytes);
            std::string preview = text.substr(0, previewLength);
            while (!preview.empty() &&
                   (static_cast<unsigned char>(preview.back()) & 0xC0U) == 0x80U) {
                preview.pop_back();
            }
            if (previewLength < text.size()) preview += "\n... (preview truncated)";

            status_.SetText("Status: Read " + DisplayFileName(path));
            MessageDialog::Show(window_,
                                "Preview of " + path + ":\n\n" + preview,
                                "First File Preview",
                                MessageDialogButtons::Ok,
                                MessageDialogIcon::Information);
        } catch (const std::exception& error) {
            status_.SetText("Status: Read failed");
            MessageDialog::Show(window_,
                                "Unable to read " + path + ":\n" +
                                    error.what(),
                                "File Drop Error", MessageDialogButtons::Ok,
                                MessageDialogIcon::Error);
        }
    }

    void UpdateCount() {
        count_.SetText("Count: " + std::to_string(files_.GetItemCount()));
    }

    static std::string DisplayFileName(const std::string& path) {
        const std::size_t separator = path.find_last_of("/\\");
        return separator == std::string::npos ? path : path.substr(separator + 1);
    }

    Window window_;
    Label heading_{"File Drop Demo"};
    Label instruction_{"Drop files anywhere in this window."};
    Label filesLabel_{"Dropped files:"};
    ListBox files_;
    Label count_;
    Button clearButton_{"Clear List"};
    Button openButton_{"Open First File"};
    Label status_;
};

} // namespace

int main() {
#ifdef GUIDEXOS_FILE_DROP_TEST_HELPER
    StartFileDropTestAgent();
#endif
    Application app("com.guidexos.samples.file-drop");
    FileDropDemo demo(app);
    if (!demo.Show()) return 1;
    return app.Run();
}

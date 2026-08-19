#include <guidexos/appmodel/appmodel.hpp>

#include <exception>
#include <filesystem>
#include <optional>
#include <string>

using namespace guidexos::appmodel;

namespace {

std::string WorkingDirectoryUtf8() {
    const auto value = std::filesystem::current_path().u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

class DialogWindow final {
public:
    explicit DialogWindow(Application& application)
        : window_(application),
          file_("&File"),
          dialogs_("&Dialogs"),
          openMenu_("&Open..."),
          saveMenu_("&Save As..."),
          exitMenu_("E&xit"),
          informationMenu_("&Information"),
          warningMenu_("&Warning"),
          errorMenu_("&Error"),
          confirmMenu_("&Confirm") {
        file_.Add(openMenu_);
        file_.Add(saveMenu_);
        file_.AddSeparator();
        file_.Add(exitMenu_);
        dialogs_.Add(informationMenu_);
        dialogs_.Add(warningMenu_);
        dialogs_.Add(errorMenu_);
        dialogs_.Add(confirmMenu_);
        menuBar_.Add(file_);
        menuBar_.Add(dialogs_);

        action_.AddItem("Choose action");
        action_.AddItem("Show Information");

        openButton_.OnClick([this] { ShowOpen(); });
        saveButton_.OnClick([this] { ShowSave(); });
        informationButton_.OnClick([this] { ShowInformation(); });
        confirmButton_.OnClick([this] { ShowConfirmation(); });
        check_.OnCheckedChanged([this](bool checked) {
            if (!checked) return;
            ShowConfirmation();
            check_.SetChecked(false);
        });
        action_.OnSelectionChanged([this](std::optional<std::size_t> index) {
            if (index && *index == 1) ShowInformation();
            if (index) action_.SetSelectedIndex(0);
        });

        openMenu_.OnInvoked([this] { ShowOpen(); });
        saveMenu_.OnInvoked([this] { ShowSave(); });
        exitMenu_.OnInvoked([this] { window_.Close(); });
        informationMenu_.OnInvoked([this] { ShowInformation(); });
        warningMenu_.OnInvoked([this] { ShowWarning(); });
        errorMenu_.OnInvoked([this] { ShowError(); });
        confirmMenu_.OnInvoked([this] { ShowConfirmation(); });

        window_.SetTitle("Dialog and File Picker Demo");
        window_.SetSize(900, 620);
        window_.SetMenuBar(menuBar_);

        Layout openRow(Orientation::Horizontal, 0, 8);
        openRow.Add(openLabel_);
        openRow.Add(openValue_, LayoutSizing::Expand);

        Layout saveRow(Orientation::Horizontal, 0, 8);
        saveRow.Add(saveLabel_);
        saveRow.Add(saveValue_, LayoutSizing::Expand);

        Layout buttons(Orientation::Horizontal, 0, 8);
        buttons.Add(openButton_);
        buttons.Add(saveButton_);
        buttons.Add(informationButton_);
        buttons.Add(confirmButton_);

        Layout content(Orientation::Vertical, 24, 12);
        content.Add(openRow);
        content.Add(saveRow);
        content.Add(buttons);
        content.Add(check_);
        content.Add(action_);
        content.Add(status_);
        window_.SetContent(content);
    }

    bool Show() { return window_.Show(); }

private:
    void SetStatus(const std::string& status) { status_.SetText(status); }

    void ShowOpen() {
        try {
            OpenFileDialog dialog;
            dialog.SetTitle("Open Profile " "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" " " "\xF0\x9F\x9A\x80");
            dialog.SetInitialDirectory(WorkingDirectoryUtf8());
            dialog.AddFilter("guideXOS Profile", {"*.gxprofile"});
            dialog.AddFilter("Text Files", {"*.txt", "*.md"});
            dialog.AddFilter("All Files", {"*.*"});
            const auto path = dialog.Show(window_);
            if (path) {
                openValue_.SetText(*path);
                SetStatus("Status: Open path selected");
            } else {
                SetStatus("Status: Open cancelled");
            }
        } catch (const std::exception&) {
            SetStatus("Status: Open dialog failed");
        }
    }

    void ShowSave() {
        try {
            SaveFileDialog dialog;
            dialog.SetTitle("Save Profile As " "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
            dialog.SetInitialDirectory(WorkingDirectoryUtf8());
            dialog.SetSuggestedFileName("profile.gxprofile");
            dialog.AddFilter("guideXOS Profile", {"*.gxprofile"});
            dialog.AddFilter("All Files", {"*.*"});
            const auto path = dialog.Show(window_);
            if (path) {
                saveValue_.SetText(*path);
                SetStatus("Status: Save path selected");
            } else {
                SetStatus("Status: Save cancelled");
            }
        } catch (const std::exception&) {
            SetStatus("Status: Save dialog failed");
        }
    }

    void ShowInformation() {
        try {
            (void)MessageDialog::Show(
                window_, "Information: " "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" " " "\xF0\x9F\x9A\x80", "Information",
                MessageDialogButtons::Ok, MessageDialogIcon::Information);
            SetStatus("Status: Information returned");
        } catch (const std::exception&) {
            SetStatus("Status: Information dialog failed");
        }
    }

    void ShowWarning() {
        try {
            const auto result = MessageDialog::Show(
                window_, "Warning: changes are only in memory.", "Warning",
                MessageDialogButtons::OkCancel, MessageDialogIcon::Warning);
            SetStatus(result == MessageDialogResult::Cancel
                          ? "Status: Warning cancelled"
                          : "Status: Warning returned");
        } catch (const std::exception&) {
            SetStatus("Status: Warning dialog failed");
        }
    }

    void ShowError() {
        try {
            (void)MessageDialog::Show(
                window_, "Error: no file was written.", "Error",
                MessageDialogButtons::Ok, MessageDialogIcon::Error);
            SetStatus("Status: Error returned");
        } catch (const std::exception&) {
            SetStatus("Status: Error dialog failed");
        }
    }

    void ShowConfirmation() {
        try {
            const auto result = MessageDialog::Show(
                window_, "Delete this profile? " "\xE5\x89\x8A\xE9\x99\xA4?", "Confirm",
                MessageDialogButtons::YesNo, MessageDialogIcon::Question);
            SetStatus(result == MessageDialogResult::Yes
                          ? "Status: Confirmed Yes"
                          : result == MessageDialogResult::No
                                ? "Status: Confirmed No"
                                : "Status: Confirm cancelled");
        } catch (const std::exception&) {
            SetStatus("Status: Confirm dialog failed");
        }
    }

    Window window_;
    MenuBar menuBar_;
    Menu file_;
    Menu dialogs_;
    MenuItem openMenu_;
    MenuItem saveMenu_;
    MenuItem exitMenu_;
    MenuItem informationMenu_;
    MenuItem warningMenu_;
    MenuItem errorMenu_;
    MenuItem confirmMenu_;
    Label openLabel_{"Selected Open File:"};
    Label openValue_{"<none>"};
    Label saveLabel_{"Selected Save File:"};
    Label saveValue_{"<none>"};
    Label status_{"Status: Ready"};
    Button openButton_{"Open File..."};
    Button saveButton_{"Save File As..."};
    Button informationButton_{"Show Information"};
    Button confirmButton_{"Confirm Action"};
    CheckBox check_{"Check to confirm"};
    ComboBox action_;
};

} // namespace

int main() {
    Application app("com.guidexos.samples.dialog-app");
    DialogWindow window(app);
    if (!window.Show()) return 1;
    return app.Run();
}

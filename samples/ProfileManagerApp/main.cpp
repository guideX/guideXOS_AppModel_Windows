#include <guidexos/appmodel/appmodel.hpp>

#include "profile_manager_model.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

using namespace guidexos::appmodel;
using namespace guidexos::samples::profilemanager;

#ifdef GUIDEXOS_FILE_DROP_TEST_HELPER
void StartFileDropTestAgent();
#endif
#ifdef GUIDEXOS_STATUS_TEST_HELPER
void ReportStatusForTest(const std::string& text);
void ResetToolTipsForTest();
void ReportToolTipForTest(const std::string& text);
#endif

namespace {

class ProfileManagerWindow final {
public:
    explicit ProfileManagerWindow(Application& application)
        : window_(application),
          enabled_("Enabled", true),
          standard_("Standard"),
          advanced_("Advanced"),
          compatibility_("Compatibility") {
        window_.SetTitle("Profile Manager - Untitled");
        window_.SetSize(900, 900);

        modeGroup_.Add(standard_);
        modeGroup_.Add(advanced_);
        modeGroup_.Add(compatibility_);

        for (const auto& profile : document_.GetModel().GetProfiles()) {
            profiles_.AddItem(profile.values.name);
        }

        profiles_.SetToolTip("Select a profile");
        name_.SetToolTip("Enter the profile name");
        description_.SetToolTip("Enter the profile description");
        newButton_.SetToolTip("Create a new profile");
        saveButton_.SetToolTip("Save changes to the current profile");
        deleteButton_.SetToolTip("Delete the selected profile");
#ifdef GUIDEXOS_STATUS_TEST_HELPER
        ResetToolTipsForTest();
        ReportToolTipForTest(profiles_.GetToolTip());
        ReportToolTipForTest(name_.GetToolTip());
        ReportToolTipForTest(description_.GetToolTip());
        ReportToolTipForTest(newButton_.GetToolTip());
        ReportToolTipForTest(saveButton_.GetToolTip());
        ReportToolTipForTest(deleteButton_.GetToolTip());
#endif

        newDocumentItem_.SetShortcut(KeyShortcut::Ctrl('N'));
        openDocumentItem_.SetShortcut(KeyShortcut::Ctrl('O'));
        saveDocumentItem_.SetShortcut(KeyShortcut::Ctrl('S'));
        saveAsDocumentItem_.SetShortcut(KeyShortcut::CtrlShift('S'));
        fileMenu_.Add(newDocumentItem_);
        fileMenu_.Add(openDocumentItem_);
        fileMenu_.Add(saveDocumentItem_);
        fileMenu_.Add(saveAsDocumentItem_);
        fileMenu_.AddSeparator();
        fileMenu_.Add(exitItem_);
        menuBar_.Add(fileMenu_);

        cutItem_.SetShortcut(KeyShortcut::Ctrl('X'));
        copyItem_.SetShortcut(KeyShortcut::Ctrl('C'));
        pasteItem_.SetShortcut(KeyShortcut::Ctrl('V'));
        selectAllItem_.SetShortcut(KeyShortcut::Ctrl('A'));
        editMenu_.Add(cutItem_);
        editMenu_.Add(copyItem_);
        editMenu_.Add(pasteItem_);
        editMenu_.Add(deleteItem_);
        editMenu_.AddSeparator();
        editMenu_.Add(selectAllItem_);
        menuBar_.Add(editMenu_);

        Layout content(Orientation::Vertical, 24, 12);
        Layout nameRow(Orientation::Horizontal, 0, 8);
        Layout descriptionRow(Orientation::Horizontal, 0, 8);
        Layout actionRow(Orientation::Horizontal, 0, 8);

        nameRow.Add(nameLabel_);
        nameRow.Add(name_, LayoutSizing::Expand);
        descriptionRow.Add(descriptionLabel_);
        descriptionRow.Add(description_, LayoutSizing::Expand);
        actionRow.Add(newButton_);
        actionRow.Add(saveButton_);
        actionRow.Add(deleteButton_);
        actionRow.AddSpacer(LayoutSizing::Expand);

        content.Add(heading_);
        content.Add(profilesLabel_);
        content.Add(profiles_, LayoutSizing::Expand);
        content.Add(nameRow);
        content.Add(descriptionRow);
        content.Add(enabled_);
        content.Add(modeLabel_);
        content.Add(standard_);
        content.Add(advanced_);
        content.Add(compatibility_);
        content.Add(actionRow);
        window_.SetContent(content);
        window_.SetMenuBar(menuBar_);
        window_.SetStatusBar(statusBar_);

        ConnectCallbacks();
        document_.SelectIndex(0);
        RefreshEditor();
        RefreshDocumentPresentation();
        SetStatus("Status: Development selected");
    }

    bool Show() { return window_.Show(); }

private:
    void ConnectCallbacks() {
        profiles_.OnSelectionChanged(
            [this](std::optional<std::size_t> index) { HandleSelection(index); });
        name_.OnTextChanged([this](const std::string& value) {
            if (!loading_) UpdateDraft([&](ProfileDraft& draft) { draft.name = value; });
        });
        description_.OnTextChanged([this](const std::string& value) {
            if (!loading_) {
                UpdateDraft([&](ProfileDraft& draft) { draft.description = value; });
            }
        });
        enabled_.OnCheckedChanged([this](bool checked) {
            if (!loading_) {
                UpdateDraft([&](ProfileDraft& draft) { draft.enabled = checked; });
            }
        });
        standard_.OnSelectedChanged([this](bool selected) {
            if (selected && !loading_) SetMode(ProfileMode::Standard);
        });
        advanced_.OnSelectedChanged([this](bool selected) {
            if (selected && !loading_) SetMode(ProfileMode::Advanced);
        });
        compatibility_.OnSelectedChanged([this](bool selected) {
            if (selected && !loading_) SetMode(ProfileMode::Compatibility);
        });
        newButton_.OnClick([this]() { BeginNew(); });
        saveButton_.OnClick([this]() { SaveProfile(); });
        deleteButton_.OnClick([this]() { Delete(); });

        newDocumentItem_.OnInvoked([this]() { NewDocument(); });
        openDocumentItem_.OnInvoked([this]() { OpenDocument(); });
        saveDocumentItem_.OnInvoked([this]() { SaveDocument(); });
        saveAsDocumentItem_.OnInvoked([this]() { SaveAs(); });
        exitItem_.OnInvoked([this]() { window_.Close(); });
        editMenu_.OnOpening([this]() { UpdateEditMenuState(); });
        cutItem_.OnInvoked([this]() { RunFocusedEdit([](TextBoxRef target) {
            target.Cut();
        }); });
        copyItem_.OnInvoked([this]() { RunFocusedEdit([](TextBoxRef target) {
            target.Copy();
        }); });
        pasteItem_.OnInvoked([this]() { RunFocusedEdit([](TextBoxRef target) {
            target.Paste();
        }); });
        deleteItem_.OnInvoked([this]() {
            RunFocusedEdit([](TextBoxRef target) { target.DeleteSelection(); });
        });
        selectAllItem_.OnInvoked([this]() {
            RunFocusedEdit([](TextBoxRef target) { target.SelectAll(); });
        });
        window_.OnClosing([this](WindowClosingEvent& event) {
            if (!ConfirmDocumentCanClose()) event.Cancel();
        });
        window_.OnFilesDropped(
            [this](const FileDropEvent& event) { HandleFilesDropped(event); });
    }

    template <typename Change>
    void UpdateDraft(Change&& change) {
        if (!document_.GetModel().HasEditor()) return;
        ProfileDraft draft = document_.GetModel().GetDraft();
        change(draft);
        document_.SetDraft(std::move(draft));
        RefreshCommands();
        SetStatus(document_.IsEditorDirty() ? "Status: Unsaved profile edits"
                                            : "Status: No unsaved profile edits");
    }

    void SetMode(ProfileMode mode) {
        UpdateDraft([&](ProfileDraft& draft) { draft.mode = mode; });
    }

    void HandleSelection(std::optional<std::size_t> index) {
        if (loading_) return;
        const bool discarded = document_.IsEditorDirty();
        if (index) {
            document_.SelectIndex(*index);
            RefreshEditor();
            SetStatus("Status: " + document_.GetModel().GetDraft().name + " selected" +
                      (discarded ? "; unsaved changes discarded" : ""));
        } else {
            document_.ClearSelection();
            RefreshEditor();
            SetStatus("Status: No profile selected");
        }
    }

    void BeginNew() {
        document_.BeginNewProfile();
        RefreshEditor();
        SetStatus("Status: Creating new profile");
    }

    void SaveProfile() {
        if (!document_.GetModel().HasEditor()) return;
        const bool creating = document_.GetModel().IsCreatingNew();
        const ProfileId id = document_.SaveProfileChanges();
        const auto index = document_.GetModel().GetSelectedIndex();
        if (!index || id == 0) return;

        if (creating) {
            profiles_.AddItem(document_.GetModel().GetProfile(*index).values.name);
        } else {
            profiles_.SetItem(*index, document_.GetModel().GetProfile(*index).values.name);
        }

        RefreshEditor();
        RefreshDocumentPresentation();
        SetStatus("Status: " + document_.GetModel().GetDraft().name +
                  (creating ? " created"
                            : " updated") +
                  (document_.IsDirty() ? "; document unsaved" : ""));
    }

    void Delete() {
        const auto selected = document_.GetModel().GetSelectedIndex();
        if (!selected) return;
        const auto removed = document_.DeleteSelectedProfile();
        if (!removed) return;

        profiles_.RemoveItem(*removed);
        RefreshDocumentPresentation();
        if (profiles_.GetItemCount() == 0) {
            RefreshEditor();
            SetStatus("Status: No profiles remain; document unsaved");
            return;
        }

        const std::size_t adjacent = std::min(
            *removed, profiles_.GetItemCount() - 1);
        // RemoveItem clears the selected index synchronously. The next
        // assignment therefore exercises the normal callback path that loads
        // the adjacent profile while this command is still running.
        profiles_.SetSelectedIndex(adjacent);
    }

    void NewDocument() {
        if (!ConfirmDocumentCommand("creating a new document")) return;
        document_.NewDocument();
        RebuildProfilesList();
        RefreshEditor();
        RefreshDocumentPresentation();
        SetStatus("Status: New document");
    }

    void OpenDocument() {
        SetStatus("Status: Open requested");

        OpenFileDialog dialog;
        dialog.SetTitle("Open Profile Set");
        dialog.AddFilter("guideXOS Profile Set (*.gxprofiles)", {"*.gxprofiles"});
        dialog.AddFilter("All Files (*.*)", {"*.*"});
        const auto selected = dialog.Show(window_);
        window_.Show();
        if (!selected) {
            SetStatus("Status: Open cancelled");
            return;
        }

        OpenDocumentFromPath(*selected);
    }

    bool OpenDocumentFromPath(const std::string& path) {
        if (!ConfirmDocumentCommand("opening a document")) {
            SetStatus("Status: Open cancelled");
            return false;
        }
        try {
            document_.LoadSerialized(File::ReadAllText(path), path);
            RebuildProfilesList();
            RefreshEditor();
            RefreshDocumentPresentation();
            SetStatus("Status: Opened " + DisplayFileName(path));
            return true;
        } catch (const std::exception& error) {
            ShowFileError("open", error);
            return false;
        }
    }

    static bool HasProfileExtension(const std::string& path) {
        constexpr std::string_view extension = ".gxprofiles";
        if (path.size() < extension.size()) return false;
        const std::size_t start = path.size() - extension.size();
        for (std::size_t index = 0; index < extension.size(); ++index) {
            const unsigned char actual =
                static_cast<unsigned char>(path[start + index]);
            const unsigned char expected =
                static_cast<unsigned char>(extension[index]);
            if (std::tolower(actual) != std::tolower(expected)) return false;
        }
        return true;
    }

    void HandleFilesDropped(const FileDropEvent& event) {
        if (event.GetFiles().size() != 1U) {
            SetStatus("Status: Drop exactly one .gxprofiles file");
            MessageDialog::Show(
                window_, "Drop exactly one .gxprofiles file to open it.",
                "Profile Manager", MessageDialogButtons::Ok,
                MessageDialogIcon::Information);
            return;
        }
        const std::string& path = event.GetFiles().front();
        if (!HasProfileExtension(path)) {
            SetStatus("Status: Dropped path is not a .gxprofiles file");
            MessageDialog::Show(
                window_, "Only files with the .gxprofiles extension can be opened.",
                "Profile Manager", MessageDialogButtons::Ok,
                MessageDialogIcon::Information);
            return;
        }
        OpenDocumentFromPath(path);
    }

    bool SaveDocument() {
        if (!document_.GetCurrentPath()) return SaveAs();

        if (!CommitPendingEditor()) return false;
        const std::string path = *document_.GetCurrentPath();
        try {
            File::WriteAllText(path, document_.Serialize());
            document_.MarkSaved(path);
            RefreshDocumentPresentation();
            SetStatus("Status: Saved " + DisplayFileName(path));
            return true;
        } catch (const std::exception& error) {
            ShowFileError("save", error);
            return false;
        }
    }

    bool SaveAs() {
        SaveFileDialog dialog;
        dialog.SetTitle("Save Profile Set");
        dialog.SetSuggestedFileName("profiles.gxprofiles");
        dialog.AddFilter("guideXOS Profile Set (*.gxprofiles)", {"*.gxprofiles"});
        dialog.AddFilter("All Files (*.*)", {"*.*"});
        const auto selected = dialog.Show(window_);
        window_.Show();
        if (!selected) {
            SetStatus("Status: Save As cancelled");
            return false;
        }

        if (!CommitPendingEditor()) return false;
        try {
            File::WriteAllText(*selected, document_.Serialize());
            document_.MarkSaved(*selected);
            RefreshDocumentPresentation();
            SetStatus("Status: Saved " + DisplayFileName(*selected));
            return true;
        } catch (const std::exception& error) {
            ShowFileError("save as", error);
            return false;
        }
    }

    bool CommitPendingEditor() {
        const auto& model = document_.GetModel();
        if (!model.HasEditor() || (!document_.IsEditorDirty() &&
                                   !model.IsCreatingNew())) {
            return true;
        }
        const bool creating = model.IsCreatingNew();
        const ProfileId id = document_.SaveProfileChanges();
        const auto index = document_.GetModel().GetSelectedIndex();
        if (id == 0 || !index) return false;
        if (creating) {
            profiles_.AddItem(document_.GetModel().GetProfile(*index).values.name);
        } else {
            profiles_.SetItem(*index, document_.GetModel().GetProfile(*index).values.name);
        }
        RefreshEditor();
        RefreshDocumentPresentation();
        return true;
    }

    bool ConfirmDocumentCommand(const std::string& operation) {
        if (!document_.IsDirty()) return true;
        SetStatus("Status: Asking to save before " + operation);
        MessageDialogResult result;
        try {
            result = MessageDialog::Show(
                window_, "Save changes before " + operation + "?", "Unsaved Changes",
                MessageDialogButtons::YesNoCancel, MessageDialogIcon::Question);
        } catch (const std::exception& error) {
            SetStatus("Status: confirmation failed " + std::string(error.what()));
            return false;
        }
        SetStatus("Status: Confirmation returned");
        if (result == MessageDialogResult::Cancel) return false;
        if (result == MessageDialogResult::Yes) return SaveDocument();
        return true;
    }

    bool ConfirmDocumentCanClose() {
        if (!document_.IsDirty()) return true;
        SetStatus("Status: Asking to save before exiting");
        MessageDialogResult result;
        try {
            result = MessageDialog::Show(
                window_, "Save changes before exiting?", "Unsaved Changes",
                MessageDialogButtons::YesNoCancel, MessageDialogIcon::Question);
        } catch (const std::exception& error) {
            SetStatus("Status: confirmation failed " + std::string(error.what()));
            return false;
        }
        if (result == MessageDialogResult::Cancel) return false;
        if (result == MessageDialogResult::Yes) return SaveDocument();
        return true;
    }

    static std::string DisplayFileName(const std::string& path) {
        const std::size_t separator = path.find_last_of("/\\");
        if (separator == std::string::npos || separator + 1 >= path.size()) {
            return path;
        }
        return path.substr(separator + 1);
    }

    void ShowFileError(const std::string& operation,
                       const std::exception& error) {
        SetStatus("Status: " + operation + " failed");
        MessageDialog::Show(
            window_, "Unable to " + operation + " the profile document.\n" +
                         std::string(error.what()),
            "Profile Manager Error", MessageDialogButtons::Ok,
            MessageDialogIcon::Error);
    }

    void RebuildProfilesList() {
        const bool previousLoading = loading_;
        loading_ = true;
        profiles_.ClearItems();
        for (const auto& profile : document_.GetModel().GetProfiles()) {
            profiles_.AddItem(profile.values.name);
        }
        loading_ = previousLoading;
    }

    void RefreshDocumentPresentation() {
        std::string name = "Untitled";
        if (document_.GetCurrentPath()) {
            name = DisplayFileName(*document_.GetCurrentPath());
        }
        window_.SetTitle("Profile Manager - " + name +
                        (document_.IsDirty() ? " *" : ""));
    }

    void RefreshEditor() {
        const bool previousLoading = loading_;
        loading_ = true;
        if (document_.GetModel().HasEditor()) {
            const auto& draft = document_.GetModel().GetDraft();
            enabled_.SetChecked(draft.enabled);
            switch (draft.mode) {
            case ProfileMode::Standard: modeGroup_.Select(standard_); break;
            case ProfileMode::Advanced: modeGroup_.Select(advanced_); break;
            case ProfileMode::Compatibility:
                modeGroup_.Select(compatibility_);
                break;
            }
            name_.SetText(draft.name);
            description_.SetText(draft.description);
            if (document_.GetModel().IsCreatingNew()) {
                profiles_.SetSelectedIndex(std::nullopt);
            } else {
                profiles_.SetSelectedIndex(document_.GetModel().GetSelectedIndex());
            }
        } else {
            name_.SetText({});
            description_.SetText({});
            enabled_.SetChecked(false);
            modeGroup_.ClearSelection();
            profiles_.SetSelectedIndex(std::nullopt);
        }
        loading_ = previousLoading;
        RefreshCommands();
    }

    void RefreshCommands() {
        const bool hasEditor = document_.GetModel().HasEditor();
        name_.SetEnabled(hasEditor);
        description_.SetEnabled(hasEditor);
        enabled_.SetEnabled(hasEditor);
        standard_.SetEnabled(hasEditor);
        advanced_.SetEnabled(hasEditor);
        compatibility_.SetEnabled(hasEditor);
        saveButton_.SetEnabled(hasEditor);
        deleteButton_.SetEnabled(hasEditor && !document_.GetModel().IsCreatingNew() &&
                                  document_.GetModel().GetSelectedIndex().has_value());
        profiles_.SetEnabled(document_.GetModel().GetProfileCount() != 0);
        UpdateEditMenuState();
    }

    template <typename Action>
    void RunFocusedEdit(Action&& action) {
        const auto target = window_.GetFocusedControl().AsTextBox();
        if (!target) return;
        try {
            action(*target);
        } catch (const ClipboardError& error) {
            SetStatus("Status: Clipboard error: " + std::string(error.what()));
        }
    }

    void UpdateEditMenuState() {
        const auto target = window_.GetFocusedControl().AsTextBox();
        const bool hasSelection = target && target->HasSelection();
        bool hasClipboardText = false;
        try {
            hasClipboardText = Clipboard::HasText();
        } catch (const ClipboardError&) {
            hasClipboardText = false;
        }
        cutItem_.SetEnabled(hasSelection);
        copyItem_.SetEnabled(hasSelection);
        pasteItem_.SetEnabled(target.has_value() && hasClipboardText);
        deleteItem_.SetEnabled(hasSelection);
        selectAllItem_.SetEnabled(target && target->HasText());
    }

    void SetStatus(std::string text) {
        constexpr std::string_view prefix = "Status: ";
        if (text.starts_with(prefix)) text.erase(0, prefix.size());
        statusBar_.SetText(std::move(text));
#ifdef GUIDEXOS_STATUS_TEST_HELPER
        ReportStatusForTest(statusBar_.GetText());
#endif
    }

    ProfileManagerDocument document_;
    Window window_;
    Label heading_{"Profile Manager"};
    Label profilesLabel_{"Profiles"};
    ListBox profiles_;
    Label nameLabel_{"Name"};
    TextBox name_;
    Label descriptionLabel_{"Description"};
    TextBox description_;
    CheckBox enabled_;
    Label modeLabel_{"Mode"};
    RadioButton standard_;
    RadioButton advanced_;
    RadioButton compatibility_;
    RadioGroup modeGroup_;
    Button newButton_{"New"};
    Button saveButton_{"Save Changes"};
    Button deleteButton_{"Delete"};
    StatusBar statusBar_{"Ready"};
    MenuBar menuBar_;
    Menu fileMenu_{"&File"};
    Menu editMenu_{"&Edit"};
    MenuItem newDocumentItem_{"&New"};
    MenuItem openDocumentItem_{"&Open..."};
    MenuItem saveDocumentItem_{"&Save"};
    MenuItem saveAsDocumentItem_{"Save &As..."};
    MenuItem exitItem_{"E&xit"};
    MenuItem cutItem_{"&Cut"};
    MenuItem copyItem_{"&Copy"};
    MenuItem pasteItem_{"&Paste"};
    MenuItem deleteItem_{"&Delete"};
    MenuItem selectAllItem_{"Select &All"};
    bool loading_ = false;
};

} // namespace

int main() {
#ifdef GUIDEXOS_FILE_DROP_TEST_HELPER
    StartFileDropTestAgent();
#endif
    Application app("com.guidexos.samples.profile-manager");
    ProfileManagerWindow profileManager(app);
    if (!profileManager.Show()) return 1;
    return app.Run();
}

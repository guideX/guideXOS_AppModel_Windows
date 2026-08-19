#include <guidexos/appmodel/appmodel.hpp>

#include <optional>
#include <string>

using namespace guidexos::appmodel;

namespace {

class MenuWindow final {
public:
    explicit MenuWindow(Application& application)
        : window_(application),
          file_("&File"),
          edit_("&Edit"),
          chooseMode_("Choose &Mode"),
          help_("&Help") {
        mode_.AddItem("Standard");
        mode_.AddItem("Advanced");
        mode_.AddItem("Compatibility");
        mode_.SetSelectedIndex(0);
        clearStatus_.SetEnabled(false);

        newItem_.SetShortcut(KeyShortcut::Ctrl('N'));
        resetItem_.SetShortcut(KeyShortcut::Ctrl('R'));
        exitItem_.SetShortcut(KeyShortcut::Ctrl('Q'));

        file_.Add(newItem_);
        file_.Add(resetItem_);
        file_.Add(clearStatus_);
        file_.AddSeparator();
        file_.Add(exitItem_);

        edit_.Add(toggleOption_);
        chooseMode_.Add(standardItem_);
        chooseMode_.Add(advancedItem_);
        chooseMode_.Add(compatibilityItem_);
        edit_.Add(chooseMode_);

        help_.Add(aboutItem_);
        // This duplicate label intentionally has a different command state
        // and callback from File > Reset.
        help_.Add(helpResetItem_);
        help_.Add(unicodeItem_);

        menuBar_.Add(file_);
        menuBar_.Add(edit_);
        menuBar_.Add(help_);

        ConnectCallbacks();

        window_.SetTitle("guideXOS Menu App");
        window_.SetSize(800, 500);
        window_.SetMenuBar(menuBar_);

        Layout content(Orientation::Vertical, 24, 12);
        content.Add(status_);
        content.Add(input_);
        content.Add(option_);
        content.Add(mode_);
        window_.SetContent(content);
    }

    bool Show() { return window_.Show(); }

private:
    void ConnectCallbacks() {
        input_.OnTextChanged([this](const std::string& value) {
            clearStatus_.SetEnabled(!value.empty());
            status_.SetText(value.empty() ? "Status: Ready"
                                          : "Status: Input changed");
        });
        chooseMode_.OnOpening([this]() {
            status_.SetText("Status: Choose Mode opened");
        });

        newItem_.OnInvoked([this]() {
            input_.SetText({});
            option_.SetChecked(false);
            mode_.SetSelectedIndex(0);
            clearStatus_.SetEnabled(false);
            status_.SetText("Status: New selected");
        });
        resetItem_.OnInvoked([this]() {
            input_.SetText("Default text");
            option_.SetChecked(false);
            mode_.SetSelectedIndex(0);
            status_.SetText("Status: Reset selected");
        });
        clearStatus_.OnInvoked([this]() {
            status_.SetText("Status: Cleared");
            clearStatus_.SetEnabled(false);
        });
        exitItem_.OnInvoked([this]() { window_.Close(); });

        toggleOption_.OnInvoked([this]() {
            option_.SetChecked(!option_.IsChecked());
            toggleOption_.SetChecked(option_.IsChecked());
            status_.SetText(option_.IsChecked()
                                ? "Status: Option enabled"
                                : "Status: Option disabled");
        });
        standardItem_.OnInvoked([this]() { SelectMode(0, "Standard"); });
        advancedItem_.OnInvoked([this]() { SelectMode(1, "Advanced"); });
        compatibilityItem_.OnInvoked(
            [this]() { SelectMode(2, "Compatibility"); });

        aboutItem_.OnInvoked([this]() {
            aboutItem_.SetText("&About (selected)");
            status_.SetText("Status: About selected");
        });
        helpResetItem_.OnInvoked([this]() {
            status_.SetText("Status: Help Reset selected");
        });
        unicodeItem_.OnInvoked([this]() {
            status_.SetText("Status: Unicode selected");
        });
    }

    void SelectMode(std::size_t index, const char* name) {
        mode_.SetSelectedIndex(index);
        status_.SetText(std::string("Status: Mode ") + name);
    }

    Window window_;
    MenuBar menuBar_;
    Menu file_;
    Menu edit_;
    Menu chooseMode_;
    Menu help_;
    MenuItem newItem_{"&New"};
    MenuItem resetItem_{"&Reset"};
    MenuItem clearStatus_{"Clear Status"};
    MenuItem exitItem_{"E&xit"};
    MenuItem toggleOption_{"&Toggle Option"};
    MenuItem standardItem_{"&Standard"};
    MenuItem advancedItem_{"&Advanced"};
    MenuItem compatibilityItem_{"&Compatibility"};
    MenuItem aboutItem_{"&About"};
    MenuItem helpResetItem_{"&Reset"};
    MenuItem unicodeItem_{
        "Caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x9A\x80"};
    Label status_{"Status: Ready"};
    TextBox input_{"Initial text"};
    CheckBox option_{"Option"};
    ComboBox mode_;
};

} // namespace

int main() {
    Application app("com.guidexos.samples.menu-app");
    MenuWindow window(app);
    if (!window.Show()) return 1;
    return app.Run();
}

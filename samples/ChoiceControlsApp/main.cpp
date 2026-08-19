#include <guidexos/appmodel/appmodel.hpp>

#include <string>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.samples.choice-controls");
    Window window(app);
    window.SetTitle("guideXOS Choice Controls");
    window.SetSize(700, 760);

    Label heading("Preferences");
    CheckBox notifications("Enable notifications", true);
    CheckBox startup("Start automatically");
    Label themeHeading("Theme");
    RadioButton systemTheme("System");
    RadioButton lightTheme("Light");
    RadioButton darkTheme("Dark");
    RadioGroup themeGroup;
    themeGroup.Add(systemTheme);
    themeGroup.Add(lightTheme);
    themeGroup.Add(darkTheme);
    themeGroup.Select(systemTheme);

    Label languageHeading("Language");
    RadioButton english("English");
    RadioButton spanish("Espa\xC3\xB1ol");
    RadioButton japanese("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
    RadioGroup languageGroup;
    languageGroup.Add(english);
    languageGroup.Add(spanish);
    languageGroup.Add(japanese);
    languageGroup.Select(english);

    Label status("Status: Notifications enabled; System theme; English");
    Button toggleNotifications("Toggle Notifications");
    Button selectDark("Select Dark");
    Button disableTheme("Disable Theme Options");

    const auto updateStatus = [&]() {
        std::string theme = "none";
        if (const auto index = themeGroup.GetSelectedIndex()) {
            theme = index == 0 ? "System" : (index == 1 ? "Light" : "Dark");
        }
        std::string language = "none";
        if (const auto index = languageGroup.GetSelectedIndex()) {
            language = index == 0 ? "English" : (index == 1 ? "Espa\xC3\xB1ol" :
                                                      "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
        }
        status.SetText("Status: Notifications " +
                       std::string(notifications.IsChecked() ? "enabled" : "disabled") +
                       "; " + theme + " theme; " + language);
    };

    notifications.OnCheckedChanged([&](bool) { updateStatus(); });
    startup.OnCheckedChanged([&](bool) { updateStatus(); });
    systemTheme.OnSelectedChanged([&](bool selected) {
        if (selected) updateStatus();
    });
    lightTheme.OnSelectedChanged([&](bool selected) {
        if (selected) updateStatus();
    });
    darkTheme.OnSelectedChanged([&](bool selected) {
        if (selected) updateStatus();
    });
    english.OnSelectedChanged([&](bool selected) {
        if (selected) updateStatus();
    });
    spanish.OnSelectedChanged([&](bool selected) {
        if (selected) updateStatus();
    });
    japanese.OnSelectedChanged([&](bool selected) {
        if (selected) updateStatus();
    });

    toggleNotifications.OnClick([&]() {
        notifications.SetChecked(!notifications.IsChecked());
    });
    selectDark.OnClick([&]() { themeGroup.Select(darkTheme); });
    disableTheme.OnClick([&]() {
        const bool enable = !systemTheme.IsEnabled();
        systemTheme.SetEnabled(enable);
        lightTheme.SetEnabled(enable);
        darkTheme.SetEnabled(enable);
        if (!enable) themeGroup.Select(darkTheme);
        disableTheme.SetText(enable ? "Disable Theme Options" :
                                      "Enable Theme Options");
        updateStatus();
    });

    Layout content;
    content.Add(heading);
    content.Add(notifications);
    content.Add(startup);
    content.Add(themeHeading);
    content.Add(systemTheme);
    content.Add(lightTheme);
    content.Add(darkTheme);
    content.Add(languageHeading);
    content.Add(english);
    content.Add(spanish);
    content.Add(japanese);
    content.Add(status);
    content.Add(toggleNotifications);
    content.Add(selectDark);
    content.Add(disableTheme);
    window.SetContent(content);

    if (!window.Show()) return 1;
    return app.Run();
}

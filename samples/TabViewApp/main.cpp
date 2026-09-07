#include <guidexos/appmodel/appmodel.hpp>

#include <string>

using namespace guidexos::appmodel;

int main() {
    Application application("com.guidexos.samples.tab-view");
    Window window(application);
    window.SetTitle("Settings Demo");
    window.SetSize(900, 620);

    TabView tabs;
    auto general = tabs.AddTab("General");
    auto network = tabs.AddTab("Network");
    auto diagnostics = tabs.AddTab("Diagnostics");

    Label nameLabel("Display name:");
    TextBox nameBox("guideXOS user");
    CheckBox notifications("Enable notifications", true);
    Label volumeLabel("Volume:");
    Slider volume;
    volume.SetMinimum(0);
    volume.SetMaximum(100);
    volume.SetValue(50);
    ProgressBar generalProgress;
    generalProgress.SetValue(volume.GetValue());
    Layout generalLayout = general.GetLayout();
    generalLayout.Add(nameLabel);
    generalLayout.Add(nameBox);
    generalLayout.Add(notifications);
    generalLayout.Add(volumeLabel);
    generalLayout.Add(volume);
    generalLayout.Add(generalProgress);

    Label serverLabel("Server:");
    TextBox server("example.com");
    Label portLabel("Port:");
    TextBox port("443");
    Button connect("Connect");
    Layout networkLayout = network.GetLayout();
    networkLayout.Add(serverLabel);
    networkLayout.Add(server);
    networkLayout.Add(portLabel);
    networkLayout.Add(port);
    networkLayout.Add(connect);

    Label progressLabel("Progress:");
    ProgressBar diagnosticsProgress;
    Button startTest("Start Test");
    TextArea log("Ready to run diagnostics.");
    Layout diagnosticsLayout = diagnostics.GetLayout();
    diagnosticsLayout.Add(progressLabel);
    diagnosticsLayout.Add(diagnosticsProgress);
    diagnosticsLayout.Add(startTest);
    diagnosticsLayout.Add(log, LayoutSizing::Expand);
    Label status("Status: General selected");
    tabs.OnSelectionChanged([&](std::optional<std::size_t> index) {
        if (!index) {
            status.SetText("Status: no page selected");
            return;
        }
        status.SetText("Status: " + tabs.GetTab(*index).GetTitle() +
                       " selected");
    });
    volume.OnChanged([&]() { generalProgress.SetValue(volume.GetValue()); });
    connect.OnClick([&]() { log.SetText("Connected to " + server.GetText()); });
    startTest.OnClick([&]() {
        diagnosticsProgress.SetValue(100);
        log.SetText("Diagnostics completed successfully.");
    });

    Layout content;
    content.Add(tabs, LayoutSizing::Expand);
    content.Add(status);
    window.SetContent(content);
    if (!window.Show()) return 1;
    return application.Run();
}

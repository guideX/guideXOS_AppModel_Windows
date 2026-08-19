#include <guidexos/appmodel/appmodel.hpp>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.samples.helloapp");

    Window window(app);
    window.SetTitle("guideXOS App Model for Windows");
    window.SetSize(800, 500);

    Label statusLabel("Hello from the guideXOS App Model");
    Button actionButton("Click Me");

    actionButton.OnClick([&]() {
        statusLabel.SetText("The button was clicked.");
    });

    Layout content;
    content.Add(statusLabel);
    content.Add(actionButton);
    window.SetContent(content);

    if (!window.Show()) return 1;
    return app.Run();
}

#include <guidexos/appmodel/appmodel.hpp>

#include <string>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.samples.multiwindowapp");

    Window primary(app);
    primary.SetTitle("guideXOS Multi-Window Primary");
    primary.SetSize(800, 500);

    Window secondary(app);
    secondary.SetTitle("guideXOS Multi-Window Secondary");
    secondary.SetSize(600, 400);

    Label primaryStatus("Primary window is ready.");
    Label secondaryStatus("Secondary window is closed.");

    Button openSecondary("Open Secondary Window");
    Button updateSecondary("Update Secondary Label");
    Button closePrimary("Close Primary Window");

    Button updatePrimary("Update Primary Label");
    Button closeSecondary("Close Secondary Window");

    MenuBar primaryMenuBar;
    Menu primaryActions("&Actions");
    MenuItem primaryMenuUpdate("Update");
    primaryActions.Add(primaryMenuUpdate);
    primaryMenuBar.Add(primaryActions);
    primary.SetMenuBar(primaryMenuBar);

    MenuBar secondaryMenuBar;
    Menu secondaryActions("&Actions");
    MenuItem secondaryMenuUpdate("Update");
    secondaryActions.Add(secondaryMenuUpdate);
    secondaryMenuBar.Add(secondaryActions);
    secondary.SetMenuBar(secondaryMenuBar);

    int secondaryOpenCount = 0;

    openSecondary.OnClick([&]() {
        if (!secondary.IsShown()) {
            ++secondaryOpenCount;
            secondaryStatus.SetText("Secondary open count: " +
                                    std::to_string(secondaryOpenCount));
            if (!secondary.Show()) {
                primaryStatus.SetText("Secondary could not be opened.");
                return;
            }
        }
        primaryStatus.SetText("Secondary window is open.");
    });

    updateSecondary.OnClick([&]() {
        if (secondary.IsShown()) {
            secondaryStatus.SetText("Updated by the primary window.");
        } else {
            primaryStatus.SetText("Secondary is closed. Open it first.");
        }
    });

    closePrimary.OnClick([&]() {
        primary.Close();
    });

    updatePrimary.OnClick([&]() {
        primaryStatus.SetText("Updated by the secondary window.");
    });

    closeSecondary.OnClick([&]() {
        secondary.Close();
        primaryStatus.SetText("Secondary window is closed.");
    });
    primaryMenuUpdate.OnInvoked([&]() {
        primaryStatus.SetText("Updated by the primary menu.");
    });
    secondaryMenuUpdate.OnInvoked([&]() {
        secondaryStatus.SetText("Updated by the secondary menu.");
    });

    Layout primaryContent;
    primaryContent.Add(primaryStatus);
    primaryContent.Add(openSecondary);
    primaryContent.Add(updateSecondary);
    primaryContent.Add(closePrimary);
    primary.SetContent(primaryContent);

    Layout secondaryContent;
    secondaryContent.Add(secondaryStatus);
    secondaryContent.Add(updatePrimary);
    secondaryContent.Add(closeSecondary);
    secondary.SetContent(secondaryContent);

    if (!primary.Show()) return 1;
    return app.Run();
}

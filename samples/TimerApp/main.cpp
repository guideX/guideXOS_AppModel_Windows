#include <guidexos/appmodel/appmodel.hpp>

#include <chrono>
#include <string>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.samples.timer", ShutdownMode::Explicit);
    Window window(app);
    window.SetTitle("guideXOS Timer");
    window.SetSize(520, 280);

    Label status("Ticks: 0");
    Button toggle("Stop Timer");
    Button quit("Quit");
    Timer timer(app, std::chrono::milliseconds(50));
    int tickCount = 0;

    timer.OnTick([&]() {
        ++tickCount;
        status.SetText("Ticks: " + std::to_string(tickCount));
    });
    toggle.OnClick([&]() {
        if (timer.IsRunning()) {
            timer.Stop();
            toggle.SetText("Start Timer");
            status.SetText("Stopped at " + std::to_string(tickCount));
        } else {
            timer.Start();
            toggle.SetText("Stop Timer");
            status.SetText("Ticks: " + std::to_string(tickCount));
        }
    });
    quit.OnClick([&]() { app.Quit(); });

    Layout content;
    content.Add(status);
    content.Add(toggle);
    content.Add(quit);
    window.SetContent(content);

    if (!window.Show()) return 1;
    timer.Start();
    return app.Run();
}

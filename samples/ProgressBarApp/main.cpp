#include <guidexos/appmodel/appmodel.hpp>

#include <chrono>
#include <string>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.samples.progress-bar");
    Window window(app);
    window.SetTitle("guideXOS Progress Demo");
    window.SetSize(620, 360);

    Label heading("Progress Demo");
    Label description("Preparing operation...");
    ProgressBar progress;
    progress.SetMinimum(0);
    progress.SetMaximum(100);
    Label percentage("0%");
    Button start("Start");
    Button reset("Reset");
    CheckBox unknownDuration("Unknown duration");
    Label status("Status: Ready");
    Timer timer(app, std::chrono::milliseconds(100));

    const auto updateProgressText = [&]() {
        percentage.SetText(std::to_string(progress.GetValue()) + "%");
    };
    const auto updateRunningStatus = [&]() {
        status.SetText(progress.IsIndeterminate()
                           ? "Status: Running (unknown duration)"
                           : "Status: Running");
    };

    timer.OnTick([&]() {
        const int next = progress.GetValue() >= progress.GetMaximum()
            ? progress.GetMaximum() : progress.GetValue() + 1;
        progress.SetValue(next);
        updateProgressText();
        if (progress.GetValue() >= progress.GetMaximum()) {
            timer.Stop();
            status.SetText("Status: Complete");
        }
    });

    start.OnClick([&]() {
        timer.Stop();
        progress.SetValue(progress.GetMinimum());
        progress.SetIndeterminate(unknownDuration.IsChecked());
        updateProgressText();
        updateRunningStatus();
        timer.Start();
    });
    reset.OnClick([&]() {
        timer.Stop();
        progress.SetValue(progress.GetMinimum());
        updateProgressText();
        status.SetText("Status: Ready");
    });
    unknownDuration.OnCheckedChanged([&](bool checked) {
        progress.SetIndeterminate(checked);
        if (timer.IsRunning()) {
            updateRunningStatus();
        } else {
            status.SetText(checked ? "Status: Unknown duration"
                                   : "Status: Ready");
        }
    });

    Layout actions(Orientation::Horizontal, 0, 12);
    actions.Add(start);
    actions.Add(reset);
    actions.AddSpacer();

    Layout content(Orientation::Vertical, 24, 10);
    content.Add(heading);
    content.Add(description);
    content.Add(progress, LayoutSizing::Expand);
    content.Add(percentage);
    content.Add(actions);
    content.Add(unknownDuration);
    content.Add(status);
    window.SetContent(content);

    if (!window.Show()) return 1;
    return app.Run();
}

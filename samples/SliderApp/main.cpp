#include <guidexos/appmodel/appmodel.hpp>

#include <string>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.samples.slider");
    Window window(app);
    window.SetTitle("guideXOS Slider Demo");
    window.SetSize(720, 420);

    Label heading("Value Control");
    Label valueLabel("Value: 42");
    Slider slider;
    slider.SetMinimum(0);
    slider.SetMaximum(100);
    slider.SetValue(42);
    ProgressBar progress;
    progress.SetMinimum(0);
    progress.SetMaximum(100);
    progress.SetValue(42);
    Label progressValue("Progress: 42");
    Button reset("Reset");
    Label rangeLabel("Range: 0 .. 100");

    slider.OnChanged([&]() {
        progress.SetValue(slider.GetValue());
        valueLabel.SetText("Value: " + std::to_string(slider.GetValue()));
        progressValue.SetText("Progress: " +
                              std::to_string(progress.GetValue()));
    });
    reset.OnClick([&]() { slider.SetValue(42); });

    Layout actions(Orientation::Horizontal, 0, 12);
    actions.Add(reset);
    actions.AddSpacer();

    Layout content(Orientation::Vertical, 24, 10);
    content.Add(heading);
    content.Add(valueLabel);
    content.Add(slider, LayoutSizing::Expand);
    content.Add(progress, LayoutSizing::Expand);
    content.Add(progressValue);
    content.Add(actions);
    content.Add(rangeLabel);
    window.SetContent(content);

    if (!window.Show()) return 1;
    return app.Run();
}

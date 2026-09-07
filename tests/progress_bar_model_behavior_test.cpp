#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (false)

using namespace guidexos::appmodel;

int main() {
    ProgressBar progress;
    assert(progress.GetMinimum() == 0);
    assert(progress.GetMaximum() == 100);
    assert(progress.GetValue() == 0);
    assert(!progress.IsIndeterminate());

    progress.SetValue(35);
    assert(progress.GetValue() == 35);
    progress.SetValue(150);
    assert(progress.GetValue() == 100);
    progress.SetValue(-5);
    assert(progress.GetValue() == 0);

    progress.SetMinimum(25);
    assert(progress.GetMinimum() == 25);
    assert(progress.GetMaximum() == 100);
    assert(progress.GetValue() == 25);
    progress.SetMaximum(75);
    assert(progress.GetMaximum() == 75);
    assert(progress.GetValue() == 25);
    progress.SetValue(50);
    progress.SetMinimum(75);
    assert(progress.GetMinimum() == 75);
    assert(progress.GetValue() == 75);
    progress.SetMaximum(25);
    assert(progress.GetMaximum() == 75);
    assert(progress.GetValue() == 75);

    // Crossing an endpoint clamps that endpoint to the other endpoint and
    // leaves a deterministic zero-width range.
    progress.SetMinimum(100);
    assert(progress.GetMinimum() == 75);
    assert(progress.GetMaximum() == 75);
    progress.SetMaximum(42);
    assert(progress.GetMaximum() == 75);
    progress.SetMinimum(42);
    progress.SetMaximum(42);
    assert(progress.GetMinimum() == 42);
    assert(progress.GetMaximum() == 42);
    progress.SetValue(-100);
    assert(progress.GetValue() == 42);
    progress.SetValue(100);
    assert(progress.GetValue() == 42);

    progress.SetMinimum(-100);
    progress.SetMaximum(100);
    progress.SetValue(42);
    progress.SetIndeterminate(true);
    assert(progress.IsIndeterminate());
    assert(progress.GetMinimum() == -100);
    assert(progress.GetMaximum() == 100);
    assert(progress.GetValue() == 42);
    progress.SetValue(70);
    assert(progress.GetValue() == 70);
    progress.SetIndeterminate(false);
    assert(!progress.IsIndeterminate());
    assert(progress.GetValue() == 70);

    progress.SetEnabled(false);
    assert(!progress.IsEnabled());
    progress.SetEnabled(true);
    assert(progress.IsEnabled());
    progress.SetToolTip("Completion");
    assert(progress.GetToolTip() == "Completion");
    progress.SetToolTip({});
    assert(progress.GetToolTip().empty());
    const ControlRef progressRef = progress.GetControlRef();
    assert(progressRef.IsValid());
    assert(progressRef.GetType() == ControlType::ProgressBar);
    assert(!progressRef.Focus());
    const auto progressCapability = progressRef.AsProgressBar();
    assert(progressCapability.has_value());
    assert(progressCapability->IsValid());
    assert(progressCapability->GetValue() == 70);
    assert(!progressRef.AsTextBox().has_value());

    ProgressBar invalidEndpoint;
    invalidEndpoint.SetMaximum(50);
    invalidEndpoint.SetValue(40);
    invalidEndpoint.SetMinimum(100);
    assert(invalidEndpoint.GetMinimum() == 50);
    assert(invalidEndpoint.GetMaximum() == 50);
    assert(invalidEndpoint.GetValue() == 50);

    // Properties are authoritative before realization and remain mutable
    // after the native child exists.
    Application app("com.guidexos.tests.progress-bar-model",
                    ShutdownMode::Explicit);
    Window window(app);
    ProgressBar before;
    before.SetMinimum(10);
    before.SetMaximum(200);
    before.SetValue(75);
    Layout content;
    content.Add(before, LayoutSizing::Expand);
    window.SetContent(content);
    assert(before.GetValue() == 75);
    assert(window.Show());
    before.SetMinimum(20);
    before.SetMaximum(180);
    before.SetValue(160);
    before.SetIndeterminate(true);
    assert(before.GetMinimum() == 20);
    assert(before.GetMaximum() == 180);
    assert(before.GetValue() == 160);
    assert(before.IsIndeterminate());
    before.SetIndeterminate(false);
    assert(!before.IsIndeterminate());

    ProgressBar independent;
    independent.SetValue(11);
    assert(before.GetValue() == 160);
    assert(independent.GetValue() == 11);

    window.Close();
    assert(!window.IsShown());
    before.SetValue(21);
    assert(before.GetValue() == 21);
    assert(window.Show());
    assert(before.GetValue() == 21);
    window.Close();
    app.Quit();
    assert(app.Run() == 0);

    ControlRef destroyedRef;
    {
        ProgressBar temporary;
        destroyedRef = temporary.GetControlRef();
        assert(destroyedRef.IsValid());
    }
    assert(!destroyedRef.IsValid());

    // A progress model retains ordinary state through application shutdown.
    Application shutdownApp("com.guidexos.tests.progress-bar-shutdown",
                            ShutdownMode::WhenLastWindowCloses);
    Window shutdownWindow(shutdownApp);
    ProgressBar shutdownProgress;
    Layout shutdownContent;
    shutdownContent.Add(shutdownProgress);
    shutdownWindow.SetContent(shutdownContent);
    assert(shutdownWindow.Show());
    shutdownProgress.SetValue(9);
    shutdownWindow.Close();
    assert(shutdownApp.Run() == 0);
    assert(shutdownProgress.GetValue() == 9);

    return 0;
}

#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace guidexos::appmodel;

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (false)

namespace {

bool ThrowsLogicError(const auto& operation) {
    try {
        operation();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    Slider slider;
    assert(slider.GetMinimum() == 0);
    assert(slider.GetMaximum() == 100);
    assert(slider.GetValue() == 0);
    assert(slider.IsEnabled());

    std::vector<int> events;
    bool callbackSawCurrentValue = true;
    slider.OnChanged([&]() {
        events.push_back(slider.GetValue());
        callbackSawCurrentValue = callbackSawCurrentValue &&
            events.back() == slider.GetValue();
    });

    slider.SetValue(35);
    assert(slider.GetValue() == 35);
    assert(events == std::vector<int>{35});
    slider.SetValue(35);
    assert(events.size() == 1);
    slider.SetValue(150);
    slider.SetValue(-5);
    assert(slider.GetValue() == 0);
    assert(events == std::vector<int>({35, 100, 0}));
    assert(callbackSawCurrentValue);

    // Endpoint mutation uses ProgressBar's clamping policy and dispatches at
    // most one event for the effective value transition.
    slider.SetMinimum(25);
    assert(slider.GetMinimum() == 25);
    assert(slider.GetMaximum() == 100);
    assert(slider.GetValue() == 25);
    assert(events.back() == 25);
    const auto eventCountAfterValueClamp = events.size();
    slider.SetMinimum(10);
    assert(slider.GetMinimum() == 10);
    assert(slider.GetValue() == 25);
    assert(events.size() == eventCountAfterValueClamp);

    slider.SetValue(60);
    slider.SetMaximum(40);
    assert(slider.GetMaximum() == 40);
    assert(slider.GetValue() == 40);
    assert(events.back() == 40);
    const auto eventCountAfterMaximumClamp = events.size();
    slider.SetMaximum(20);
    assert(slider.GetMaximum() == 20);
    assert(slider.GetValue() == 20);
    assert(events.back() == 20);
    assert(events.size() == eventCountAfterMaximumClamp + 1);

    slider.SetMaximum(-10);
    assert(slider.GetMinimum() == 10);
    assert(slider.GetMaximum() == 10);
    assert(slider.GetValue() == 10);
    assert(events.back() == 10);
    slider.SetValue(-100);
    slider.SetValue(100);
    assert(slider.GetValue() == 10);
    slider.SetMinimum(100);
    assert(slider.GetMinimum() == 10);
    slider.SetMaximum(-10);
    assert(slider.GetMaximum() == 10);

    slider.SetMinimum(-100);
    slider.SetMaximum(100);
    slider.SetValue(-25);
    assert(slider.GetMinimum() == -100);
    assert(slider.GetMaximum() == 100);
    assert(slider.GetValue() == -25);
    assert(events.back() == -25);

    slider.SetToolTip("Signed adjustment");
    assert(slider.GetToolTip() == "Signed adjustment");
    slider.SetToolTip({});
    assert(slider.GetToolTip().empty());

    slider.SetEnabled(false);
    assert(!slider.IsEnabled());
    slider.SetValue(12);
    assert(slider.GetValue() == 12);
    assert(events.back() == 12);
    slider.SetEnabled(true);
    assert(slider.IsEnabled());

    int replacementEvents = 0;
    slider.OnChanged([&]() { ++replacementEvents; });
    slider.SetValue(13);
    assert(replacementEvents == 1);
    assert(events.back() == 12);
    slider.OnChanged({});
    slider.SetValue(14);
    assert(replacementEvents == 1);

    Slider reentrant;
    std::vector<int> reentrantEvents;
    reentrant.OnChanged([&]() {
        reentrantEvents.push_back(reentrant.GetValue());
        if (reentrant.GetValue() > 75) reentrant.SetValue(75);
    });
    reentrant.SetValue(90);
    assert(reentrant.GetValue() == 75);
    assert(reentrantEvents == std::vector<int>({90, 75}));

    Slider disableSelf;
    disableSelf.OnChanged([&]() { disableSelf.SetEnabled(false); });
    disableSelf.SetValue(1);
    assert(disableSelf.GetValue() == 1);
    assert(!disableSelf.IsEnabled());

    Slider second;
    second.SetValue(22);
    assert(slider.GetValue() == 14);
    assert(second.GetValue() == 22);

    const ControlRef sliderRef = slider.GetControlRef();
    assert(sliderRef.IsValid());
    assert(sliderRef.GetType() == ControlType::Slider);
    assert(!sliderRef.Focus());
    const auto sliderCapability = sliderRef.AsSlider();
    assert(sliderCapability.has_value());
    assert(sliderCapability->IsValid());
    assert(sliderCapability->GetMinimum() == -100);
    assert(sliderCapability->GetMaximum() == 100);
    assert(sliderCapability->GetValue() == 14);
    assert(!sliderRef.AsProgressBar().has_value());

    Layout geometry(Orientation::Horizontal, 4, 6);
    Slider geometrySlider;
    geometry.Add(geometrySlider, LayoutSizing::Expand);
    const auto natural = geometrySlider.GetControlRef().AsSlider();
    assert(natural.has_value());
    assert(geometry.GetNaturalSize().width >= 220);
    assert(geometry.GetNaturalSize().height >= 24);
    const auto rectangles = geometry.CalculateGeometry({0, 0, 500, 60});
    assert(rectangles.size() == 1);
    assert(rectangles[0].width > 0);
    assert(rectangles[0].height >= 24);
    assert(ThrowsLogicError([&]() { geometry.Add(geometrySlider); }));

    // Pre-realization and post-realization mutation converge on the same
    // retained model state, and realization itself emits no value event.
    Application app("com.guidexos.tests.slider-model", ShutdownMode::Explicit);
    Window window(app);
    Slider live;
    live.SetMinimum(-50);
    live.SetMaximum(50);
    live.SetValue(10);
    int liveEvents = 0;
    live.OnChanged([&]() { ++liveEvents; });
    Layout content;
    content.Add(live, LayoutSizing::Expand);
    window.SetContent(content);
    assert(live.GetValue() == 10);
    assert(window.Show());
    assert(liveEvents == 0);
    live.SetMinimum(-25);
    live.SetMaximum(25);
    live.SetValue(20);
    assert(live.GetMinimum() == -25);
    assert(live.GetMaximum() == 25);
    assert(live.GetValue() == 20);
    assert(liveEvents == 1);

    const ControlRef retainedRef = live.GetControlRef();
    window.Close();
    assert(!window.IsShown());
    live.SetValue(-20);
    assert(live.GetValue() == -20);
    assert(window.Show());
    assert(live.GetValue() == -20);
    assert(liveEvents == 2);
    window.Close();
    app.Quit();
    assert(app.Run() == 0);

    ControlRef destroyedRef;
    {
        Slider temporary;
        destroyedRef = temporary.GetControlRef();
        assert(destroyedRef.IsValid());
        temporary.OnChanged([&]() {});
    }
    assert(!destroyedRef.IsValid());
    assert(retainedRef.IsValid());

    Application shutdownApp("com.guidexos.tests.slider-shutdown",
                            ShutdownMode::WhenLastWindowCloses);
    Window shutdownWindow(shutdownApp);
    Slider shutdownSlider;
    Layout shutdownContent;
    shutdownContent.Add(shutdownSlider);
    shutdownWindow.SetContent(shutdownContent);
    assert(shutdownWindow.Show());
    shutdownSlider.SetValue(9);
    shutdownWindow.Close();
    assert(shutdownApp.Run() == 0);
    assert(shutdownSlider.GetValue() == 9);

    return 0;
}

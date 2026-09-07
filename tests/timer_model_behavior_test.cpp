#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <chrono>
#include <stdexcept>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.tests.timer", ShutdownMode::Explicit);

    bool invalidIntervalRejected = false;
    try {
        Timer invalid(app, std::chrono::milliseconds::zero());
        (void)invalid;
    } catch (const std::invalid_argument&) {
        invalidIntervalRejected = true;
    }
    assert(invalidIntervalRejected);

    Timer timer(app, std::chrono::milliseconds(5));
    assert(timer.GetInterval() == std::chrono::milliseconds(5));
    assert(!timer.IsRunning());

    int ticks = 0;
    timer.OnTick([&]() {
        ++ticks;
        assert(timer.IsRunning());
        if (ticks == 1) {
            timer.SetInterval(std::chrono::milliseconds(7));
            assert(timer.GetInterval() == std::chrono::milliseconds(7));
        }
        if (ticks == 3) {
            timer.Stop();
            assert(!timer.IsRunning());
            app.Quit(17);
        }
    });

    timer.Start();
    assert(timer.IsRunning());
    timer.Start();
    assert(timer.IsRunning());

    Window window(app);
    assert(window.Show());
    assert(app.Run() == 17);
    assert(ticks == 3);
    assert(!timer.IsRunning());

    timer.OnTick({});
    timer.Stop();
    assert(!timer.IsRunning());
    return 0;
}

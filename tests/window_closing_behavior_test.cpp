#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <string>

using namespace guidexos::appmodel;

namespace {

void TestCancelAllowReplacementAndClear() {
    Application app("com.guidexos.tests.window-closing-basic");
    Window window(app);
    assert(!window.IsShown());
    int callbacks = 0;

    window.OnClosing([&](WindowClosingEvent& event) {
        ++callbacks;
        event.Cancel();
    });
    assert(window.Show());
    window.Close();
    assert(callbacks == 1);
    assert(window.IsShown());
    assert(app.GetLiveWindowCount() == 1);

    window.OnClosing([&](WindowClosingEvent&) { ++callbacks; });
    window.Close();
    assert(callbacks == 2);
    assert(!window.IsShown());
    assert(app.GetLiveWindowCount() == 0);
    window.Close();
    assert(callbacks == 2);

    assert(window.Show());
    window.OnClosing({});
    window.Close();
    assert(callbacks == 2);
    assert(!window.IsShown());
}

void TestRecursiveSelfCloseAndReentrantUpdates() {
    Application app("com.guidexos.tests.window-closing-reentrant");
    Window window(app);
    Label status("before");
    TextBox editor("before");
    Layout content;
    content.Add(status);
    content.Add(editor);
    window.SetContent(content);

    int callbacks = 0;
    window.OnClosing([&](WindowClosingEvent&) {
        ++callbacks;
        status.SetText("updated");
        editor.SetText("edited");
        window.Close();
    });

    assert(window.Show());
    window.Close();
    assert(callbacks == 1);
    assert(!window.IsShown());
    assert(status.GetText() == "updated");
    assert(editor.GetText() == "edited");
}

void TestCloseOtherWindowAndReopen() {
    Application app("com.guidexos.tests.window-closing-multiple");
    Window first(app);
    Window second(app);
    int firstCallbacks = 0;
    int secondCallbacks = 0;

    second.OnClosing([&](WindowClosingEvent&) { ++secondCallbacks; });
    first.OnClosing([&](WindowClosingEvent&) {
        ++firstCallbacks;
        second.Close();
    });

    assert(first.Show());
    assert(second.Show());
    first.Close();
    assert(firstCallbacks == 1);
    assert(secondCallbacks == 1);
    assert(!first.IsShown());
    assert(!second.IsShown());
    assert(app.GetLiveWindowCount() == 0);

    assert(first.Show());
    first.Close();
    assert(firstCallbacks == 2);
}

void TestCallbackReplacementDuringDispatch() {
    Application app("com.guidexos.tests.window-closing-callbacks");
    Window window(app);
    int firstCallbacks = 0;
    int replacementCallbacks = 0;

    window.OnClosing([&](WindowClosingEvent& event) {
        ++firstCallbacks;
        window.OnClosing([&](WindowClosingEvent&) { ++replacementCallbacks; });
        event.Cancel();
    });

    assert(window.Show());
    window.Close();
    assert(firstCallbacks == 1);
    assert(replacementCallbacks == 0);
    assert(window.IsShown());

    window.Close();
    assert(replacementCallbacks == 1);
    assert(!window.IsShown());
}

void TestShutdownModes() {
    {
        Application app("com.guidexos.tests.window-closing-default");
        Window window(app);
        window.OnClosing([](WindowClosingEvent& event) { event.Cancel(); });
        assert(window.Show());
        window.Close();
        assert(window.IsShown());
        assert(app.GetLiveWindowCount() == 1);
        window.OnClosing({});
        window.Close();
        assert(!window.IsShown());
        assert(app.GetLiveWindowCount() == 0);
    }

    {
        Application app("com.guidexos.tests.window-closing-explicit",
                        ShutdownMode::Explicit);
        Window window(app);
        int callbacks = 0;
        window.OnClosing([&](WindowClosingEvent& event) {
            ++callbacks;
            event.Cancel();
        });
        assert(window.Show());
        window.Close();
        assert(callbacks == 1);
        assert(window.IsShown());
        assert(app.GetLiveWindowCount() == 1);
        app.Quit(19);
        assert(app.Run() == 19);
        assert(!window.IsShown());
        assert(callbacks == 1);
    }

    {
        Application app("com.guidexos.tests.window-closing-quit",
                        ShutdownMode::Explicit);
        Window window(app);
        window.OnClosing([&](WindowClosingEvent&) { app.Quit(23); });
        assert(window.Show());
        window.Close();
        assert(!window.IsShown());
        assert(app.Run() == 23);
    }
}

} // namespace

int main() {
    TestCancelAllowReplacementAndClear();
    TestRecursiveSelfCloseAndReentrantUpdates();
    TestCloseOtherWindowAndReopen();
    TestCallbackReplacementDuringDispatch();
    TestShutdownModes();
    return 0;
}

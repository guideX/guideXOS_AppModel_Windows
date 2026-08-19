#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <memory>
#include <stdexcept>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.tests.lifecycle");
    assert(app.GetShutdownMode() == ShutdownMode::WhenLastWindowCloses);
    assert(app.GetLiveWindowCount() == 0);

    Window primary(app);
    Window secondary(app);
    Label primaryLabel("primary");
    Label secondaryLabel("secondary");
    Button openSecondary("open");
    Button closePrimary("close primary");
    Button closeSecondaryFromPrimary("close secondary");
    Button closeSecondary("close secondary");
    Button updateOther("update other");

    Layout primaryLayout;
    primaryLayout.Add(primaryLabel);
    primaryLayout.Add(openSecondary);
    primaryLayout.Add(closePrimary);
    primaryLayout.Add(closeSecondaryFromPrimary);
    primary.SetContent(primaryLayout);

    Layout secondaryLayout;
    secondaryLayout.Add(secondaryLabel);
    secondaryLayout.Add(closeSecondary);
    secondaryLayout.Add(updateOther);
    secondary.SetContent(secondaryLayout);

    int openCount = 0;
    openSecondary.OnClick([&]() {
        if (!secondary.IsShown()) {
            ++openCount;
            secondary.Show();
        }
    });
    closePrimary.OnClick([&]() { primary.Close(); });
    closeSecondaryFromPrimary.OnClick([&]() { secondary.Close(); });
    closeSecondary.OnClick([&]() { secondary.Close(); });
    updateOther.OnClick([&]() { primaryLabel.SetText("updated by secondary"); });

    assert(primary.Show());
    assert(primary.Show());
    assert(app.GetLiveWindowCount() == 1);

    openSecondary.Click();
    openSecondary.Click();
    assert(openCount == 1);
    assert(secondary.IsShown());
    assert(app.GetLiveWindowCount() == 2);

    updateOther.Click();
    assert(primaryLabel.GetText() == "updated by secondary");

    closeSecondary.Click();
    assert(!secondary.IsShown());
    assert(primary.IsShown());
    assert(app.GetLiveWindowCount() == 1);

    closeSecondary.Click();
    assert(app.GetLiveWindowCount() == 1);
    assert(secondary.Show());
    assert(app.GetLiveWindowCount() == 2);

    closeSecondaryFromPrimary.Click();
    assert(!secondary.IsShown());
    assert(primary.IsShown());
    assert(app.GetLiveWindowCount() == 1);
    assert(secondary.Show());
    assert(app.GetLiveWindowCount() == 2);

    closePrimary.Click();
    assert(!primary.IsShown());
    assert(secondary.IsShown());
    assert(app.GetLiveWindowCount() == 1);

    // A model control remains usable after its parent native window closes.
    secondaryLabel.SetText("still attached to the model");
    assert(secondaryLabel.GetText() == "still attached to the model");

    // A Button keeps a callback only while its public object is alive.
    std::weak_ptr<int> callbackToken;
    {
        Button temporary("temporary");
        auto token = std::make_shared<int>(1);
        callbackToken = token;
        temporary.OnClick([token]() {});
    }
    assert(callbackToken.expired());

    // Duplicate controls and cross-Application moves are rejected without
    // corrupting the original layout.
    bool duplicateRejected = false;
    try {
        primaryLayout.Add(primaryLabel);
    } catch (const std::logic_error&) {
        duplicateRejected = true;
    }
    assert(duplicateRejected);

    Application otherApp("com.guidexos.tests.other-lifecycle");
    Window otherWindow(otherApp);
    bool crossApplicationRejected = false;
    try {
        otherWindow.SetContent(primaryLayout);
    } catch (const std::logic_error&) {
        crossApplicationRejected = true;
    }
    assert(crossApplicationRejected);
    assert(otherWindow.Show());
    assert(otherApp.GetLiveWindowCount() == 1);
    otherWindow.Close();
    assert(otherApp.GetLiveWindowCount() == 0);
    assert(otherWindow.Show());
    otherWindow.Close();
    assert(otherApp.GetLiveWindowCount() == 0);

    // Text input dispatch updates the logical value before callbacks run,
    // supports cross-control updates, and remains safe when callbacks close
    // both their own and another native window.
    Application textApp("com.guidexos.tests.text-lifecycle");
    Window textPrimary(textApp);
    Window textSecondary(textApp);
    Label textPrimaryStatus("primary ready");
    Label textSecondaryStatus("secondary ready");
    TextBox primaryInput("retained");
    TextBox secondaryInput("secondary");

    Layout textPrimaryLayout;
    textPrimaryLayout.Add(textPrimaryStatus);
    textPrimaryLayout.Add(primaryInput);
    textPrimary.SetContent(textPrimaryLayout);

    Layout textSecondaryLayout;
    textSecondaryLayout.Add(textSecondaryStatus);
    textSecondaryLayout.Add(secondaryInput);
    textSecondary.SetContent(textSecondaryLayout);

    int primaryTextEvents = 0;
    int secondaryTextEvents = 0;
    primaryInput.OnTextChanged([&](const std::string& text) {
        ++primaryTextEvents;
        assert(primaryInput.GetText() == text);
        textPrimaryStatus.SetText(text);
        if (text == "close") {
            textPrimary.Close();
            textPrimary.Close();
            textSecondary.Close();
            textSecondary.Close();
        } else {
            secondaryInput.SetText("mirror: " + text);
        }
    });
    secondaryInput.OnTextChanged([&](const std::string& text) {
        ++secondaryTextEvents;
        assert(secondaryInput.GetText() == text);
        textSecondaryStatus.SetText(text);
    });

    assert(textPrimary.Show());
    assert(textSecondary.Show());
    primaryInput.SetText("hello");
    assert(primaryTextEvents == 1);
    assert(secondaryTextEvents == 1);
    assert(secondaryInput.GetText() == "mirror: hello");
    primaryInput.SetText("hello");
    assert(primaryTextEvents == 1);
    primaryInput.SetText("close");
    assert(primaryTextEvents == 2);
    assert(!textPrimary.IsShown());
    assert(!textSecondary.IsShown());
    assert(textApp.GetLiveWindowCount() == 0);

    // Logical state, callbacks, and C++ objects survive native detachment.
    primaryInput.SetText("detached");
    assert(primaryInput.GetText() == "detached");
    assert(primaryTextEvents == 3);
    assert(textPrimary.Show());
    assert(textSecondary.Show());
    assert(primaryInput.GetText() == "detached");
    assert(secondaryInput.GetText() == "mirror: detached");
    textPrimary.Close();
    textSecondary.Close();
    assert(textApp.Run() == 0);

    // Closing the final shown window queues the normal policy-driven exit.
    secondary.Close();
    assert(app.GetLiveWindowCount() == 0);
    assert(app.Run() == 0);

    // Show after Application shutdown is rejected deterministically while
    // model reads and writes remain ordinary C++ operations.
    assert(!secondary.Show());
    secondaryLabel.SetText("after shutdown");
    assert(secondaryLabel.GetText() == "after shutdown");

    // Explicit mode keeps the process alive after the last window closes and
    // exits only when the application requests it.
    Application explicitApp("com.guidexos.tests.explicit-lifecycle",
                            ShutdownMode::Explicit);
    Window explicitWindow(explicitApp);
    assert(explicitWindow.Show());
    explicitWindow.Close();
    assert(explicitApp.GetLiveWindowCount() == 0);
    explicitApp.Quit(7);
    assert(explicitApp.Run() == 7);
    return 0;
}

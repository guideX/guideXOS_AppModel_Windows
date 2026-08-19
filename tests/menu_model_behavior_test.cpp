#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace guidexos::appmodel;

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (false)

int main() {
    MenuBar empty;
    assert(empty.GetMenuCount() == 0);

    Menu file("&File");
    Menu recent("&Recent");
    MenuItem newItem("&New");
    MenuItem duplicateNew("&New");
    MenuItem alpha("Alpha");
    MenuItem beta("Beta");
    file.Add(newItem);
    file.Add(recent);
    recent.Add(alpha);
    recent.AddSeparator();
    recent.Add(beta);
    assert(file.GetEntryCount() == 2);
    assert(recent.GetEntryCount() == 3);

    MenuBar bar;
    bar.Add(file);
    assert(bar.GetMenuCount() == 1);
    assert(newItem.GetText() == "&New");
    assert(newItem.IsEnabled());
    assert(!newItem.IsChecked());

    bool rejected = false;
    try {
        file.Add(newItem);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        bar.Add(file);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        recent.Add(file);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);

    Menu cycle("Cycle");
    Menu child("Child");
    cycle.Add(child);
    rejected = false;
    try {
        child.Add(cycle);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);

    // Duplicate labels remain separate model identities.
    file.Add(duplicateNew);
    int firstInvocations = 0;
    int secondInvocations = 0;
    newItem.OnInvoked([&]() { ++firstInvocations; });
    duplicateNew.OnInvoked([&]() { ++secondInvocations; });

    Application app("com.guidexos.tests.menu-model");
    Window window(app);
    window.SetMenuBar(bar);
    newItem.Invoke();
    duplicateNew.Invoke();
    assert(firstInvocations == 1);
    assert(secondInvocations == 1);

    newItem.SetEnabled(false);
    newItem.Invoke();
    assert(firstInvocations == 1);
    newItem.SetEnabled(true);
    newItem.SetChecked(true);
    assert(newItem.IsChecked());

    // Callback replacement and clearing are synchronous and deterministic.
    newItem.OnInvoked([&]() { ++firstInvocations; });
    newItem.Invoke();
    assert(firstInvocations == 2);
    newItem.OnInvoked({});
    newItem.Invoke();
    assert(firstInvocations == 2);

    Label status("ready");
    TextBox input;
    CheckBox option;
    ComboBox mode;
    mode.AddItem("Standard");
    mode.AddItem("Advanced");
    MenuItem reentrant("Reentrant");
    file.Add(reentrant);
    bool callbackSawEnabled = false;
    reentrant.OnInvoked([&]() {
        callbackSawEnabled = reentrant.IsEnabled();
        status.SetText("invoked");
        input.SetText("updated");
        option.SetChecked(true);
        mode.SetSelectedIndex(1);
        newItem.SetEnabled(false);
        reentrant.SetEnabled(false);
        reentrant.SetText("Reentrant changed");
    });
    reentrant.Invoke();
    assert(callbackSawEnabled);
    assert(status.GetText() == "invoked");
    assert(input.GetText() == "updated");
    assert(option.IsChecked());
    assert(mode.GetSelectedIndex() == 1);
    assert(!newItem.IsEnabled());
    assert(!reentrant.IsEnabled());
    assert(reentrant.GetText() == "Reentrant changed");

    // Removing an item detaches its command identity; a stale logical or
    // native invocation cannot call its callback.
    const int beforeDetach = secondInvocations;
    assert(file.Remove(duplicateNew));
    duplicateNew.Invoke();
    assert(secondInvocations == beforeDetach);
    assert(!file.Remove(duplicateNew));

    KeyShortcut ctrlS = KeyShortcut::Ctrl('s');
    KeyShortcut ctrlShiftS = KeyShortcut::CtrlShift('S');
    assert(ctrlS.GetLetter() == 'S');
    assert(!ctrlS.HasShift());
    assert(ctrlShiftS.HasShift());
    reentrant.SetShortcut(ctrlS);
    rejected = false;
    try {
        newItem.SetShortcut(ctrlS);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);
    newItem.SetShortcut(ctrlShiftS);
    newItem.ClearShortcut();

    rejected = false;
    try {
        MenuItem invalid(std::string("\x80", 1));
        (void)invalid;
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        newItem.SetText(std::string("\xC0\xAF", 2));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    MenuBar otherBar;
    Menu otherFile("&File");
    MenuItem otherItem("&New");
    otherFile.Add(otherItem);
    otherBar.Add(otherFile);
    Window otherWindow(app);
    rejected = false;
    try {
        otherWindow.SetMenuBar(bar);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);
    otherWindow.SetMenuBar(otherBar);

    Application otherApp("com.guidexos.tests.menu-other");
    Window crossWindow(otherApp);
    rejected = false;
    try {
        crossWindow.SetMenuBar(bar);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);

    window.ClearMenuBar();
    assert(window.Show());
    window.Close();
    assert(!window.IsShown());
    assert(window.Show());
    window.Close();
    otherWindow.Close();

    // Menu callbacks can close their own and another native window without
    // invalidating command state. Callback state also survives realization
    // recreation, and an application shutdown request remains synchronous.
    Application lifecycleApp("com.guidexos.tests.menu-lifecycle",
                            ShutdownMode::Explicit);
    Window lifecyclePrimary(lifecycleApp);
    Window lifecycleSecondary(lifecycleApp);
    MenuBar lifecyclePrimaryBar;
    Menu lifecyclePrimaryMenu("File");
    MenuItem closeBoth("Close Both");
    MenuItem shutdown("Shutdown");
    lifecyclePrimaryMenu.Add(closeBoth);
    lifecyclePrimaryMenu.Add(shutdown);
    lifecyclePrimaryBar.Add(lifecyclePrimaryMenu);
    lifecyclePrimary.SetMenuBar(lifecyclePrimaryBar);
    MenuBar lifecycleSecondaryBar;
    Menu lifecycleSecondaryMenu("File");
    MenuItem secondaryCommand("Update");
    lifecycleSecondaryMenu.Add(secondaryCommand);
    lifecycleSecondaryBar.Add(lifecycleSecondaryMenu);
    lifecycleSecondary.SetMenuBar(lifecycleSecondaryBar);

    int closeBothCount = 0;
    closeBoth.OnInvoked([&]() {
        ++closeBothCount;
        lifecyclePrimary.Close();
        lifecycleSecondary.Close();
    });
    assert(lifecyclePrimary.Show());
    assert(lifecycleSecondary.Show());
    closeBoth.Invoke();
    assert(closeBothCount == 1);
    assert(!lifecyclePrimary.IsShown());
    assert(!lifecycleSecondary.IsShown());
    assert(lifecycleApp.GetLiveWindowCount() == 0);

    assert(lifecyclePrimary.Show());
    shutdown.OnInvoked([&]() { lifecycleApp.Quit(17); });
    shutdown.Invoke();
    assert(lifecycleApp.Run() == 17);
    assert(!lifecyclePrimary.IsShown());
    return 0;
}

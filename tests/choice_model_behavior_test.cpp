#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
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
    Application app("com.guidexos.tests.choice-model");

    CheckBox notifications;
    assert(notifications.GetText().empty());
    assert(!notifications.IsChecked());
    assert(notifications.IsEnabled());

    CheckBox initial("Initial", true);
    assert(initial.IsChecked());
    assert(initial.GetText() == "Initial");

    Label status;
    TextBox mirror;
    CheckBox secondary("Secondary");
    int notificationEvents = 0;
    std::vector<bool> notificationValues;
    bool callbackSawCheckedState = true;
    notifications.OnCheckedChanged([&](bool checked) {
        ++notificationEvents;
        notificationValues.push_back(checked);
        callbackSawCheckedState = callbackSawCheckedState &&
            notifications.IsChecked() == checked;
        status.SetText(checked ? "enabled" : "disabled");
        mirror.SetText(checked ? "on" : "off");
        if (checked) secondary.SetChecked(true);
    });

    notifications.SetChecked(true);
    assert(notifications.IsChecked());
    assert(notificationEvents == 1);
    assert(callbackSawCheckedState);
    assert(status.GetText() == "enabled");
    assert(mirror.GetText() == "on");
    assert(secondary.IsChecked());
    assert(notifications.GetText().empty());
    notifications.SetChecked(true);
    assert(notificationEvents == 1);
    notifications.SetChecked(false);
    assert(notificationEvents == 2);
    assert(notificationValues.size() == 2);

    int replacementEvents = 0;
    notifications.OnCheckedChanged([&](bool) { ++replacementEvents; });
    notifications.SetChecked(true);
    assert(replacementEvents == 1);
    assert(notificationEvents == 2);
    notifications.OnCheckedChanged({});
    notifications.SetChecked(false);
    assert(replacementEvents == 1);

    notifications.SetEnabled(false);
    assert(!notifications.IsEnabled());
    notifications.SetChecked(true);
    assert(notifications.IsChecked());
    notifications.SetEnabled(true);
    assert(notifications.IsEnabled());

    CheckBox selfToggle("Self");
    std::vector<bool> selfEvents;
    selfToggle.OnCheckedChanged([&](bool checked) {
        selfEvents.push_back(checked);
        if (checked) selfToggle.SetChecked(false);
    });
    selfToggle.SetChecked(true);
    assert(!selfToggle.IsChecked());
    assert(selfEvents.size() == 2);
    assert(selfEvents[0] && !selfEvents[1]);

    CheckBox disableSelf("Disable self");
    disableSelf.OnCheckedChanged([&](bool checked) {
        if (checked) disableSelf.SetEnabled(false);
    });
    disableSelf.SetChecked(true);
    assert(disableSelf.IsChecked());
    assert(!disableSelf.IsEnabled());

    ListBox choiceList;
    choiceList.AddItem("choice-list item");
    CheckBox listMutator("Mutate list");
    listMutator.OnCheckedChanged([&](bool checked) {
        if (checked) choiceList.SetSelectedIndex(0);
    });
    listMutator.SetChecked(true);
    assert(choiceList.GetSelectedIndex() == std::optional<std::size_t>(0));

    std::weak_ptr<int> callbackToken;
    {
        CheckBox temporary("temporary");
        auto token = std::make_shared<int>(1);
        callbackToken = token;
        temporary.OnCheckedChanged([token](bool) {});
        token.reset();
        temporary.OnCheckedChanged({});
        assert(callbackToken.expired());
    }
    assert(callbackToken.expired());

    const std::string accented = "caf\xC3\xA9";
    const std::string nonLatin = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    const std::string supplementary = "rocket \xF0\x9F\x9A\x80";
    CheckBox unicode(accented);
    unicode.SetText(nonLatin);
    assert(unicode.GetText() == nonLatin);
    unicode.SetText(supplementary);
    assert(unicode.GetText() == supplementary);
    unicode.SetText({});
    assert(unicode.GetText().empty());
    bool invalidRejected = false;
    try {
        CheckBox invalid(std::string("\xC0\xAF", 2));
        (void)invalid;
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);

    RadioGroup theme;
    RadioButton system("System");
    RadioButton light("Light");
    RadioButton dark("Dark");
    assert(!theme.GetSelectedIndex());
    assert(theme.GetMemberCount() == 0);
    theme.Add(system);
    theme.Add(light);
    theme.Add(dark);
    assert(theme.GetMemberCount() == 3);
    assert(theme.Contains(system));
    assert(!theme.GetSelectedIndex());
    assert(!system.IsSelected() && !light.IsSelected() && !dark.IsSelected());
    assert(ThrowsLogicError([&]() { theme.Add(system); }));
    RadioGroup other;
    assert(ThrowsLogicError([&]() { other.Add(system); }));

    std::vector<std::string> selectionEvents;
    bool callbacksSawDark = true;
    bool observeDarkTransition = false;
    Label radioStatus;
    TextBox radioText;
    CheckBox radioSideEffect;
    system.OnSelectedChanged([&](bool selected) {
        selectionEvents.push_back(selected ? "system+" : "system-");
        if (observeDarkTransition) {
            callbacksSawDark = callbacksSawDark &&
                theme.GetSelectedIndex() == std::optional<std::size_t>(2);
        }
    });
    dark.OnSelectedChanged([&](bool selected) {
        selectionEvents.push_back(selected ? "dark+" : "dark-");
        if (selected) {
            radioStatus.SetText("dark selected");
            radioText.SetText("Dark");
            radioSideEffect.SetChecked(true);
        }
        if (observeDarkTransition) {
            callbacksSawDark = callbacksSawDark &&
                theme.GetSelectedIndex() == std::optional<std::size_t>(2);
        }
    });
    theme.Select(system);
    assert(theme.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(system.IsSelected());
    assert(selectionEvents.size() == 1 && selectionEvents[0] == "system+");
    theme.Select(system);
    assert(selectionEvents.size() == 1);
    observeDarkTransition = true;
    theme.Select(dark);
    assert(theme.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(!system.IsSelected() && dark.IsSelected());
    assert(selectionEvents.size() == 3);
    assert(selectionEvents[1] == "system-");
    assert(selectionEvents[2] == "dark+");
    assert(callbacksSawDark);
    assert(radioStatus.GetText() == "dark selected");
    assert(radioText.GetText() == "Dark");
    assert(radioSideEffect.IsChecked());

    theme.ClearSelection();
    assert(!theme.GetSelectedIndex());
    assert(!dark.IsSelected());
    assert(selectionEvents.back() == "dark-");
    theme.ClearSelection();
    assert(selectionEvents.size() == 4);

    RadioGroup reentrant;
    RadioButton reentrantA("A");
    RadioButton reentrantB("B");
    RadioButton reentrantC("C");
    reentrant.Add(reentrantA);
    reentrant.Add(reentrantB);
    reentrant.Add(reentrantC);
    std::vector<std::string> reentrantEvents;
    reentrantA.OnSelectedChanged([&](bool selected) {
        reentrantEvents.push_back(selected ? "A+" : "A-");
        if (!selected) reentrant.Select(reentrantC);
    });
    reentrantB.OnSelectedChanged([&](bool selected) {
        reentrantEvents.push_back(selected ? "B+" : "B-");
    });
    reentrantC.OnSelectedChanged([&](bool selected) {
        reentrantEvents.push_back(selected ? "C+" : "C-");
    });
    reentrant.Select(reentrantA);
    reentrantEvents.clear();
    reentrant.Select(reentrantB);
    assert(reentrant.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(reentrantEvents.size() == 4);
    assert(reentrantEvents[0] == "A-");
    assert(reentrantEvents[1] == "B-");
    assert(reentrantEvents[2] == "C+");
    assert(reentrantEvents[3] == "B+");

    RadioGroup callbackSelect;
    RadioButton callbackA("callback A");
    RadioButton callbackB("callback B");
    RadioButton callbackC("callback C");
    callbackSelect.Add(callbackA);
    callbackSelect.Add(callbackB);
    callbackSelect.Add(callbackC);
    std::vector<std::string> callbackSelectEvents;
    callbackB.OnSelectedChanged([&](bool selected) {
        callbackSelectEvents.push_back(selected ? "B+" : "B-");
        if (selected) callbackSelect.Select(callbackC);
    });
    callbackC.OnSelectedChanged([&](bool selected) {
        callbackSelectEvents.push_back(selected ? "C+" : "C-");
    });
    callbackSelect.Select(callbackA);
    callbackSelectEvents.clear();
    callbackSelect.Select(callbackB);
    assert(callbackSelect.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(callbackSelectEvents.size() == 3);
    assert(callbackSelectEvents[0] == "B+");
    assert(callbackSelectEvents[1] == "B-");
    assert(callbackSelectEvents[2] == "C+");

    CheckBox crossChoice("cross choice");
    crossChoice.OnCheckedChanged([&](bool checked) {
        if (checked) callbackSelect.Select(callbackA);
    });
    crossChoice.SetChecked(true);
    assert(callbackSelect.GetSelectedIndex() == std::optional<std::size_t>(0));

    RadioGroup independent;
    RadioButton independentA("English");
    RadioButton independentB("日本語");
    independent.Add(independentA);
    independent.Add(independentB);
    independent.Select(independentB);
    assert(independent.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(callbackSelect.GetSelectedIndex() == std::optional<std::size_t>(0));

    RadioGroup removalGroup;
    RadioButton removalMember("removal");
    int removalFalseEvents = 0;
    removalMember.OnSelectedChanged([&](bool selected) {
        if (!selected) ++removalFalseEvents;
    });
    removalGroup.Add(removalMember);
    removalGroup.Select(removalMember);
    removalGroup.Remove(removalMember);
    assert(!removalGroup.GetSelectedIndex());
    assert(!removalMember.IsSelected());
    assert(removalFalseEvents == 1);

    theme.Remove(dark);
    assert(theme.GetMemberCount() == 2);
    assert(!theme.GetSelectedIndex());
    assert(!theme.Contains(dark));
    theme.Add(dark);
    theme.Select(dark);
    {
        RadioButton temporary("temporary member");
        theme.Add(temporary);
        theme.Select(temporary);
        assert(theme.GetSelectedIndex() == std::optional<std::size_t>(3));
    }
    assert(!theme.GetSelectedIndex());
    assert(!dark.IsSelected());
    theme.Select(system);
    assert(theme.GetSelectedIndex() == std::optional<std::size_t>(0));
    {
        RadioGroup temporaryGroup;
        RadioButton temporary("temporary group member");
        temporaryGroup.Add(temporary);
        temporaryGroup.Select(temporary);
        assert(temporary.IsSelected());
    }
    assert(system.IsSelected());

    Application otherApp("com.guidexos.tests.choice-other-model");
    Window firstWindow(app);
    Layout firstLayout;
    firstLayout.Add(system);
    firstWindow.SetContent(firstLayout);
    RadioButton otherApplicationRadio("other app");
    Window secondWindow(otherApp);
    Layout secondLayout;
    secondLayout.Add(otherApplicationRadio);
    secondWindow.SetContent(secondLayout);
    assert(ThrowsLogicError([&]() { theme.Add(otherApplicationRadio); }));

    RadioButton radioUnicode(accented);
    radioUnicode.SetText(nonLatin);
    radioUnicode.SetText(supplementary);
    radioUnicode.SetText({});
    invalidRejected = false;
    try {
        radioUnicode.SetText(std::string("\x80", 1));
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);

    return 0;
}

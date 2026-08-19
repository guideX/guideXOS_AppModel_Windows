#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>
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

bool ThrowsOutOfRange(const auto& operation) {
    try {
        operation();
    } catch (const std::out_of_range&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    Application app("com.guidexos.tests.combobox-model");
    ComboBox mode;

    assert(mode.GetItemCount() == 0);
    assert(!mode.GetSelectedIndex());
    assert(mode.IsEnabled());
    assert(ThrowsOutOfRange([&]() { mode.GetItem(0); }));
    assert(ThrowsOutOfRange([&]() { mode.SetItem(0, "missing"); }));
    assert(ThrowsOutOfRange([&]() { mode.RemoveItem(0); }));
    assert(ThrowsOutOfRange([&]() { mode.InsertItem(1, "missing"); }));
    assert(ThrowsOutOfRange([&]() { mode.SetSelectedIndex(0); }));

    const std::string accented = "caf\xC3\xA9";
    const std::string nonLatin =
        "\xE6\x97\xA5" "\xE6\x9C\xAC" "\xE8\xAA\x9E";
    const std::string supplementary = "rocket \xF0\x9F\x9A\x80";
    mode.AddItem("first");
    mode.AddItem(accented);
    mode.AddItem(nonLatin);
    mode.AddItem(supplementary);
    mode.AddItem("");
    mode.AddItem("duplicate");
    mode.AddItem("duplicate");
    assert(mode.GetItemCount() == 7);
    assert(mode.GetItem(1) == accented);
    assert(mode.GetItem(2) == nonLatin);
    assert(mode.GetItem(3) == supplementary);
    assert(mode.GetItem(4).empty());
    assert(mode.GetItem(5) == mode.GetItem(6));

    bool invalidRejected = false;
    try {
        mode.AddItem(std::string("\x80", 1));
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);
    assert(mode.GetItemCount() == 7);
    invalidRejected = false;
    try {
        mode.SetItem(0, std::string("\xC0", 1));
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);
    assert(mode.GetItem(0) == "first");

    mode.InsertItem(0, "zero");
    mode.InsertItem(4, "middle");
    mode.InsertItem(mode.GetItemCount(), "last");
    assert(mode.GetItem(0) == "zero");
    assert(mode.GetItem(4) == "middle");
    assert(mode.GetItem(mode.GetItemCount() - 1) == "last");
    mode.SetItem(4, "replaced");
    assert(mode.GetItem(4) == "replaced");
    mode.RemoveItem(4);
    assert(mode.GetItem(4) == supplementary);
    mode.RemoveItem(0);
    mode.RemoveItem(mode.GetItemCount() - 1);

    int eventCount = 0;
    std::vector<std::optional<std::size_t>> events;
    bool callbackSawUpdatedModel = true;
    Label selectionLabel;
    TextBox selectionText;
    CheckBox selectionCheck;
    RadioButton selectionRadio;
    RadioGroup selectionGroup;
    selectionGroup.Add(selectionRadio);
    ComboBox other;
    other.AddItem("other-0");
    other.AddItem("other-1");
    int otherEvents = 0;
    other.OnSelectionChanged([&](std::optional<std::size_t>) { ++otherEvents; });

    mode.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++eventCount;
        events.push_back(index);
        callbackSawUpdatedModel = callbackSawUpdatedModel &&
            index == mode.GetSelectedIndex();
        if (index) {
            const std::string item = mode.GetItem(*index);
            selectionLabel.SetText(item);
            selectionText.SetText(item);
            selectionCheck.SetChecked(true);
            selectionGroup.Select(selectionRadio);
            other.SetSelectedIndex(1);
        } else {
            selectionLabel.SetText("none");
            selectionText.SetText({});
            selectionCheck.SetChecked(false);
            selectionGroup.ClearSelection();
        }
    });

    mode.SetSelectedIndex(2);
    assert(mode.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(eventCount == 1);
    assert(selectionLabel.GetText() == mode.GetItem(2));
    assert(selectionText.GetText() == mode.GetItem(2));
    assert(selectionCheck.IsChecked());
    assert(selectionGroup.GetSelectedIndex() == std::optional<std::size_t>(0));
    assert(other.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(otherEvents == 1);
    mode.SetSelectedIndex(2);
    assert(eventCount == 1);
    mode.SetItem(2, "changed text");
    assert(mode.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(eventCount == 1);

    mode.InsertItem(0, "before selected");
    assert(mode.GetSelectedIndex() == std::optional<std::size_t>(3));
    assert(eventCount == 1);
    mode.RemoveItem(0);
    assert(mode.GetSelectedIndex() == std::optional<std::size_t>(2));
    mode.RemoveItem(1);
    assert(mode.GetSelectedIndex() == std::optional<std::size_t>(1));
    mode.RemoveItem(1);
    assert(!mode.GetSelectedIndex());
    assert(eventCount == 2);
    assert(!events.back());
    assert(!selectionCheck.IsChecked());
    assert(!selectionGroup.GetSelectedIndex());
    mode.SetSelectedIndex(std::nullopt);
    assert(eventCount == 2);
    assert(callbackSawUpdatedModel);

    int replacementEvents = 0;
    mode.OnSelectionChanged([&](std::optional<std::size_t>) {
        ++replacementEvents;
    });
    mode.SetSelectedIndex(0);
    assert(replacementEvents == 1);
    mode.OnSelectionChanged({});
    mode.SetSelectedIndex(std::nullopt);
    assert(replacementEvents == 1);

    ComboBox reentrant;
    reentrant.AddItem("a");
    reentrant.AddItem("b");
    reentrant.AddItem("c");
    std::vector<std::optional<std::size_t>> reentrantEvents;
    reentrant.OnSelectionChanged([&](std::optional<std::size_t> index) {
        reentrantEvents.push_back(index);
        if (index == std::optional<std::size_t>(0)) reentrant.SetSelectedIndex(1);
    });
    reentrant.SetSelectedIndex(0);
    assert(reentrant.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(reentrantEvents.size() == 2);
    assert(reentrantEvents[0] == std::optional<std::size_t>(0));
    assert(reentrantEvents[1] == std::optional<std::size_t>(1));

    ComboBox removeOther;
    removeOther.AddItem("0");
    removeOther.AddItem("1");
    removeOther.AddItem("2");
    int removeOtherEvents = 0;
    removeOther.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++removeOtherEvents;
        if (index == std::optional<std::size_t>(2)) removeOther.RemoveItem(0);
    });
    removeOther.SetSelectedIndex(2);
    assert(removeOther.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(removeOtherEvents == 1);

    ComboBox removeSelected;
    removeSelected.AddItem("0");
    removeSelected.AddItem("1");
    std::vector<std::optional<std::size_t>> removeSelectedEvents;
    removeSelected.OnSelectionChanged([&](std::optional<std::size_t> index) {
        removeSelectedEvents.push_back(index);
        if (index == std::optional<std::size_t>(1)) removeSelected.RemoveItem(1);
    });
    removeSelected.SetSelectedIndex(1);
    assert(!removeSelected.GetSelectedIndex());
    assert(removeSelected.GetItemCount() == 1);
    assert(removeSelectedEvents.size() == 2);
    assert(!removeSelectedEvents.back());

    ComboBox clearFromCallback;
    clearFromCallback.AddItem("clear me");
    int clearEvents = 0;
    clearFromCallback.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++clearEvents;
        if (index) clearFromCallback.ClearItems();
    });
    clearFromCallback.SetSelectedIndex(0);
    assert(clearFromCallback.GetItemCount() == 0);
    assert(!clearFromCallback.GetSelectedIndex());
    assert(clearEvents == 2);

    ComboBox addFromCallback;
    addFromCallback.AddItem("initial");
    addFromCallback.OnSelectionChanged([&](std::optional<std::size_t> index) {
        if (index) addFromCallback.AddItem("added from callback");
    });
    addFromCallback.SetSelectedIndex(0);
    assert(addFromCallback.GetItemCount() == 2);
    assert(addFromCallback.GetSelectedIndex() == std::optional<std::size_t>(0));

    mode.SetEnabled(false);
    assert(!mode.IsEnabled());
    mode.SetSelectedIndex(1);
    assert(mode.GetSelectedIndex() == std::optional<std::size_t>(1));
    mode.SetEnabled(true);
    assert(mode.IsEnabled());

    mode.ClearItems();
    assert(mode.GetItemCount() == 0);
    assert(!mode.GetSelectedIndex());
    mode.ClearItems();
    return 0;
}

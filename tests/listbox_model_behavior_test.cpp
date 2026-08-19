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
    Application app("com.guidexos.tests.listbox-model");
    ListBox list;

    assert(list.GetItemCount() == 0);
    assert(!list.GetSelectedIndex());
    assert(ThrowsOutOfRange([&]() { list.GetItem(0); }));
    assert(ThrowsOutOfRange([&]() { list.SetItem(0, "missing"); }));
    assert(ThrowsOutOfRange([&]() { list.RemoveItem(0); }));
    assert(ThrowsOutOfRange([&]() { list.InsertItem(1, "missing"); }));
    assert(ThrowsOutOfRange([&]() { list.SetSelectedIndex(0); }));

    const std::string accented = "caf\xC3\xA9";
    const std::string nonLatin =
        "\xE6\x97\xA5" "\xE6\x9C\xAC" "\xE8\xAA\x9E";
    const std::string supplementary = "rocket \xF0\x9F\x9A\x80";
    list.AddItem("first");
    list.AddItem(accented);
    list.AddItem(nonLatin);
    list.AddItem(supplementary);
    list.AddItem("");
    list.AddItem("duplicate");
    list.AddItem("duplicate");
    assert(list.GetItemCount() == 7);
    assert(list.GetItem(1) == accented);
    assert(list.GetItem(2) == nonLatin);
    assert(list.GetItem(3) == supplementary);
    assert(list.GetItem(4).empty());
    assert(list.GetItem(5) == list.GetItem(6));

    bool invalidRejected = false;
    try {
        list.AddItem(std::string("\x80", 1));
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);
    assert(list.GetItemCount() == 7);

    list.InsertItem(0, "zero");
    list.InsertItem(4, "middle");
    list.InsertItem(list.GetItemCount(), "last");
    assert(list.GetItem(0) == "zero");
    assert(list.GetItem(4) == "middle");
    assert(list.GetItem(list.GetItemCount() - 1) == "last");
    list.SetItem(4, "replaced");
    assert(list.GetItem(4) == "replaced");
    list.RemoveItem(4);
    assert(list.GetItem(4) == supplementary);
    list.RemoveItem(0);
    list.RemoveItem(list.GetItemCount() - 1);

    int eventCount = 0;
    std::vector<std::optional<std::size_t>> events;
    bool callbackSawUpdatedModel = true;
    Label selectionLabel;
    TextBox selectionText;
    list.OnSelectionChanged([&](std::optional<std::size_t> index) {
        ++eventCount;
        events.push_back(index);
        callbackSawUpdatedModel = callbackSawUpdatedModel &&
            index == list.GetSelectedIndex();
        if (index) {
            selectionLabel.SetText(list.GetItem(*index));
            selectionText.SetText(list.GetItem(*index));
        } else {
            selectionLabel.SetText("none");
            selectionText.SetText({});
        }
    });

    list.SetSelectedIndex(2);
    assert(list.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(eventCount == 1);
    assert(selectionLabel.GetText() == list.GetItem(2));
    assert(selectionText.GetText() == list.GetItem(2));
    list.SetSelectedIndex(2);
    assert(eventCount == 1);
    list.SetItem(2, "changed text");
    assert(list.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(eventCount == 1);

    list.InsertItem(0, "before selected");
    assert(list.GetSelectedIndex() == std::optional<std::size_t>(3));
    assert(eventCount == 1);
    list.RemoveItem(0);
    assert(list.GetSelectedIndex() == std::optional<std::size_t>(2));
    assert(eventCount == 1);
    list.RemoveItem(1);
    assert(list.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(eventCount == 1);
    list.RemoveItem(1);
    assert(!list.GetSelectedIndex());
    assert(eventCount == 2);
    assert(!events.back());
    list.SetSelectedIndex(std::nullopt);
    assert(eventCount == 2);
    assert(callbackSawUpdatedModel);

    int replacementEvents = 0;
    list.OnSelectionChanged([&](std::optional<std::size_t>) { ++replacementEvents; });
    list.SetSelectedIndex(0);
    assert(replacementEvents == 1);
    list.OnSelectionChanged({});
    list.SetSelectedIndex(std::nullopt);
    assert(replacementEvents == 1);

    list.SetSelectedIndex(0);
    assert(eventCount == 2);

    ListBox reentrant;
    reentrant.AddItem("a");
    reentrant.AddItem("b");
    reentrant.AddItem("c");
    std::vector<std::optional<std::size_t>> reentrantEvents;
    reentrant.OnSelectionChanged([&](std::optional<std::size_t> index) {
        reentrantEvents.push_back(index);
        if (index == std::optional<std::size_t>(0)) {
            reentrant.SetSelectedIndex(1);
        }
    });
    reentrant.SetSelectedIndex(0);
    assert(reentrant.GetSelectedIndex() == std::optional<std::size_t>(1));
    assert(reentrantEvents.size() == 2);
    assert(reentrantEvents[0] == std::optional<std::size_t>(0));
    assert(reentrantEvents[1] == std::optional<std::size_t>(1));

    ListBox removeOther;
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

    ListBox removeSelected;
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

    ListBox clearFromCallback;
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

    std::weak_ptr<int> callbackToken;
    {
        ListBox temporary;
        auto token = std::make_shared<int>(1);
        callbackToken = token;
        temporary.OnSelectionChanged([token](std::optional<std::size_t>) {});
        token.reset();
        temporary.OnSelectionChanged({});
        assert(callbackToken.expired());
    }
    assert(callbackToken.expired());

    list.ClearItems();
    assert(list.GetItemCount() == 0);
    assert(!list.GetSelectedIndex());
    assert(eventCount == 2);
    list.ClearItems();
    assert(eventCount == 2);
    return 0;
}

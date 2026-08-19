#include <guidexos/appmodel/appmodel.hpp>

#include <optional>
#include <string>

using namespace guidexos::appmodel;

namespace {

const std::string kCafe = "caf\xC3\xA9.txt";
const std::string kJapanese =
    "\xE6\x97\xA5" "\xE6\x9C\xAC" "\xE8\xAA\x9E";
const std::string kRocket = "rocket \xF0\x9F\x9A\x80";

} // namespace

int main() {
    Application app("com.guidexos.samples.listboxapp");

    Window window(app);
    window.SetTitle("guideXOS ListBox Sample");
    window.SetSize(900, 700);

    Label itemsLabel("Items");
    ListBox items;
    Label newItemLabel("New item:");
    TextBox newItem;
    Button addButton("Add");
    Button removeButton("Remove Selected");
    Button clearButton("Clear");
    Label selectedLabel;
    Label countLabel;

    items.AddItem("README.md");
    items.AddItem("src");
    items.AddItem("docs");
    items.AddItem(kCafe);
    items.AddItem(kJapanese);
    items.AddItem(kRocket);
    items.AddItem("docs");

    const auto updateStatus = [&]() {
        const auto selected = items.GetSelectedIndex();
        if (selected) {
            selectedLabel.SetText("Selected [" + std::to_string(*selected) + "]: " +
                                 items.GetItem(*selected));
        } else {
            selectedLabel.SetText("Selected: none");
        }
        countLabel.SetText("Count: " + std::to_string(items.GetItemCount()));
    };

    items.OnSelectionChanged([&](std::optional<std::size_t>) {
        updateStatus();
    });

    addButton.OnClick([&]() {
        items.AddItem(newItem.GetText());
        newItem.SetText({});
        updateStatus();
    });

    removeButton.OnClick([&]() {
        if (const auto selected = items.GetSelectedIndex()) {
            items.RemoveItem(*selected);
        }
        updateStatus();
    });

    clearButton.OnClick([&]() {
        items.ClearItems();
        updateStatus();
    });

    Layout content;
    content.Add(itemsLabel);
    content.Add(items);
    content.Add(newItemLabel);
    content.Add(newItem);
    content.Add(addButton);
    content.Add(removeButton);
    content.Add(clearButton);
    content.Add(selectedLabel);
    content.Add(countLabel);
    window.SetContent(content);

    updateStatus();
    if (!window.Show()) return 1;
    return app.Run();
}

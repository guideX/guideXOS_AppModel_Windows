#include <guidexos/appmodel/appmodel.hpp>

#include <optional>
#include <string>

using namespace guidexos::appmodel;

namespace {

const std::string kJapanese =
    "\xE6\x97\xA5" "\xE6\x9C\xAC" "\xE8\xAA\x9E";
const std::string kRocket = "rocket \xF0\x9F\x9A\x80";

} // namespace

int main() {
    Application app("com.guidexos.samples.combobox");

    Window window(app);
    window.SetTitle("guideXOS ComboBox Demo");
    window.SetSize(760, 560);

    Label heading("ComboBox Demo");
    Label modeLabel("Mode");
    ComboBox mode;
    Label status;
    TextBox newItem;
    Button addButton("Add Item");
    Button removeButton("Remove Selected");
    Button clearButton("Clear");
    Button selectCompatibility("Select Compatibility");
    Button toggleEnabled("Disable ComboBox");

    mode.AddItem("Standard");
    mode.AddItem("Advanced");
    mode.AddItem("Compatibility");
    mode.AddItem(kJapanese);
    mode.AddItem("Standard");
    mode.AddItem(kRocket);

    const auto updateStatus = [&]() {
        if (const auto selected = mode.GetSelectedIndex()) {
            status.SetText("Selected [" + std::to_string(*selected) + "]: " +
                           mode.GetItem(*selected));
        } else {
            status.SetText("Selected: none");
        }
    };

    mode.OnSelectionChanged([&](std::optional<std::size_t>) {
        updateStatus();
    });
    mode.SetSelectedIndex(0);
    updateStatus();

    addButton.OnClick([&]() {
        const std::string text = newItem.GetText().empty()
            ? "Custom item"
            : newItem.GetText();
        mode.InsertItem(mode.GetItemCount() > 1 ? 1 : mode.GetItemCount(), text);
        newItem.SetText({});
        updateStatus();
    });
    removeButton.OnClick([&]() {
        if (const auto selected = mode.GetSelectedIndex()) {
            mode.RemoveItem(*selected);
        }
        updateStatus();
    });
    clearButton.OnClick([&]() {
        mode.ClearItems();
        updateStatus();
    });
    const auto selectCompatibilityItem = [&]() {
        for (std::size_t index = 0; index < mode.GetItemCount(); ++index) {
            if (mode.GetItem(index) == "Compatibility") {
                mode.SetSelectedIndex(index);
                return;
            }
        }
    };
    selectCompatibility.OnClick(selectCompatibilityItem);
    toggleEnabled.OnClick([&]() {
        const bool enabled = !mode.IsEnabled();
        mode.SetEnabled(enabled);
        toggleEnabled.SetText(enabled ? "Disable ComboBox" :
                                       "Enable ComboBox");
    });

    Layout content;
    content.Add(heading);
    content.Add(modeLabel);
    content.Add(mode);
    content.Add(status);
    content.Add(newItem);
    content.Add(addButton);
    content.Add(removeButton);
    content.Add(clearButton);
    content.Add(selectCompatibility);
    content.Add(toggleEnabled);
    window.SetContent(content);

    if (!window.Show()) return 1;
    return app.Run();
}

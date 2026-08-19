#include <guidexos/appmodel/appmodel.hpp>

#include <string>

using namespace guidexos::appmodel;

namespace {

const std::string kUnicodeName = "Zo\xC3\xAB \xF0\x9F\x9A\x80";
const std::string kUnicodeMessage =
    "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF "
    "\xE4\xB8\x96\xE7\x95\x8C";

} // namespace

int main() {
    Application app("com.guidexos.samples.textinputapp");

    Window window(app);
    window.SetTitle("guideXOS Text Input Sample");
    window.SetSize(900, 600);

    Label nameLabel("Name:");
    TextBox nameInput("Ada Lovelace");
    Label messageLabel("Message:");
    TextBox messageInput("Welcome to guideXOS.");
    Label previewLabel;
    Label statusLabel("Meaningful text changes: 0");
    Button unicodeButton("Insert Unicode Sample");
    Button clearButton("Clear");

    int meaningfulChanges = 0;
    const auto updatePreview = [&]() {
        previewLabel.SetText("Preview: Hello, " + nameInput.GetText() +
                             " \xE2\x80\x94 " + messageInput.GetText());
        statusLabel.SetText("Meaningful text changes: " +
                            std::to_string(meaningfulChanges));
    };

    nameInput.OnTextChanged([&](const std::string&) {
        ++meaningfulChanges;
        updatePreview();
    });
    messageInput.OnTextChanged([&](const std::string&) {
        ++meaningfulChanges;
        updatePreview();
    });

    unicodeButton.OnClick([&]() {
        nameInput.SetText(kUnicodeName);
        messageInput.SetText(kUnicodeMessage);
    });

    clearButton.OnClick([&]() {
        nameInput.SetText({});
        messageInput.SetText({});
    });

    Layout content;
    content.Add(nameLabel);
    content.Add(nameInput);
    content.Add(messageLabel);
    content.Add(messageInput);
    content.Add(previewLabel);
    content.Add(unicodeButton);
    content.Add(clearButton);
    content.Add(statusLabel);
    window.SetContent(content);

    updatePreview();

    if (!window.Show()) return 1;
    return app.Run();
}

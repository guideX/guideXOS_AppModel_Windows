#include <guidexos/appmodel/appmodel.hpp>

#include <string>

using namespace guidexos::appmodel;

namespace {

const std::string kExampleText =
    "This is editable multiline text.\n\nMultiple lines should work here.";

} // namespace

int main() {
    Application app("com.guidexos.samples.multilinetextapp");
    Window window(app);
    window.SetTitle("guideXOS Multiline Text Demo");
    window.SetSize(760, 600);

    Label heading("Multiline Text Demo");
    Label titleLabel("Title:");
    TextBox title("My Notes");
    Label bodyLabel("Body:");
    TextArea body(kExampleText);
    body.SetToolTip("Editable multiline text");
    CheckBox readOnly("Read only");
    Button copy("Copy Body");
    Button paste("Paste Body");
    Button clear("Clear");
    Button restore("Restore");
    Label characters("Characters: " + std::to_string(body.GetText().size()));
    Label status("Status: Ready");

    body.OnTextChanged([&](const std::string& value) {
        characters.SetText("Characters: " + std::to_string(value.size()));
        status.SetText("Status: Body changed");
    });
    readOnly.OnCheckedChanged([&](bool checked) {
        body.SetReadOnly(checked);
        status.SetText(checked ? "Status: Read only" : "Status: Editable");
    });
    copy.OnClick([&]() {
        body.SelectAll();
        body.Copy();
        status.SetText("Status: Body copied");
    });
    paste.OnClick([&]() {
        body.Paste();
        status.SetText("Status: Body pasted");
    });
    clear.OnClick([&]() {
        body.SetText({});
        status.SetText("Status: Body cleared");
    });
    restore.OnClick([&]() {
        body.SetText(kExampleText);
        status.SetText("Status: Example restored");
    });

    Layout content(Orientation::Vertical, 20, 8);
    content.Add(heading);
    content.Add(titleLabel);
    content.Add(title);
    content.Add(bodyLabel);
    content.Add(body, LayoutSizing::Expand);
    content.Add(readOnly);
    content.Add(copy);
    content.Add(paste);
    content.Add(clear);
    content.Add(restore);
    content.Add(characters);
    content.Add(status);
    window.SetContent(content);

    if (!window.Show()) return 1;
    return app.Run();
}

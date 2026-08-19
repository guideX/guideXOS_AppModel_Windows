#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>

using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.tests.focus", ShutdownMode::Explicit);
    Window window(app);
    TextBox textBox("text");
    Button button("button");
    ListBox list;
    list.AddItem("one");
    ComboBox combo;
    combo.AddItem("one");
    CheckBox check("check");
    RadioButton radio("radio");
    Label label("label");

    Layout content(Orientation::Vertical, 8, 4);
    content.Add(textBox);
    content.Add(button);
    content.Add(list, LayoutSizing::Expand);
    content.Add(combo);
    content.Add(check);
    content.Add(radio);
    content.Add(label);
    window.SetContent(content);

    assert(!window.GetFocusedControl().IsValid());
    assert(!textBox.GetControlRef().HasFocus());
    assert(!textBox.Focus());
    assert(!label.GetControlRef().Focus());
    assert(window.Show());
    assert(!window.GetFocusedControl().IsValid());

    assert(textBox.Focus());
    assert(window.GetFocusedControl() == textBox.GetControlRef());
    assert(textBox.GetControlRef().HasFocus());
    assert(window.GetFocusedControl().GetType() == ControlType::TextBox);
    assert(window.GetFocusedControl().AsTextBox()->HasText());

    assert(button.Focus());
    assert(window.GetFocusedControl() == button.GetControlRef());
    assert(!window.GetFocusedControl().AsTextBox());
    assert(list.Focus());
    assert(window.GetFocusedControl() == list.GetControlRef());
    assert(combo.Focus());
    assert(window.GetFocusedControl() == combo.GetControlRef());
    assert(check.Focus());
    assert(window.GetFocusedControl() == check.GetControlRef());
    assert(radio.Focus());
    assert(window.GetFocusedControl() == radio.GetControlRef());

    assert(textBox.Focus());
    textBox.SetEnabled(false);
    assert(!textBox.GetControlRef().HasFocus());
    assert(window.GetFocusedControl() != textBox.GetControlRef());
    assert(!textBox.Focus());
    textBox.SetEnabled(true);
    assert(textBox.Focus());

    bool callbackSawFocus = false;
    button.OnClick([&]() {
        callbackSawFocus = window.GetFocusedControl() == button.GetControlRef();
        assert(textBox.Focus());
    });
    assert(button.Focus());
    button.Click();
    assert(callbackSawFocus);
    assert(window.GetFocusedControl() == textBox.GetControlRef());

    bool textCallbackMovedFocus = false;
    textBox.OnTextChanged([&](const std::string&) {
        textCallbackMovedFocus = true;
        assert(button.Focus());
    });
    textBox.SetText("changed");
    assert(textCallbackMovedFocus);
    assert(window.GetFocusedControl() == button.GetControlRef());

    bool closingSawFocus = false;
    window.OnClosing([&](WindowClosingEvent&) {
        closingSawFocus = window.GetFocusedControl() == button.GetControlRef();
    });
    assert(button.Focus());
    window.Close();
    assert(closingSawFocus);
    assert(!window.GetFocusedControl().IsValid());
    assert(window.Show());
    assert(!window.GetFocusedControl().IsValid());
    assert(textBox.Focus());

    TextBox detached("detached");
    assert(!detached.Focus());
    ControlRef detachedRef = detached.GetControlRef();
    ControlRef temporaryRef;
    {
        TextBox temporary("temporary");
        temporaryRef = temporary.GetControlRef();
        assert(temporaryRef.IsValid());
    }
    // A weak identity never dangles when the public wrapper is destroyed.
    assert(!temporaryRef.IsValid());

    Window otherWindow(app);
    TextBox otherText("other");
    Layout otherContent;
    otherContent.Add(otherText);
    otherWindow.SetContent(otherContent);
    assert(otherWindow.Show());
    assert(textBox.Focus());
    assert(window.GetFocusedControl() == textBox.GetControlRef());
    assert(!otherWindow.GetFocusedControl().IsValid());
    assert(otherText.Focus());
    assert(!window.GetFocusedControl().IsValid());
    assert(otherWindow.GetFocusedControl() == otherText.GetControlRef());
    otherWindow.Close();
    assert(!otherWindow.GetFocusedControl().IsValid());
    assert(!window.GetFocusedControl().IsValid());
    assert(textBox.Focus());
    assert(window.GetFocusedControl() == textBox.GetControlRef());

    window.Close();
    app.Quit();
    assert(app.Run() == 0);
    (void)detachedRef;
    return 0;
}

#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <cstdio>
#include <memory>
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
    Application app("com.guidexos.tests.model-behavior");
    assert(app.GetId() == "com.guidexos.tests.model-behavior");

    Label label("before");
    Button button("activate");
    TextBox input;
    TextBox initial("initial value");
    Layout layout;
    layout.Add(label);
    layout.Add(button);
    layout.Add(input);
    assert(layout.ChildCount() == 3);
    Window owner(app);
    owner.SetContent(layout);
    assert(input.GetText().empty());
    assert(initial.GetText() == "initial value");

    int clickCount = 0;
    button.OnClick([&]() {
        ++clickCount;
        label.SetText("after");
    });
    button.Click();

    assert(clickCount == 1);
    assert(label.GetText() == "after");
    button.SetText("activate again");
    assert(button.GetText() == "activate again");

    const std::string accented = "caf\xC3\xA9";
    const std::string nonLatin =
        "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    const std::string supplementary = "rocket \xF0\x9F\x9A\x80";
    TextBox unicode(accented);
    assert(unicode.GetText() == accented);
    unicode.SetText(nonLatin);
    assert(unicode.GetText() == nonLatin);
    unicode.SetText(supplementary);
    assert(unicode.GetText() == supplementary);
    unicode.SetText({});
    assert(unicode.GetText().empty());

    bool invalidRejected = false;
    try {
        TextBox invalid(std::string("\xC0\xAF", 2));
        (void)invalid;
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);

    invalidRejected = false;
    try {
        input.SetText(std::string("\x80", 1));
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);
    assert(input.GetText().empty());

    int textChangeCount = 0;
    std::string callbackText;
    bool callbackReadMatched = false;
    input.OnTextChanged([&](const std::string& text) {
        ++textChangeCount;
        callbackText = text;
        callbackReadMatched = input.GetText() == text;
        label.SetText(text);
    });
    input.SetText("first");
    input.SetText("first");
    input.SetText("second");
    assert(textChangeCount == 2);
    assert(callbackText == "second");
    assert(callbackReadMatched);
    assert(label.GetText() == "second");

    TextBox normalizing("initial");
    int normalizationCount = 0;
    normalizing.OnTextChanged([&](const std::string& text) {
        ++normalizationCount;
        if (text != "normalized") normalizing.SetText("normalized");
    });
    normalizing.SetText(" raw ");
    assert(normalizing.GetText() == "normalized");
    assert(normalizationCount == 2);
    normalizing.SetText("normalized");
    assert(normalizationCount == 2);

    TextBox source("source");
    TextBox mirror;
    int mirrorChangeCount = 0;
    mirror.OnTextChanged([&](const std::string&) { ++mirrorChangeCount; });
    source.OnTextChanged([&](const std::string& text) {
        mirror.SetText("mirror: " + text);
    });
    source.SetText("updated");
    assert(source.GetText() == "updated");
    assert(mirror.GetText() == "mirror: updated");
    assert(mirrorChangeCount == 1);

    std::weak_ptr<int> callbackToken;
    {
        TextBox temporary("temporary");
        auto token = std::make_shared<int>(1);
        callbackToken = token;
        temporary.OnTextChanged([token](const std::string&) {});
        assert(!callbackToken.expired());
        token.reset();
        temporary.OnTextChanged({});
        assert(callbackToken.expired());
    }
    assert(callbackToken.expired());

    // A TextBox remains a usable logical object even when no native control
    // exists. This also documents that public SetText still dispatches its
    // callback while a window is detached.
    Application otherApp("com.guidexos.tests.other-model");
    Window otherWindow(otherApp);
    Layout otherLayout;
    bool controlParentRejected = false;
    try {
        otherLayout.Add(input);
    } catch (const std::logic_error&) {
        controlParentRejected = true;
    }
    assert(controlParentRejected);
    bool crossApplicationRejected = false;
    try {
        otherWindow.SetContent(layout);
    } catch (const std::logic_error&) {
        crossApplicationRejected = true;
    }
    assert(crossApplicationRejected);
    input.SetText("after detached");
    assert(input.GetText() == "after detached");

    return 0;
}

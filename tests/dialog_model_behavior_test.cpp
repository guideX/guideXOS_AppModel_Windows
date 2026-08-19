#include <guidexos/appmodel/appmodel.hpp>

#include <cassert>
#include <stdexcept>
#include <string>

using namespace guidexos::appmodel;

namespace {

template <typename Callback>
void ExpectInvalidArgument(Callback&& callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

template <typename Callback>
void ExpectLogicError(Callback&& callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main() {
    OpenFileDialog open;
    open.SetTitle("Ouvrir " "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" " \xF0\x9F\x9A\x80");
    open.AddFilter("Texte", {"*.txt", "*.md"});
    open.AddFilter("Tous les fichiers", {"*.*"});

    SaveFileDialog save;
    save.SetTitle("Enregistrer " "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
    save.SetSuggestedFileName("profil-" "\xC3\xBC" "ber.gxprofile");
    save.AddFilter("Profil guideXOS", {"*.gxprofile"});

    ExpectInvalidArgument([] {
        OpenFileDialog invalid;
        invalid.AddFilter("Empty", {});
    });
    ExpectInvalidArgument([] {
        OpenFileDialog invalid;
        invalid.AddFilter("Empty pattern", {""});
    });
    ExpectInvalidArgument([] {
        OpenFileDialog invalid;
        invalid.AddFilter("Bad", {std::string("\x80", 1)});
    });
    ExpectInvalidArgument([] {
        SaveFileDialog invalid;
        invalid.SetSuggestedFileName(std::string("bad \x80", 5));
    });

    Application app("com.guidexos.tests.dialog-model");
    Window window(app);
    ExpectInvalidArgument([&] {
        (void)MessageDialog::Show(window, std::string("bad \x80", 5), "Title");
    });
    ExpectLogicError([&] { (void)MessageDialog::Show(window, "message", "title"); });
    ExpectLogicError([&] { (void)open.Show(window); });
    ExpectLogicError([&] { (void)save.Show(window); });

    assert(window.Show());
    window.Close();
    ExpectLogicError([&] { (void)MessageDialog::Show(window, "message", "title"); });
    ExpectLogicError([&] { (void)open.Show(window); });

    Application shutdownApp("com.guidexos.tests.dialog-shutdown");
    Window shutdownWindow(shutdownApp);
    assert(shutdownWindow.Show());
    shutdownWindow.Close();
    assert(shutdownApp.Run() == 0);
    assert(!shutdownWindow.Show());
    ExpectLogicError([&] {
        (void)MessageDialog::Show(shutdownWindow, "message", "title");
    });

    // Dialog objects remain ordinary reusable value-like configuration
    // objects; showing them never changes their title, filters, or suggestion.
    open.SetTitle("Open again");
    save.SetSuggestedFileName("again.gxprofile");
    return 0;
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>

#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
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

constexpr char kPngBase64[] =
    "iVBORw0KGgoAAAANSUhEUgAAAAMAAAACCAYAAACddGYaAAAAAXNSR0IArs4c6QAA"
    "AARnQU1BAACxjwv8YQUAAAAcSURBVBhXFcixDQAACMAgTvfzqhMJIuNJBeNyAYwaCfj7yMTOAAAAAElFTkSuQmCC";

int Base64Value(char value) noexcept {
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}

std::vector<unsigned char> DecodeBase64(const char* text) {
    std::vector<unsigned char> result;
    int accumulator = 0;
    int bits = 0;
    for (const char* current = text; *current; ++current) {
        if (*current == '=') break;
        const int value = Base64Value(*current);
        if (value < 0) continue;
        accumulator = (accumulator << 6) | value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<unsigned char>(
                (accumulator >> bits) & 0xFF));
        }
    }
    return result;
}

std::string WriteFixture() {
    const auto path = std::filesystem::temp_directory_path() /
        "guideXOS_Phase7_ScrollView.png";
    const auto bytes = DecodeBase64(kPngBase64);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return path.string();
}

HWND FindChildByClass(HWND parent, const wchar_t* className,
                      HWND previous = nullptr) {
    return FindWindowExW(parent, previous, className, nullptr);
}

HWND FindChildByText(HWND parent, const wchar_t* className,
                     const wchar_t* text) {
    HWND current = nullptr;
    while ((current = FindChildByClass(parent, className, current)) != nullptr) {
        wchar_t buffer[256]{};
        GetWindowTextW(current, buffer, static_cast<int>(std::size(buffer)));
        if (std::wstring(buffer) == text) return current;
    }
    return nullptr;
}

SCROLLINFO ReadScrollInfo(HWND host) {
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;
    GetScrollInfo(host, SB_VERT, &info);
    return info;
}

} // namespace

int main() {
    const std::string imagePath = WriteFixture();
    Application app("com.guidexos.tests.scroll-view-native",
                     ShutdownMode::Explicit);
    Window window(app);
    window.SetTitle("guideXOS ScrollView Native");
    window.SetSize(620, 360);

    TabView tabs;
    auto page = tabs.AddTab("Scrollable Settings");
    auto otherPage = tabs.AddTab("Other");
    Label otherLabel("Second page");
    ScrollView scroll;
    Label title("Profile");
    TextBox profile("guideXOS");
    Image image;
    image.SetSource(ImageSource::FromFile(imagePath));
    TextArea notes("Notes remain editable after scrolling.");
    Button reveal("Reveal lower control");
    bool clicked = false;
    reveal.OnClick([&] { clicked = true; });
    std::vector<std::unique_ptr<Label>> rows;
    for (int index = 0; index < 16; ++index) {
        rows.push_back(std::make_unique<Label>(
            "Setting row " + std::to_string(index)));
        scroll.ContentLayout().Add(*rows.back());
    }
    TabView nestedTabs;
    auto nestedPage = nestedTabs.AddTab("Nested");
    Label nestedLabel("Nested TabView content");
    nestedPage.GetLayout().Add(nestedLabel);

    scroll.ContentLayout().Add(title);
    scroll.ContentLayout().Add(profile);
    scroll.ContentLayout().Add(image);
    scroll.ContentLayout().Add(nestedTabs);
    scroll.ContentLayout().Add(notes, LayoutSizing::Expand);
    scroll.ContentLayout().Add(reveal);
    page.GetLayout().Add(scroll, LayoutSizing::Expand);
    otherPage.GetLayout().Add(otherLabel);

    Layout root;
    root.Add(tabs, LayoutSizing::Expand);
    window.SetContent(root);
    assert(window.Show());

    const HWND nativeWindow = FindWindowW(
        nullptr, L"guideXOS ScrollView Native");
    assert(nativeWindow != nullptr);
    const HWND host = FindChildByClass(
        nativeWindow, L"guideXOS.AppModel.Windows.ScrollView");
    assert(host != nullptr);
    assert(GetParent(host) == nativeWindow);
    assert((GetWindowLongPtrW(host, GWL_STYLE) & WS_CLIPCHILDREN) != 0);
    const HWND nativeImage = FindChildByClass(
        host, L"guideXOS.AppModel.Windows.Image");
    assert(nativeImage != nullptr && GetParent(nativeImage) == host);
    assert(image.GetLoadStatus() == ImageLoadStatus::Loaded);
    const HWND nativeButton = FindChildByText(
        host, L"BUTTON", L"Reveal lower control");
    assert(nativeButton != nullptr && GetParent(nativeButton) == host);

    SCROLLINFO initial = ReadScrollInfo(host);
    assert(scroll.GetMaximumVerticalOffset() > 0);
    assert(initial.nMax > static_cast<int>(initial.nPage));
    assert(initial.nPos == scroll.GetVerticalOffset());

    scroll.SetVerticalOffset(scroll.GetMaximumVerticalOffset());
    assert(ReadScrollInfo(host).nPos == scroll.GetVerticalOffset());
    SendMessageW(nativeButton, BM_CLICK, 0, 0);
    assert(clicked);

    scroll.SetVerticalOffset(0);
    SendMessageW(host, WM_MOUSEWHEEL,
                 MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), 0);
    assert(scroll.GetVerticalOffset() > 0);
    SendMessageW(host, WM_MOUSEWHEEL,
                 MAKEWPARAM(0, static_cast<WORD>(WHEEL_DELTA)), 0);
    assert(scroll.GetVerticalOffset() < scroll.GetMaximumVerticalOffset());

    scroll.SetVerticalOffset(scroll.GetMaximumVerticalOffset());
    const int bottomOffset = scroll.GetVerticalOffset();
    tabs.SetSelectedIndex(1);
    tabs.SetSelectedIndex(0);
    assert(scroll.GetVerticalOffset() == bottomOffset);
    assert(profile.GetText() == "guideXOS");

    const int smallMaximum = scroll.GetMaximumVerticalOffset();
    window.SetSize(900, 760);
    assert(scroll.GetMaximumVerticalOffset() < smallMaximum);
    window.SetSize(620, 360);
    assert(scroll.GetMaximumVerticalOffset() >= smallMaximum);
    const int retainedAfterResize = scroll.GetVerticalOffset();

    const HWND oldWindow = nativeWindow;
    window.Close();
    assert(!IsWindow(oldWindow));
    assert(window.Show());
    const HWND reopened = FindWindowW(
        nullptr, L"guideXOS ScrollView Native");
    assert(reopened != nullptr && reopened != oldWindow);
    const HWND reopenedHost = FindChildByClass(
        reopened, L"guideXOS.AppModel.Windows.ScrollView");
    assert(reopenedHost != nullptr);
    assert(scroll.GetVerticalOffset() == retainedAfterResize);
    assert(ReadScrollInfo(reopenedHost).nPos == retainedAfterResize);

    window.Close();
    app.Quit();
    assert(app.Run() == 0);
    std::error_code ignored;
    std::filesystem::remove(imagePath, ignored);
    return 0;
}

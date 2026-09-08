#include "windows_backend.hpp"

#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <wincodec.h>

#include "appmodel/runtime.hpp"
#include "appmodel/text_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <new>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace guidexos::appmodel::detail {
namespace {

constexpr wchar_t kWindowClassName[] = L"guideXOS.AppModel.Windows.Foundation";
constexpr wchar_t kImageWindowClassName[] = L"guideXOS.AppModel.Windows.Image";
constexpr wchar_t kScrollViewWindowClassName[] =
    L"guideXOS.AppModel.Windows.ScrollView";
constexpr int kMinimumClientWidth = 320;
constexpr int kMinimumClientHeight = 180;
constexpr UINT kMaximumImageDimension = 16384;
constexpr std::uint64_t kMaximumImagePixels = 64ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaximumImageBytes = kMaximumImagePixels * 4ULL;

struct NativeScrollViewContext {
    WindowsBackend* backend = nullptr;
    std::weak_ptr<WindowState> window;
    std::weak_ptr<ControlState> model;
};

bool InitializeCommonControls() noexcept {
    static std::once_flag once;
    static bool initialized = false;
    std::call_once(once, []() noexcept {
        INITCOMMONCONTROLSEX initialization{};
        initialization.dwSize = sizeof(initialization);
        initialization.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES;
        initialized = InitCommonControlsEx(&initialization) != FALSE;
    });
    return initialized;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("App Model text is too long to convert");
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           value.data(), static_cast<int>(value.size()),
                                           nullptr, 0);
    if (length <= 0) {
        throw std::invalid_argument("App Model text is not valid UTF-8");
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            value.data(), static_cast<int>(value.size()),
                            result.data(), length) != length) {
        throw std::invalid_argument("App Model text could not be converted to UTF-16");
    }
    return result;
}

std::string Utf16ToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Native text is too long to convert");
    }

    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                           value.data(), static_cast<int>(value.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        throw std::runtime_error("Native text is not valid UTF-16");
    }

    std::string result(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            value.data(), static_cast<int>(value.size()),
                            result.data(), length, nullptr, nullptr) != length) {
        throw std::runtime_error("Native text could not be converted to UTF-8");
    }
    return result;
}

std::vector<std::string> ReadNativeFileDrop(HDROP drop) {
    std::vector<std::string> paths;
    if (!drop) return paths;

    const UINT count = DragQueryFileW(drop, 0xFFFFFFFFU, nullptr, 0);
    const UINT limit = std::min<UINT>(
        count, static_cast<UINT>(kMaximumFileDropCount));
    paths.reserve(limit);
    std::size_t nativePayloadUnits = 0;
    std::size_t utf8PayloadBytes = 0;
    for (UINT index = 0; index < limit; ++index) {
        const UINT length = DragQueryFileW(drop, index, nullptr, 0);
        if (length == 0 || length > kMaximumFileDropPathBytes) continue;
        if (static_cast<std::size_t>(length) >
            kMaximumFileDropPayloadBytes - nativePayloadUnits) {
            break;
        }
        nativePayloadUnits += length;

        std::wstring wide(static_cast<std::size_t>(length) + 1U, L'\0');
        if (DragQueryFileW(drop, index, wide.data(), length + 1) != length) {
            continue;
        }
        wide.resize(length);
        try {
            std::string path = Utf16ToUtf8(wide);
            if (path.empty() || path.size() > kMaximumFileDropPathBytes ||
                path.size() >
                    kMaximumFileDropPayloadBytes - utf8PayloadBytes) {
                break;
            }
            utf8PayloadBytes += path.size();
            paths.push_back(std::move(path));
        } catch (const std::exception&) {
            // A malformed native path cannot enter the UTF-8 App Model
            // event. Continue deterministically with the remaining entries.
        }
    }
    return paths;
}

class ScopedNativeFileDrop final {
public:
    explicit ScopedNativeFileDrop(HDROP drop) noexcept : drop_(drop) {}
    ~ScopedNativeFileDrop() {
        if (drop_) DragFinish(drop_);
    }

    ScopedNativeFileDrop(const ScopedNativeFileDrop&) = delete;
    ScopedNativeFileDrop& operator=(const ScopedNativeFileDrop&) = delete;

    HDROP Get() const noexcept { return drop_; }

    void Reset() noexcept {
        if (drop_) DragFinish(drop_);
        drop_ = nullptr;
    }

private:
    HDROP drop_{};
};

class ScopedComApartment final {
public:
    ScopedComApartment() {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (result == RPC_E_CHANGED_MODE) {
            throw std::runtime_error(
                "Windows file dialogs require a compatible COM apartment");
        }
        if (FAILED(result)) {
            throw std::runtime_error("Unable to initialize COM for file dialog");
        }
        initialized_ = true;
    }

    ~ScopedComApartment() {
        if (initialized_) CoUninitialize();
    }

    ScopedComApartment(const ScopedComApartment&) = delete;
    ScopedComApartment& operator=(const ScopedComApartment&) = delete;

private:
    bool initialized_ = false;
};

template <typename T>
struct ComReleaser {
    void operator()(T* value) const noexcept {
        if (value) value->Release();
    }
};

template <typename T>
using ComPtr = std::unique_ptr<T, ComReleaser<T>>;

class NativeImageSurface final {
public:
    NativeImageSurface(UINT width, UINT height) noexcept
        : width_(width), height_(height) {
        if (width_ == 0 || height_ == 0 || width_ > kMaximumImageDimension ||
            height_ > kMaximumImageDimension) {
            return;
        }

        dc_ = CreateCompatibleDC(nullptr);
        if (!dc_) return;

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
        bitmapInfo.bmiHeader.biWidth = static_cast<LONG>(width_);
        bitmapInfo.bmiHeader.biHeight = -static_cast<LONG>(height_);
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;
        bitmap_ = CreateDIBSection(dc_, &bitmapInfo, DIB_RGB_COLORS,
                                   &pixels_, nullptr, 0);
        if (!bitmap_ || !pixels_) return;
        previous_ = SelectObject(dc_, bitmap_);
        if (!previous_) {
            DeleteObject(bitmap_);
            bitmap_ = nullptr;
            pixels_ = nullptr;
            return;
        }
        valid_ = true;
    }

    ~NativeImageSurface() {
        if (dc_ && previous_) SelectObject(dc_, previous_);
        if (bitmap_) DeleteObject(bitmap_);
        if (dc_) DeleteDC(dc_);
    }

    NativeImageSurface(const NativeImageSurface&) = delete;
    NativeImageSurface& operator=(const NativeImageSurface&) = delete;

    bool IsValid() const noexcept { return valid_; }
    UINT GetWidth() const noexcept { return width_; }
    UINT GetHeight() const noexcept { return height_; }
    HDC GetDC() const noexcept { return dc_; }
    void* GetPixels() const noexcept { return pixels_; }

private:
    UINT width_{};
    UINT height_{};
    HDC dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ previous_{};
    void* pixels_{};
    bool valid_{false};
};

struct NativeImageWindowContext {
    std::weak_ptr<ControlState> model;
    std::shared_ptr<NativeImageSurface> surface;
};

void PaintImageWindow(HWND hwnd, NativeImageWindowContext* context) noexcept {
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(hwnd, &paint);
    if (!dc) return;

    RECT client{};
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, GetSysColorBrush(COLOR_WINDOW));

    try {
        const auto model = context ? context->model.lock() : nullptr;
        const auto surface = context ? context->surface : nullptr;
        if (model && surface && surface->IsValid()) {
            const int width = std::max(0L, client.right - client.left);
            const int height = std::max(0L, client.bottom - client.top);
            const ImageRenderGeometry geometry =
                CalculateImageRenderGeometry(
                    static_cast<int>(surface->GetWidth()),
                    static_cast<int>(surface->GetHeight()),
                    LayoutRect{0, 0, width, height},
                    model->imageScaleMode);
            if (geometry.width > 0 && geometry.height > 0 &&
                geometry.sourceWidth > 0 && geometry.sourceHeight > 0) {
                BLENDFUNCTION blend{};
                blend.BlendOp = AC_SRC_OVER;
                blend.SourceConstantAlpha = 255;
                blend.AlphaFormat = AC_SRC_ALPHA;
                AlphaBlend(dc, geometry.x, geometry.y, geometry.width,
                           geometry.height, surface->GetDC(),
                           geometry.sourceX, geometry.sourceY,
                           geometry.sourceWidth, geometry.sourceHeight,
                           blend);
            }
        }
    } catch (...) {
        // Paint is a best-effort backend operation. The already-cleared
        // background remains visible if a surface or geometry operation fails.
    }

    EndPaint(hwnd, &paint);
}

struct CoTaskMemReleaser {
    void operator()(wchar_t* value) const noexcept {
        if (value) CoTaskMemFree(value);
    }
};

struct NativeFileFilters {
    std::vector<std::wstring> descriptions;
    std::vector<std::wstring> patterns;
    std::vector<COMDLG_FILTERSPEC> specifications;

    explicit NativeFileFilters(const std::vector<FileDialogFilter>& filters) {
        descriptions.reserve(filters.size());
        patterns.reserve(filters.size());
        specifications.reserve(filters.size());
        for (const auto& filter : filters) {
            descriptions.push_back(Utf8ToWide(filter.description));
            std::wstring combined;
            for (const auto& pattern : filter.patterns) {
                if (!combined.empty()) combined += L';';
                combined += Utf8ToWide(pattern);
            }
            patterns.push_back(std::move(combined));
        }
        for (std::size_t index = 0; index < descriptions.size(); ++index) {
            specifications.push_back(COMDLG_FILTERSPEC{
                descriptions[index].c_str(), patterns[index].c_str()});
        }
    }
};

void RequireNativeSuccess(HRESULT result, const char* message) {
    if (FAILED(result)) throw std::runtime_error(message);
}

bool IsDialogCancellation(HRESULT result) noexcept {
    return result == HRESULT_FROM_WIN32(ERROR_CANCELLED) || result == E_ABORT;
}

std::optional<std::string> ShowNativeFileDialog(
    HWND owner, const std::string& title,
    const std::string& suggestedFileName,
    const std::optional<std::string>& initialDirectory,
    const std::vector<FileDialogFilter>& filters, bool save) {
    ScopedComApartment comApartment;

    IFileDialog* rawDialog = nullptr;
    const CLSID& classId = save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
    RequireNativeSuccess(
        CoCreateInstance(classId, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&rawDialog)),
        save ? "Unable to create Save File dialog"
             : "Unable to create Open File dialog");
    ComPtr<IFileDialog> dialog(rawDialog);

    const std::wstring nativeTitle = Utf8ToWide(title);
    RequireNativeSuccess(dialog->SetTitle(nativeTitle.c_str()),
                         "Unable to configure file dialog title");

    DWORD options = 0;
    RequireNativeSuccess(dialog->GetOptions(&options),
                         "Unable to configure file dialog options");
    options |= FOS_FORCEFILESYSTEM;
    RequireNativeSuccess(dialog->SetOptions(options),
                         "Unable to configure file dialog options");

    NativeFileFilters nativeFilters(filters);
    if (!nativeFilters.specifications.empty()) {
        RequireNativeSuccess(
            dialog->SetFileTypes(
                static_cast<UINT>(nativeFilters.specifications.size()),
                nativeFilters.specifications.data()),
            "Unable to configure file dialog filters");
    }

    if (initialDirectory) {
        const std::wstring nativeDirectory = Utf8ToWide(*initialDirectory);
        IShellItem* rawFolder = nullptr;
        RequireNativeSuccess(
            SHCreateItemFromParsingName(nativeDirectory.c_str(), nullptr,
                                        IID_PPV_ARGS(&rawFolder)),
            "Unable to configure file dialog initial directory");
        ComPtr<IShellItem> folder(rawFolder);
        RequireNativeSuccess(dialog->SetFolder(folder.get()),
                             "Unable to configure file dialog initial directory");
    }

    if (save && !suggestedFileName.empty()) {
        const std::wstring nativeSuggestedName = Utf8ToWide(suggestedFileName);
        RequireNativeSuccess(dialog->SetFileName(nativeSuggestedName.c_str()),
                             "Unable to configure suggested file name");
    }

    const HRESULT showResult = dialog->Show(owner);
    // Some desktop/session configurations leave the owner hidden after the
    // Common Item Dialog closes. Restore the App Model window before any
    // subsequent synchronous MessageDialog or application callback runs.
    if (owner && IsWindow(owner) != FALSE) {
        ::ShowWindow(owner, SW_SHOWNORMAL);
        UpdateWindow(owner);
    }
    if (IsDialogCancellation(showResult)) return std::nullopt;
    RequireNativeSuccess(showResult,
                         save ? "Unable to show Save File dialog"
                              : "Unable to show Open File dialog");

    IShellItem* rawItem = nullptr;
    RequireNativeSuccess(dialog->GetResult(&rawItem),
                         save ? "Unable to read Save File dialog result"
                              : "Unable to read Open File dialog result");
    ComPtr<IShellItem> item(rawItem);

    PWSTR rawPath = nullptr;
    RequireNativeSuccess(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath),
                         "Unable to read selected filesystem path");
    std::unique_ptr<wchar_t, CoTaskMemReleaser> path(rawPath);
    return Utf16ToUtf8(path ? std::wstring(path.get()) : std::wstring{});
}

std::wstring ReadNativeText(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return {};

    // GetWindowTextW's size includes the terminating null character.
    std::wstring result(static_cast<std::size_t>(length) + 1, L'\0');
    const int actualLength = GetWindowTextW(hwnd, result.data(), length + 1);
    if (actualLength < 0) {
        throw std::runtime_error("Native text could not be read");
    }
    result.resize(static_cast<std::size_t>(actualLength));
    return result;
}

std::wstring NormalizeNativeTextAreaNewlines(const std::wstring& text) {
    std::wstring normalized;
    normalized.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] == L'\r') {
            normalized.push_back(L'\n');
            if (index + 1 < text.size() && text[index + 1] == L'\n') ++index;
        } else {
            normalized.push_back(text[index]);
        }
    }
    return normalized;
}

std::size_t NativeTextAreaOffsetForScalarIndex(const std::wstring& text,
                                               std::size_t scalarIndex) {
    std::size_t scalar = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        if (scalar == scalarIndex) return offset;
        if (text[offset] == L'\r' && offset + 1 < text.size() &&
            text[offset + 1] == L'\n') {
            offset += 2;
        } else if (text[offset] >= 0xD800 && text[offset] <= 0xDBFF &&
                   offset + 1 < text.size() && text[offset + 1] >= 0xDC00 &&
                   text[offset + 1] <= 0xDFFF) {
            offset += 2;
        } else {
            ++offset;
        }
        ++scalar;
    }
    return text.size();
}

std::size_t NativeTextAreaScalarIndexForOffset(const std::wstring& text,
                                               std::size_t nativeOffset) {
    const std::size_t limit = std::min(nativeOffset, text.size());
    std::size_t scalar = 0;
    for (std::size_t offset = 0; offset < limit;) {
        if (text[offset] == L'\r' && offset + 1 < text.size() &&
            text[offset + 1] == L'\n' && offset + 1 < limit) {
            offset += 2;
        } else if (text[offset] >= 0xD800 && text[offset] <= 0xDBFF &&
                   offset + 1 < text.size() && text[offset + 1] >= 0xDC00 &&
                   text[offset + 1] <= 0xDFFF && offset + 1 < limit) {
            offset += 2;
        } else {
            ++offset;
        }
        ++scalar;
    }
    return scalar;
}

bool IsTextEdit(ControlKind kind) noexcept {
    return kind == ControlKind::TextBox || kind == ControlKind::TextArea;
}

void SynchronizeTextEditStateFromNative(
    const std::shared_ptr<ControlState>& control, HWND hwnd) {
    if (!control || !IsTextEdit(control->kind) || !hwnd) return;

    DWORD nativeStart = 0;
    DWORD nativeEnd = 0;
    SendMessageW(hwnd, EM_GETSEL,
                 reinterpret_cast<WPARAM>(&nativeStart),
                 reinterpret_cast<LPARAM>(&nativeEnd));
    const std::wstring rawText = ReadNativeText(hwnd);
    const bool isTextArea = control->kind == ControlKind::TextArea;
    const std::size_t first = std::min<std::size_t>(nativeStart, nativeEnd);
    const std::size_t last = std::max<std::size_t>(nativeStart, nativeEnd);
    const std::size_t normalizedFirst = isTextArea
        ? NativeTextAreaScalarIndexForOffset(rawText, first)
        : Utf16ScalarIndexForCodeUnitOffset(rawText, first);
    const std::size_t normalizedLast = isTextArea
        ? NativeTextAreaScalarIndexForOffset(rawText, last)
        : Utf16ScalarIndexForCodeUnitOffset(rawText, last);
    const std::size_t start = normalizedFirst;
    const std::size_t end = normalizedLast;
    control->selection = TextRange{start, end - start};
    control->caretIndex = end;
}

struct ControlSubclassState {
    WNDPROC previous = nullptr;
    std::weak_ptr<ControlState> model;
    std::weak_ptr<WindowState> window;
    HWND toolTip{};
    unsigned int activeCalls = 0;
    bool destroyed = false;
};

constexpr wchar_t kControlSubclassProperty[] =
    L"guideXOS.AppModel.ControlSubclassState";

bool IsTextEditPositionMessage(UINT message) noexcept {
    switch (message) {
    case WM_CHAR:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_MOUSEMOVE:
    case WM_SETTEXT:
    case WM_CUT:
    case WM_PASTE:
    case WM_CLEAR:
    case WM_UNDO:
    case EM_SETSEL:
        return true;
    default:
        return false;
    }
}

bool IsToolTipRelayMessage(UINT message) noexcept {
    switch (message) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_NCMOUSEMOVE:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
        return true;
    default:
        return false;
    }
}

void UpdateToolTipRect(TOOLINFOW& info, HWND parent, HWND child) noexcept {
    if (!parent || !child || !GetWindowRect(child, &info.rect)) return;
    POINT topLeft{info.rect.left, info.rect.top};
    POINT bottomRight{info.rect.right, info.rect.bottom};
    if (!ScreenToClient(parent, &topLeft) ||
        !ScreenToClient(parent, &bottomRight)) {
        return;
    }
    info.rect = RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

HWND FindScrollViewHost(HWND child) noexcept {
    HWND current = child ? GetParent(child) : nullptr;
    wchar_t className[128]{};
    while (current) {
        const int length = GetClassNameW(current, className,
                                         static_cast<int>(std::size(className)));
        if (length > 0 &&
            std::wstring(className, static_cast<std::size_t>(length)) ==
                kScrollViewWindowClassName) {
            return current;
        }
        current = GetParent(current);
    }
    return nullptr;
}

LRESULT CALLBACK ControlSubclassProcedure(HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam) noexcept {
    auto* state = static_cast<ControlSubclassState*>(
        GetPropW(hwnd, kControlSubclassProperty));
    if (!state || !state->previous) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    ++state->activeCalls;
    if (message == WM_MOUSEWHEEL) {
        if (const HWND host = FindScrollViewHost(hwnd)) {
            SendMessageW(host, message, wParam, lParam);
            --state->activeCalls;
            return 0;
        }
    }
    const WNDPROC previous = state->previous;
    const auto model = state->model;
    const auto window = state->window;
    const LRESULT result = CallWindowProcW(previous, hwnd, message, wParam,
                                           lParam);
    if (message == WM_NCDESTROY) {
        RemovePropW(hwnd, kControlSubclassProperty);
        state->destroyed = true;
    }
    if (!state->destroyed) {
        try {
            if (state->toolTip && IsToolTipRelayMessage(message)) {
                MSG relay{hwnd, message, wParam, lParam, 0, {}};
                GetCursorPos(&relay.pt);
                SendMessageW(state->toolTip, TTM_RELAYEVENT, 0,
                             reinterpret_cast<LPARAM>(&relay));
            }
            const auto lockedModel = model.lock();
            const auto lockedWindow = window.lock();
            if (message == WM_SETFOCUS) {
                SetNativeFocus(lockedWindow, lockedModel);
            } else if (message == WM_KILLFOCUS) {
                ClearNativeFocus(lockedWindow, lockedModel);
            }
            if (lockedModel && IsTextEdit(lockedModel->kind) &&
                IsTextEditPositionMessage(message)) {
                SynchronizeTextEditStateFromNative(lockedModel, hwnd);
            }
        } catch (...) {
            // Native controls should never expose invalid focus/selection
            // state, but a transient teardown cannot escape WndProc.
        }
    }
    --state->activeCalls;
    if (state->destroyed && state->activeCalls == 0) delete state;
    return result;
}

void SetControlSubclassToolTip(HWND hwnd, HWND toolTip) noexcept {
    if (!hwnd) return;
    auto* state = static_cast<ControlSubclassState*>(
        GetPropW(hwnd, kControlSubclassProperty));
    if (state) state->toolTip = toolTip;
}

void InstallControlSubclass(
    HWND hwnd, const std::shared_ptr<ControlState>& model,
    const std::shared_ptr<WindowState>& window) noexcept {
    if (!hwnd || !model || !window) return;
    auto* state = new (std::nothrow) ControlSubclassState();
    if (!state) return;
    state->model = model;
    state->window = window;
    const LONG_PTR previous = SetWindowLongPtrW(
        hwnd, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&ControlSubclassProcedure));
    if (!previous) {
        delete state;
        return;
    }
    state->previous = reinterpret_cast<WNDPROC>(previous);
    if (!SetPropW(hwnd, kControlSubclassProperty, state)) {
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, previous);
        delete state;
    }
}

int SaturatingDimension(int value) {
    return std::clamp(value, 1, std::numeric_limits<int>::max() / 2);
}

int SaturatingMetric(std::int64_t value) {
    if (value <= 0) return 0;
    return value >= std::numeric_limits<int>::max()
        ? std::numeric_limits<int>::max()
        : static_cast<int>(value);
}

int AddMetric(int value, int addition) {
    return SaturatingMetric(static_cast<std::int64_t>(value) + addition);
}

struct NativeTextMetrics {
    int width = 0;
    int height = 0;
};

std::optional<NativeTextMetrics> MeasureNativeText(
    HWND hwnd, const std::wstring& text) {
    if (!hwnd || text.size() > static_cast<std::size_t>(
                           std::numeric_limits<int>::max())) {
        return std::nullopt;
    }

    HDC dc = GetDC(hwnd);
    if (!dc) return std::nullopt;

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(
        hwnd, WM_GETFONT, 0, 0));
    if (!font) font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;

    TEXTMETRICW textMetrics{};
    SIZE textSize{};
    const BOOL metricsSucceeded = GetTextMetricsW(dc, &textMetrics);
    const BOOL extentSucceeded = text.empty()
        ? TRUE
        : GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()),
                                &textSize);

    if (previous) SelectObject(dc, previous);
    ReleaseDC(hwnd, dc);
    if (!metricsSucceeded || !extentSucceeded) return std::nullopt;

    return NativeTextMetrics{
        std::max(1, static_cast<int>(textSize.cx)),
        std::max(1, static_cast<int>(textMetrics.tmHeight))};
}

class WindowsMeasurementProvider final : public LayoutMeasurementProvider {
public:
    using ControlLookup = std::function<HWND(const ControlState&)>;

    explicit WindowsMeasurementProvider(ControlLookup lookup)
        : lookup_(std::move(lookup)) {}

    LayoutMeasurement Measure(const ControlState& control) const override {
        try {
            const HWND hwnd = lookup_ ? lookup_(control) : nullptr;
            const auto native = MeasureNativeText(hwnd, Utf8ToWide(control.text));
            if (!native) return GetNeutralControlMeasurement(control);

            const int fontHeight = native->height;
            switch (control.kind) {
            case ControlKind::Label:
                return {{native->width, fontHeight}, {0, fontHeight}};
            case ControlKind::Button: {
                const int height = std::max(28, AddMetric(fontHeight, 10));
                return {{std::max(80, AddMetric(native->width, 24)), height},
                        {64, height}};
            }
            case ControlKind::CheckBox:
            case ControlKind::RadioButton: {
                const int indicator = std::max(16, GetSystemMetrics(SM_CXMENUCHECK));
                const int height = std::max(22, AddMetric(fontHeight, 4));
                const int width = AddMetric(AddMetric(native->width, indicator), 12);
                return {{std::max(32, width), height},
                        {AddMetric(indicator, 12), height}};
            }
            case ControlKind::TextBox: {
                const int height = std::max(28, AddMetric(fontHeight, 8));
                return {{180, height}, {72, height}};
            }
            case ControlKind::TextArea: {
                const int height = std::max(140, AddMetric(fontHeight, 112));
                return {{220, height}, {96, std::max(48, AddMetric(fontHeight, 16))}};
            }
            case ControlKind::ListBox: {
                const int viewportHeight = std::max(140,
                                                     AddMetric(fontHeight, 112));
                const int minimumHeight = std::max(48, AddMetric(fontHeight, 16));
                return {{220, viewportHeight}, {96, minimumHeight}};
            }
            case ControlKind::ComboBox: {
                // The closed control has a bounded preferred width. Item
                // text intentionally does not widen the form; the native
                // popup remains responsible for displaying its item content.
                const int height = std::max(28, AddMetric(fontHeight, 8));
                return {{220, height}, {112, height}};
            }
            case ControlKind::ProgressBar:
                return {{220, 22}, {96, 22}};
            case ControlKind::Slider:
                return {{220, 32}, {96, 24}};
            case ControlKind::TabView:
                return {{360, 260}, {180, 120}};
            case ControlKind::Image:
                return GetNeutralControlMeasurement(control);
            case ControlKind::ScrollView:
                return {{360, 260}, {180, 120}};
            }
        } catch (...) {
            // Native realization is an optimization. A valid neutral
            // measurement keeps layout deterministic if a native metric is
            // unavailable during creation or teardown.
        }
        return GetNeutralControlMeasurement(control);
    }

private:
    ControlLookup lookup_;
};

} // namespace

struct WindowsBackend::ChildBinding {
    std::weak_ptr<ControlState> model;
    HWND hwnd{};
    HWND parent{};
    int commandId{};
    bool synchronizingText{false};
    bool synchronizingSelection{false};
    bool synchronizingCheck{false};
    bool synchronizingSlider{false};
    bool synchronizingTabSelection{false};
    bool nativeReadOnly{false};
    bool nativeWordWrap{true};
    std::vector<std::string> nativeItems;
    std::unique_ptr<NativeImageWindowContext> imageContext;
    std::string nativeImagePath;
    bool imageDecodeAttempted{false};
    std::unique_ptr<NativeScrollViewContext> scrollContext;
};

struct WindowsBackend::ToolTipBinding {
    std::weak_ptr<ControlState> model;
    std::wstring text;
    TOOLINFOW info{};
};

struct WindowsBackend::WindowBinding {
    std::shared_ptr<WindowState> model;
    HWND hwnd{};
    HWND statusBar{};
    std::shared_ptr<StatusBarState> statusBarModel;
    HWND toolTip{};
    HMENU menu{};
    HACCEL accelerators{};
    std::vector<ChildBinding> children;
    std::vector<std::unique_ptr<ToolTipBinding>> toolTips;
    std::unordered_map<UINT, std::weak_ptr<MenuItemState>> menuCommands;
    std::unordered_map<HMENU, std::weak_ptr<MenuState>> menuOpenings;
    bool menuOpening{false};
    bool menuRefreshPending{false};
};

struct WindowsBackend::ComInitialization {
    ComInitialization() noexcept {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        initialized = result == S_OK || result == S_FALSE;
    }

    ~ComInitialization() {
        if (initialized) CoUninitialize();
    }

    ComInitialization(const ComInitialization&) = delete;
    ComInitialization& operator=(const ComInitialization&) = delete;

    bool initialized = false;
};

struct WindowsBackend::NativeImageDecoder {
    NativeImageDecoder() noexcept {
        IWICImagingFactory* rawFactory = nullptr;
        if (SUCCEEDED(CoCreateInstance(
                CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&rawFactory)))) {
            factory.reset(rawFactory);
        }
    }

    std::shared_ptr<NativeImageSurface> Decode(const std::string& path) const {
        if (!factory || path.empty()) return nullptr;

        const std::wstring nativePath = Utf8ToWide(path);
        IWICBitmapDecoder* rawDecoder = nullptr;
        if (FAILED(factory->CreateDecoderFromFilename(
                nativePath.c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnLoad, &rawDecoder))) {
            return nullptr;
        }
        ComPtr<IWICBitmapDecoder> decoder(rawDecoder);

        IWICBitmapFrameDecode* rawFrame = nullptr;
        if (FAILED(decoder->GetFrame(0, &rawFrame))) return nullptr;
        ComPtr<IWICBitmapFrameDecode> frame(rawFrame);

        UINT width = 0;
        UINT height = 0;
        if (FAILED(frame->GetSize(&width, &height)) || width == 0 ||
            height == 0 || width > kMaximumImageDimension ||
            height > kMaximumImageDimension) {
            return nullptr;
        }

        const std::uint64_t pixels = static_cast<std::uint64_t>(width) * height;
        if (pixels == 0 || pixels > kMaximumImagePixels ||
            pixels > kMaximumImageBytes / 4ULL) {
            return nullptr;
        }
        const std::uint64_t stride64 = static_cast<std::uint64_t>(width) * 4ULL;
        const std::uint64_t bufferSize64 = stride64 * height;
        if (stride64 > std::numeric_limits<UINT>::max() ||
            bufferSize64 > std::numeric_limits<UINT>::max()) {
            return nullptr;
        }

        IWICFormatConverter* rawConverter = nullptr;
        if (FAILED(factory->CreateFormatConverter(&rawConverter))) {
            return nullptr;
        }
        ComPtr<IWICFormatConverter> converter(rawConverter);
        if (FAILED(converter->Initialize(
                frame.get(), GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0,
                WICBitmapPaletteTypeCustom))) {
            return nullptr;
        }

        auto surface = std::make_shared<NativeImageSurface>(width, height);
        if (!surface->IsValid()) return nullptr;
        if (FAILED(converter->CopyPixels(
                nullptr, static_cast<UINT>(stride64),
                static_cast<UINT>(bufferSize64),
                static_cast<BYTE*>(surface->GetPixels())))) {
            return nullptr;
        }
        return surface;
    }

    ComPtr<IWICImagingFactory> factory;
};

WindowsBackend::WindowsBackend(std::shared_ptr<ApplicationState> application)
    : application_(std::move(application)),
      instance_(GetModuleHandleW(nullptr)),
      commonControlsReady_(InitializeCommonControls()) {
    // COM and the decoder factory live for the backend lifetime. Painting
    // never performs apartment initialization or codec discovery.
    comInitialization_ = std::make_unique<ComInitialization>();
    imageDecoder_ = std::make_unique<NativeImageDecoder>();
}

WindowsBackend::~WindowsBackend() {
    Shutdown();
}

bool WindowsBackend::RegisterWindowClass() {
    if (classAtom_ != 0) {
        return RegisterImageWindowClass() && RegisterScrollViewClass();
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &WindowsBackend::WindowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    classAtom_ = RegisterClassExW(&windowClass);
    if (classAtom_ != 0) {
        registeredClass_ = true;
        return RegisterImageWindowClass() && RegisterScrollViewClass();
    }

    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS &&
        GetClassInfoExW(instance_, kWindowClassName, &windowClass) != FALSE) {
        classAtom_ = 1;
        return RegisterImageWindowClass() && RegisterScrollViewClass();
    }
    return false;
}

bool WindowsBackend::RegisterScrollViewClass() {
    if (scrollViewClassAtom_ != 0) return true;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &WindowsBackend::ScrollViewWindowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kScrollViewWindowClassName;

    scrollViewClassAtom_ = RegisterClassExW(&windowClass);
    if (scrollViewClassAtom_ != 0) {
        scrollViewClassRegistered_ = true;
        return true;
    }

    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS &&
        GetClassInfoExW(instance_, kScrollViewWindowClassName,
                        &windowClass) != FALSE) {
        scrollViewClassAtom_ = 1;
        return true;
    }
    return false;
}

bool WindowsBackend::RegisterImageWindowClass() {
    if (imageClassAtom_ != 0) return true;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &WindowsBackend::ImageWindowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kImageWindowClassName;

    imageClassAtom_ = RegisterClassExW(&windowClass);
    if (imageClassAtom_ != 0) {
        imageClassRegistered_ = true;
        return true;
    }

    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS &&
        GetClassInfoExW(instance_, kImageWindowClassName, &windowClass) != FALSE) {
        imageClassAtom_ = 1;
        return true;
    }
    return false;
}

WindowsBackend::WindowBinding* WindowsBackend::FindWindowBinding(HWND hwnd) noexcept {
    const auto iterator = windows_.find(hwnd);
    return iterator == windows_.end() ? nullptr : iterator->second.get();
}

WindowsBackend::WindowBinding* WindowsBackend::FindWindowBinding(
    const std::shared_ptr<WindowState>& window) noexcept {
    for (const auto& [hwnd, binding] : windows_) {
        (void)hwnd;
        if (binding->model == window) return binding.get();
    }
    return nullptr;
}

void WindowsBackend::DestroyToolTips(WindowBinding& binding) noexcept {
    for (const auto& child : binding.children) {
        SetControlSubclassToolTip(child.hwnd, nullptr);
    }
    if (binding.toolTip && IsWindow(binding.toolTip) != FALSE) {
        for (const auto& tool : binding.toolTips) {
            if (tool) {
                SendMessageW(binding.toolTip, TTM_DELTOOLW, 0,
                             reinterpret_cast<LPARAM>(&tool->info));
            }
        }
        DestroyWindow(binding.toolTip);
    }
    binding.toolTip = nullptr;
    binding.toolTips.clear();
}

void WindowsBackend::RebuildToolTips(WindowBinding& binding) {
    DestroyToolTips(binding);
    if (!commonControlsReady_ || !binding.hwnd || !binding.model ||
        !binding.model->content) {
        return;
    }

    std::vector<std::shared_ptr<ControlState>> controls;
    CollectAllControls(binding.model->content, controls);
    bool hasToolTips = false;
    for (const auto& control : controls) {
        if (control && !control->toolTip.empty()) {
            hasToolTips = true;
            break;
        }
    }
    if (!hasToolTips) return;

    HWND toolTip = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, binding.hwnd, nullptr, instance_, nullptr);
    if (!toolTip) throw std::runtime_error("Native ToolTip creation failed");
    binding.toolTip = toolTip;

    try {
        for (const auto& child : binding.children) {
            const auto control = child.model.lock();
            if (!control || control->toolTip.empty() || !child.hwnd) continue;

            auto tool = std::make_unique<ToolTipBinding>();
            tool->model = control;
            tool->text = Utf8ToWide(control->toolTip);
            tool->info.cbSize = TTTOOLINFOW_V1_SIZE;
            tool->info.uFlags = 0;
            tool->info.hwnd = binding.hwnd;
            tool->info.uId = reinterpret_cast<UINT_PTR>(child.hwnd);
            tool->info.hinst = instance_;
            tool->info.lpszText = tool->text.data();
            UpdateToolTipRect(tool->info, binding.hwnd, child.hwnd);
            const LRESULT added = SendMessageW(
                binding.toolTip, TTM_ADDTOOLW, 0,
                reinterpret_cast<LPARAM>(&tool->info));
            if (added == FALSE) {
                throw std::runtime_error("Native ToolTip registration failed");
            }
            binding.toolTips.push_back(std::move(tool));
        }
        for (const auto& child : binding.children) {
            SetControlSubclassToolTip(child.hwnd, binding.toolTip);
        }
    } catch (...) {
        DestroyToolTips(binding);
        throw;
    }
}

void WindowsBackend::DestroyStatusBar(WindowBinding& binding) noexcept {
    if (binding.statusBar && IsWindow(binding.statusBar) != FALSE) {
        DestroyWindow(binding.statusBar);
    }
    binding.statusBar = nullptr;
    binding.statusBarModel.reset();
}

void WindowsBackend::RebuildStatusBar(WindowBinding& binding) {
    const auto desired = binding.model ? binding.model->statusBar : nullptr;
    if (binding.statusBar && binding.statusBarModel == desired) return;

    DestroyStatusBar(binding);
    if (!desired || !commonControlsReady_ || !binding.hwnd) return;

    const HWND statusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, binding.hwnd, nullptr, instance_, nullptr);
    if (!statusBar) throw std::runtime_error("Native StatusBar creation failed");
    binding.statusBar = statusBar;
    binding.statusBarModel = desired;
    int parts[] = {-1};
    SendMessageW(statusBar, SB_SETPARTS, 1,
                 reinterpret_cast<LPARAM>(parts));
}

void WindowsBackend::SynchronizeStatusBar(WindowBinding& binding) {
    const auto& statusBar = binding.statusBarModel;
    if (!binding.statusBar || !statusBar) return;
    const std::wstring text = Utf8ToWide(statusBar->text);
    SendMessageW(
        binding.statusBar, SB_SETTEXTW, 0,
        reinterpret_cast<LPARAM>(text.c_str()));
}

int WindowsBackend::GetStatusBarHeight(
    const WindowBinding& binding) const noexcept {
    if (!binding.statusBar || IsWindow(binding.statusBar) == FALSE) return 0;

    HDC dc = GetDC(binding.statusBar);
    if (!dc) return std::max(18, GetSystemMetrics(SM_CYVSCROLL));
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(
        binding.statusBar, WM_GETFONT, 0, 0));
    if (!font) font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;
    TEXTMETRICW metrics{};
    const BOOL succeeded = GetTextMetricsW(dc, &metrics);
    if (previous) SelectObject(dc, previous);
    ReleaseDC(binding.statusBar, dc);
    if (!succeeded) return std::max(18, GetSystemMetrics(SM_CYVSCROLL));

    const int border = std::max(1, GetSystemMetrics(SM_CYBORDER));
    const int preferred = std::max(18, static_cast<int>(metrics.tmHeight) +
                                        border * 4);
    RECT current{};
    if (GetWindowRect(binding.statusBar, &current) &&
        current.bottom - current.top > preferred) {
        return current.bottom - current.top;
    }

    RECT parentClient{};
    GetClientRect(binding.hwnd, &parentClient);
    MoveWindow(binding.statusBar, 0, 0,
               std::max<LONG>(1L, parentClient.right - parentClient.left),
               preferred, FALSE);
    if (GetWindowRect(binding.statusBar, &current)) {
        return std::max(preferred,
                        static_cast<int>(current.bottom - current.top));
    }
    return preferred;
}

UINT WindowsBackend::AllocateMenuCommandId() {
    constexpr std::uint32_t firstMenuCommandId = 0x4000;
    constexpr std::uint32_t lastMenuCommandId = 0xFFFF;
    if (nextMenuCommandId_ < firstMenuCommandId ||
        nextMenuCommandId_ > lastMenuCommandId) {
        throw std::runtime_error("Menu command ID space is exhausted");
    }
    return static_cast<UINT>(nextMenuCommandId_++);
}

void WindowsBackend::DestroyMenuBar(WindowBinding& binding) noexcept {
    if (binding.hwnd && IsWindow(binding.hwnd) != FALSE) {
        SetMenu(binding.hwnd, nullptr);
        DrawMenuBar(binding.hwnd);
    }
    if (binding.menu) DestroyMenu(binding.menu);
    if (binding.accelerators) DestroyAcceleratorTable(binding.accelerators);
    binding.menu = nullptr;
    binding.accelerators = nullptr;
    binding.menuCommands.clear();
    binding.menuOpenings.clear();
    binding.menuRefreshPending = false;
}

void WindowsBackend::RebuildMenuBar(WindowBinding& binding) {
    HMENU newMenu = nullptr;
    HACCEL newAccelerators = nullptr;
    std::unordered_map<UINT, std::weak_ptr<MenuItemState>> newCommands;
    std::unordered_map<HMENU, std::weak_ptr<MenuState>> newOpenings;
    std::vector<ACCEL> acceleratorEntries;

    try {
        const auto menuBar = binding.model ? binding.model->menuBar : nullptr;
        if (menuBar && !menuBar->menus.empty()) {
            newMenu = CreateMenu();
            if (!newMenu) throw std::runtime_error("Native menu creation failed");

            std::function<HMENU(const std::shared_ptr<MenuState>&)> buildMenu;
            buildMenu = [&](const std::shared_ptr<MenuState>& menu) {
                if (!menu) throw std::logic_error("Invalid Menu state");
                HMENU nativeMenu = CreatePopupMenu();
                if (!nativeMenu) {
                    throw std::runtime_error("Native submenu creation failed");
                }
                try {
                    newOpenings.emplace(nativeMenu, menu);
                    for (const auto& entry : menu->entries) {
                        if (entry.separator) {
                            if (!AppendMenuW(nativeMenu, MF_SEPARATOR, 0, nullptr)) {
                                throw std::runtime_error(
                                    "Native menu separator creation failed");
                            }
                            continue;
                        }
                        if (entry.menu) {
                            const HMENU childMenu = buildMenu(entry.menu);
                            const std::wstring text = Utf8ToWide(entry.menu->text);
                            if (!AppendMenuW(nativeMenu, MF_POPUP,
                                             reinterpret_cast<UINT_PTR>(childMenu),
                                             text.c_str())) {
                                DestroyMenu(childMenu);
                                throw std::runtime_error(
                                    "Native submenu insertion failed");
                            }
                            continue;
                        }
                        if (!entry.item) {
                            throw std::logic_error("Invalid Menu entry state");
                        }

                        const UINT commandId = AllocateMenuCommandId();
                        std::wstring text = Utf8ToWide(entry.item->text);
                        if (entry.item->shortcut) {
                            text += L"\tCtrl+";
                            if (entry.item->shortcut->HasShift()) text += L"Shift+";
                            text.push_back(static_cast<wchar_t>(
                                entry.item->shortcut->GetLetter()));
                            acceleratorEntries.push_back(ACCEL{
                                static_cast<BYTE>(FVIRTKEY | FCONTROL |
                                    (entry.item->shortcut->HasShift() ? FSHIFT : 0)),
                                static_cast<WORD>(entry.item->shortcut->GetLetter()),
                                static_cast<WORD>(commandId)});
                        }
                        UINT flags = MF_STRING;
                        if (!entry.item->enabled) flags |= MF_GRAYED;
                        if (entry.item->checked) flags |= MF_CHECKED;
                        if (!AppendMenuW(nativeMenu, flags, commandId, text.c_str())) {
                            throw std::runtime_error(
                                "Native menu command insertion failed");
                        }
                        newCommands.emplace(commandId, entry.item);
                    }
                } catch (...) {
                    DestroyMenu(nativeMenu);
                    throw;
                }
                return nativeMenu;
            };

            for (const auto& menu : menuBar->menus) {
                const HMENU nativeMenu = buildMenu(menu);
                const std::wstring text = Utf8ToWide(menu->text);
                if (!AppendMenuW(newMenu, MF_POPUP,
                                 reinterpret_cast<UINT_PTR>(nativeMenu),
                                 text.c_str())) {
                    DestroyMenu(nativeMenu);
                    throw std::runtime_error("Native menu insertion failed");
                }
            }

            if (!acceleratorEntries.empty()) {
                newAccelerators = CreateAcceleratorTableW(
                    acceleratorEntries.data(),
                    static_cast<int>(acceleratorEntries.size()));
                if (!newAccelerators) {
                    throw std::runtime_error("Native accelerator creation failed");
                }
            }
        }

        if (SetMenu(binding.hwnd, newMenu) == FALSE) {
            throw std::runtime_error("Native menu attachment failed");
        }

        const HMENU oldMenu = binding.menu;
        const HACCEL oldAccelerators = binding.accelerators;
        binding.menu = newMenu;
        binding.accelerators = newAccelerators;
        binding.menuCommands = std::move(newCommands);
        binding.menuOpenings = std::move(newOpenings);
        newMenu = nullptr;
        newAccelerators = nullptr;
        if (oldMenu) DestroyMenu(oldMenu);
        if (oldAccelerators) DestroyAcceleratorTable(oldAccelerators);
        DrawMenuBar(binding.hwnd);
    } catch (...) {
        if (newMenu) DestroyMenu(newMenu);
        if (newAccelerators) DestroyAcceleratorTable(newAccelerators);
        throw;
    }
}

bool WindowsBackend::ShowWindow(const std::shared_ptr<WindowState>& window) {
    if (shutdown_ || !window || !RegisterWindowClass()) return false;
    if (auto* existing = FindWindowBinding(window)) {
        ::ShowWindow(existing->hwnd, SW_SHOWNORMAL);
        UpdateWindow(existing->hwnd);
        return true;
    }

    const DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    const DWORD extendedStyle = WS_EX_APPWINDOW;
    const bool hasMenu = window->menuBar && !window->menuBar->menus.empty();
    RECT desiredClient{0, 0, SaturatingDimension(window->width),
                       SaturatingDimension(window->height)};
    if (!AdjustWindowRectEx(&desiredClient, style, hasMenu, extendedStyle)) return false;

    auto binding = std::make_unique<WindowBinding>();
    binding->model = window;
    HWND hwnd = CreateWindowExW(
        extendedStyle,
        kWindowClassName,
        Utf8ToWide(window->title).c_str(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        desiredClient.right - desiredClient.left,
        desiredClient.bottom - desiredClient.top,
        nullptr,
        nullptr,
        instance_,
        this);
    if (!hwnd) return false;

    binding->hwnd = hwnd;
    windows_.emplace(hwnd, std::move(binding));
    window->shown = true;
    window->closeState = WindowCloseState::Open;
    RefreshWindow(window);
    ::ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    ResizeWindow(window);
    if (auto* current = FindWindowBinding(window)) LayoutControls(*current);
    return true;
}

bool WindowsBackend::FocusControl(
    const std::shared_ptr<WindowState>& window,
    const std::shared_ptr<ControlState>& control) {
    auto* binding = FindWindowBinding(window);
    if (!binding || !binding->hwnd || !control || !control->enabled ||
        IsWindow(binding->hwnd) == FALSE) {
        return false;
    }

    for (const auto& child : binding->children) {
        if (child.model.lock() != control || !child.hwnd ||
            IsWindow(child.hwnd) == FALSE ||
            IsWindowEnabled(child.hwnd) == FALSE ||
            IsWindowVisible(child.hwnd) == FALSE) {
            continue;
        }
        SetActiveWindow(binding->hwnd);
        SetFocus(child.hwnd);
        if (GetFocus() == child.hwnd) {
            SetNativeFocus(window, control);
            return true;
        }
        return false;
    }
    return false;
}

void WindowsBackend::RefreshWindow(const std::shared_ptr<WindowState>& window) {
    auto* binding = FindWindowBinding(window);
    if (!binding || !binding->hwnd || !window) return;

    try {
        // Registration is the opt-in switch. WM_DROPFILES remains private to
        // this backend; clearing the callback turns shell acceptance off.
        DragAcceptFiles(binding->hwnd, window->onFilesDropped ? TRUE : FALSE);
        const std::wstring title = Utf8ToWide(window->title);
        SetWindowTextW(binding->hwnd, title.c_str());
        if (binding->menuOpening) {
            binding->menuRefreshPending = true;
        } else {
            RebuildMenuBar(*binding);
        }

        RebuildStatusBar(*binding);
        DestroyToolTips(*binding);

        const auto content = window->content;
        std::vector<std::shared_ptr<ControlState>> controls;
        CollectAllControls(content, controls);
        bool rebuild = binding->children.size() != controls.size();
        if (!rebuild) {
            for (std::size_t index = 0; index < controls.size(); ++index) {
                const auto& child = controls[index];
                if (binding->children[index].model.lock() != child) {
                    rebuild = true;
                    break;
                }
            }
        }

        if (rebuild) {
            auto* current = FindWindowBinding(window);
            if (!current) return;
            RebuildControls(*current);
        }

        for (std::size_t index = 0; index < controls.size(); ++index) {
            auto* current = FindWindowBinding(window);
            if (!current || index >= current->children.size()) return;

            const auto control = current->children[index].model.lock();
            if (!control) continue;
            const HWND childHwnd = current->children[index].hwnd;
            EnableWindow(childHwnd, control->enabled ? TRUE : FALSE);
            if (control->kind == ControlKind::ListBox) {
                SynchronizeListBox(current->children[index], *control);
                continue;
            }
            if (control->kind == ControlKind::ComboBox) {
                SynchronizeComboBox(current->children[index], *control);
                continue;
            }
            if (control->kind == ControlKind::ProgressBar) {
                SynchronizeProgressBar(current->children[index], *control);
                continue;
            }
            if (control->kind == ControlKind::Slider) {
                SynchronizeSlider(current->children[index], *control);
                continue;
            }
            if (control->kind == ControlKind::TabView) {
                SynchronizeTabView(current->children[index], *control);
                continue;
            }
            if (control->kind == ControlKind::Image) {
                SynchronizeImage(current->children[index], *control);
                continue;
            }
            if (control->kind == ControlKind::CheckBox) {
                SynchronizeCheckBox(current->children[index], *control);
            } else if (control->kind == ControlKind::RadioButton) {
                SynchronizeRadioButton(current->children[index], *control);
            } else if (control->kind == ControlKind::TextArea) {
                SynchronizeTextAreaProperties(current->children[index], *control);
            }
            const std::wstring desiredText = Utf8ToWide(control->text);

            if (IsTextEdit(control->kind)) {
                const TextRange desiredSelection = control->selection;
                const std::size_t desiredCaret = control->caretIndex;
                // SetWindowTextW on an EDIT may synchronously emit EN_CHANGE.
                // Keep that native notification private while applying the
                // already-authoritative model value.
                const std::wstring nativeText = ReadNativeText(childHwnd);
                const std::wstring comparableNativeText =
                    control->kind == ControlKind::TextArea
                    ? NormalizeNativeTextAreaNewlines(nativeText) : nativeText;
                if (comparableNativeText != desiredText) {
                    auto* live = FindWindowBinding(window);
                    if (!live) return;
                    ChildBinding* childBinding = nullptr;
                    for (auto& candidate : live->children) {
                        if (candidate.hwnd == childHwnd) {
                            childBinding = &candidate;
                            break;
                        }
                    }
                    if (!childBinding) return;
                    childBinding->synchronizingText = true;
                    SetWindowTextW(childHwnd, desiredText.c_str());
                    live = FindWindowBinding(window);
                    if (!live) return;
                    for (auto& candidate : live->children) {
                        if (candidate.hwnd == childHwnd) {
                            candidate.synchronizingText = false;
                            break;
                        }
                    }
                    // WM_SETTEXT normally collapses the native caret to the
                    // beginning. The subclass observes that private write;
                    // restore the model-authoritative position before the
                    // selection synchronization below.
                    control->selection = desiredSelection;
                    control->caretIndex = desiredCaret;
                }

                auto* live = FindWindowBinding(window);
                if (!live) return;
                for (auto& candidate : live->children) {
                    if (candidate.hwnd == childHwnd) {
                        SynchronizeTextEdit(candidate, *control);
                        break;
                    }
                }
                continue;
            }

            if (ReadNativeText(childHwnd) == desiredText) continue;
            SetWindowTextW(childHwnd, desiredText.c_str());
        }
        if (auto* current = FindWindowBinding(window)) {
            SynchronizeStatusBar(*current);
            ResizeWindow(window);
            if (auto* live = FindWindowBinding(window)) {
                LayoutControls(*live);
                RebuildToolTips(*live);
                if (auto* refreshed = FindWindowBinding(window)) {
                    LayoutControls(*refreshed);
                }
            }
        }
    } catch (const std::exception&) {
        // Public mutators are intentionally simple. If a later mutation
        // contains malformed UTF-8, keep the native window alive and leave
        // its previous text in place rather than throwing through WndProc.
    } catch (...) {
    }
}

void WindowsBackend::RefreshMenuBar(const std::shared_ptr<WindowState>& window) {
    auto* binding = FindWindowBinding(window);
    if (!binding || !binding->hwnd || !window) return;
    if (binding->menuOpening) {
        binding->menuRefreshPending = true;
        return;
    }
    try {
        RebuildMenuBar(*binding);
        ResizeWindow(window);
        if (auto* current = FindWindowBinding(window)) LayoutControls(*current);
    } catch (const std::exception&) {
        // Keep the last valid native menu if a later model mutation cannot be
        // realized. Public model state remains authoritative and will be
        // retried by the next explicit window refresh or Show().
    }
}

void WindowsBackend::ResizeWindow(const std::shared_ptr<WindowState>& window) {
    auto* binding = FindWindowBinding(window);
    if (!binding || !binding->hwnd || !window) return;

    const int statusHeight = GetStatusBarHeight(*binding);
    RECT desiredClient{0, 0, SaturatingDimension(window->width),
                       SaturatingDimension(
                           static_cast<int>(static_cast<std::int64_t>(
                               SaturatingDimension(window->height)) +
                               statusHeight))};
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(binding->hwnd, GWL_STYLE));
    const DWORD extendedStyle = static_cast<DWORD>(GetWindowLongPtrW(binding->hwnd, GWL_EXSTYLE));
    if (!AdjustWindowRectEx(&desiredClient, style, binding->menu != nullptr,
                            extendedStyle)) return;
    SetWindowPos(binding->hwnd, nullptr, 0, 0,
                 desiredClient.right - desiredClient.left,
                 desiredClient.bottom - desiredClient.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowsBackend::RebuildControls(WindowBinding& binding) {
    for (const auto& child : binding.children) {
        if (child.hwnd && child.parent == binding.hwnd &&
            IsWindow(child.hwnd) != FALSE) {
            DestroyWindow(child.hwnd);
        }
    }
    for (const auto& child : binding.children) {
        if (child.hwnd && IsWindow(child.hwnd) != FALSE) {
            DestroyWindow(child.hwnd);
        }
    }
    binding.children.clear();

    if (!binding.model || !binding.model->content) return;

    const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    std::shared_ptr<RadioGroupState> previousRadioGroup;
    std::vector<std::shared_ptr<ControlState>> controls;
    CollectAllControls(binding.model->content, controls);
    for (const auto& control : controls) {
        if (!control) continue;

        const bool isButton = control->kind == ControlKind::Button;
        const bool isCheckBox = control->kind == ControlKind::CheckBox;
        const bool isTextBox = control->kind == ControlKind::TextBox;
        const bool isTextArea = control->kind == ControlKind::TextArea;
        const bool isListBox = control->kind == ControlKind::ListBox;
        const bool isComboBox = control->kind == ControlKind::ComboBox;
        const bool isProgressBar = control->kind == ControlKind::ProgressBar;
        const bool isSlider = control->kind == ControlKind::Slider;
        const bool isRadioButton = control->kind == ControlKind::RadioButton;
        const bool isTabView = control->kind == ControlKind::TabView;
        const bool isImage = control->kind == ControlKind::Image;
        const bool isScrollView = control->kind == ControlKind::ScrollView;
        const bool isInteractive = isButton || isCheckBox || isTextBox ||
            isTextArea || isListBox || isComboBox || isRadioButton || isSlider ||
            isTabView;
        DWORD style = WS_CHILD | WS_VISIBLE |
            (isScrollView ? WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL : 0) |
            (isInteractive ? WS_TABSTOP : (isProgressBar || isImage ? 0 : SS_LEFT)) |
            (isCheckBox ? BS_AUTOCHECKBOX | BS_LEFT | BS_VCENTER : 0) |
            (isRadioButton ? BS_AUTORADIOBUTTON | BS_LEFT | BS_VCENTER : 0) |
            (isTextBox ? ES_AUTOHSCROLL | ES_LEFT : 0) |
            (isTextArea ? ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN |
                 ES_LEFT | WS_VSCROLL |
                 (control->wordWrap ? 0 : ES_AUTOHSCROLL | WS_HSCROLL) : 0) |
            (isTextArea && control->readOnly ? ES_READONLY : 0) |
            (isListBox ? LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL : 0) |
            (isComboBox ? CBS_DROPDOWNLIST | CBS_HASSTRINGS | CBS_AUTOHSCROLL |
                         WS_VSCROLL : 0) |
            (isProgressBar ? PBS_MARQUEE : 0) |
            (isSlider ? TBS_HORZ : 0) |
            (isTabView ? TCS_TABS : 0);
        if (isRadioButton) {
            const auto group = control->radioGroup.lock();
            if (group != previousRadioGroup) style |= WS_GROUP;
            previousRadioGroup = group;
        } else {
            previousRadioGroup.reset();
        }

        const int commandId = isButton || isCheckBox || isRadioButton
            ? nextControlId_++
            : 0;
        const wchar_t* nativeClass = L"STATIC";
        if (isButton || isCheckBox || isRadioButton) {
            nativeClass = L"BUTTON";
        } else if (isTextBox || isTextArea) {
            nativeClass = L"EDIT";
        } else if (isListBox) {
            nativeClass = L"LISTBOX";
        } else if (isComboBox) {
            nativeClass = L"COMBOBOX";
        } else if (isProgressBar) {
            nativeClass = PROGRESS_CLASSW;
        } else if (isSlider) {
            nativeClass = TRACKBAR_CLASSW;
        } else if (isTabView) {
            nativeClass = WC_TABCONTROLW;
        } else if (isImage) {
            nativeClass = kImageWindowClassName;
        } else if (isScrollView) {
            nativeClass = kScrollViewWindowClassName;
        }

        std::unique_ptr<NativeImageWindowContext> imageContext;
        if (isImage) {
            imageContext = std::make_unique<NativeImageWindowContext>();
            imageContext->model = control;
        }
        HWND parent = binding.hwnd;
        if (const auto scrollView = FindOwningScrollView(control)) {
            for (const auto& candidate : binding.children) {
                if (candidate.model.lock() == scrollView) {
                    parent = candidate.hwnd;
                    break;
                }
            }
        }
        std::unique_ptr<NativeScrollViewContext> scrollContext;
        if (isScrollView) {
            scrollContext = std::make_unique<NativeScrollViewContext>();
            scrollContext->backend = this;
            scrollContext->window = binding.model;
            scrollContext->model = control;
        }
        HWND child = CreateWindowExW(
            isScrollView ? WS_EX_CONTROLPARENT
                         : (isTextBox || isTextArea || isListBox
                                ? WS_EX_CLIENTEDGE : 0),
            nativeClass,
            isScrollView ? L"" : Utf8ToWide(control->text).c_str(),
            style,
            0,
            0,
            0,
            0,
            parent,
            !isScrollView && commandId != 0
                ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(commandId))
                : nullptr,
            instance_,
            scrollContext ? static_cast<void*>(scrollContext.get())
                          : static_cast<void*>(imageContext.get()));
        if (!child) continue;

        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        EnableWindow(child, control->enabled ? TRUE : FALSE);
        ChildBinding childBinding{control, child, parent, commandId};
        childBinding.nativeReadOnly = isTextArea && control->readOnly;
        childBinding.nativeWordWrap = !isTextArea || control->wordWrap;
        childBinding.imageContext = std::move(imageContext);
        childBinding.scrollContext = std::move(scrollContext);
        binding.children.push_back(std::move(childBinding));
        if (!isScrollView) {
            InstallControlSubclass(child, control, binding.model);
        }
    }
}

void WindowsBackend::SynchronizeTextEdit(ChildBinding& binding,
                                         const ControlState& control) {
    struct SelectionSynchronizationGuard {
        bool& value;
        bool previous;

        ~SelectionSynchronizationGuard() { value = previous; }
    } guard{binding.synchronizingSelection, binding.synchronizingSelection};
    binding.synchronizingSelection = true;

    const std::wstring nativeText = ReadNativeText(binding.hwnd);
    const std::size_t end = control.selection.start + control.selection.length;
    const auto nativeOffsetForScalar = [&](std::size_t index) {
        return control.kind == ControlKind::TextArea
            ? NativeTextAreaOffsetForScalarIndex(nativeText, index)
            : Utf16CodeUnitOffsetForScalarIndex(nativeText, index);
    };
    const std::size_t nativeStart = nativeOffsetForScalar(control.selection.start);
    const std::size_t nativeEnd = nativeOffsetForScalar(end);
    SendMessageW(binding.hwnd, EM_SETSEL,
                 static_cast<WPARAM>(nativeStart),
                 static_cast<LPARAM>(nativeEnd));
}

void WindowsBackend::SynchronizeTextAreaProperties(
    ChildBinding& binding, const ControlState& control) {
    if (control.kind != ControlKind::TextArea) return;

    if (binding.nativeReadOnly != control.readOnly) {
        SendMessageW(binding.hwnd, EM_SETREADONLY,
                     static_cast<WPARAM>(control.readOnly ? TRUE : FALSE), 0);
        binding.nativeReadOnly = control.readOnly;
    }

    if (binding.nativeWordWrap != control.wordWrap) {
        LONG_PTR style = GetWindowLongPtrW(binding.hwnd, GWL_STYLE);
        const LONG_PTR horizontalStyles =
            static_cast<LONG_PTR>(ES_AUTOHSCROLL) |
            static_cast<LONG_PTR>(WS_HSCROLL);
        if (control.wordWrap) {
            style &= ~horizontalStyles;
        } else {
            style |= horizontalStyles;
        }
        SetWindowLongPtrW(binding.hwnd, GWL_STYLE, style);
        SetWindowPos(binding.hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOACTIVATE | SWP_FRAMECHANGED);
        InvalidateRect(binding.hwnd, nullptr, TRUE);
        binding.nativeWordWrap = control.wordWrap;
    }
}

void WindowsBackend::SynchronizeCheckBox(ChildBinding& binding,
                                         const ControlState& control) {
    const LRESULT nativeChecked = SendMessageW(binding.hwnd, BM_GETCHECK, 0, 0);
    const LRESULT desiredChecked = control.checked ? BST_CHECKED : BST_UNCHECKED;
    if (nativeChecked == desiredChecked) return;

    const bool previous = binding.synchronizingCheck;
    binding.synchronizingCheck = true;
    SendMessageW(binding.hwnd, BM_SETCHECK,
                 static_cast<WPARAM>(desiredChecked), 0);
    binding.synchronizingCheck = previous;
}

void WindowsBackend::SynchronizeListBox(ChildBinding& binding,
                                        const ControlState& control) {
    struct SelectionSynchronizationGuard {
        bool& value;
        bool previous;

        ~SelectionSynchronizationGuard() { value = previous; }
    } guard{binding.synchronizingSelection, binding.synchronizingSelection};
    binding.synchronizingSelection = true;

    const auto& desired = control.items;

    const auto resetNativeItems = [&]() {
        SendMessageW(binding.hwnd, LB_RESETCONTENT, 0, 0);
        for (const auto& item : desired) {
            const std::wstring wide = Utf8ToWide(item);
            const LRESULT result = SendMessageW(
                binding.hwnd, LB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(wide.c_str()));
            if (result == LB_ERR || result == LB_ERRSPACE) return false;
        }
        binding.nativeItems = desired;
        return true;
    };

    if (binding.nativeItems != desired) {
        bool appliedNarrowChange = false;

        if (desired.size() == binding.nativeItems.size() + 1) {
            std::size_t insertionIndex = 0;
            while (insertionIndex < binding.nativeItems.size() &&
                   binding.nativeItems[insertionIndex] == desired[insertionIndex]) {
                ++insertionIndex;
            }
            bool matchesAfterInsertion = true;
            for (std::size_t oldIndex = insertionIndex;
                 oldIndex < binding.nativeItems.size(); ++oldIndex) {
                if (binding.nativeItems[oldIndex] != desired[oldIndex + 1]) {
                    matchesAfterInsertion = false;
                    break;
                }
            }
            if (matchesAfterInsertion) {
                const std::wstring wide = Utf8ToWide(desired[insertionIndex]);
                const LRESULT result = SendMessageW(
                    binding.hwnd, LB_INSERTSTRING,
                    static_cast<WPARAM>(insertionIndex),
                    reinterpret_cast<LPARAM>(wide.c_str()));
                if (result != LB_ERR && result != LB_ERRSPACE) {
                    binding.nativeItems.insert(
                        binding.nativeItems.begin() +
                            static_cast<std::ptrdiff_t>(insertionIndex),
                        desired[insertionIndex]);
                    appliedNarrowChange = true;
                }
            }
        } else if (binding.nativeItems.size() == desired.size() + 1) {
            std::size_t removalIndex = 0;
            while (removalIndex < desired.size() &&
                   binding.nativeItems[removalIndex] == desired[removalIndex]) {
                ++removalIndex;
            }
            bool matchesAfterRemoval = true;
            for (std::size_t newIndex = removalIndex;
                 newIndex < desired.size(); ++newIndex) {
                if (binding.nativeItems[newIndex + 1] != desired[newIndex]) {
                    matchesAfterRemoval = false;
                    break;
                }
            }
            if (matchesAfterRemoval) {
                const LRESULT result = SendMessageW(
                    binding.hwnd, LB_DELETESTRING,
                    static_cast<WPARAM>(removalIndex), 0);
                if (result != LB_ERR) {
                    binding.nativeItems.erase(
                        binding.nativeItems.begin() +
                            static_cast<std::ptrdiff_t>(removalIndex));
                    appliedNarrowChange = true;
                }
            }
        } else if (binding.nativeItems.size() == desired.size()) {
            std::size_t changedIndex = desired.size();
            std::size_t differenceCount = 0;
            for (std::size_t index = 0; index < desired.size(); ++index) {
                if (binding.nativeItems[index] != desired[index]) {
                    changedIndex = index;
                    ++differenceCount;
                }
            }
            if (differenceCount == 1) {
                const LRESULT deleted = SendMessageW(
                    binding.hwnd, LB_DELETESTRING,
                    static_cast<WPARAM>(changedIndex), 0);
                const std::wstring wide = Utf8ToWide(desired[changedIndex]);
                const LRESULT inserted = deleted == LB_ERR
                    ? LB_ERR
                    : SendMessageW(binding.hwnd, LB_INSERTSTRING,
                                    static_cast<WPARAM>(changedIndex),
                                    reinterpret_cast<LPARAM>(wide.c_str()));
                if (inserted != LB_ERR && inserted != LB_ERRSPACE) {
                    binding.nativeItems[changedIndex] = desired[changedIndex];
                    appliedNarrowChange = true;
                }
            }
        }

        if (!appliedNarrowChange && !resetNativeItems()) return;
    }

    const LRESULT nativeSelection = SendMessageW(
        binding.hwnd, LB_GETCURSEL, 0, 0);
    const LRESULT desiredSelection = control.selectedIndex
        ? static_cast<LRESULT>(*control.selectedIndex)
        : static_cast<LRESULT>(LB_ERR);
    if (nativeSelection != desiredSelection) {
        SendMessageW(binding.hwnd, LB_SETCURSEL,
                     static_cast<WPARAM>(desiredSelection), 0);
    }
}

void WindowsBackend::SynchronizeComboBox(ChildBinding& binding,
                                         const ControlState& control) {
    struct SelectionSynchronizationGuard {
        bool& value;
        bool previous;

        ~SelectionSynchronizationGuard() { value = previous; }
    } guard{binding.synchronizingSelection, binding.synchronizingSelection};
    binding.synchronizingSelection = true;

    const auto& desired = control.items;
    if (binding.nativeItems != desired) {
        SendMessageW(binding.hwnd, CB_RESETCONTENT, 0, 0);
        for (const auto& item : desired) {
            const std::wstring wide = Utf8ToWide(item);
            const LRESULT result = SendMessageW(
                binding.hwnd, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(wide.c_str()));
            if (result == CB_ERR || result == CB_ERRSPACE) return;
        }
        binding.nativeItems = desired;
    }

    const LRESULT nativeSelection = SendMessageW(
        binding.hwnd, CB_GETCURSEL, 0, 0);
    const LRESULT desiredSelection = control.selectedIndex
        ? static_cast<LRESULT>(*control.selectedIndex)
        : static_cast<LRESULT>(CB_ERR);
    if (nativeSelection != desiredSelection) {
        SendMessageW(binding.hwnd, CB_SETCURSEL,
                     static_cast<WPARAM>(desiredSelection), 0);
    }
}

void WindowsBackend::SynchronizeProgressBar(ChildBinding& binding,
                                            const ControlState& control) {
    if (control.kind != ControlKind::ProgressBar) return;

    // The portable model uses the same bounded signed integer domain as the
    // native 32-bit progress messages. No application-visible conversion is
    // needed, and the model has already enforced minimum <= value <= maximum.
    SendMessageW(binding.hwnd, PBM_SETRANGE32,
                 static_cast<WPARAM>(static_cast<std::intptr_t>(
                     control.minimum)),
                 static_cast<LPARAM>(control.maximum));
    SendMessageW(binding.hwnd, PBM_SETPOS,
                 static_cast<WPARAM>(static_cast<std::intptr_t>(control.value)),
                 0);
    SendMessageW(binding.hwnd, PBM_SETMARQUEE,
                 static_cast<WPARAM>(control.indeterminate ? TRUE : FALSE),
                 static_cast<LPARAM>(control.indeterminate ? 50 : 0));
}

void WindowsBackend::SynchronizeSlider(ChildBinding& binding,
                                       const ControlState& control) {
    if (control.kind != ControlKind::Slider) return;

    const bool previous = binding.synchronizingSlider;
    binding.synchronizingSlider = true;
    SendMessageW(binding.hwnd, TBM_SETRANGEMIN, TRUE,
                 static_cast<LPARAM>(control.minimum));
    SendMessageW(binding.hwnd, TBM_SETRANGEMAX, TRUE,
                 static_cast<LPARAM>(control.maximum));
    SendMessageW(binding.hwnd, TBM_SETPOS, TRUE,
                 static_cast<LPARAM>(control.value));
    binding.synchronizingSlider = previous;
}

void WindowsBackend::SynchronizeTabView(ChildBinding& binding,
                                        const ControlState& control) {
    if (control.kind != ControlKind::TabView || !control.tabView) return;

    const bool previous = binding.synchronizingTabSelection;
    binding.synchronizingTabSelection = true;
    TabCtrl_DeleteAllItems(binding.hwnd);
    for (const auto& page : control.tabView->pages) {
        if (!page) continue;
        const std::wstring title = Utf8ToWide(page->title);
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<wchar_t*>(title.c_str());
        TabCtrl_InsertItem(binding.hwnd,
                           TabCtrl_GetItemCount(binding.hwnd), &item);
    }
    const int desiredSelection = control.selectedIndex
        ? static_cast<int>(*control.selectedIndex) : -1;
    if (TabCtrl_GetCurSel(binding.hwnd) != desiredSelection) {
        TabCtrl_SetCurSel(binding.hwnd, desiredSelection);
    }
    binding.synchronizingTabSelection = previous;
}

void WindowsBackend::SynchronizeImage(ChildBinding& binding,
                                      const ControlState& control) {
    if (control.kind != ControlKind::Image || !binding.imageContext) return;

    const auto source = control.imageSource;
    if (!source) {
        binding.imageContext->surface.reset();
        binding.nativeImagePath.clear();
        binding.imageDecodeAttempted = false;
        if (control.imageLoadStatus != ImageLoadStatus::Empty) {
            SetImageLoadResult(binding.model.lock(), ImageLoadStatus::Empty,
                               0, 0, {});
        }
        InvalidateRect(binding.hwnd, nullptr, TRUE);
        return;
    }

    const std::string& path = source->GetFilePath();
    if (binding.imageDecodeAttempted && binding.nativeImagePath == path &&
        control.imageLoadStatus != ImageLoadStatus::Pending) {
        // A scale-mode mutation reaches RefreshWindow without changing the
        // source. Repaint explicitly so geometry changes do not depend on
        // MoveWindow deciding that an unchanged rectangle needs repainting.
        InvalidateRect(binding.hwnd, nullptr, TRUE);
        return;
    }

    // Clear before decoding so a failed replacement can never leave the old
    // image visible. The local shared pointer in WM_PAINT keeps an image
    // alive only for the duration of an already-running paint.
    binding.imageContext->surface.reset();
    binding.nativeImagePath = path;
    binding.imageDecodeAttempted = true;

    std::shared_ptr<NativeImageSurface> surface;
    try {
        surface = imageDecoder_ ? imageDecoder_->Decode(path) : nullptr;
    } catch (...) {
        surface.reset();
    }

    const auto model = binding.model.lock();
    if (surface && surface->IsValid()) {
        binding.imageContext->surface = surface;
        SetImageLoadResult(
            model, ImageLoadStatus::Loaded,
            static_cast<int>(surface->GetWidth()),
            static_cast<int>(surface->GetHeight()), {});
    } else {
        SetImageLoadResult(
            model, ImageLoadStatus::Failed, 0, 0,
            "The image source could not be decoded as a supported raster file");
    }
    InvalidateRect(binding.hwnd, nullptr, TRUE);
}

void WindowsBackend::SynchronizeRadioButton(ChildBinding& binding,
                                            const ControlState& control) {
    const LRESULT nativeSelected = SendMessageW(binding.hwnd, BM_GETCHECK, 0, 0);
    const LRESULT desiredSelected = control.selected ? BST_CHECKED : BST_UNCHECKED;
    if (nativeSelected == desiredSelected) return;

    const bool previous = binding.synchronizingCheck;
    binding.synchronizingCheck = true;
    SendMessageW(binding.hwnd, BM_SETCHECK,
                 static_cast<WPARAM>(desiredSelected), 0);
    binding.synchronizingCheck = previous;
}

void WindowsBackend::LayoutControls(WindowBinding& binding) {
    if (!binding.hwnd || !binding.model || !binding.model->content) return;

    RECT client{};
    if (!GetClientRect(binding.hwnd, &client)) return;
    const int clientWidth = std::max(0L, client.right - client.left);
    const int clientHeight = std::max(0L, client.bottom - client.top);
    const int statusBarHeight = GetStatusBarHeight(binding);
    const int contentHeight = std::max(0, clientHeight - statusBarHeight);
    if (binding.statusBar) {
        MoveWindow(binding.statusBar, 0, contentHeight, clientWidth,
                   statusBarHeight, TRUE);
    }
    WindowsMeasurementProvider measurementProvider(
        [&binding](const ControlState& control) {
            for (const auto& child : binding.children) {
                if (const auto model = child.model.lock();
                    model && model.get() == &control) {
                    return child.hwnd;
                }
            }
            return HWND{};
        });
    std::unordered_map<const ControlState*, LayoutRect> placements;
    std::unordered_set<const ControlState*> visible;
    std::function<void(const std::shared_ptr<LayoutState>&, LayoutRect)> placeLayout;
    placeLayout = [&](const std::shared_ptr<LayoutState>& layout,
                      LayoutRect bounds) {
        const auto direct = CalculateControlPlacements(
            layout, bounds, &measurementProvider);
        for (const auto& placement : direct) {
            if (!placement.control) continue;
            const auto* key = placement.control.get();
            placements[key] = placement.bounds;
            visible.insert(key);

            if (placement.control->kind == ControlKind::ScrollView &&
                placement.control->scrollContent) {
                const auto contentMeasurement = GetLayoutMeasurement(
                    placement.control->scrollContent, &measurementProvider);
                const LayoutSize viewport{
                    std::max(0, placement.bounds.width),
                    std::max(0, placement.bounds.height)};
                const LayoutSize contentSize{
                    std::max(viewport.width, contentMeasurement.natural.width),
                    std::max(0, contentMeasurement.natural.height)};
                UpdateScrollViewGeometry(placement.control, viewport,
                                         contentSize);
                placeLayout(
                    placement.control->scrollContent,
                    LayoutRect{placement.bounds.x,
                               placement.bounds.y -
                                   placement.control->verticalOffset,
                               contentSize.width, contentSize.height});
                continue;
            }

            if (placement.control->kind != ControlKind::TabView ||
                !placement.control->tabView || !placement.control->selectedIndex ||
                *placement.control->selectedIndex >=
                    placement.control->tabView->pages.size()) {
                continue;
            }

            HWND tab = nullptr;
            for (const auto& child : binding.children) {
                if (child.model.lock() == placement.control) {
                    tab = child.hwnd;
                    break;
                }
            }
            RECT pageRect{0, 0, std::max(0, placement.bounds.width),
                          std::max(0, placement.bounds.height)};
            if (!tab) continue;
            TabCtrl_AdjustRect(tab, FALSE, &pageRect);
            const auto& page = placement.control->tabView->pages[
                *placement.control->selectedIndex];
            if (!page) continue;
            placeLayout(page->layout,
                        LayoutRect{placement.bounds.x + pageRect.left,
                                   placement.bounds.y + pageRect.top,
                                   std::max(0L, pageRect.right - pageRect.left),
                                   std::max(0L, pageRect.bottom - pageRect.top)});
        }
    };
    placeLayout(binding.model->content,
                LayoutRect{0, 0, clientWidth, contentHeight});

    // The host's client area is a real clipping boundary. Update its native
    // range after model geometry has been measured, before descendants are
    // positioned relative to that host.
    for (const auto& child : binding.children) {
        const auto control = child.model.lock();
        if (!control || control->kind != ControlKind::ScrollView || !child.hwnd) {
            continue;
        }
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin = 0;
        info.nMax = control->contentSize.height > 0
            ? control->contentSize.height - 1 : 0;
        info.nPage = static_cast<UINT>(std::max(1, control->viewportSize.height));
        info.nPos = control->verticalOffset;
        SetScrollInfo(child.hwnd, SB_VERT, &info, TRUE);
        ShowScrollBar(child.hwnd, SB_VERT,
                      control->maximumVerticalOffset > 0 ? TRUE : FALSE);
    }

    for (const auto& child : binding.children) {
        const auto control = child.model.lock();
        if (!control || !child.hwnd) continue;
        const auto placement = placements.find(control.get());
        if (placement == placements.end() || visible.find(control.get()) ==
            visible.end()) {
            ::ShowWindow(child.hwnd, SW_HIDE);
            continue;
        }
        const auto& rectangle = placement->second;
        int x = rectangle.x;
        int y = rectangle.y;
        HWND parent = child.parent ? child.parent : binding.hwnd;
        const auto owner = FindOwningScrollView(control);
        if (owner) {
            const auto ownerPlacement = placements.find(owner.get());
            if (ownerPlacement != placements.end()) {
                x -= ownerPlacement->second.x;
                y -= ownerPlacement->second.y;
            }
        }
        RECT parentClient{};
        bool hasParentClient = false;
        if (owner) {
            parentClient = RECT{0, 0, owner->viewportSize.width,
                                owner->viewportSize.height};
            hasParentClient = true;
        } else {
            hasParentClient = GetClientRect(parent, &parentClient) != FALSE;
        }
        const RECT childRect{x, y, x + std::max(0, rectangle.width),
                             y + std::max(0, rectangle.height)};
        RECT intersection{};
        const bool intersects = !hasParentClient ||
            IntersectRect(&intersection, &parentClient, &childRect) != FALSE;
        MoveWindow(child.hwnd, x, y,
                   std::max(0, rectangle.width),
                   std::max(0, rectangle.height), TRUE);
        ::ShowWindow(child.hwnd, intersects ? SW_SHOW : SW_HIDE);
    }
    if (binding.toolTip) {
        for (const auto& tool : binding.toolTips) {
            if (!tool) continue;
            const HWND child = reinterpret_cast<HWND>(tool->info.uId);
            UpdateToolTipRect(tool->info, binding.hwnd, child);
            SendMessageW(binding.toolTip, TTM_NEWTOOLRECTW, 0,
                         reinterpret_cast<LPARAM>(&tool->info));
        }
    }
}

void WindowsBackend::CloseWindow(const std::shared_ptr<WindowState>& window) noexcept {
    auto* binding = FindWindowBinding(window);
    if (!binding) {
        if (window) {
            window->shown = false;
            window->closeState = WindowCloseState::Closed;
        }
        return;
    }
    if (binding->hwnd) DestroyWindow(binding->hwnd);
}

UINT_PTR WindowsBackend::FindTimerId(
    const std::shared_ptr<TimerState>& timer) const noexcept {
    if (!timer) return 0;
    for (const auto& [timerId, weakTimer] : timers_) {
        if (weakTimer.lock() == timer) return timerId;
    }
    return 0;
}

bool WindowsBackend::StartTimer(const std::shared_ptr<TimerState>& timer) {
    if (shutdown_ || !timer || timer->interval.count() <= 0 ||
        static_cast<std::uintmax_t>(timer->interval.count()) >
            static_cast<std::uintmax_t>(std::numeric_limits<UINT>::max())) {
        return false;
    }
    if (FindTimerId(timer) != 0) return true;

    if (nextTimerId_ == 0) nextTimerId_ = 1;
    const UINT_PTR requestedId = nextTimerId_++;
    const UINT_PTR timerId = SetTimer(
        nullptr, requestedId, static_cast<UINT>(timer->interval.count()), nullptr);
    if (timerId == 0) return false;
    timers_.emplace(timerId, timer);
    return true;
}

void WindowsBackend::StopTimer(
    const std::shared_ptr<TimerState>& timer) noexcept {
    if (!timer) return;
    for (auto iterator = timers_.begin(); iterator != timers_.end();) {
        if (iterator->second.lock() == timer) {
            KillTimer(nullptr, iterator->first);
            iterator = timers_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

MessageDialogResult WindowsBackend::ShowMessageDialog(
    const std::shared_ptr<WindowState>& owner, const std::string& message,
    const std::string& title, MessageDialogButtons buttons,
    MessageDialogIcon icon) {
    auto* binding = FindWindowBinding(owner);
    if (!binding || !binding->hwnd || IsWindow(binding->hwnd) == FALSE) {
        throw std::logic_error("Dialog owner Window has no native realization");
    }
    ::ShowWindow(binding->hwnd, SW_SHOWNORMAL);
    EnableWindow(binding->hwnd, TRUE);
    SetActiveWindow(binding->hwnd);
    SetForegroundWindow(binding->hwnd);

    UINT flags = MB_APPLMODAL | MB_SETFOREGROUND | MB_TOPMOST;
    switch (buttons) {
    case MessageDialogButtons::Ok:
        flags |= MB_OK;
        break;
    case MessageDialogButtons::OkCancel:
        flags |= MB_OKCANCEL;
        break;
    case MessageDialogButtons::YesNo:
        flags |= MB_YESNO;
        break;
    case MessageDialogButtons::YesNoCancel:
        flags |= MB_YESNOCANCEL;
        break;
    }
    switch (icon) {
    case MessageDialogIcon::None:
        break;
    case MessageDialogIcon::Information:
        flags |= MB_ICONINFORMATION;
        break;
    case MessageDialogIcon::Warning:
        flags |= MB_ICONWARNING;
        break;
    case MessageDialogIcon::Error:
        flags |= MB_ICONERROR;
        break;
    case MessageDialogIcon::Question:
        flags |= MB_ICONQUESTION;
        break;
    }

    const std::wstring nativeMessage = Utf8ToWide(message);
    const std::wstring nativeTitle = Utf8ToWide(title);
    const int result = MessageBoxW(binding->hwnd, nativeMessage.c_str(),
                                   nativeTitle.c_str(), flags);
    switch (result) {
    case IDOK:
        return MessageDialogResult::Ok;
    case IDYES:
        return MessageDialogResult::Yes;
    case IDNO:
        return MessageDialogResult::No;
    case IDCANCEL:
        return MessageDialogResult::Cancel;
    default:
        throw std::runtime_error("Unable to show message dialog");
    }
}

std::optional<std::string> WindowsBackend::ShowOpenFileDialog(
    const std::shared_ptr<WindowState>& owner, const std::string& title,
    const std::optional<std::string>& initialDirectory,
    const std::vector<FileDialogFilter>& filters) {
    auto* binding = FindWindowBinding(owner);
    if (!binding || !binding->hwnd || IsWindow(binding->hwnd) == FALSE) {
        throw std::logic_error("Dialog owner Window has no native realization");
    }
    return ShowNativeFileDialog(binding->hwnd, title, {}, initialDirectory,
                                filters, false);
}

std::optional<std::string> WindowsBackend::ShowSaveFileDialog(
    const std::shared_ptr<WindowState>& owner, const std::string& title,
    const std::string& suggestedFileName,
    const std::optional<std::string>& initialDirectory,
    const std::vector<FileDialogFilter>& filters) {
    auto* binding = FindWindowBinding(owner);
    if (!binding || !binding->hwnd || IsWindow(binding->hwnd) == FALSE) {
        throw std::logic_error("Dialog owner Window has no native realization");
    }
    return ShowNativeFileDialog(binding->hwnd, title, suggestedFileName,
                                initialDirectory, filters, true);
}

int WindowsBackend::Run() {
    MSG message{};
    while (!shutdown_) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == 0) return static_cast<int>(message.wParam);
        if (result == -1) return -1;
        if (message.message == WM_TIMER && message.hwnd == nullptr) {
            HandleTimer(static_cast<UINT_PTR>(message.wParam));
            continue;
        }
        bool handledAsAccelerator = false;
        HWND focus = GetFocus();
        HWND target = focus ? GetAncestor(focus, GA_ROOT) : GetActiveWindow();
        if (target) {
            const auto* current = FindWindowBinding(target);
            if (current && current->accelerators &&
                TranslateAcceleratorW(target, current->accelerators, &message) !=
                    FALSE) {
                handledAsAccelerator = true;
            }
        }
        if (handledAsAccelerator) continue;
        bool handledAsDialogMessage = false;
        for (const auto& [hwnd, binding] : windows_) {
            (void)binding;
            if (IsDialogMessageW(hwnd, &message) != FALSE) {
                handledAsDialogMessage = true;
                break;
            }
        }
        if (handledAsDialogMessage) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}

void WindowsBackend::HandleTimer(UINT_PTR timerId) noexcept {
    const auto iterator = timers_.find(timerId);
    if (iterator == timers_.end()) return;

    const auto timer = iterator->second.lock();
    if (!timer) {
        KillTimer(nullptr, timerId);
        timers_.erase(iterator);
        return;
    }
    try {
        DispatchTimerTick(timer);
    } catch (const std::exception&) {
        timer->running = false;
        KillTimer(nullptr, timerId);
        timers_.erase(timerId);
        RequestQuit(-1);
    } catch (...) {
        timer->running = false;
        KillTimer(nullptr, timerId);
        timers_.erase(timerId);
        RequestQuit(-1);
    }
}

void WindowsBackend::RequestQuit(int exitCode) noexcept {
    PostQuitMessage(exitCode);
}

void WindowsBackend::HandleNativeDestroyed(HWND hwnd) noexcept {
    const auto iterator = windows_.find(hwnd);
    if (iterator == windows_.end()) return;
    ClearNativeFocus(iterator->second->model,
                     iterator->second->model->focusedControl.lock());
    DestroyToolTips(*iterator->second);
    DestroyStatusBar(*iterator->second);
    DestroyMenuBar(*iterator->second);
    if (iterator->second->model) {
        iterator->second->model->shown = false;
        iterator->second->model->closeState = WindowCloseState::Closed;
    }
    windows_.erase(iterator);

    // Only the configured App Model policy can end the event loop. An
    // unrelated native handle, or a non-final App Model window, cannot post
    // WM_QUIT here.
    if (!shutdown_) {
        const auto application = application_.lock();
        if (ShouldQuitAfterWindowClosed(application)) {
            PostQuitMessage(application->exitCode);
        }
    }
}

void WindowsBackend::HandleScrollViewMessage(HWND hwnd, UINT message,
                                              WPARAM wParam,
                                              LPARAM lParam) noexcept {
    auto* binding = FindWindowBinding(GetAncestor(hwnd, GA_ROOT));
    if (!binding) return;

    std::shared_ptr<ControlState> scrollView;
    for (const auto& child : binding->children) {
        if (child.hwnd == hwnd) {
            scrollView = child.model.lock();
            break;
        }
    }
    if (!scrollView || scrollView->kind != ControlKind::ScrollView) return;

    if (message == WM_COMMAND || message == WM_NOTIFY || message == WM_HSCROLL ||
        (message == WM_VSCROLL && lParam != 0 &&
         reinterpret_cast<HWND>(lParam) != hwnd)) {
        SendMessageW(GetAncestor(hwnd, GA_ROOT), message, wParam, lParam);
        return;
    }

    int nextOffset = scrollView->verticalOffset;
    if (message == WM_MOUSEWHEEL) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta == 0) return;
        const int steps = std::max(1, std::abs(delta) / WHEEL_DELTA);
        const int movement = steps * 48;
        nextOffset += delta > 0 ? -movement : movement;
    } else if (message == WM_VSCROLL) {
        const int code = LOWORD(static_cast<DWORD>(wParam));
        switch (code) {
        case SB_LINEUP: nextOffset -= 48; break;
        case SB_LINEDOWN: nextOffset += 48; break;
        case SB_PAGEUP:
            nextOffset -= std::max(1, scrollView->viewportSize.height);
            break;
        case SB_PAGEDOWN:
            nextOffset += std::max(1, scrollView->viewportSize.height);
            break;
        case SB_TOP: nextOffset = 0; break;
        case SB_BOTTOM: nextOffset = scrollView->maximumVerticalOffset; break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK: {
            SCROLLINFO info{};
            info.cbSize = sizeof(info);
            info.fMask = SIF_TRACKPOS;
            if (GetScrollInfo(hwnd, SB_VERT, &info)) nextOffset = info.nTrackPos;
            break;
        }
        default: return;
        }
    } else {
        return;
    }

    nextOffset = std::clamp(nextOffset, 0,
                            std::max(0, scrollView->maximumVerticalOffset));
    if (nextOffset == scrollView->verticalOffset) return;
    scrollView->verticalOffset = nextOffset;
    scrollView->requestedVerticalOffset = nextOffset;
    NotifyControlChanged(scrollView);
}

void WindowsBackend::Shutdown() noexcept {
    if (shutdown_) return;
    shutdown_ = true;
    for (const auto& [timerId, weakTimer] : timers_) {
        KillTimer(nullptr, timerId);
        if (const auto timer = weakTimer.lock()) timer->running = false;
    }
    timers_.clear();
    while (!windows_.empty()) {
        const HWND hwnd = windows_.begin()->first;
        if (IsWindow(hwnd)) {
            DestroyWindow(hwnd);
            const auto stillPresent = windows_.find(hwnd);
            if (stillPresent != windows_.end()) {
                DestroyToolTips(*stillPresent->second);
                DestroyStatusBar(*stillPresent->second);
                DestroyMenuBar(*stillPresent->second);
                if (stillPresent->second->model) {
                    stillPresent->second->model->shown = false;
                    stillPresent->second->model->closeState = WindowCloseState::Closed;
                }
                windows_.erase(stillPresent);
            }
        } else {
            const auto iterator = windows_.begin();
            DestroyToolTips(*iterator->second);
            DestroyStatusBar(*iterator->second);
            if (iterator->second->model) {
                iterator->second->model->shown = false;
                iterator->second->model->closeState = WindowCloseState::Closed;
            }
            windows_.erase(iterator);
        }
    }
    if (registeredClass_) {
        UnregisterClassW(kWindowClassName, instance_);
        registeredClass_ = false;
    }
    classAtom_ = 0;
    if (imageClassRegistered_) {
        UnregisterClassW(kImageWindowClassName, instance_);
        imageClassRegistered_ = false;
    }
    imageClassAtom_ = 0;
    if (scrollViewClassRegistered_) {
        UnregisterClassW(kScrollViewWindowClassName, instance_);
        scrollViewClassRegistered_ = false;
    }
    scrollViewClassAtom_ = 0;
}

LRESULT WindowsBackend::HandleMessage(HWND hwnd, UINT message, WPARAM wParam,
                                      LPARAM lParam) noexcept {
    auto* binding = FindWindowBinding(hwnd);
    switch (message) {
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        if (info) {
            RECT minimum{0, 0, kMinimumClientWidth, kMinimumClientHeight};
            AdjustWindowRectEx(&minimum, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW);
            info->ptMinTrackSize.x = minimum.right - minimum.left;
            info->ptMinTrackSize.y = minimum.bottom - minimum.top;
        }
        return 0;
    }
    case WM_SIZE:
        if (binding) LayoutControls(*binding);
        return 0;
    case WM_INITMENUPOPUP:
        if (binding) {
            const auto opening = binding->menuOpenings.find(
                reinterpret_cast<HMENU>(wParam));
            if (opening != binding->menuOpenings.end()) {
                const auto menu = opening->second.lock();
                binding->menuOpening = true;
                try {
                    DispatchMenuOpening(menu);
                } catch (const std::exception&) {
                    RequestQuit(-1);
                } catch (...) {
                    RequestQuit(-1);
                }

                auto* current = FindWindowBinding(hwnd);
                if (current) {
                    current->menuOpening = false;
                    const bool refresh = current->menuRefreshPending;
                    current->menuRefreshPending = false;
                    if (refresh) {
                        try {
                            RebuildMenuBar(*current);
                            ResizeWindow(current->model);
                            if (auto* live = FindWindowBinding(hwnd)) {
                                LayoutControls(*live);
                            }
                        } catch (const std::exception&) {
                            RequestQuit(-1);
                        } catch (...) {
                            RequestQuit(-1);
                        }
                    }
                }
            }
        }
        return 0;
    case WM_NOTIFY:
        if (binding) {
            const auto* header = reinterpret_cast<const NMHDR*>(lParam);
            if (header && header->code == TCN_SELCHANGE) {
                for (const auto& childBinding : binding->children) {
                    if (childBinding.hwnd != header->hwndFrom) continue;
                    const auto control = childBinding.model.lock();
                    if (!control || control->kind != ControlKind::TabView ||
                        childBinding.synchronizingTabSelection ||
                        !control->enabled ||
                        IsWindowEnabled(childBinding.hwnd) == FALSE) {
                        return 0;
                    }
                    try {
                        const int nativeSelection =
                            TabCtrl_GetCurSel(childBinding.hwnd);
                        std::optional<std::size_t> selection;
                        if (nativeSelection >= 0 && control->tabView &&
                            static_cast<std::size_t>(nativeSelection) <
                                control->tabView->pages.size()) {
                            selection = static_cast<std::size_t>(nativeSelection);
                        }
                        DispatchSelectionChanged(control, selection);
                    } catch (const std::exception&) {
                        RequestQuit(-1);
                    } catch (...) {
                        RequestQuit(-1);
                    }
                    return 0;
                }
            }
        }
        break;
    case WM_COMMAND:
        if (binding) {
            const UINT commandId = LOWORD(static_cast<DWORD>(wParam));
            const HWND source = reinterpret_cast<HWND>(lParam);
            if (source == nullptr) {
                const auto command = binding->menuCommands.find(commandId);
                if (command != binding->menuCommands.end()) {
                    const auto item = command->second.lock();
                    const bool fromAccelerator =
                        HIWORD(static_cast<DWORD>(wParam)) == 1U;
                    if (!item || !item->parent.lock() || !item->menuBar.lock() ||
                        !binding->model ||
                        item->menuBar.lock() != binding->model->menuBar ||
                        (!item->enabled && !fromAccelerator)) {
                        return 0;
                    }
                    try {
                        // The callback may close this window, another window,
                        // or rebuild this menu. Keep only the shared model
                        // reference across the synchronous dispatch.
                        if (fromAccelerator) {
                            // Accelerator dispatch is deliberately allowed to
                            // reach a stale dynamic item state. The callback
                            // must revalidate its focused target, which keeps
                            // Ctrl commands useful after selection changes
                            // without adding selection-change events. A
                            // disabled item invoked from the visible menu is
                            // still suppressed above.
                            DispatchMenuItemAcceleratorInvocation(item);
                        } else {
                            DispatchMenuItemInvocation(item);
                        }
                    } catch (const std::exception&) {
                        RequestQuit(-1);
                    } catch (...) {
                        RequestQuit(-1);
                    }
                    return 0;
                }
            }
            const HWND child = source;
            std::shared_ptr<ControlState> control;
            bool synchronizingText = false;
            bool synchronizingSelection = false;
            bool synchronizingCheck = false;
            for (const auto& childBinding : binding->children) {
                if (childBinding.hwnd == child) {
                    control = childBinding.model.lock();
                    synchronizingText = childBinding.synchronizingText;
                    synchronizingSelection = childBinding.synchronizingSelection;
                    synchronizingCheck = childBinding.synchronizingCheck;
                    break;
                }
            }
            const WORD notification = HIWORD(static_cast<DWORD>(wParam));
            if (control && control->kind == ControlKind::ListBox &&
                notification == LBN_SELCHANGE) {
                if (synchronizingSelection || !control->enabled ||
                    IsWindowEnabled(child) == FALSE) return 0;
                try {
                    const LRESULT nativeSelection = SendMessageW(
                        child, LB_GETCURSEL, 0, 0);
                    std::optional<std::size_t> selection;
                    if (nativeSelection != LB_ERR && nativeSelection >= 0 &&
                        static_cast<std::size_t>(nativeSelection) < control->items.size()) {
                        selection = static_cast<std::size_t>(nativeSelection);
                    }
                    // The model update and callback may close this window or
                    // mutate this ListBox. No binding pointer is used after
                    // this call returns.
                    DispatchSelectionChanged(control, selection);
                } catch (const std::exception&) {
                    RequestQuit(-1);
                } catch (...) {
                    RequestQuit(-1);
                }
                return 0;
            }
            if (control && control->kind == ControlKind::ComboBox &&
                notification == CBN_SELCHANGE) {
                if (synchronizingSelection || !control->enabled ||
                    IsWindowEnabled(child) == FALSE) return 0;
                try {
                    const LRESULT nativeSelection = SendMessageW(
                        child, CB_GETCURSEL, 0, 0);
                    std::optional<std::size_t> selection;
                    if (nativeSelection != CB_ERR && nativeSelection >= 0 &&
                        static_cast<std::size_t>(nativeSelection) < control->items.size()) {
                        selection = static_cast<std::size_t>(nativeSelection);
                    }
                    // The model update and callback may close this window or
                    // mutate this ComboBox. No binding pointer is used after
                    // this call returns.
                    DispatchSelectionChanged(control, selection);
                } catch (const std::exception&) {
                    RequestQuit(-1);
                } catch (...) {
                    RequestQuit(-1);
                }
                return 0;
            }
            if (control && control->kind == ControlKind::Button &&
                notification == BN_CLICKED) {
                if (!control->enabled || IsWindowEnabled(child) == FALSE) return 0;
                try {
                    // Keep the model reference independent of the binding:
                    // the callback may close this or another window.
                    DispatchButtonClick(control);
                } catch (const std::exception&) {
                    RequestQuit(-1);
                } catch (...) {
                    RequestQuit(-1);
                }
                return 0;
            }
            if (control && (control->kind == ControlKind::CheckBox ||
                            control->kind == ControlKind::RadioButton) &&
                notification == BN_CLICKED) {
                if (synchronizingCheck || !control->enabled ||
                    IsWindowEnabled(child) == FALSE) {
                    return 0;
                }
                try {
                    if (control->kind == ControlKind::CheckBox) {
                        const LRESULT nativeChecked = SendMessageW(
                            child, BM_GETCHECK, 0, 0);
                        DispatchCheckedChanged(control,
                                               nativeChecked == BST_CHECKED);
                    } else {
                        DispatchRadioSelection(control);
                    }
                } catch (const std::exception&) {
                    RequestQuit(-1);
                } catch (...) {
                    RequestQuit(-1);
                }
                return 0;
            }
            if (control && IsTextEdit(control->kind) &&
                notification == EN_CHANGE) {
                if (synchronizingText || !control->enabled ||
                    IsWindowEnabled(child) == FALSE) return 0;
                try {
                    const std::wstring nativeText = ReadNativeText(child);
                    SynchronizeTextEditStateFromNative(control, child);
                    const std::string text = control->kind == ControlKind::TextArea
                        ? Utf16ToUtf8(NormalizeNativeTextAreaNewlines(nativeText))
                        : Utf16ToUtf8(nativeText);
                    // DispatchTextChanged updates the model before user code
                    // runs. The callback may close this window, so no binding
                    // pointer is used after this call.
                    DispatchTextChanged(control, text);
                } catch (const std::exception&) {
                    RequestQuit(-1);
                } catch (...) {
                    RequestQuit(-1);
                }
                return 0;
            }
        }
        break;
    case WM_HSCROLL:
        if (binding) {
            const HWND child = reinterpret_cast<HWND>(lParam);
            std::shared_ptr<ControlState> control;
            bool synchronizingSlider = false;
            for (const auto& childBinding : binding->children) {
                if (childBinding.hwnd == child) {
                    control = childBinding.model.lock();
                    synchronizingSlider = childBinding.synchronizingSlider;
                    break;
                }
            }

            const WORD notification = LOWORD(static_cast<DWORD>(wParam));
            const bool isSliderNotification =
                notification == TB_LINEUP ||
                notification == TB_LINEDOWN ||
                notification == TB_PAGEUP ||
                notification == TB_PAGEDOWN ||
                notification == TB_THUMBPOSITION ||
                notification == TB_THUMBTRACK ||
                notification == TB_TOP ||
                notification == TB_BOTTOM ||
                notification == TB_ENDTRACK;
            if (control && control->kind == ControlKind::Slider) {
                if (!synchronizingSlider && control->enabled &&
                    IsWindowEnabled(child) != FALSE &&
                    isSliderNotification) {
                    try {
                        const LRESULT nativeValue =
                            SendMessageW(child, TBM_GETPOS, 0, 0);
                        // DispatchSliderChanged suppresses duplicate native
                        // notifications and updates the model before the
                        // copied callback runs. User code may close this
                        // window or mutate this Slider, so no binding pointer
                        // is used after the dispatch returns.
                        DispatchSliderChanged(control,
                                              static_cast<int>(nativeValue));
                    } catch (const std::exception&) {
                        RequestQuit(-1);
                    } catch (...) {
                        RequestQuit(-1);
                    }
                }
                return 0;
            }
        }
        break;
    case WM_DROPFILES: {
        ScopedNativeFileDrop drop(reinterpret_cast<HDROP>(wParam));
        std::vector<std::string> paths;
        try {
            paths = ReadNativeFileDrop(drop.Get());
        } catch (const std::exception&) {
            RequestQuit(-1);
            return 0;
        } catch (...) {
            RequestQuit(-1);
            return 0;
        }
        drop.Reset();
        const auto window = binding ? binding->model : nullptr;
        try {
            // All native memory is finished before user code can run. Do not
            // touch binding again after this call: the callback may close this
            // Window.
            DispatchFilesDropped(window, std::move(paths));
        } catch (const std::exception&) {
            RequestQuit(-1);
        } catch (...) {
            RequestQuit(-1);
        }
        return 0;
    }
    case WM_CLOSE:
        if (binding && binding->model) {
            RequestWindowClose(binding->model);
        }
        return 0;
    case WM_DESTROY:
        HandleNativeDestroyed(hwnd);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
}

LRESULT CALLBACK WindowsBackend::WindowProcedure(HWND hwnd, UINT message,
                                                 WPARAM wParam, LPARAM lParam) noexcept {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* backend = create ? static_cast<WindowsBackend*>(create->lpCreateParams) : nullptr;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(backend));
        return TRUE;
    }

    auto* backend = reinterpret_cast<WindowsBackend*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (backend) return backend->HandleMessage(hwnd, message, wParam, lParam);
    return DefWindowProcW(hwnd, message, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
}

LRESULT CALLBACK WindowsBackend::ImageWindowProcedure(HWND hwnd, UINT message,
                                                      WPARAM wParam,
                                                      LPARAM lParam) noexcept {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* context = create
            ? static_cast<NativeImageWindowContext*>(create->lpCreateParams)
            : nullptr;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(context));
        return TRUE;
    }

    auto* context = reinterpret_cast<NativeImageWindowContext*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:
        return 0;
    case WM_PAINT:
        PaintImageWindow(hwnd, context);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK WindowsBackend::ScrollViewWindowProcedure(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* context = create
            ? static_cast<NativeScrollViewContext*>(create->lpCreateParams)
            : nullptr;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(context));
        return TRUE;
    }

    auto* context = reinterpret_cast<NativeScrollViewContext*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (context && context->backend) {
        switch (message) {
        case WM_COMMAND:
        case WM_NOTIFY:
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_MOUSEWHEEL:
            context->backend->HandleScrollViewMessage(
                hwnd, message, wParam, lParam);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;
        default:
            break;
        }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

std::unique_ptr<PlatformBackend> CreatePlatformBackend(
    const std::shared_ptr<ApplicationState>& application) {
    return std::make_unique<WindowsBackend>(application);
}

} // namespace guidexos::appmodel::detail

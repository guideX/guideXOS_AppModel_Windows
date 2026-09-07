#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include <guidexos/appmodel/appmodel.hpp>

#include <cstdio>

#undef assert
#define assert(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAILED: %s (line %d)\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (false)
using namespace guidexos::appmodel;

int main() {
    Application app("com.guidexos.tests.progress-bar-lifecycle",
                    ShutdownMode::Explicit);
    Window window(app);
    window.SetTitle("guideXOS ProgressBar Lifecycle");
    ProgressBar progress;
    progress.SetMinimum(10);
    progress.SetMaximum(200);
    progress.SetValue(75);
    Layout content;
    content.Add(progress, LayoutSizing::Expand);
    window.SetContent(content);

    assert(window.Show());
    const HWND nativeWindow = FindWindowW(
        nullptr, L"guideXOS ProgressBar Lifecycle");
    assert(nativeWindow != nullptr);
    const HWND nativeProgress = FindWindowExW(
        nativeWindow, nullptr, PROGRESS_CLASSW, nullptr);
    assert(nativeProgress != nullptr);
    RECT nativeProgressRect{};
    assert(GetWindowRect(nativeProgress, &nativeProgressRect));
    assert(nativeProgressRect.right - nativeProgressRect.left > 96);
    assert(nativeProgressRect.bottom - nativeProgressRect.top >= 22);

    PBRANGE nativeRange{};
    SendMessageW(nativeProgress, PBM_GETRANGE, TRUE,
                 reinterpret_cast<LPARAM>(&nativeRange));
    assert(nativeRange.iLow == 10);
    assert(nativeRange.iHigh == 200);
    assert(SendMessageW(nativeProgress, PBM_GETPOS, 0, 0) == 75);
    assert((GetWindowLongPtrW(nativeProgress, GWL_STYLE) & PBS_MARQUEE) != 0);

    progress.SetValue(150);
    assert(SendMessageW(nativeProgress, PBM_GETPOS, 0, 0) == 150);
    progress.SetMinimum(40);
    progress.SetMaximum(160);
    nativeRange = {};
    SendMessageW(nativeProgress, PBM_GETRANGE, TRUE,
                 reinterpret_cast<LPARAM>(&nativeRange));
    assert(nativeRange.iLow == 40);
    assert(nativeRange.iHigh == 160);
    assert(SendMessageW(nativeProgress, PBM_GETPOS, 0, 0) == 150);

    progress.SetIndeterminate(true);
    // The native marquee message is accepted by the private progress child;
    // the AppModel remains authoritative for the mode being presented.
    SendMessageW(nativeProgress, PBM_SETMARQUEE, FALSE, 0);
    progress.SetValue(70);
    SendMessageW(nativeProgress, PBM_SETMARQUEE, FALSE, 0);
    assert(progress.GetValue() == 70);
    progress.SetIndeterminate(false);
    SendMessageW(nativeProgress, PBM_SETMARQUEE, TRUE, 50);
    SendMessageW(nativeProgress, PBM_SETMARQUEE, FALSE, 0);
    assert(SendMessageW(nativeProgress, PBM_GETPOS, 0, 0) == 70);

    progress.SetEnabled(false);
    assert(!IsWindowEnabled(nativeProgress));
    progress.SetEnabled(true);
    assert(IsWindowEnabled(nativeProgress));

    window.Close();
    assert(!window.IsShown());
    assert(!IsWindow(nativeProgress));
    assert(progress.GetValue() == 70);
    assert(window.Show());
    const HWND reopenedWindow = FindWindowW(
        nullptr, L"guideXOS ProgressBar Lifecycle");
    const HWND reopenedProgress = FindWindowExW(
        reopenedWindow, nullptr, PROGRESS_CLASSW, nullptr);
    (void)reopenedProgress;
    assert(reopenedProgress != nullptr);
    assert(reopenedProgress != nativeProgress);
    assert(SendMessageW(reopenedProgress, PBM_GETPOS, 0, 0) == 70);

    window.Close();
    app.Quit();
    assert(app.Run() == 0);
    return 0;
}

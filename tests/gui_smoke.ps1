param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class GuiSmokeNative {
    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr hwnd, StringBuilder text, int maxCount);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern bool IsIconic(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern bool IsZoomed(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr hwnd, int command);

    [DllImport("user32.dll")]
    private static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool GetClientRect(IntPtr hwnd, out RECT rect);

    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);

    [DllImport("user32.dll")]
    private static extern bool SetWindowPos(IntPtr hwnd, IntPtr insertAfter, int x, int y, int width, int height, uint flags);

    [DllImport("user32.dll")]
    private static extern bool ScreenToClient(IntPtr hwnd, ref POINT point);

    [DllImport("user32.dll")]
    private static extern bool IsWindow(IntPtr hwnd);

    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    private struct POINT { public int X, Y; }

    public static IntPtr FindTopLevelWindow(string title, int processId) {
        IntPtr match = IntPtr.Zero;
        EnumWindows((hwnd, unused) => {
            uint candidatePid;
            GetWindowThreadProcessId(hwnd, out candidatePid);
            if (candidatePid == processId && GetText(hwnd) == title) {
                match = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return match;
    }

    public static List<IntPtr> FindChildren(IntPtr parent, string className) {
        var result = new List<IntPtr>();
        EnumChildWindows(parent, (hwnd, unused) => {
            if (GetClass(hwnd) == className) result.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static string GetText(IntPtr hwnd) {
        var text = new StringBuilder(512);
        GetWindowText(hwnd, text, text.Capacity);
        return text.ToString();
    }

    private static string GetClass(IntPtr hwnd) {
        var text = new StringBuilder(256);
        GetClassName(hwnd, text, text.Capacity);
        return text.ToString();
    }

    public static bool IsVisible(IntPtr hwnd) { return IsWindowVisible(hwnd); }
    public static bool IsMinimized(IntPtr hwnd) { return IsIconic(hwnd); }
    public static bool IsMaximized(IntPtr hwnd) { return IsZoomed(hwnd); }
    public static bool IsAlive(IntPtr hwnd) { return IsWindow(hwnd); }
    public static void Minimize(IntPtr hwnd) { ShowWindow(hwnd, 6); }
    public static void Maximize(IntPtr hwnd) { ShowWindow(hwnd, 3); }
    public static void Restore(IntPtr hwnd) { ShowWindow(hwnd, 9); }
    public static void Close(IntPtr hwnd) { PostMessage(hwnd, 0x0010, IntPtr.Zero, IntPtr.Zero); }
    public static void Click(IntPtr hwnd) { SendMessage(hwnd, 0x00F5, IntPtr.Zero, IntPtr.Zero); }
    public static void Resize(IntPtr hwnd, int width, int height) {
        RECT rect;
        if (!GetWindowRect(hwnd, out rect)) throw new InvalidOperationException("GetWindowRect failed");
        if (!SetWindowPos(hwnd, IntPtr.Zero, rect.Left, rect.Top, width, height, 0x0014)) {
            throw new InvalidOperationException("SetWindowPos failed");
        }
    }

    public static int[] ClientSize(IntPtr hwnd) {
        RECT rect;
        if (!GetClientRect(hwnd, out rect)) throw new InvalidOperationException("GetClientRect failed");
        return new[] { rect.Right - rect.Left, rect.Bottom - rect.Top };
    }

    public static int[] ChildPosition(IntPtr parent, IntPtr child) {
        RECT rect;
        if (!GetWindowRect(child, out rect)) throw new InvalidOperationException("GetWindowRect failed");
        var point = new POINT { X = rect.Left, Y = rect.Top };
        if (!ScreenToClient(parent, ref point)) throw new InvalidOperationException("ScreenToClient failed");
        return new[] { point.X, point.Y, rect.Right - rect.Left, rect.Bottom - rect.Top };
    }
}
'@

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS App Model for Windows'

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    try {
        $window = [IntPtr]::Zero
        Wait-Until {
            $window = [GuiSmokeNative]::FindTopLevelWindow($title, $process.Id)
            return $window -ne [IntPtr]::Zero
        } "Cycle ${cycle}: the sample window did not appear"
        $window = [GuiSmokeNative]::FindTopLevelWindow($title, $process.Id)

        $labels = [GuiSmokeNative]::FindChildren($window, 'Static')
        $buttons = [GuiSmokeNative]::FindChildren($window, 'Button')
        if ($labels.Count -ne 1 -or $buttons.Count -ne 1) {
            throw "Cycle ${cycle}: expected one label and one button, got $($labels.Count) and $($buttons.Count)"
        }
        if (-not [GuiSmokeNative]::IsVisible($labels[0]) -or -not [GuiSmokeNative]::IsVisible($buttons[0])) {
            throw "Cycle ${cycle}: label or button is not visible"
        }
        if ([GuiSmokeNative]::GetText($labels[0]) -ne 'Hello from the guideXOS App Model') {
            throw "Cycle ${cycle}: initial label text is incorrect"
        }

        [GuiSmokeNative]::Click($buttons[0])
        Wait-Until {
            return [GuiSmokeNative]::GetText($labels[0]) -eq 'The button was clicked.'
        } "Cycle ${cycle}: button click did not update the label"

        1..5 | ForEach-Object { [GuiSmokeNative]::Click($buttons[0]) }
        if ([GuiSmokeNative]::GetText($labels[0]) -ne 'The button was clicked.') {
            throw "Cycle ${cycle}: repeated clicks changed the final label unexpectedly"
        }

        $clientBefore = [GuiSmokeNative]::ClientSize($window)
        $buttonBefore = [GuiSmokeNative]::ChildPosition($window, $buttons[0])
        if ($clientBefore[0] -ne 800 -or $clientBefore[1] -ne 500) {
            throw "Cycle ${cycle}: initial client size was $($clientBefore[0])x$($clientBefore[1]), expected 800x500"
        }
        [GuiSmokeNative]::Minimize($window)
        Wait-Until { return [GuiSmokeNative]::IsMinimized($window) } "Cycle ${cycle}: minimize did not take effect"
        [GuiSmokeNative]::Restore($window)
        Wait-Until { return [GuiSmokeNative]::IsVisible($window) -and -not [GuiSmokeNative]::IsMinimized($window) } "Cycle ${cycle}: restore did not take effect"

        [GuiSmokeNative]::Maximize($window)
        Wait-Until { return [GuiSmokeNative]::IsMaximized($window) } "Cycle ${cycle}: maximize did not take effect"
        [GuiSmokeNative]::Restore($window)
        Wait-Until { return [GuiSmokeNative]::IsVisible($window) -and -not [GuiSmokeNative]::IsMaximized($window) } "Cycle ${cycle}: restore after maximize did not take effect"

        [GuiSmokeNative]::Click($buttons[0])
        if ([GuiSmokeNative]::GetText($labels[0]) -ne 'The button was clicked.') {
            throw "Cycle ${cycle}: click after restore was not safe"
        }

        # Resize the real top-level window through user32 and verify the
        # backend's WM_SIZE layout pass keeps the controls inside the client.
        [GuiSmokeNative]::Resize($window, 1000, 650)
        Wait-Until {
            $size = [GuiSmokeNative]::ClientSize($window)
            return $size[0] -gt $clientBefore[0] -and $size[1] -gt $clientBefore[1]
        } "Cycle ${cycle}: resize did not change the client area"
        $buttonAfter = [GuiSmokeNative]::ChildPosition($window, $buttons[0])
        $clientAfter = [GuiSmokeNative]::ClientSize($window)
        if ($buttonAfter[0] -lt 0 -or $buttonAfter[1] -lt 0 -or
            $buttonAfter[0] + $buttonAfter[2] -gt $clientAfter[0] -or
            $buttonAfter[1] + $buttonAfter[3] -gt $clientAfter[1]) {
            throw "Cycle ${cycle}: button layout is outside the client area after restore"
        }
        if ($buttonAfter[2] -le $buttonBefore[2]) {
            throw "Cycle ${cycle}: button width did not grow with the resized client area"
        }
        if ($buttonBefore[0] -ne $buttonAfter[0]) {
            throw "Cycle ${cycle}: vertical layout was not stable across minimize/restore"
        }

        [GuiSmokeNative]::Close($window)
        Wait-Until { return $process.HasExited } "Cycle ${cycle}: close did not shut down the application"
        if ($process.ExitCode -ne 0) { throw "Cycle ${cycle}: sample exited with code $($process.ExitCode)" }
    }
    finally {
        if (-not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}

Write-Output "PASS: HelloApp GUI smoke completed $Cycles launch/close cycles."

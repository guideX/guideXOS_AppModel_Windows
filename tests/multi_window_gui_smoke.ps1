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

public static class MultiWindowSmokeNative {
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
    private static extern bool IsWindow(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

    [DllImport("user32.dll")]
    private static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

    public static List<IntPtr> FindTopLevelWindows(string title, int processId) {
        var result = new List<IntPtr>();
        EnumWindows((hwnd, unused) => {
            uint candidatePid;
            GetWindowThreadProcessId(hwnd, out candidatePid);
            if (candidatePid == processId && GetText(hwnd) == title) result.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return result;
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

    public static bool IsAlive(IntPtr hwnd) { return IsWindow(hwnd); }
    public static bool IsVisible(IntPtr hwnd) { return IsWindowVisible(hwnd); }
    public static void Close(IntPtr hwnd) { PostMessage(hwnd, 0x0010, IntPtr.Zero, IntPtr.Zero); }
    public static void Click(IntPtr hwnd) { SendMessage(hwnd, 0x00F5, IntPtr.Zero, IntPtr.Zero); }

    private static string GetClass(IntPtr hwnd) {
        var text = new StringBuilder(256);
        GetClassName(hwnd, text, text.Capacity);
        return text.ToString();
    }
}
'@

function Get-AppWindows([int] $ProcessId, [string] $Title) {
    return [MultiWindowSmokeNative]::FindTopLevelWindows($Title, $ProcessId)
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$primaryTitle = 'guideXOS Multi-Window Primary'
$secondaryTitle = 'guideXOS Multi-Window Secondary'

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    try {
        $primary = [IntPtr]::Zero
        Wait-Until {
            $windows = Get-AppWindows $process.Id $primaryTitle
            if ($windows.Count -eq 1) { $primary = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: primary window did not appear exactly once"
        $primary = (Get-AppWindows $process.Id $primaryTitle)[0]

        $primaryLabels = [MultiWindowSmokeNative]::FindChildren($primary, 'Static')
        $primaryButtons = [MultiWindowSmokeNative]::FindChildren($primary, 'Button')
        if ($primaryLabels.Count -ne 1 -or $primaryButtons.Count -ne 3) {
            throw "Cycle ${cycle}: expected primary controls 1 label/3 buttons, got $($primaryLabels.Count)/$($primaryButtons.Count)"
        }
        if (-not [MultiWindowSmokeNative]::IsVisible($primary)) {
            throw "Cycle ${cycle}: primary window is not visible"
        }

        $openButton = Find-Control $primary 'Button' 'Open Secondary Window'
        $updateSecondaryButton = Find-Control $primary 'Button' 'Update Secondary Label'
        $closePrimaryButton = Find-Control $primary 'Button' 'Close Primary Window'
        if ($openButton -eq [IntPtr]::Zero -or
            $updateSecondaryButton -eq [IntPtr]::Zero -or
            $closePrimaryButton -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: primary button lookup failed"
        }

        [MultiWindowSmokeNative]::Click($openButton)
        $secondary = [IntPtr]::Zero
        Wait-Until {
            $windows = Get-AppWindows $process.Id $secondaryTitle
            if ($windows.Count -eq 1) { $secondary = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: secondary window did not open exactly once"
        $secondary = (Get-AppWindows $process.Id $secondaryTitle)[0]
        if ((Get-AppWindows $process.Id $primaryTitle).Count -ne 1) {
            throw "Cycle ${cycle}: primary window was lost when secondary opened"
        }

        $secondaryLabels = [MultiWindowSmokeNative]::FindChildren($secondary, 'Static')
        $secondaryButtons = [MultiWindowSmokeNative]::FindChildren($secondary, 'Button')
        if ($secondaryLabels.Count -ne 1 -or $secondaryButtons.Count -ne 2) {
            throw "Cycle ${cycle}: expected secondary controls 1 label/2 buttons, got $($secondaryLabels.Count)/$($secondaryButtons.Count)"
        }
        if (-not [MultiWindowSmokeNative]::IsVisible($secondary)) {
            throw "Cycle ${cycle}: secondary window is not visible"
        }

        $primaryMenu = [GuiTestNative]::MenuBar($primary)
        $secondaryMenu = [GuiTestNative]::MenuBar($secondary)
        if ($primaryMenu -eq [IntPtr]::Zero -or
            $secondaryMenu -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: one multi-window menu bar was not realized"
        }
        $primaryActions = [GuiTestNative]::FindSubMenuByText($primaryMenu, 'Actions')
        $secondaryActions = [GuiTestNative]::FindSubMenuByText($secondaryMenu, 'Actions')
        $primaryMenuCommand = [GuiTestNative]::FindMenuCommandByText($primaryActions, 'Update')
        $secondaryMenuCommand = [GuiTestNative]::FindMenuCommandByText($secondaryActions, 'Update')
        [GuiTestNative]::InvokeMenuCommand($primary, $primaryMenuCommand)
        Wait-Until {
            [MultiWindowSmokeNative]::GetText($primaryLabels[0]) -eq 'Updated by the primary menu.' -and
            [MultiWindowSmokeNative]::GetText($secondaryLabels[0]) -ne 'Updated by the primary menu.'
        } "Cycle ${cycle}: primary menu command crossed into secondary window"
        $secondaryMenu = [GuiTestNative]::MenuBar($secondary)
        $secondaryActions = [GuiTestNative]::FindSubMenuByText($secondaryMenu, 'Actions')
        $secondaryMenuCommand = [GuiTestNative]::FindMenuCommandByText($secondaryActions, 'Update')
        [GuiTestNative]::InvokeMenuCommand($secondary, $secondaryMenuCommand)
        Wait-Until {
            [MultiWindowSmokeNative]::GetText($secondaryLabels[0]) -eq 'Updated by the secondary menu.' -and
            [MultiWindowSmokeNative]::GetText($primaryLabels[0]) -ne 'Updated by the secondary menu.'
        } "Cycle ${cycle}: secondary menu command crossed into primary window"

        # Exercise both directions of App Model event dispatch.
        [MultiWindowSmokeNative]::Click($updateSecondaryButton)
        Wait-Until {
            return [MultiWindowSmokeNative]::GetText($secondaryLabels[0]) -eq 'Updated by the primary window.'
        } "Cycle ${cycle}: primary did not update the secondary label"

        $updatePrimaryButton = Find-Control $secondary 'Button' 'Update Primary Label'
        $closeSecondaryButton = Find-Control $secondary 'Button' 'Close Secondary Window'
        if ($updatePrimaryButton -eq [IntPtr]::Zero -or $closeSecondaryButton -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: secondary button lookup failed"
        }
        [MultiWindowSmokeNative]::Click($updatePrimaryButton)
        Wait-Until {
            return [MultiWindowSmokeNative]::GetText($primaryLabels[0]) -eq 'Updated by the secondary window.'
        } "Cycle ${cycle}: secondary did not update the primary label"

        # Repeated Show requests are idempotent: one logical secondary stays
        # one native window.
        1..3 | ForEach-Object { [MultiWindowSmokeNative]::Click($openButton) }
        if ((Get-AppWindows $process.Id $secondaryTitle).Count -ne 1) {
            throw "Cycle ${cycle}: repeated open clicks created a duplicate secondary"
        }

        # Close and reopen the secondary while the primary remains alive.
        if (($cycle % 2) -eq 0) {
            [MultiWindowSmokeNative]::Close($secondary)
        } else {
            [MultiWindowSmokeNative]::Click($closeSecondaryButton)
        }
        Wait-Until {
            return (Get-AppWindows $process.Id $secondaryTitle).Count -eq 0 -and
                   (Get-AppWindows $process.Id $primaryTitle).Count -eq 1 -and
                   -not $process.HasExited
        } "Cycle ${cycle}: closing secondary did not leave primary alive"

        $primary = (Get-AppWindows $process.Id $primaryTitle)[0]
        $openButton = Find-Control $primary 'Button' 'Open Secondary Window'
        [MultiWindowSmokeNative]::Click($openButton)
        $secondary = [IntPtr]::Zero
        Wait-Until {
            $windows = Get-AppWindows $process.Id $secondaryTitle
            if ($windows.Count -eq 1) { $secondary = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: secondary did not reopen"
        $secondary = (Get-AppWindows $process.Id $secondaryTitle)[0]
        $secondaryLabels = [MultiWindowSmokeNative]::FindChildren($secondary, 'Static')
        if ([MultiWindowSmokeNative]::GetText($secondaryLabels[0]) -ne 'Secondary open count: 2') {
            throw "Cycle ${cycle}: reopen count did not use clean logical state"
        }

        # Close the primary while the secondary remains alive, then close the
        # final window and verify policy-driven exit with no orphan process.
        $closePrimaryButton = Find-Control $primary 'Button' 'Close Primary Window'
        [MultiWindowSmokeNative]::Click($closePrimaryButton)
        Wait-Until {
            return (Get-AppWindows $process.Id $primaryTitle).Count -eq 0 -and
                   (Get-AppWindows $process.Id $secondaryTitle).Count -eq 1 -and
                   -not $process.HasExited
        } "Cycle ${cycle}: closing primary did not leave secondary alive"

        $secondary = (Get-AppWindows $process.Id $secondaryTitle)[0]
        $closeSecondaryButton = Find-Control $secondary 'Button' 'Close Secondary Window'
        [MultiWindowSmokeNative]::Click($closeSecondaryButton)
        Wait-Until { return $process.HasExited } "Cycle ${cycle}: closing the final window did not exit"
        if ($process.ExitCode -ne 0) { throw "Cycle ${cycle}: expected exit code 0, got $($process.ExitCode)" }
    }
    finally {
        if (-not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}

Write-Output "PASS: MultiWindowApp GUI smoke completed $Cycles full interaction cycles."

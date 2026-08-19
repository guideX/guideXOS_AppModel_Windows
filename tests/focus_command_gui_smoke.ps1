param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class FocusCommandSmokeNative {
    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message,
                                             IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    private static extern bool OpenClipboard(IntPtr owner);
    [DllImport("user32.dll")]
    private static extern bool EmptyClipboard();
    [DllImport("user32.dll")]
    private static extern bool CloseClipboard();

    public static void PrepareMenu(IntPtr window, IntPtr menu) {
        SendMessage(window, 0x0117, menu, IntPtr.Zero); // WM_INITMENUPOPUP
    }

    public static void InvokeAccelerator(IntPtr window, int command) {
        var value = command | unchecked((int)0x00010000);
        SendMessage(window, 0x0111, (IntPtr)value, IntPtr.Zero); // WM_COMMAND
    }

    public static void ClearClipboard() {
        if (OpenClipboard(IntPtr.Zero)) {
            EmptyClipboard();
            CloseClipboard();
        }
    }
}
'@

function Get-MainWindow([int] $ProcessId) {
    $windows = [GuiTestNative]::FindTopLevelWindows(
        'guideXOS Focus Command Demo', $ProcessId)
    if ($windows.Count -ne 1) { return [IntPtr]::Zero }
    return $windows[0]
}

function Get-EditMenu([IntPtr] $Window, [int] $Cycle) {
    $bar = [GuiTestNative]::MenuBar($Window)
    $menu = [GuiTestNative]::FindSubMenuByText($bar, 'Edit')
    if ($menu -eq [IntPtr]::Zero) {
        throw "Cycle ${Cycle}: Edit menu was not realized"
    }
    return $menu
}

function Refresh-FocusMenu([IntPtr] $Window, [int] $Cycle) {
    $menu = Get-EditMenu $Window $Cycle
    [FocusCommandSmokeNative]::PrepareMenu($Window, $menu)
    # OnOpening may update enabled state, which replaces the small native
    # menu realization. Rediscover the current popup after the synchronous
    # callback completes.
    return Get-EditMenu $Window $Cycle
}

function Require-FocusStatus([IntPtr] $Window, [string] $Expected,
                              [int] $Cycle) {
    $status = Find-ControlPrefix $Window 'Static' 'Focused:'
    if ($status -eq [IntPtr]::Zero) {
        throw "Cycle ${Cycle}: focus status label was not found"
    }
    Wait-Until {
        [GuiTestNative]::GetText($status) -eq $Expected
    } "Cycle ${Cycle}: expected '$Expected' focus status"
}

function Invoke-Edit([IntPtr] $Window, [string] $Text, [int] $Cycle) {
    $menu = Refresh-FocusMenu $Window $Cycle
    $command = [GuiTestNative]::FindMenuCommandByText($menu, $Text)
    if ($command -lt 0) { throw "Cycle ${Cycle}: Edit command '$Text' was not found" }
    [GuiTestNative]::InvokeMenuCommand($Window, $command)
}

function Invoke-EditAccelerator([IntPtr] $Window, [string] $Text, [int] $Cycle) {
    $menu = Refresh-FocusMenu $Window $Cycle
    $command = [GuiTestNative]::FindMenuCommandByText($menu, $Text)
    if ($command -lt 0) { throw "Cycle ${Cycle}: accelerator '$Text' was not found" }
    [FocusCommandSmokeNative]::InvokeAccelerator($Window, $command)
}

function Assert-EditEnabled([IntPtr] $Window, [string] $Text, [bool] $Expected,
                             [int] $Cycle) {
    $menu = Refresh-FocusMenu $Window $Cycle
    $position = [GuiTestNative]::FindMenuPosition($menu, $Text)
    if ($position -lt 0) { throw "Cycle ${Cycle}: Edit item '$Text' was not found" }
    if ([GuiTestNative]::MenuItemEnabled($menu, $position) -ne $Expected) {
        throw "Cycle ${Cycle}: Edit item '$Text' enabled state was incorrect"
    }
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    [FocusCommandSmokeNative]::ClearClipboard()
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $candidate = Get-MainWindow $process.Id
            if ($candidate -ne [IntPtr]::Zero) {
                $script:FocusCommandWindow = $candidate
                return $true
            }
            return $false
        } "Cycle ${cycle}: focus command window did not appear exactly once"
        $window = $script:FocusCommandWindow

        $edits = [GuiTestNative]::FindChildren($window, 'Edit')
        $buttons = [GuiTestNative]::FindChildren($window, 'Button')
        if ($edits.Count -ne 2 -or $buttons.Count -ne 5) {
            throw "Cycle ${cycle}: expected 2 edits/5 native buttons, got $($edits.Count)/$($buttons.Count)"
        }
        $name = Require-Control $window 'Edit' 'Alpha' $cycle
        $description = Require-Control $window 'Edit' 'Beta' $cycle
        $focusButton = Require-Control $window 'Button' 'Focus Button' $cycle
        $list = ([GuiTestNative]::FindChildren($window, 'ListBox'))[0]
        $check = ([GuiTestNative]::FindChildren($window, 'Button') |
            Where-Object { [GuiTestNative]::GetText($_) -eq 'Check' })[0]
        $radio = ([GuiTestNative]::FindChildren($window, 'Button') |
            Where-Object { [GuiTestNative]::GetText($_) -eq 'Radio' })[0]
        $combo = ([GuiTestNative]::FindChildren($window, 'ComboBox'))[0]
        if ($list -eq [IntPtr]::Zero -or $check -eq [IntPtr]::Zero -or
            $radio -eq [IntPtr]::Zero -or $combo -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: focus probe controls were not realized"
        }

        if (-not (Focus-GuiControl $window $name)) {
            throw "Cycle ${cycle}: Name TextBox did not acquire focus"
        }
        Refresh-FocusMenu $window $cycle | Out-Null
        Require-FocusStatus $window 'Focused: TextBox' $cycle
        Assert-EditEnabled $window 'Cut' $false $cycle
        Assert-EditEnabled $window 'Copy' $false $cycle
        Assert-EditEnabled $window 'Paste' $false $cycle
        Assert-EditEnabled $window 'Delete' $false $cycle
        Assert-EditEnabled $window 'Select All' $true $cycle

        [GuiTestNative]::NativeSelectAll($name)
        Assert-EditEnabled $window 'Cut' $true $cycle
        Assert-EditEnabled $window 'Copy' $true $cycle
        Assert-EditEnabled $window 'Delete' $true $cycle
        Invoke-EditAccelerator $window 'Copy' $cycle
        if ((Get-Clipboard -Raw) -ne 'Alpha') {
            throw "Cycle ${cycle}: Copy did not target Name"
        }

        if (-not (Focus-GuiControl $window $description)) {
            throw "Cycle ${cycle}: Description TextBox did not acquire focus"
        }
        Assert-EditEnabled $window 'Paste' $true $cycle
        Invoke-EditAccelerator $window 'Paste' $cycle
        if ([GuiTestNative]::GetText($description) -ne 'BetaAlpha') {
            throw "Cycle ${cycle}: Paste did not target Description"
        }

        Focus-GuiControl $window $name | Out-Null
        [GuiTestNative]::NativeSelectAll($name)
        Invoke-Edit $window 'Cut' $cycle
        if ([GuiTestNative]::GetText($name) -ne '') {
            throw "Cycle ${cycle}: Cut did not remove Name selection"
        }
        if ([GuiTestNative]::GetText($description) -ne 'BetaAlpha') {
            throw "Cycle ${cycle}: Cut crossed into Description"
        }

        Focus-GuiControl $window $description | Out-Null
        Invoke-Edit $window 'Select All' $cycle
        $selectMenu = Refresh-FocusMenu $window $cycle
        Assert-EditEnabled $window 'Select All' $true $cycle
        if ([GuiTestNative]::GetText($description) -ne 'BetaAlpha') {
            throw "Cycle ${cycle}: Select All changed text"
        }

        foreach ($probe in @(
            @($focusButton, 'Button'),
            @($list, 'ListBox'),
            @($check, 'CheckBox'),
            @($radio, 'RadioButton'),
            @($combo, 'ComboBox'))) {
            if (-not (Focus-GuiControl $window $probe[0])) {
                throw "Cycle ${cycle}: $($probe[1]) did not acquire focus"
            }
            Refresh-FocusMenu $window $cycle | Out-Null
            Require-FocusStatus $window ('Focused: ' + $probe[1]) $cycle
            Assert-EditEnabled $window 'Cut' $false $cycle
            Assert-EditEnabled $window 'Copy' $false $cycle
            Assert-EditEnabled $window 'Paste' $false $cycle
            Assert-EditEnabled $window 'Delete' $false $cycle
            Assert-EditEnabled $window 'Select All' $false $cycle
        }

        Focus-GuiControl $window $name | Out-Null
        Send-GuiTab $window $name $false
        Refresh-FocusMenu $window $cycle | Out-Null
        Require-FocusStatus $window 'Focused: TextBox' $cycle
        Send-GuiTab $window $description $true
        Refresh-FocusMenu $window $cycle | Out-Null
        Require-FocusStatus $window 'Focused: TextBox' $cycle

        $lifecycle = [GuiTestNative]::FindTopLevelWindows(
            'Focus Command Lifecycle', $process.Id)
        if ($lifecycle.Count -ne 1) {
            throw "Cycle ${cycle}: lifecycle window was not found"
        }
        $lifecycle = $lifecycle[0]
        $close = Require-Control $lifecycle 'Button' 'Close Focus Window' $cycle
        $reopen = Require-Control $lifecycle 'Button' 'Reopen Focus Window' $cycle
        $quit = Require-Control $lifecycle 'Button' 'Quit App' $cycle
        Invoke-GuiButton $close
        Wait-Until { return (Get-MainWindow $process.Id) -eq [IntPtr]::Zero } `
            "Cycle ${cycle}: closing the focus window did not clear its realization"
        Invoke-GuiButton $reopen
        Wait-Until {
            $candidate = Get-MainWindow $process.Id
            if ($candidate -ne [IntPtr]::Zero) {
                $script:FocusCommandWindow = $candidate
                return $true
            }
            return $false
        } "Cycle ${cycle}: focus window did not reopen"
        $window = $script:FocusCommandWindow
        $name = Require-Control $window 'Edit' '' $cycle
        Refresh-FocusMenu $window $cycle | Out-Null
        Require-FocusStatus $window 'Focused: None' $cycle

        Invoke-GuiButton $quit
        if (-not $process.WaitForExit(5000)) {
            throw "Cycle ${cycle}: FocusCommandApp did not exit"
        }
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: expected exit code 0, got $($process.ExitCode)"
        }
        Write-Output "Cycle ${cycle}: focus identity, routed Edit commands, menu state, Tab order, and close/reopen passed."
    }
    finally {
        Stop-GuiProcess $process $window
    }
}

$remaining = Get-Process -Name FocusCommandApp -ErrorAction SilentlyContinue
if ($remaining) {
    $ids = ($remaining | ForEach-Object { $_.Id }) -join ', '
    throw "FocusCommandApp processes remain after smoke test: $ids"
}

Write-Output "PASS: FocusCommandApp GUI smoke completed $Cycles deterministic cycles."

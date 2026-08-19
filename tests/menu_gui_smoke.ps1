param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class MenuOpeningSmokeNative {
    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message,
                                             IntPtr wParam, IntPtr lParam);

    public static void PrepareMenu(IntPtr window, IntPtr menu) {
        SendMessage(window, 0x0117, menu, IntPtr.Zero); // WM_INITMENUPOPUP
    }
}
'@

for ($cycle = 1; $cycle -le $Cycles; ++$cycle) {
    $process = $null
    $window = [IntPtr]::Zero
    try {
        $process = Start-Process -FilePath $Executable -PassThru
        Wait-Until { $process.Refresh(); -not $process.HasExited } `
            "Cycle ${cycle}: MenuApp exited during startup"
        Wait-Until {
            $candidates = [GuiTestNative]::FindTopLevelWindows(
                'guideXOS Menu App', $process.Id)
            if ($candidates.Count -eq 1) {
                $script:MenuSmokeWindow = $candidates[0]
                return $true
            }
            return $false
        } "Cycle ${cycle}: MenuApp window was not found"
        $window = $script:MenuSmokeWindow

        $input = Require-Control $window 'Edit' 'Initial text' $cycle
        Start-GuiInputPolicy $window $input $cycle | Out-Null
        $status = Find-ControlPrefix $window 'Static' 'Status:'
        if ($status -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: status label was not found"
        }

        Wait-Until { [GuiTestNative]::MenuBar($window) -ne [IntPtr]::Zero } `
            "Cycle ${cycle}: native menu bar was not attached"
        $clientSize = [GuiTestNative]::ClientSize($window)
        if ($clientSize[0] -ne 800 -or $clientSize[1] -ne 500) {
            throw "Cycle ${cycle}: menu changed the requested 800x500 client size to $($clientSize[0])x$($clientSize[1])"
        }
        $root = [GuiTestNative]::MenuBar($window)
        if ([GuiTestNative]::MenuCount($root) -ne 3) {
            throw "Cycle ${cycle}: expected File/Edit/Help top-level menus"
        }
        foreach ($label in @('File', 'Edit', 'Help')) {
            if ([GuiTestNative]::FindMenuPosition($root, $label) -lt 0) {
                throw "Cycle ${cycle}: missing top-level menu '$label'"
            }
        }
        $rawFileText = [GuiTestNative]::MenuText(
            $root, [GuiTestNative]::FindMenuPosition($root, 'File'))
        if ($rawFileText -notlike '*&File*') {
            throw "Cycle ${cycle}: File mnemonic markup was not preserved"
        }

        $file = [GuiTestNative]::FindSubMenuByText($root, 'File')
        $edit = [GuiTestNative]::FindSubMenuByText($root, 'Edit')
        $help = [GuiTestNative]::FindSubMenuByText($root, 'Help')
        if ($file -eq [IntPtr]::Zero -or $edit -eq [IntPtr]::Zero -or
            $help -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: top-level menu submenu handle was missing"
        }
        $newCommand = [GuiTestNative]::FindMenuCommandByText($file, 'New')
        $resetCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Reset')
        $clearCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Clear Status')
        $exitCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Exit')
        if ($newCommand -lt 0 -or $resetCommand -lt 0 -or
            $clearCommand -lt 0 -or $exitCommand -lt 0) {
            throw "Cycle ${cycle}: File menu command discovery failed"
        }
        $clearPosition = [GuiTestNative]::FindMenuPosition($file, 'Clear Status')
        if ([GuiTestNative]::MenuItemEnabled($file, $clearPosition)) {
            throw "Cycle ${cycle}: disabled Clear Status command was enabled"
        }
        $statusBeforeDisabledInvoke = [GuiTestNative]::GetText($status)
        [GuiTestNative]::InvokeMenuCommand($window, $clearCommand)
        Start-Sleep -Milliseconds 100
        if ([GuiTestNative]::GetText($status) -ne $statusBeforeDisabledInvoke) {
            throw "Cycle ${cycle}: disabled Clear Status command invoked"
        }
        if ([GuiTestNative]::MenuText($file,
                [GuiTestNative]::FindMenuPosition($file, 'New')) -notlike '*Ctrl+N') {
            throw "Cycle ${cycle}: Ctrl+N accelerator was not realized"
        }

        $chooseMode = [GuiTestNative]::FindSubMenuByText($edit, 'Choose Mode')
        if ($chooseMode -eq [IntPtr]::Zero -or
            [GuiTestNative]::MenuCount($chooseMode) -ne 3) {
            throw "Cycle ${cycle}: nested Choose Mode submenu was not realized"
        }
        [MenuOpeningSmokeNative]::PrepareMenu($window, $chooseMode)
        Wait-Until { [GuiTestNative]::GetText($status) -eq 'Status: Choose Mode opened' } `
            "Cycle ${cycle}: nested Menu::OnOpening callback did not run"
        $root = [GuiTestNative]::MenuBar($window)
        $file = [GuiTestNative]::FindSubMenuByText($root, 'File')
        $newCommand = [GuiTestNative]::FindMenuCommandByText($file, 'New')

        # Direct WM_COMMAND is the deterministic native-message invocation;
        # native clicks and TranslateAccelerator use the same backend route.
        [GuiTestNative]::InvokeMenuCommand($window, $newCommand)
        Wait-Until { [GuiTestNative]::GetText($status) -eq 'Status: New selected' } `
            "Cycle ${cycle}: File > New callback did not run"
        if ([GuiTestNative]::GetText($input) -ne '') {
            throw "Cycle ${cycle}: File > New did not clear TextBox"
        }

        $file = [GuiTestNative]::FindSubMenuByText(
            [GuiTestNative]::MenuBar($window), 'File')
        $resetCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Reset')
        [GuiTestNative]::InvokeMenuCommand($window, $resetCommand)
        Wait-Until { [GuiTestNative]::GetText($status) -eq 'Status: Reset selected' } `
            "Cycle ${cycle}: File > Reset callback did not run"
        if ([GuiTestNative]::GetText($input) -ne 'Default text') {
            throw "Cycle ${cycle}: File > Reset did not update TextBox"
        }

        Send-GuiText $window $input 'changed through TextBox'
        Wait-Until {
            $fileNow = [GuiTestNative]::FindSubMenuByText(
                [GuiTestNative]::MenuBar($window), 'File')
            $position = [GuiTestNative]::FindMenuPosition($fileNow, 'Clear Status')
            $position -ge 0 -and [GuiTestNative]::MenuItemEnabled($fileNow, $position)
        } "Cycle ${cycle}: dynamic menu enable state did not update"
        $file = [GuiTestNative]::FindSubMenuByText(
            [GuiTestNative]::MenuBar($window), 'File')
        $clearCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Clear Status')
        [GuiTestNative]::InvokeMenuCommand($window, $clearCommand)
        Wait-Until { [GuiTestNative]::GetText($status) -eq 'Status: Cleared' } `
            "Cycle ${cycle}: dynamically enabled Clear Status did not run"

        $edit = [GuiTestNative]::FindSubMenuByText(
            [GuiTestNative]::MenuBar($window), 'Edit')
        $toggleCommand = [GuiTestNative]::FindMenuCommandByText($edit, 'Toggle Option')
        [GuiTestNative]::InvokeMenuCommand($window, $toggleCommand)
        $option = Find-Control $window 'Button' 'Option'
        Wait-Until { [GuiTestNative]::CheckState($option) -eq 1 } `
            "Cycle ${cycle}: Toggle Option did not update CheckBox"
        $edit = [GuiTestNative]::FindSubMenuByText(
            [GuiTestNative]::MenuBar($window), 'Edit')
        $togglePosition = [GuiTestNative]::FindMenuPosition($edit, 'Toggle Option')
        if (-not [GuiTestNative]::MenuItemChecked($edit, $togglePosition)) {
            throw "Cycle ${cycle}: checked menu state was not realized"
        }

        $edit = [GuiTestNative]::FindSubMenuByText(
            [GuiTestNative]::MenuBar($window), 'Edit')
        $chooseMode = [GuiTestNative]::FindSubMenuByText($edit, 'Choose Mode')
        $advancedCommand = [GuiTestNative]::FindMenuCommandByText($chooseMode, 'Advanced')
        [GuiTestNative]::InvokeMenuCommand($window, $advancedCommand)
        $combo = [GuiTestNative]::FindChildren($window, 'ComboBox')[0]
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 1 } `
            "Cycle ${cycle}: nested Choose Mode command did not update ComboBox"

        $help = [GuiTestNative]::FindSubMenuByText(
            [GuiTestNative]::MenuBar($window), 'Help')
        $aboutCommand = [GuiTestNative]::FindMenuCommandByText($help, 'About')
        [GuiTestNative]::InvokeMenuCommand($window, $aboutCommand)
        Wait-Until { [GuiTestNative]::GetText($status) -eq 'Status: About selected' } `
            "Cycle ${cycle}: Help > About callback did not run"
        Wait-Until {
            $updatedHelp = [GuiTestNative]::FindSubMenuByText(
                [GuiTestNative]::MenuBar($window), 'Help')
            [GuiTestNative]::FindMenuPosition($updatedHelp, 'About (selected)') -ge 0
        } "Cycle ${cycle}: dynamic menu text did not update"

        $help = [GuiTestNative]::FindSubMenuByText(
            [GuiTestNative]::MenuBar($window), 'Help')
        $helpResetCommand = [GuiTestNative]::FindMenuCommandByText($help, 'Reset')
        [GuiTestNative]::InvokeMenuCommand($window, $helpResetCommand)
        Wait-Until { [GuiTestNative]::GetText($status) -eq 'Status: Help Reset selected' } `
            "Cycle ${cycle}: duplicate Help > Reset routed incorrectly"

        $help = [GuiTestNative]::FindSubMenuByText(
            [GuiTestNative]::MenuBar($window), 'Help')
        $unicodePosition = 2
        $unicodeText = [GuiTestNative]::MenuText($help, $unicodePosition)
        if ($unicodeText.Length -lt 8 -or $unicodeText -notlike 'Caf*') {
            throw "Cycle ${cycle}: UTF-8 menu text was not realized"
        }
        [GuiTestNative]::InvokeMenuCommand($window,
            [GuiTestNative]::MenuCommand($help, $unicodePosition))
        Wait-Until { [GuiTestNative]::GetText($status) -eq 'Status: Unicode selected' } `
            "Cycle ${cycle}: Unicode menu command did not route"

        # Mnemonics use the documented ampersand convention. The raw native
        # label check above is deterministic; opening a menu through Alt is
        # left to the manual physical-input pass because USER32 may refuse
        # activation for a non-foreground test process.

        $file = [GuiTestNative]::FindSubMenuByText(
            [GuiTestNative]::MenuBar($window), 'File')
        $exitCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Exit')
        [GuiTestNative]::InvokeMenuCommand($window, $exitCommand)
        Wait-Until { $process.Refresh(); $process.HasExited } `
            "Cycle ${cycle}: Exit did not close MenuApp"
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: MenuApp exited with $($process.ExitCode)"
        }
        Write-Output "Cycle ${cycle}: MenuApp menu hierarchy/routing/state/lifecycle scenario passed."
    } finally {
        if ($window -ne [IntPtr]::Zero -and [GuiTestNative]::IsAlive($window)) {
            [GuiTestNative]::Close($window)
        }
        if ($process -and -not $process.HasExited) {
            $process.WaitForExit(5000)
        }
        if ($process -and -not $process.HasExited) {
            throw "Cycle ${cycle}: orphan MenuApp process remained"
        }
        if ($process) { $process.Dispose() }
    }
}

Write-Output "PASS: MenuApp GUI smoke completed $Cycles full menu cycles."

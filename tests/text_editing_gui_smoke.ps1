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

public static class TextEditingSmokeNative {
    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message,
                                             IntPtr wParam, IntPtr lParam);

    public static void PrepareMenu(IntPtr window, IntPtr menu) {
        SendMessage(window, 0x0117, menu, IntPtr.Zero); // WM_INITMENUPOPUP
    }
}
'@

function Read-ExactClipboardText {
    $value = Get-Clipboard -Raw -ErrorAction Stop
    if ($null -eq $value) { return '' }
    return [string]$value
}

function Require-TextEditingWindow([System.Diagnostics.Process] $Process,
                                    [int] $Cycle) {
    $result = [IntPtr]::Zero
    Wait-Until {
        $candidates = [GuiTestNative]::FindTopLevelWindows(
            'guideXOS Text Editing Demo', $Process.Id)
        if ($candidates.Count -eq 1) {
            $script:TextEditingWindow = $candidates[0]
            return $true
        }
        return $false
    } "Cycle ${Cycle}: TextEditingApp demo window was not found exactly once"
    return $script:TextEditingWindow
}

function Get-TextEditingControls([IntPtr] $Window, [int] $Cycle,
                                  [bool] $Initial = $true) {
    if ($Initial) {
        $script:TextEditingSource = Require-Control $Window 'Edit' $script:TextEditingUnicodeText $Cycle
        $script:TextEditingDestination = Require-Control $Window 'Edit' 'Paste here' $Cycle
    } else {
        $edits = [GuiTestNative]::FindChildren($Window, 'Edit')
        if ($edits.Count -ne 2) {
            throw "Cycle ${Cycle}: reopened demo did not recreate two edit controls"
        }
        $script:TextEditingSource = $edits[0]
        $script:TextEditingDestination = $edits[1]
    }
    $script:TextEditingSelectEmoji = Require-Control $Window 'Button' 'Select Emoji' $Cycle
    $script:TextEditingSelectAll = Require-Control $Window 'Button' 'Select All' $Cycle
    $script:TextEditingClearSelection = Require-Control $Window 'Button' 'Clear Selection' $Cycle
    $script:TextEditingSetCaret = Require-Control $Window 'Button' 'Set Caret: 1' $Cycle
    $script:TextEditingCopy = Require-Control $Window 'Button' 'Copy' $Cycle
    $script:TextEditingCut = Require-Control $Window 'Button' 'Cut' $Cycle
    $script:TextEditingPaste = Require-Control $Window 'Button' 'Paste' $Cycle
    $script:TextEditingDelete = Require-Control $Window 'Button' 'Delete Selection' $Cycle
    $script:TextEditingSourceTarget = Require-Control $Window 'Button' 'Target: Source' $Cycle
    $script:TextEditingDestinationTarget = Require-Control $Window 'Button' 'Target: Destination' $Cycle
    $script:TextEditingSelection = Find-ControlPrefix $Window 'Static' 'Selection:'
    $script:TextEditingCaret = Find-ControlPrefix $Window 'Static' 'Caret:'
    $script:TextEditingSelected = Find-ControlPrefix $Window 'Static' 'Selected Text:'
    if ($script:TextEditingSelection -eq [IntPtr]::Zero -or
        $script:TextEditingCaret -eq [IntPtr]::Zero -or
        $script:TextEditingSelected -eq [IntPtr]::Zero) {
        throw "Cycle ${Cycle}: text editing status labels were not found"
    }
}

function Invoke-TextEditingMenu([IntPtr] $Window, [string] $Text,
                                 [int] $Cycle) {
    $root = [GuiTestNative]::MenuBar($Window)
    $edit = [GuiTestNative]::FindSubMenuByText($root, 'Edit')
    if ($edit -eq [IntPtr]::Zero) {
        throw "Cycle ${Cycle}: Edit menu was not realized"
    }
    [TextEditingSmokeNative]::PrepareMenu($Window, $edit)
    $root = [GuiTestNative]::MenuBar($Window)
    $edit = [GuiTestNative]::FindSubMenuByText($root, 'Edit')
    $command = [GuiTestNative]::FindMenuCommandByText($edit, $Text)
    if ($command -lt 0) {
        throw "Cycle ${Cycle}: Edit menu command '$Text' was not found"
    }
    [GuiTestNative]::InvokeMenuCommand($Window, $command)
}

$unicodeText = 'A caf' + [char]0x00E9 + ' ' + [char]0x65E5 +
    [char]0x672C + ' ' + [char]::ConvertFromUtf32(0x1F680)
$script:TextEditingUnicodeText = $unicodeText
$rocket = [char]::ConvertFromUtf32(0x1F680)
$sourceAfterCut = 'A caf' + [char]0x00E9 + ' ' + [char]0x65E5 +
    [char]0x672C + ' '

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = $null
    $window = [IntPtr]::Zero
    try {
        $process = Start-Process -FilePath (Resolve-Path -LiteralPath $Executable).Path -PassThru
        $window = Require-TextEditingWindow $process $cycle
        Get-TextEditingControls $window $cycle $true
        $lifecycle = [GuiTestNative]::FindTopLevelWindows(
            'Text Editing Lifecycle', $process.Id)
        if ($lifecycle.Count -ne 1) {
            throw "Cycle ${cycle}: lifecycle window was not found exactly once"
        }
        $lifecycle = $lifecycle[0]
        $closeDemo = Require-Control $lifecycle 'Button' 'Close Demo Window' $cycle
        $reopenDemo = Require-Control $lifecycle 'Button' 'Reopen Demo Window' $cycle
        $quit = Require-Control $lifecycle 'Button' 'Quit App' $cycle

        if (-not (Focus-GuiControl $window $script:TextEditingSource)) {
            throw "Cycle ${cycle}: source TextBox did not acquire initial focus"
        }

        if ([GuiTestNative]::GetText($script:TextEditingSelection) -ne 'Selection: none' -or
            [GuiTestNative]::GetText($script:TextEditingCaret) -ne 'Caret: none') {
            throw "Cycle ${cycle}: initial no-focus state was incorrect"
        }

        # Programmatic selection and Unicode boundary behavior. The emoji is
        # one public scalar at index 10, even though the native edit uses two
        # UTF-16 code units for it.
        Invoke-GuiButton $script:TextEditingSelectEmoji
        Wait-Until {
            [GuiTestNative]::GetText($script:TextEditingSelection) -eq 'Selection: start=10 length=1' -and
            [GuiTestNative]::GetText($script:TextEditingCaret) -eq 'Caret: 11' -and
            [GuiTestNative]::GetText($script:TextEditingSelected) -eq ('Selected Text: ' + $rocket)
        } "Cycle ${cycle}: programmatic emoji selection state was incorrect"

        Invoke-TextEditingMenu $window 'Copy' $cycle
        Wait-Until { (Read-ExactClipboardText) -eq $rocket } `
            "Cycle ${cycle}: Copy did not publish the exact selected emoji"

        Invoke-TextEditingMenu $window 'Cut' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($script:TextEditingSource) -eq $sourceAfterCut -and
            [GuiTestNative]::GetText($script:TextEditingSelection) -eq 'Selection: start=10 length=0' -and
            [GuiTestNative]::GetText($script:TextEditingCaret) -eq 'Caret: 10'
        } "Cycle ${cycle}: Cut did not remove the exact selection or collapse at its start"

        # Copy/paste between independent TextBoxes uses the same process-wide
        # App Model clipboard, without sharing model state.
        Invoke-GuiButton $script:TextEditingDestinationTarget
        Invoke-TextEditingMenu $window 'Paste' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($script:TextEditingDestination) -eq ('Paste here' + $rocket)
        } "Cycle ${cycle}: Paste did not insert clipboard text at the caret"

        Invoke-GuiButton $script:TextEditingSourceTarget
        Send-GuiText $window $script:TextEditingSource $unicodeText
        Invoke-GuiButton $script:TextEditingSetCaret
        Wait-Until {
            [GuiTestNative]::GetText($script:TextEditingSelection) -eq 'Selection: start=1 length=0' -and
            [GuiTestNative]::GetText($script:TextEditingCaret) -eq 'Caret: 1'
        } "Cycle ${cycle}: SetCaretIndex did not reach the model/native edit"

        # Native targeted EM_SETSEL is used only to exercise the user-edit
        # synchronization boundary; the assertion is made through public
        # commands and displayed model state.
        [GuiTestNative]::NativeSelectRange($script:TextEditingSource, 10, 12)
        Invoke-TextEditingMenu $window 'Copy' $cycle
        Wait-Until { (Read-ExactClipboardText) -eq $rocket } `
            "Cycle ${cycle}: native UTF-16 selection did not copy the exact scalar selection"
        Invoke-GuiButton $script:TextEditingClearSelection
        Wait-Until {
            [GuiTestNative]::GetText($script:TextEditingSelection) -eq 'Selection: start=11 length=0' -and
            [GuiTestNative]::GetText($script:TextEditingCaret) -eq 'Caret: 11'
        } "Cycle ${cycle}: native selection synchronization did not expose the endpoint"

        Invoke-TextEditingMenu $window 'Select All' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($script:TextEditingSelection) -eq 'Selection: start=0 length=11' -and
            [GuiTestNative]::GetText($script:TextEditingSelected) -eq ('Selected Text: ' + $unicodeText)
        } "Cycle ${cycle}: SelectAll did not select the complete Unicode text"
        Invoke-TextEditingMenu $window 'Copy' $cycle
        Wait-Until { (Read-ExactClipboardText) -eq $unicodeText } `
            "Cycle ${cycle}: full-text Copy did not preserve Unicode/emoji text"

        Invoke-TextEditingMenu $window 'Cut' $cycle
        Wait-Until { [GuiTestNative]::GetText($script:TextEditingSource) -eq '' } `
            "Cycle ${cycle}: full-text Cut did not empty the source"
        Invoke-GuiButton $script:TextEditingDestinationTarget
        Invoke-TextEditingMenu $window 'Select All' $cycle
        Invoke-TextEditingMenu $window 'Paste' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($script:TextEditingDestination) -eq $unicodeText
        } "Cycle ${cycle}: Paste did not replace the destination selection"

        Invoke-TextEditingMenu $window 'Select All' $cycle
        Invoke-TextEditingMenu $window 'Delete Selection' $cycle
        if ([GuiTestNative]::GetText($script:TextEditingDestination) -ne '') {
            throw "Cycle ${cycle}: DeleteSelection did not remove the selected destination text"
        }
        if ((Read-ExactClipboardText) -ne $unicodeText) {
            throw "Cycle ${cycle}: DeleteSelection changed the clipboard"
        }
        Invoke-TextEditingMenu $window 'Copy' $cycle
        if ((Read-ExactClipboardText) -ne $unicodeText) {
            throw "Cycle ${cycle}: empty Copy did not remain a no-op"
        }

        # Menu commands use the same control methods through the App Model's
        # current focused-control query.
        Invoke-GuiButton $script:TextEditingSourceTarget
        Send-GuiText $window $script:TextEditingSource $unicodeText
        Invoke-TextEditingMenu $window 'Select All' $cycle
        Wait-Until { [GuiTestNative]::GetText($script:TextEditingSelection) -eq 'Selection: start=0 length=11' } `
            "Cycle ${cycle}: menu Select All did not route to TextBox::SelectAll"
        Invoke-TextEditingMenu $window 'Copy' $cycle
        Wait-Until { (Read-ExactClipboardText) -eq $unicodeText } `
            "Cycle ${cycle}: menu Copy did not route to TextBox::Copy"
        Invoke-TextEditingMenu $window 'Cut' $cycle
        Wait-Until { [GuiTestNative]::GetText($script:TextEditingSource) -eq '' } `
            "Cycle ${cycle}: menu Cut did not route to TextBox::Cut"
        Invoke-GuiButton $script:TextEditingDestinationTarget
        Invoke-TextEditingMenu $window 'Paste' $cycle
        Wait-Until { [GuiTestNative]::GetText($script:TextEditingDestination) -eq $unicodeText } `
            "Cycle ${cycle}: menu Paste did not route to TextBox::Paste"
        Invoke-TextEditingMenu $window 'Select All' $cycle
        Invoke-TextEditingMenu $window 'Delete Selection' $cycle
        Wait-Until { [GuiTestNative]::GetText($script:TextEditingDestination) -eq '' } `
            "Cycle ${cycle}: menu Delete Selection did not route to TextBox::DeleteSelection"

        # Close/reopen the logical demo window while its lifecycle window keeps
        # the explicit application alive; the model selection/text state is
        # rediscovered after native controls are recreated.
        Invoke-GuiButton $closeDemo
        Wait-Until {
            $candidates = [GuiTestNative]::FindTopLevelWindows(
                'guideXOS Text Editing Demo', $process.Id)
            return $candidates.Count -eq 0
        } "Cycle ${cycle}: demo window did not close safely"
        Invoke-GuiButton $reopenDemo
        $window = Require-TextEditingWindow $process $cycle
        Get-TextEditingControls $window $cycle $false
        if ([GuiTestNative]::GetText($script:TextEditingSource) -ne '' -or
            [GuiTestNative]::GetText($script:TextEditingDestination) -ne '') {
            throw "Cycle ${cycle}: close/reopen did not preserve final model text state"
        }

        Invoke-GuiButton $quit
        if (-not $process.WaitForExit(5000)) {
            throw "Cycle ${cycle}: TextEditingApp did not exit after Quit App"
        }
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: TextEditingApp exited with code $($process.ExitCode)"
        }
        Write-Output "Cycle ${cycle}: TextBox selection, Unicode, clipboard commands, menu routing, and close/reopen passed."
    }
    finally {
        Stop-GuiProcess $process $window
    }
}

$remaining = Get-Process -Name TextEditingApp -ErrorAction SilentlyContinue
if ($remaining) {
    $ids = ($remaining | ForEach-Object { $_.Id }) -join ', '
    throw "TextEditingApp processes remain after smoke test: $ids"
}

Write-Output "PASS: TextEditingApp GUI smoke completed $Cycles deterministic cycles."

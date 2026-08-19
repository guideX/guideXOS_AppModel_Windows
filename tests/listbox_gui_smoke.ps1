param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')


function Send-Tab([IntPtr] $Window, [IntPtr] $Current) {
    Send-GuiTab $Window $Current $false
}

function Send-ShiftTab([IntPtr] $Window, [IntPtr] $Current) {
    Send-GuiTab $Window $Current $true
}

function Send-Text([IntPtr] $Window, [IntPtr] $Control, [string] $Text) {
    Send-GuiText $Window $Control $Text
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS ListBox Sample'
$unicodeItem = 'new ' + [char]0x65B0 + [char]0x898F + ' ' + [char]0xD83D + [char]0xDE80

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    try {
        $window = [IntPtr]::Zero
        Wait-Until {
            $windows = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
            if ($windows.Count -eq 1) { $window = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: ListBoxApp window did not appear exactly once"
        $window = ([GuiTestNative]::FindTopLevelWindows($title, $process.Id))[0]

        $lists = [GuiTestNative]::FindChildren($window, 'ListBox')
        $edits = [GuiTestNative]::FindChildren($window, 'Edit')
        $buttons = [GuiTestNative]::FindChildren($window, 'Button')
        $labels = [GuiTestNative]::FindChildren($window, 'Static')
        if ($lists.Count -ne 1 -or $edits.Count -ne 1 -or $buttons.Count -ne 3 -or $labels.Count -ne 4) {
            throw "Cycle ${cycle}: expected 1 list/1 edit/3 buttons/4 labels, got $($lists.Count)/$($edits.Count)/$($buttons.Count)/$($labels.Count)"
        }
        $list = $lists[0]
        $textBox = $edits[0]
        $add = Find-Control $window 'Button' 'Add'
        $remove = Find-Control $window 'Button' 'Remove Selected'
        $clear = Find-Control $window 'Button' 'Clear'
        $selected = Find-Control $window 'Static' 'Selected: none'
        $count = Find-Control $window 'Static' 'Count: 7'
        if ($add -eq [IntPtr]::Zero -or $remove -eq [IntPtr]::Zero -or $clear -eq [IntPtr]::Zero -or
            $selected -eq [IntPtr]::Zero -or $count -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: initial control lookup failed"
        }
        foreach ($control in @($list, $textBox, $add, $remove, $clear, $selected, $count)) {
            if (-not [GuiTestNative]::IsVisible($control)) { throw "Cycle ${cycle}: a required control is not visible" }
        }
        if ([GuiTestNative]::ListCount($list) -ne 7 -or
            [GuiTestNative]::ListItem($list, 3) -ne [GuiTestNative]::Cafe() -or
            [GuiTestNative]::ListItem($list, 4) -ne [GuiTestNative]::Japanese() -or
            [GuiTestNative]::ListItem($list, 5) -ne [GuiTestNative]::Rocket()) {
            throw "Cycle ${cycle}: initial list content or Unicode is incorrect"
        }
        $docs = 0
        0..6 | ForEach-Object { if ([GuiTestNative]::ListItem($list, $_) -eq 'docs') { $docs++ } }
        if ($docs -ne 2) { throw "Cycle ${cycle}: duplicate initial items were not preserved" }

        Start-GuiInputPolicy $window $textBox $cycle

        Send-Tab $window $textBox
        Wait-Until { return [GuiTestNative]::FocusedControl($window) -eq $add } "Cycle ${cycle}: Tab did not reach Add"
        Send-ShiftTab $window $add
        Wait-Until { return [GuiTestNative]::FocusedControl($window) -eq $textBox } "Cycle ${cycle}: Shift+Tab did not return to input"
        Send-ShiftTab $window $textBox
        Wait-Until { return [GuiTestNative]::FocusedControl($window) -eq $list } "Cycle ${cycle}: Shift+Tab did not reach ListBox"

        # Native selection is the deterministic fallback for the physical
        # click. Manual mouse validation remains a release checkpoint.
        [GuiTestNative]::NativeListSelect($window, $list, 2)
        Wait-Until { return [GuiTestNative]::SelectedIndex($list) -eq 2 -and
            [GuiTestNative]::GetText($selected) -eq ('Selected [2]: docs') } "Cycle ${cycle}: selection callback did not update selected label"

        Send-GuiKey $window $list 0x28 # Down
        Wait-Until { return [GuiTestNative]::SelectedIndex($list) -eq 3 -and
            [GuiTestNative]::GetText($selected) -eq ('Selected [3]: ' + [GuiTestNative]::Cafe()) } "Cycle ${cycle}: Down keyboard selection failed"
        Send-GuiKey $window $list 0x26 # Up
        Wait-Until { return [GuiTestNative]::SelectedIndex($list) -eq 2 } "Cycle ${cycle}: Up keyboard selection failed"

        if (-not (Focus-GuiControl $window $textBox)) {
            throw "Cycle ${cycle}: TextBox focus failed before Unicode insertion"
        }
        if ([GuiTestNative]::FocusedControl($window) -ne $textBox) {
            throw "Cycle ${cycle}: TextBox focus was lost before Unicode insertion"
        }
        if (-not [GuiTestNative]::IsAlive($textBox)) {
            throw "Cycle ${cycle}: TextBox native handle was destroyed before Unicode insertion"
        }
        Send-GuiSelectAll $window $textBox
        Send-Text $window $textBox $unicodeItem
        [GuiTestNative]::NativeButton($add)
        Wait-Until { return [GuiTestNative]::ListCount($list) -eq 8 } "Cycle ${cycle}: Unicode item was not added"
        if ([GuiTestNative]::ListItem($list, 7) -ne $unicodeItem) {
            throw "Cycle ${cycle}: Unicode item did not display; expected=<$unicodeItem>, actual=<$( [GuiTestNative]::ListItem($list, 7) )>, edit=<$( [GuiTestNative]::GetText($textBox) )>"
        }
        if ([GuiTestNative]::SelectedIndex($list) -ne 2) { throw "Cycle ${cycle}: append changed logical selection" }

        Send-GuiSelectAll $window $textBox
        Send-Text $window $textBox 'docs'
        [GuiTestNative]::NativeButton($add)
        Wait-Until { return [GuiTestNative]::ListCount($list) -eq 9 } "Cycle ${cycle}: duplicate item was not added"
        [GuiTestNative]::NativeListSelect($window, $list, 8)
        Wait-Until { return [GuiTestNative]::SelectedIndex($list) -eq 8 } "Cycle ${cycle}: appended duplicate was not selectable"
        [GuiTestNative]::NativeButton($remove)
        Wait-Until { return [GuiTestNative]::ListCount($list) -eq 8 -and
            [GuiTestNative]::SelectedIndex($list) -eq -1 -and
            [GuiTestNative]::GetText($selected) -eq 'Selected: none' } "Cycle ${cycle}: Remove Selected contract failed"

        1..3 | ForEach-Object {
            Send-GuiSelectAll $window $textBox
            Send-Text $window $textBox ("cycle-" + $_)
            [GuiTestNative]::NativeButton($add)
            Wait-Until { return [GuiTestNative]::ListCount($list) -eq 9 } "Cycle ${cycle}: repeated add $($_) failed"
            [GuiTestNative]::NativeListSelect($window, $list, 8)
            Wait-Until { return [GuiTestNative]::SelectedIndex($list) -eq 8 } "Cycle ${cycle}: repeated select $($_) failed"
            [GuiTestNative]::NativeButton($remove)
            Wait-Until { return [GuiTestNative]::ListCount($list) -eq 8 -and
                [GuiTestNative]::SelectedIndex($list) -eq -1 } "Cycle ${cycle}: repeated remove $($_) failed"
        }

        [GuiTestNative]::NativeButton($clear)
        Wait-Until { return [GuiTestNative]::ListCount($list) -eq 0 -and
            [GuiTestNative]::SelectedIndex($list) -eq -1 -and
            [GuiTestNative]::GetText($count) -eq 'Count: 0' } "Cycle ${cycle}: Clear did not empty list and selection"

        # Select once immediately before closing to exercise the close-after-
        # dispatch path on a live native ListBox.
        Send-GuiSelectAll $window $textBox
        Send-Text $window $textBox 'closing'
        [GuiTestNative]::NativeButton($add)
        [GuiTestNative]::NativeListSelect($window, $list, 0)
        Wait-Until { return [GuiTestNative]::SelectedIndex($list) -eq 0 } "Cycle ${cycle}: final selection failed"
        [GuiTestNative]::Close($window)
        Wait-Until { return $process.HasExited } "Cycle ${cycle}: close after selection did not exit normally"
        if ($process.ExitCode -ne 0) { throw "Cycle ${cycle}: ListBoxApp exited with code $($process.ExitCode)" }
    }
    finally {
        if (-not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}

Write-Output "PASS: ListBoxApp GUI smoke completed $Cycles full native selection/mutation cycles."

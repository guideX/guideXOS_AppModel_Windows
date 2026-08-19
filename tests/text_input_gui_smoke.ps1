param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

# Native details are isolated in the shared GUI smoke helper.

function Send-SelectAll([IntPtr] $Control) {
    Send-GuiSelectAll $script:GuiWindow $Control
}

function Send-Text([IntPtr] $Control, [string] $Text) {
    Send-GuiText $script:GuiWindow $Control $Text
}

function Send-EditKey([IntPtr] $Control, [uint16] $Key) {
    Send-GuiKey $script:GuiWindow $Control $Key
}

function Send-Tab([IntPtr] $Control) {
    Send-GuiTab $script:GuiWindow $Control $false
}

function Send-ShiftTab([IntPtr] $Window, [IntPtr] $Control) {
    Send-GuiTab $Window $Control $true
}

function Send-ButtonKey([IntPtr] $Control, [uint16] $Key) {
    Send-GuiButtonKey $script:GuiWindow $Control $Key
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS Text Input Sample'
$unicodeName = [GuiTestNative]::UnicodeName()
$unicodeMessage = [GuiTestNative]::UnicodeMessage()
$emDash = [GuiTestNative]::EmDash()

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    try {
        $window = [IntPtr]::Zero
        Wait-Until {
            $windows = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
            if ($windows.Count -eq 1) { $window = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: TextInputApp window did not appear exactly once"
        $window = ([GuiTestNative]::FindTopLevelWindows($title, $process.Id))[0]

        $edits = [GuiTestNative]::FindChildren($window, 'Edit')
        $buttons = [GuiTestNative]::FindChildren($window, 'Button')
        $labels = [GuiTestNative]::FindChildren($window, 'Static')
        if ($edits.Count -ne 2 -or $buttons.Count -ne 2 -or $labels.Count -ne 4) {
            throw "Cycle ${cycle}: expected 2 edits/2 buttons/4 labels, got $($edits.Count)/$($buttons.Count)/$($labels.Count)"
        }

        $nameInput = Find-Control $window 'Edit' 'Ada Lovelace'
        $messageInput = Find-Control $window 'Edit' 'Welcome to guideXOS.'
        $unicodeButton = Find-Control $window 'Button' 'Insert Unicode Sample'
        $clearButton = Find-Control $window 'Button' 'Clear'
        $preview = Find-ControlPrefix $window 'Static' 'Preview:'
        if ($nameInput -eq [IntPtr]::Zero -or $messageInput -eq [IntPtr]::Zero -or
            $unicodeButton -eq [IntPtr]::Zero -or $clearButton -eq [IntPtr]::Zero -or
            $preview -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: initial control lookup failed"
        }
        foreach ($control in @($nameInput, $messageInput, $unicodeButton, $clearButton, $preview)) {
            if (-not [GuiTestNative]::IsVisible($control)) {
                throw "Cycle ${cycle}: a required control is not visible"
            }
        }
        if ([GuiTestNative]::GetText($preview) -ne 'Preview: Hello, Ada Lovelace ' + $emDash + ' Welcome to guideXOS.') {
            throw "Cycle ${cycle}: initial preview text is incorrect"
        }

        $script:GuiWindow = $window
        Start-GuiInputPolicy $window $nameInput $cycle
        Send-SelectAll $nameInput
        Send-Text $nameInput 'Grace'
        Wait-Until {
            return [GuiTestNative]::GetText($nameInput) -eq 'Grace' -and
                   [GuiTestNative]::GetText($preview) -eq ('Preview: Hello, Grace ' + $emDash + ' Welcome to guideXOS.')
        } "Cycle ${cycle}: keyboard editing did not update the preview; name=<$([GuiTestNative]::GetText($nameInput))>, preview=<$([GuiTestNative]::GetText($preview))>"

        Send-Tab $nameInput
        Wait-Until { return [GuiTestNative]::FocusedControl($window) -eq $messageInput } "Cycle ${cycle}: Tab did not focus the message TextBox"
        Send-ShiftTab $window $messageInput
        Wait-Until { return [GuiTestNative]::FocusedControl($window) -eq $nameInput } "Cycle ${cycle}: Shift+Tab did not return to the name TextBox"

        # Exercise native Backspace and Delete behavior through real keyboard input.
        Send-SelectAll $nameInput
        Send-Text $nameInput 'AB'
        Send-EditKey $nameInput 0x24 # Home
        Send-EditKey $nameInput 0x2E # Delete
        Send-EditKey $nameInput 0x23 # End
        Send-EditKey $nameInput 0x08 # Backspace
        try {
            Wait-Until { return [GuiTestNative]::GetText($nameInput) -eq '' } "Cycle ${cycle}: Delete/Backspace editing was incorrect"
        } catch {
            throw "Cycle ${cycle}: Delete/Backspace editing was incorrect; name=<$([GuiTestNative]::GetText($nameInput))>"
        }
        Send-Text $nameInput 'Edit-safe'
        Wait-Until { return [GuiTestNative]::GetText($nameInput) -eq 'Edit-safe' } "Cycle ${cycle}: repeated keyboard edit failed"

        Send-Tab $nameInput
        Wait-Until { return [GuiTestNative]::FocusedControl($window) -eq $messageInput } "Cycle ${cycle}: second Tab did not focus the message TextBox"
        Send-Tab $messageInput
        Wait-Until { return [GuiTestNative]::FocusedControl($window) -eq $unicodeButton } "Cycle ${cycle}: Tab did not follow creation/layout order to the button"
        Send-ButtonKey $unicodeButton 0x0D # Enter activates the focused button.
        try {
            Wait-Until {
                return [GuiTestNative]::GetText($nameInput) -eq $unicodeName -and
                       [GuiTestNative]::GetText($messageInput) -eq $unicodeMessage -and
                       [GuiTestNative]::GetText($preview) -eq ('Preview: Hello, ' + $unicodeName + ' ' + $emDash + ' ' + $unicodeMessage)
            } "Cycle ${cycle}: the Unicode/programmatic update did not round-trip"
        } catch {
            throw "Cycle ${cycle}: the Unicode/programmatic update did not round-trip; name=<$([GuiTestNative]::GetText($nameInput))>, message=<$([GuiTestNative]::GetText($messageInput))>, preview=<$([GuiTestNative]::GetText($preview))>"
        }

        Send-Tab $unicodeButton
        Wait-Until { return [GuiTestNative]::FocusedControl($window) -eq $clearButton } "Cycle ${cycle}: Tab did not focus the Clear button"
        Send-ButtonKey $clearButton 0x20 # Space activates the focused button.
        Wait-Until {
            return [GuiTestNative]::GetText($nameInput) -eq '' -and
                   [GuiTestNative]::GetText($messageInput) -eq '' -and
                   [GuiTestNative]::GetText($preview) -eq ('Preview: Hello,  ' + $emDash + ' ')
        } "Cycle ${cycle}: Clear did not reset both TextBoxes"

        if (-not (Focus-GuiControl $window $nameInput)) {
            throw "Cycle ${cycle}: final edit focus failed"
        }
        Send-Text $nameInput 'closing'
        [GuiTestNative]::Close($window)
        Wait-Until { return $process.HasExited } "Cycle ${cycle}: closing after editing did not exit normally"
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: TextInputApp exited with code $($process.ExitCode)"
        }
    }
    finally {
        if (-not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}

Write-Output "PASS: TextInputApp GUI smoke completed $Cycles full keyboard/input cycles."

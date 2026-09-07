param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

function Normalize-AppText([string] $Value) {
    if ($null -eq $Value) { return '' }
    return $Value.Replace("`r`n", "`n").Replace("`r", "`n")
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS Multiline Text Demo'
$exampleText = 'This is editable multiline text.' + "`n`n" + 'Multiple lines should work here.'
$editedText = 'first line' + "`n" + 'second line'
$clipboardBefore = $null
$clipboardCaptured = $false
try {
    $clipboardBefore = Get-Clipboard -Raw -ErrorAction Stop
    $clipboardCaptured = $true
} catch {
    # Clipboard restoration is best effort when the desktop clipboard is unavailable.
}

try {
    for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
        $process = $null
        $window = [IntPtr]::Zero
        try {
            $process = Start-Process -FilePath $resolvedExecutable -PassThru
            Wait-Until {
                $process.Refresh()
                return -not $process.HasExited
            } "Cycle ${cycle}: MultilineTextApp exited during startup"
            Wait-Until {
                $windows = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
                if ($windows.Count -eq 1) {
                    $script:MultilineTextWindow = $windows[0]
                    return $true
                }
                return $false
            } "Cycle ${cycle}: multiline demo window did not appear exactly once"
            $window = $script:MultilineTextWindow

            Wait-Until {
                $edits = [GuiTestNative]::FindChildren($window, 'Edit')
                return $edits.Count -eq 2
            } "Cycle ${cycle}: expected title and body edits were not realized (buttons=$([GuiTestNative]::FindChildren($window, 'Button').Count), statics=$([GuiTestNative]::FindChildren($window, 'Static').Count))"
            $edits = [GuiTestNative]::FindChildren($window, 'Edit')
            $titleEdit = Find-Control $window 'Edit' 'My Notes'
            $body = $edits[1]
            $readOnly = Require-Control $window 'Button' 'Read only' $cycle
            $copy = Require-Control $window 'Button' 'Copy Body' $cycle
            $paste = Require-Control $window 'Button' 'Paste Body' $cycle
            $clear = Require-Control $window 'Button' 'Clear' $cycle
            $restore = Require-Control $window 'Button' 'Restore' $cycle
            $characters = Require-Control $window 'Static' 'Characters: 66' $cycle
            $status = Require-Control $window 'Static' 'Status: Ready' $cycle
            if ($titleEdit -eq [IntPtr]::Zero) {
                throw "Cycle ${cycle}: title TextBox was not discovered"
            }

            Start-GuiInputPolicy $window $body $cycle
            Invoke-GuiButton $clear
            Wait-Until {
                return (Normalize-AppText ([GuiTestNative]::GetText($body))) -eq ''
            } "Cycle ${cycle}: initial Clear did not empty the multiline editor"
            if (-not [GuiTestNative]::FocusNative($window, $body)) {
                throw "Cycle ${cycle}: body editor could not receive typing focus"
            }
            [GuiTestNative]::NativeAppendText($body, 'first line')
            [GuiTestNative]::NativeChar($body, 0x0D)
            [GuiTestNative]::NativeAppendText($body, 'second line')
            Wait-Until {
                return (Normalize-AppText ([GuiTestNative]::GetText($body))) -eq $editedText -and
                       [GuiTestNative]::GetText($characters) -eq 'Characters: 22' -and
                       [GuiTestNative]::GetText($status) -eq 'Status: Body changed'
            } "Cycle ${cycle}: native multiline typing/newline did not update AppModel-visible state"
            if ([GuiTestNative]::FocusedControl($window) -ne $body) {
                throw "Cycle ${cycle}: body editor did not retain keyboard focus"
            }

            [GuiTestNative]::NativeSelectAll($body)
            Invoke-GuiButton $copy
            Wait-Until {
                return (Normalize-AppText ([string](Get-Clipboard -Raw))) -eq $editedText
            } "Cycle ${cycle}: multiline selection/copy did not preserve both lines"

            Invoke-GuiButton $clear
            Wait-Until {
                return (Normalize-AppText ([GuiTestNative]::GetText($body))) -eq ''
            } "Cycle ${cycle}: Clear did not empty the multiline editor"
            Invoke-GuiButton $paste
            Wait-Until {
                return (Normalize-AppText ([GuiTestNative]::GetText($body))) -eq $editedText
            } "Cycle ${cycle}: Paste did not restore the selected multiline block"

            Invoke-GuiButton $readOnly
            Wait-Until { return [GuiTestNative]::CheckState($readOnly) -eq 1 } `
                "Cycle ${cycle}: read-only checkbox did not turn on"
            if (-not [GuiTestNative]::IsEnabled($body)) {
                throw "Cycle ${cycle}: read-only editor was incorrectly disabled instead of remaining selectable"
            }
            if (-not [GuiTestNative]::FocusNative($window, $body)) {
                throw "Cycle ${cycle}: read-only editor could not receive focus"
            }
            [GuiTestNative]::NativeChar($body, [uint16][char]'X')
            if ((Normalize-AppText ([GuiTestNative]::GetText($body))) -ne $editedText) {
                throw "Cycle ${cycle}: read-only editor accepted a native edit"
            }

            Invoke-GuiButton $readOnly
            Wait-Until { return [GuiTestNative]::CheckState($readOnly) -eq 0 } `
                "Cycle ${cycle}: read-only checkbox did not turn off"
            [GuiTestNative]::NativeChar($body, [uint16][char]'X')
            Wait-Until {
                return (Normalize-AppText ([GuiTestNative]::GetText($body))) -eq ($editedText + 'X')
            } "Cycle ${cycle}: editable mode did not resume after read-only"

            Invoke-GuiButton $restore
            Wait-Until {
                return (Normalize-AppText ([GuiTestNative]::GetText($body))) -eq $exampleText
            } "Cycle ${cycle}: Restore did not reset the multiline model text"

            [GuiTestNative]::Resize($window, 900, 700)
            Wait-Until {
                return [GuiTestNative]::IsAlive($window) -and
                       ([GuiTestNative]::ChildPosition($window, $body))[3] -ge 140
            } "Cycle ${cycle}: multiline editor did not retain meaningful vertical layout space"

            [GuiTestNative]::Close($window)
            Wait-Until {
                $process.Refresh()
                return $process.HasExited
            } "Cycle ${cycle}: MultilineTextApp did not exit after close"
            if ($process.ExitCode -ne 0) {
                throw "Cycle ${cycle}: MultilineTextApp exited with code $($process.ExitCode)"
            }
            Write-Output "Cycle ${cycle}: multiline typing, selection/copy, paste, read-only, resize, and cleanup passed."
        } finally {
            if ($process -and -not $process.HasExited) {
                if ($window -ne [IntPtr]::Zero -and [GuiTestNative]::IsAlive($window)) {
                    [GuiTestNative]::Close($window)
                }
                if (-not $process.WaitForExit(3000)) {
                    $process.Kill()
                    $process.WaitForExit()
                }
            }
            if ($process) { $process.Dispose() }
        }
    }
} finally {
    if ($clipboardCaptured) {
        try { Set-Clipboard -Value ([string]$clipboardBefore) } catch { }
    }
}

Write-Output "PASS: MultilineTextApp GUI smoke completed $Cycles full cycles."

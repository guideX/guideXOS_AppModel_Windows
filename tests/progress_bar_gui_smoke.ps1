param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS Progress Demo'

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $windows = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
            if ($windows.Count -eq 1) { $window = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: ProgressBarApp window did not appear exactly once"

        $window = ([GuiTestNative]::FindTopLevelWindows($title, $process.Id))[0]
        Wait-Until {
            ([GuiTestNative]::FindChildren($window, 'msctls_progress32')).Count -eq 1
        } "Cycle ${cycle}: native progress control was not created"
        $progress = ([GuiTestNative]::FindChildren($window, 'msctls_progress32'))[0]
        if (-not [GuiTestNative]::IsVisible($progress)) {
            throw "Cycle ${cycle}: native progress control is not visible"
        }
        $start = Require-Control $window 'Button' 'Start' $cycle
        $reset = Require-Control $window 'Button' 'Reset' $cycle
        $unknown = Require-Control $window 'Button' 'Unknown duration' $cycle
        $percentage = Find-ControlPrefix $window 'Static' '0%'
        if ($percentage -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: ProgressBarApp percentage label was not found"
        }

        [GuiTestNative]::NativeButton($start)
        Wait-Until {
            $text = [GuiTestNative]::GetText($percentage)
            $text -match '^[1-9][0-9]?%$'
        } "Cycle ${cycle}: determinate progress did not advance"
        $first = [GuiTestNative]::GetText($percentage)
        Wait-Until {
            [GuiTestNative]::GetText($percentage) -eq '100%'
        } "Cycle ${cycle}: determinate progress did not complete" 15000

        [GuiTestNative]::NativeButton($reset)
        Wait-Until {
            [GuiTestNative]::GetText($percentage) -eq '0%'
        } "Cycle ${cycle}: Reset did not restore the minimum value"

        [GuiTestNative]::NativeButton($unknown)
        Wait-Until {
            [GuiTestNative]::GetText($percentage) -eq '0%'
        } "Cycle ${cycle}: indeterminate toggle changed stored progress"
        [GuiTestNative]::NativeButton($unknown)
        [GuiTestNative]::NativeButton($start)
        Wait-Until {
            [GuiTestNative]::GetText($percentage) -ne '0%'
        } "Cycle ${cycle}: determinate progress did not restart after indeterminate mode"

        [GuiTestNative]::Close($window)
        Wait-Until { $process.HasExited } "Cycle ${cycle}: ProgressBarApp did not exit cleanly"
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: ProgressBarApp exited with code $($process.ExitCode)"
        }
        Write-Output "Cycle ${cycle}: ProgressBarApp determinate/indeterminate cycle passed (first value $first)."
    } finally {
        if (-not $process.HasExited) {
            try { [GuiTestNative]::Close($window) } catch {}
            try { $process.WaitForExit(5000) } catch {}
            if (-not $process.HasExited) { $process.Kill() }
        }
        $process.Dispose()
    }
}

Write-Output "PASS: ProgressBarApp GUI smoke completed $Cycles full progress cycles."

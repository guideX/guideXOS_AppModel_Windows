param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS Timer'

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $windows = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
            if ($windows.Count -eq 1) { $window = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: TimerApp window did not appear exactly once"

        $window = ([GuiTestNative]::FindTopLevelWindows($title, $process.Id))[0]
        $status = Find-ControlPrefix $window 'Static' 'Ticks: '
        if ($status -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: timer status label was not found"
        }
        $stop = Require-Control $window 'Button' 'Stop Timer' $cycle
        $quit = Require-Control $window 'Button' 'Quit' $cycle

        Wait-Until {
            ([GuiTestNative]::GetText($status) -match '^Ticks: [1-9][0-9]*$')
        } "Cycle ${cycle}: timer did not deliver a native tick"
        $beforeStop = [GuiTestNative]::GetText($status)

        [GuiTestNative]::NativeButton($stop)
        $start = Require-Control $window 'Button' 'Start Timer' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($status).StartsWith('Stopped at ', [StringComparison]::Ordinal)
        } "Cycle ${cycle}: stopping the timer did not update the AppModel"

        [GuiTestNative]::NativeButton($start)
        [void](Require-Control $window 'Button' 'Stop Timer' $cycle)
        Wait-Until {
            $text = [GuiTestNative]::GetText($status)
            $text -match '^Ticks: [0-9]+$' -and $text -ne $beforeStop
        } "Cycle ${cycle}: restarting the timer did not deliver another tick"

        [GuiTestNative]::PostButton($quit)
        Wait-Until { $process.HasExited } "Cycle ${cycle}: TimerApp did not exit after Quit"
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: TimerApp exited with code $($process.ExitCode)"
        }
        Write-Output "Cycle ${cycle}: TimerApp timer lifecycle passed."
    } finally {
        if (-not $process.HasExited) {
            try { [GuiTestNative]::Close($window) } catch {}
            try { $process.WaitForExit(5000) } catch {}
            if (-not $process.HasExited) { $process.Kill() }
        }
        $process.Dispose()
    }
}

Write-Output "PASS: TimerApp GUI smoke completed $Cycles full timer cycles."

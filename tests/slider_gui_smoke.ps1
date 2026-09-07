param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

function Require-ControlPrefix([IntPtr] $Window, [string] $ClassName,
                                [string] $Prefix, [int] $Cycle) {
    Wait-Until {
        (Find-ControlPrefix $Window $ClassName $Prefix) -ne [IntPtr]::Zero
    } "Cycle ${Cycle}: missing $ClassName control with prefix '$Prefix'"
    return Find-ControlPrefix $Window $ClassName $Prefix
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS Slider Demo'

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $windows = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
            if ($windows.Count -eq 1) { $window = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: SliderApp window did not appear exactly once"

        $window = ([GuiTestNative]::FindTopLevelWindows($title, $process.Id))[0]
        Wait-Until {
            ([GuiTestNative]::FindChildren($window, 'msctls_trackbar32')).Count -eq 1
        } "Cycle ${cycle}: native Slider control was not created"
        $slider = ([GuiTestNative]::FindChildren($window, 'msctls_trackbar32'))[0]
        if (-not [GuiTestNative]::IsVisible($slider)) {
            throw "Cycle ${cycle}: native Slider control is not visible"
        }
        $value = Require-ControlPrefix $window 'Static' 'Value: ' $cycle
        $progress = Require-ControlPrefix $window 'Static' 'Progress: ' $cycle
        $reset = Require-Control $window 'Button' 'Reset' $cycle

        if ([GuiTestNative]::GetText($value) -ne 'Value: 42') {
            throw "Cycle ${cycle}: initial Slider value was not 42"
        }
        if ([GuiTestNative]::GetText($progress) -ne 'Progress: 42') {
            throw "Cycle ${cycle}: initial ProgressBar composition value was not 42"
        }

        if (-not [GuiTestNative]::FocusNative($window, $slider)) {
            throw "Cycle ${cycle}: Slider did not acquire focus"
        }
        for ($index = 0; $index -lt 5; $index++) {
            [GuiTestNative]::NativeKey($slider, 0x27) # VK_RIGHT
        }
        Wait-Until {
            [GuiTestNative]::GetText($value) -eq 'Value: 47' -and
            [GuiTestNative]::GetText($progress) -eq 'Progress: 47'
        } "Cycle ${cycle}: Right Arrow did not increase Slider value"

        [GuiTestNative]::NativeKey($slider, 0x25) # VK_LEFT
        Wait-Until {
            [GuiTestNative]::GetText($value) -eq 'Value: 46' -and
            [GuiTestNative]::GetText($progress) -eq 'Progress: 46'
        } "Cycle ${cycle}: Left Arrow did not decrease Slider value"

        [GuiTestNative]::NativeKey($slider, 0x24) # VK_HOME
        Wait-Until {
            [GuiTestNative]::GetText($value) -eq 'Value: 0' -and
            [GuiTestNative]::GetText($progress) -eq 'Progress: 0'
        } "Cycle ${cycle}: Home did not reach Slider minimum"

        [GuiTestNative]::NativeKey($slider, 0x23) # VK_END
        Wait-Until {
            [GuiTestNative]::GetText($value) -eq 'Value: 100' -and
            [GuiTestNative]::GetText($progress) -eq 'Progress: 100'
        } "Cycle ${cycle}: End did not reach Slider maximum"

        [GuiTestNative]::NativeButton($reset)
        Wait-Until {
            [GuiTestNative]::GetText($value) -eq 'Value: 42' -and
            [GuiTestNative]::GetText($progress) -eq 'Progress: 42'
        } "Cycle ${cycle}: Reset did not restore Slider and ProgressBar composition"

        [GuiTestNative]::Close($window)
        Wait-Until { $process.HasExited } "Cycle ${cycle}: SliderApp did not exit cleanly"
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: SliderApp exited with code $($process.ExitCode)"
        }
        Write-Output "Cycle ${cycle}: Slider keyboard/composition cycle passed."
    } finally {
        if (-not $process.HasExited) {
            try { [GuiTestNative]::Close($window) } catch {}
            try { $process.WaitForExit(5000) } catch {}
            if (-not $process.HasExited) { $process.Kill() }
        }
        $process.Dispose()
    }
}

Write-Output "PASS: SliderApp GUI smoke completed $Cycles full Slider cycles."

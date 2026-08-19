param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')


$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS Choice Controls'
$spanish = [GuiTestNative]::Spanish()
$japanese = [GuiTestNative]::Japanese()

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    try {
        $window = [IntPtr]::Zero
        Wait-Until {
            $windows = @([GuiTestNative]::FindTopLevelWindows($title, $process.Id))
            return $windows.Count -eq 1
        } "Cycle ${cycle}: ChoiceControlsApp window did not appear exactly once"
        $windows = @([GuiTestNative]::FindTopLevelWindows($title, $process.Id))
        $firstWindow = $windows | Select-Object -First 1
        $window = [IntPtr]::new($firstWindow.ToInt64())

        $notifications = Require-Control $window 'Button' 'Enable notifications' $cycle
        $startup = Require-Control $window 'Button' 'Start automatically' $cycle
        $system = Require-Control $window 'Button' 'System' $cycle
        $light = Require-Control $window 'Button' 'Light' $cycle
        $dark = Require-Control $window 'Button' 'Dark' $cycle
        $english = Require-Control $window 'Button' 'English' $cycle
        $spanishControl = Require-Control $window 'Button' $spanish $cycle
        $japaneseControl = Require-Control $window 'Button' $japanese $cycle
        $toggle = Require-Control $window 'Button' 'Toggle Notifications' $cycle
        $selectDark = Require-Control $window 'Button' 'Select Dark' $cycle
        $disableTheme = Require-Control $window 'Button' 'Disable Theme Options' $cycle
        $status = Require-Control $window 'Static' 'Status: Notifications enabled; System theme; English' $cycle

        if ([GuiTestNative]::CheckState($notifications) -ne 1 -or
            [GuiTestNative]::CheckState($startup) -ne 0 -or
            [GuiTestNative]::CheckState($system) -ne 1 -or
            [GuiTestNative]::CheckState($light) -ne 0 -or
            [GuiTestNative]::CheckState($dark) -ne 0 -or
            [GuiTestNative]::CheckState($english) -ne 1 -or
            [GuiTestNative]::CheckState($spanishControl) -ne 0 -or
            [GuiTestNative]::CheckState($japaneseControl) -ne 0) {
            throw "Cycle ${cycle}: initial choice states are incorrect"
        }

        $script:GuiWindow = $window
        Start-GuiInputPolicy $window $notifications $cycle
        if (-not (Focus-GuiControl $window $notifications)) {
            throw "Cycle ${cycle}: notifications checkbox could not receive focus"
        }
        $nextTab = [GuiTestNative]::NextTab($window, $notifications, $false)
        if ($nextTab -ne $startup) { throw "Cycle ${cycle}: Tab did not reach the next checkbox" }
        $nextTab = [GuiTestNative]::NextTab($window, $startup, $false)
        if ($nextTab -ne $system) { throw "Cycle ${cycle}: Tab did not reach the first radio button" }
        Write-Output "Cycle ${cycle}: focus and Tab order validated with deterministic native focus fallback."

        [GuiTestNative]::NativeChoice($window, $notifications, 0)
        Wait-Until {
            [GuiTestNative]::GetText($status) -eq 'Status: Notifications disabled; System theme; English'
        } "Cycle ${cycle}: native checkbox toggle did not update status"
        if ([GuiTestNative]::CheckState($notifications) -ne 0) {
            throw "Cycle ${cycle}: native checkbox state did not update"
        }

        Invoke-GuiButton $toggle
        Wait-Until {
            [GuiTestNative]::GetText($status) -eq 'Status: Notifications enabled; System theme; English'
        } "Cycle ${cycle}: programmatic checkbox toggle did not update status"
        if ([GuiTestNative]::CheckState($notifications) -ne 1) {
            throw "Cycle ${cycle}: programmatic checkbox state did not update native control"
        }

        # Space is sent directly to the focused native control. Manual
        # physical-key validation remains a release checkpoint.
        Send-GuiKey $window $notifications 0x20
        Wait-Until {
            [GuiTestNative]::GetText($status) -eq 'Status: Notifications disabled; System theme; English'
        } "Cycle ${cycle}: Space did not toggle a focused CheckBox"

        [GuiTestNative]::NativeChoice($window, $light, 1)
        Wait-Until {
            [GuiTestNative]::GetText($status) -eq 'Status: Notifications disabled; Light theme; English'
        } "Cycle ${cycle}: native radio selection did not update status"
        if ([GuiTestNative]::CheckState($light) -ne 1 -or
            [GuiTestNative]::CheckState($system) -ne 0) {
            throw "Cycle ${cycle}: native radio exclusivity failed"
        }

        Invoke-GuiButton $selectDark
        Wait-Until {
            [GuiTestNative]::GetText($status) -eq 'Status: Notifications disabled; Dark theme; English'
        } "Cycle ${cycle}: programmatic radio selection did not update status"
        if ([GuiTestNative]::CheckState($dark) -ne 1 -or
            [GuiTestNative]::CheckState($light) -ne 0) {
            throw "Cycle ${cycle}: programmatic radio exclusivity failed"
        }

        [GuiTestNative]::NativeChoice($window, $japaneseControl, 1)
        Wait-Until {
            [GuiTestNative]::GetText($status) -like "*${japanese}"
        } "Cycle ${cycle}: independent language RadioGroup did not update status"
        if ([GuiTestNative]::CheckState($dark) -ne 1 -or
            [GuiTestNative]::CheckState($english) -ne 0) {
            throw "Cycle ${cycle}: independent RadioGroup changed theme selection"
        }

        Invoke-GuiButton $disableTheme
        Wait-Until {
            -not [GuiTestNative]::IsEnabled($system) -and
            -not [GuiTestNative]::IsEnabled($light) -and
            -not [GuiTestNative]::IsEnabled($dark) -and
            [GuiTestNative]::CheckState($dark) -eq 1
        } "Cycle ${cycle}: disabled theme options or programmatic selection failed"
        [GuiTestNative]::NativeChoice($window, $light, 1)
        if ([GuiTestNative]::CheckState($dark) -ne 1) {
            throw "Cycle ${cycle}: disabled radio accepted native interaction"
        }

        Invoke-GuiButton $disableTheme
        Wait-Until {
            [GuiTestNative]::IsEnabled($dark) -and
            [GuiTestNative]::CheckState($dark) -eq 1
        } "Cycle ${cycle}: theme options did not re-enable deterministically"

        1..2 | ForEach-Object {
            [GuiTestNative]::NativeChoice($window, $light, 1)
            Wait-Until { [GuiTestNative]::CheckState($light) -eq 1 } "Cycle ${cycle}: repeated Light selection failed"
            Invoke-GuiButton $selectDark
            Wait-Until { [GuiTestNative]::CheckState($dark) -eq 1 -and [GuiTestNative]::CheckState($light) -eq 0 } "Cycle ${cycle}: repeated Dark selection failed"
        }

        [GuiTestNative]::Close($window)
        Wait-Until { $process.HasExited } "Cycle ${cycle}: close after callback dispatch did not exit normally" 5000
        if ($process.ExitCode -ne 0) { throw "Cycle ${cycle}: ChoiceControlsApp exited with $($process.ExitCode)" }
    } finally {
        if ($process -and -not $process.HasExited) {
            if ($window -ne [IntPtr]::Zero) { [GuiTestNative]::Close($window) }
            if (-not $process.WaitForExit(3000)) {
                Stop-Process -Id $process.Id -Force
            }
        }
    }
}
$remaining = Get-Process -Name 'ChoiceControlsApp' -ErrorAction SilentlyContinue
if ($remaining) {
    $ids = ($remaining | Select-Object -ExpandProperty Id) -join ', '
    throw "ChoiceControlsApp processes remain after smoke test: $ids"
}

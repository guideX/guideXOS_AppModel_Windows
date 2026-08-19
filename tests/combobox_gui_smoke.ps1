param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')


$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS ComboBox Demo'
$japanese = [string]::Concat([char]0x65E5, [char]0x672C, [char]0x8A9E)

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $windows = @([GuiTestNative]::FindTopLevelWindows($title, $process.Id))
            return $windows.Count -eq 1
        } "Cycle ${cycle}: ComboBoxApp window did not appear exactly once"
        $window = [IntPtr]::new((@([GuiTestNative]::FindTopLevelWindows($title, $process.Id)) | Select-Object -First 1).ToInt64())
        Wait-Until {
            return @([GuiTestNative]::FindChildren($window, 'ComboBox')).Count -eq 1
        } "Cycle ${cycle}: missing ComboBox control"
        $combo = [GuiTestNative]::FindChildren($window, 'ComboBox') | Select-Object -First 1
        $status = Require-Control $window 'Static' 'Selected [0]: Standard' $cycle
        $edit = [GuiTestNative]::FindChildren($window, 'Edit') | Select-Object -First 1
        $add = Require-Control $window 'Button' 'Add Item' $cycle
        $remove = Require-Control $window 'Button' 'Remove Selected' $cycle
        $clear = Require-Control $window 'Button' 'Clear' $cycle
        $selectCompatibility = Require-Control $window 'Button' 'Select Compatibility' $cycle
        $toggle = Require-Control $window 'Button' 'Disable ComboBox' $cycle
        if ($edit -eq [IntPtr]::Zero) { throw "Cycle ${cycle}: missing item-entry TextBox" }
        if ([GuiTestNative]::Count($combo) -ne 6 -or
            [GuiTestNative]::ComboSelectedIndex($combo) -ne 0 -or
            [GuiTestNative]::Item($combo, 0) -ne 'Standard') {
            throw "Cycle ${cycle}: initial ComboBox population or selection is incorrect"
        }

        $script:GuiWindow = $window
        Start-GuiInputPolicy $window $combo $cycle

        [GuiTestNative]::Open($combo, $true)
        Wait-Until { [GuiTestNative]::Dropped($combo) } "Cycle ${cycle}: ComboBox did not open"
        [GuiTestNative]::Open($combo, $false)
        Wait-Until { -not [GuiTestNative]::Dropped($combo) } "Cycle ${cycle}: ComboBox did not close"

        [GuiTestNative]::NativeComboSelect($window, $combo, 1)
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 1 -and
            [GuiTestNative]::GetText($status) -eq 'Selected [1]: Advanced' } `
            "Cycle ${cycle}: native selection did not update model/status"
        [GuiTestNative]::NativeComboKey($window, $combo, 0x28)
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 2 } "Cycle ${cycle}: Down did not change selection"
        [GuiTestNative]::NativeComboKey($window, $combo, 0x26)
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 1 } "Cycle ${cycle}: Up did not change selection"

        if (-not (Focus-GuiControl $window $combo)) { throw "Cycle ${cycle}: ComboBox focus failed" }
        if ([GuiTestNative]::NextTab($window, $combo, $false) -eq [IntPtr]::Zero) { throw "Cycle ${cycle}: Tab order has no next control" }

        Invoke-GuiButton $selectCompatibility
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 2 -and
            [GuiTestNative]::GetText($status) -eq 'Selected [2]: Compatibility' } `
            "Cycle ${cycle}: programmatic selection did not update native/model state"

        Send-GuiText $window $edit 'inserted item'
        Invoke-GuiButton $add
        Wait-Until { [GuiTestNative]::Count($combo) -eq 7 } "Cycle ${cycle}: insertion failed"
        if ([GuiTestNative]::Item($combo, 1) -ne 'inserted item' -or
            [GuiTestNative]::ComboSelectedIndex($combo) -ne 3) {
            throw "Cycle ${cycle}: insertion did not preserve the selected logical item"
        }

        [GuiTestNative]::NativeComboSelect($window, $combo, 4)
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 4 -and
            [GuiTestNative]::GetText($status) -eq "Selected [4]: ${japanese}" } `
            "Cycle ${cycle}: Unicode item did not select/display correctly"
        [GuiTestNative]::NativeComboSelect($window, $combo, 0)
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 0 } "Cycle ${cycle}: first duplicate selection failed"
        [GuiTestNative]::NativeComboSelect($window, $combo, 5)
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 5 -and
            [GuiTestNative]::GetText($status) -eq 'Selected [5]: Standard' } `
            "Cycle ${cycle}: duplicate strings were not distinguishable by index"
        Invoke-GuiButton $remove
        Wait-Until { [GuiTestNative]::Count($combo) -eq 6 -and
            [GuiTestNative]::ComboSelectedIndex($combo) -eq -1 -and
            [GuiTestNative]::GetText($status) -eq 'Selected: none' } `
            "Cycle ${cycle}: Remove Selected contract failed"

        Invoke-GuiButton $toggle
        Wait-Until { -not [GuiTestNative]::IsEnabled($combo) } "Cycle ${cycle}: ComboBox did not disable"
        [GuiTestNative]::NativeComboSelect($window, $combo, 0)
        if ([GuiTestNative]::GetText($status) -ne 'Selected: none') {
            throw "Cycle ${cycle}: disabled ComboBox accepted native input"
        }
        Invoke-GuiButton $selectCompatibility
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 3 -and
            [GuiTestNative]::GetText($status) -eq 'Selected [3]: Compatibility' } `
            "Cycle ${cycle}: programmatic disabled selection failed"

        [GuiTestNative]::Resize($window, 420, 360)
        Wait-Until { [GuiTestNative]::IsAlive($window) } "Cycle ${cycle}: small resize invalidated window"
        [GuiTestNative]::Resize($window, 920, 700)
        Wait-Until { [GuiTestNative]::IsAlive($window) } "Cycle ${cycle}: large resize invalidated window"

        Invoke-GuiButton $clear
        Wait-Until { [GuiTestNative]::Count($combo) -eq 0 -and
            [GuiTestNative]::ComboSelectedIndex($combo) -eq -1 -and
            [GuiTestNative]::GetText($status) -eq 'Selected: none' } `
            "Cycle ${cycle}: Clear did not empty items and selection"
        Invoke-GuiButton $toggle
        Wait-Until { [GuiTestNative]::IsEnabled($combo) } "Cycle ${cycle}: ComboBox did not re-enable"
        Send-GuiText $window $edit 'closing item'
        Invoke-GuiButton $add
        Wait-Until { [GuiTestNative]::Count($combo) -eq 1 } "Cycle ${cycle}: final item insertion failed"
        [GuiTestNative]::NativeComboSelect($window, $combo, 0)
        Wait-Until { [GuiTestNative]::ComboSelectedIndex($combo) -eq 0 } "Cycle ${cycle}: final selection failed"
        [GuiTestNative]::Close($window)
        Wait-Until { $process.HasExited } "Cycle ${cycle}: close after selection did not exit normally"
        if ($process.ExitCode -ne 0) { throw "Cycle ${cycle}: ComboBoxApp exited with code $($process.ExitCode)" }
        Write-Output "Cycle ${cycle}: ComboBox native-message/focus fallback path validated."
    }
    finally {
        if ($process -and -not $process.HasExited) {
            if ($window -ne [IntPtr]::Zero) { [GuiTestNative]::Close($window) }
            if (-not $process.WaitForExit(3000)) { Stop-Process -Id $process.Id -Force }
        }
        if ($process) { $process.Dispose() }
    }
}

$remaining = Get-Process -Name 'ComboBoxApp' -ErrorAction SilentlyContinue
if ($remaining) {
    $ids = ($remaining | Select-Object -ExpandProperty Id) -join ', '
    throw "ComboBoxApp processes remain after smoke test: $ids"
}

Write-Output "PASS: ComboBoxApp GUI smoke completed $Cycles full native selection/mutation cycles."

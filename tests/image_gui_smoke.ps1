param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS Image Demo'
$imageClass = 'guideXOS.AppModel.Windows.Image'

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $matches = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
            if ($matches.Count -gt 0) { $window = $matches[0]; return $true }
            return $false
        } "Cycle ${cycle}: ImageApp window did not appear"
        $window = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)[0]

        Wait-Until {
            return [GuiTestNative]::FindChildren($window, $imageClass).Count -eq 1
        } "Cycle ${cycle}: Image control was not realized"
        $image = [GuiTestNative]::FindChildren($window, $imageClass)[0]
        if (-not [GuiTestNative]::IsVisible($image)) {
            throw "Cycle ${cycle}: Image control is not visible"
        }

        Wait-Until {
            $status = Find-ControlPrefix $window 'Static' 'Status:'
            return $status -ne [IntPtr]::Zero -and
                ([GuiTestNative]::GetText($status) -like 'Status: PNG * (Fit)')
        } "Cycle ${cycle}: initial PNG/Fit status was not reported"

        $stretch = Require-Control $window 'Button' 'Stretch' $cycle
        Invoke-GuiButton $stretch
        Wait-Until {
            $status = Find-ControlPrefix $window 'Static' 'Status:'
            return $status -ne [IntPtr]::Zero -and
                ([GuiTestNative]::GetText($status) -like '* (Stretch)')
        } "Cycle ${cycle}: Fit to Stretch did not synchronize"

        $fit = Require-Control $window 'Button' 'Fit' $cycle
        Invoke-GuiButton $fit
        Wait-Until {
            $status = Find-ControlPrefix $window 'Static' 'Status:'
            return $status -ne [IntPtr]::Zero -and
                ([GuiTestNative]::GetText($status) -like 'Status: PNG * (Fit)')
        } "Cycle ${cycle}: Stretch to Fit did not synchronize"

        $loadJpeg = Require-Control $window 'Button' 'Load JPEG' $cycle
        Invoke-GuiButton $loadJpeg
        Wait-Until {
            $status = Find-ControlPrefix $window 'Static' 'Status:'
            return $status -ne [IntPtr]::Zero -and
                ([GuiTestNative]::GetText($status) -like 'Status: JPEG * (Fit)')
        } "Cycle ${cycle}: bundled JPEG replacement did not load"

        $clear = Require-Control $window 'Button' 'Clear' $cycle
        Invoke-GuiButton $clear
        Wait-Until {
            $status = Find-ControlPrefix $window 'Static' 'Status:'
            return $status -ne [IntPtr]::Zero -and
                [GuiTestNative]::GetText($status) -eq 'Status: no image source'
        } "Cycle ${cycle}: clearing the source did not update status"

        $loadPng = Require-Control $window 'Button' 'Load PNG' $cycle
        Invoke-GuiButton $loadPng
        Wait-Until {
            $status = Find-ControlPrefix $window 'Static' 'Status:'
            return $status -ne [IntPtr]::Zero -and
                ([GuiTestNative]::GetText($status) -like 'Status: PNG * (Fit)')
        } "Cycle ${cycle}: PNG restoration did not load"

        $before = [GuiTestNative]::ClientSize($window)
        [GuiTestNative]::Resize($window, 1080, 720)
        Wait-Until {
            $size = [GuiTestNative]::ClientSize($window)
            return $size[0] -gt $before[0] -and $size[1] -gt $before[1]
        } "Cycle ${cycle}: ImageApp resize did not take effect"
        if (-not [GuiTestNative]::IsAlive($image)) {
            throw "Cycle ${cycle}: Image control was destroyed during resize"
        }

        [GuiTestNative]::Close($window)
        Wait-Until { return $process.HasExited } "Cycle ${cycle}: ImageApp did not exit cleanly"
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: ImageApp exited with code $($process.ExitCode)"
        }
        Write-Output "Cycle ${cycle}: PNG/JPEG, scale changes, clear/restore, resize, and close passed."
    }
    finally {
        if (-not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}

Write-Output "PASS: ImageApp GUI smoke completed $Cycles cycles."

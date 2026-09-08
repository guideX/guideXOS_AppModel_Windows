param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'guideXOS ScrollView Settings'
$scrollClass = 'guideXOS.AppModel.Windows.ScrollView'

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $matches = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
            return $matches.Count -eq 1
        } "Cycle ${cycle}: ScrollViewApp window did not appear"
        $window = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)[0]

        Wait-Until {
            return [GuiTestNative]::FindChildren($window, $scrollClass).Count -eq 1
        } "Cycle ${cycle}: ScrollView host was not realized"
        $scrollHost = [GuiTestNative]::FindChildren($window, $scrollClass)[0]
        $tab = [GuiTestNative]::FindChildren($window, 'SysTabControl32')[0]
        if ($tab -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: TabView was not realized"
        }

        $profile = Require-Control $window 'Edit' 'guideXOS' $cycle
        $advance = Find-Control $window 'Button' 'Advance progress'
        if ($advance -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: lower progress button was not realized"
        }
        if ([GuiTestNative]::IsVisible($advance)) {
            throw "Cycle ${cycle}: lower control was visible before scrolling"
        }

        $initialMaximum = [GuiTestNative]::VerticalScrollMaximum($scrollHost)
        if ($initialMaximum -le 0) {
            throw "Cycle ${cycle}: content did not exceed the viewport"
        }
        [GuiTestNative]::MouseWheel($scrollHost, -120)
        Wait-Until {
            return [GuiTestNative]::VerticalScrollPosition($scrollHost) -gt 0
        } "Cycle ${cycle}: wheel scroll did not increase the offset"
        [GuiTestNative]::MouseWheel($scrollHost, 120)
        Wait-Until {
            return [GuiTestNative]::VerticalScrollPosition($scrollHost) -eq 0
        } "Cycle ${cycle}: wheel scroll did not return to the top"

        for ($step = 0; $step -lt 30; $step++) {
            [GuiTestNative]::MouseWheel($scrollHost, -120)
        }
        Wait-Until {
            return [GuiTestNative]::VerticalScrollPosition($scrollHost) -eq
                [GuiTestNative]::VerticalScrollMaximum($scrollHost)
        } "Cycle ${cycle}: repeated wheel input did not reach the bottom bound"
        if (-not [GuiTestNative]::IsVisible($advance)) {
            throw "Cycle ${cycle}: lower control did not become visible after scrolling"
        }
        Invoke-GuiButton $advance

        [GuiTestNative]::Resize($window, 1080, 760)
        Wait-Until {
            return [GuiTestNative]::VerticalScrollMaximum($scrollHost) -lt $initialMaximum
        } "Cycle ${cycle}: larger resize did not reduce the scroll range"
        [GuiTestNative]::Resize($window, 620, 360)
        Wait-Until {
            return [GuiTestNative]::VerticalScrollMaximum($scrollHost) -ge $initialMaximum
        } "Cycle ${cycle}: smaller resize did not restore the scroll range"

        [GuiTestNative]::Close($window)
        Wait-Until { return $process.HasExited } "Cycle ${cycle}: ScrollViewApp did not exit cleanly"
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: ScrollViewApp exited with code $($process.ExitCode)"
        }
        Write-Output "Cycle ${cycle}: viewport, wheel, bottom interaction, resize, TabView realization, and close passed."
    }
    finally {
        if ($process -and -not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}

Write-Output "PASS: ScrollViewApp GUI smoke completed $Cycles cycles."

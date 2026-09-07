param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class TabViewSmokeNative {
    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message,
                                             IntPtr wParam, IntPtr lParam);
    public static int TabSelection(IntPtr tab) {
        return SendMessage(tab, 0x130B, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }
    public static int SliderPosition(IntPtr slider) {
        return SendMessage(slider, 0x0400, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }
    public static int ProgressPosition(IntPtr progress) {
        return SendMessage(progress, 0x0400 + 8, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }
}
'@

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'Settings Demo'

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $windows = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
            if ($windows.Count -eq 1) { $window = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: TabViewApp window did not appear exactly once"
        $window = ([GuiTestNative]::FindTopLevelWindows($title, $process.Id))[0]

        Wait-Until {
            ([GuiTestNative]::FindChildren($window, 'SysTabControl32')).Count -eq 1
        } "Cycle ${cycle}: native TabView was not created"
        $tab = ([GuiTestNative]::FindChildren($window, 'SysTabControl32'))[0]
        if (-not [GuiTestNative]::IsVisible($tab)) {
            throw "Cycle ${cycle}: native TabView is not visible"
        }
        Wait-Until {
            ([GuiTestNative]::FindChildren($window, 'Edit')).Count -eq 4 -and
                ([GuiTestNative]::FindChildren($window, 'msctls_trackbar32')).Count -eq 1 -and
                ([GuiTestNative]::FindChildren($window, 'msctls_progress32')).Count -eq 2
        } "Cycle ${cycle}: TabView page controls were not created"
        $edits = @([GuiTestNative]::FindChildren($window, 'Edit'))
        $sliders = @([GuiTestNative]::FindChildren($window, 'msctls_trackbar32'))
        $progressBars = @([GuiTestNative]::FindChildren($window, 'msctls_progress32'))
        if ($edits.Count -ne 4 -or $sliders.Count -ne 1 -or
            $progressBars.Count -ne 2) {
            throw "Cycle ${cycle}: expected 4 edits/1 slider/2 progress bars, got $($edits.Count)/$($sliders.Count)/$($progressBars.Count)"
        }
        $name = Require-Control $window 'Edit' 'guideXOS user' $cycle
        Wait-Until { (Find-Control $window 'Edit' 'example.com') -ne [IntPtr]::Zero } `
            "Cycle ${cycle}: Network server control was not created"
        $server = Find-Control $window 'Edit' 'example.com'
        Wait-Until { (Find-Control $window 'Edit' '443') -ne [IntPtr]::Zero } `
            "Cycle ${cycle}: Network port control was not created"
        $port = Find-Control $window 'Edit' '443'
        Wait-Until { (Find-Control $window 'Edit' 'Ready to run diagnostics.') -ne [IntPtr]::Zero } `
            "Cycle ${cycle}: Diagnostics log control was not created"
        $log = Find-Control $window 'Edit' 'Ready to run diagnostics.'
        $slider = $sliders[0]
        $generalProgress = $progressBars[0]
        $diagnosticsProgress = $progressBars[1]
        Wait-Until { (Find-Control $window 'Button' 'Connect') -ne [IntPtr]::Zero } `
            "Cycle ${cycle}: Connect button was not created"
        $connect = Find-Control $window 'Button' 'Connect'
        Wait-Until { (Find-Control $window 'Button' 'Start Test') -ne [IntPtr]::Zero } `
            "Cycle ${cycle}: Start Test button was not created"
        $start = Find-Control $window 'Button' 'Start Test'
        $status = Require-Control $window 'Static' 'Status: General selected' $cycle

        if ([TabViewSmokeNative]::TabSelection($tab) -ne 0) {
            throw "Cycle ${cycle}: initial TabView selection was not General"
        }
        if (-not [GuiTestNative]::IsVisible($name) -or
            [GuiTestNative]::IsVisible($server)) {
            throw "Cycle ${cycle}: initial page visibility was incorrect"
        }

        Send-GuiText $window $name 'Alice'
        if ([GuiTestNative]::GetText($name) -ne 'Alice') {
            throw "Cycle ${cycle}: General text did not update"
        }
        if (-not [GuiTestNative]::FocusNative($window, $slider)) {
            throw "Cycle ${cycle}: Slider did not acquire focus"
        }
        for ($index = 0; $index -lt 23; $index++) {
            [GuiTestNative]::NativeKey($slider, 0x27) # VK_RIGHT
        }
        Wait-Until {
            [TabViewSmokeNative]::SliderPosition($slider) -eq 73 -and
                [TabViewSmokeNative]::ProgressPosition($generalProgress) -eq 73
        } "Cycle ${cycle}: Slider/ProgressBar composition did not reach 73"

        if (-not [GuiTestNative]::FocusNative($window, $tab)) {
            throw "Cycle ${cycle}: TabView did not acquire focus"
        }
        [GuiTestNative]::NativeKey($tab, 0x27) # select Network
        Wait-Until {
            [TabViewSmokeNative]::TabSelection($tab) -eq 1 -and
                [GuiTestNative]::IsVisible($server) -and
                -not [GuiTestNative]::IsVisible($name)
        } "Cycle ${cycle}: keyboard/user selection did not activate Network"
        Send-GuiText $window $server 'server.test'
        [GuiTestNative]::NativeButton($connect)

        [GuiTestNative]::NativeKey($tab, 0x27) # select Diagnostics
        Wait-Until { [TabViewSmokeNative]::TabSelection($tab) -eq 2 } `
            "Cycle ${cycle}: Diagnostics selection did not arrive"
        [GuiTestNative]::NativeButton($start)
        Wait-Until {
            [TabViewSmokeNative]::ProgressPosition($diagnosticsProgress) -eq 100 -and
                [GuiTestNative]::GetText($log) -like '*completed*'
        } "Cycle ${cycle}: Diagnostics controls did not remain functional"

        [GuiTestNative]::NativeKey($tab, 0x25) # back to Network
        [GuiTestNative]::NativeKey($tab, 0x25) # back to General
        Wait-Until {
            [TabViewSmokeNative]::TabSelection($tab) -eq 0 -and
                [GuiTestNative]::GetText($name) -eq 'Alice' -and
                [TabViewSmokeNative]::SliderPosition($slider) -eq 73
        } "Cycle ${cycle}: General state was not preserved"
        [GuiTestNative]::NativeKey($tab, 0x27)
        Wait-Until {
            [TabViewSmokeNative]::TabSelection($tab) -eq 1 -and
                [GuiTestNative]::GetText($server) -eq 'server.test'
        } "Cycle ${cycle}: Network state was not preserved"

        [GuiTestNative]::Resize($window, 1100, 760)
        Wait-Until {
            [GuiTestNative]::Width($window) -ge 1080 -and
                [GuiTestNative]::Height($window) -ge 740 -and
                [GuiTestNative]::IsVisible($server)
        } "Cycle ${cycle}: resize did not preserve the active page"

        [GuiTestNative]::Close($window)
        Wait-Until { $process.HasExited } "Cycle ${cycle}: TabViewApp did not exit cleanly"
        if ($process.ExitCode -ne 0) {
            throw "Cycle ${cycle}: TabViewApp exited with code $($process.ExitCode)"
        }
        Write-Output "Cycle ${cycle}: TabView selection/state/resize cycle passed."
    } finally {
        if (-not $process.HasExited) {
            try { [GuiTestNative]::Close($window) } catch {}
            try { $process.WaitForExit(5000) } catch {}
            if (-not $process.HasExited) { $process.Kill() }
        }
        $process.Dispose()
    }
}

Write-Output "PASS: TabViewApp GUI smoke completed $Cycles full application cycles."

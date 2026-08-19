param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

function Has-Tooltip([string] $Path, [string] $Text) {
    return @((Get-TestStatusText $Path) -split "`r?`n" |
        Where-Object { $_ -eq $Text }).Count -eq 1
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'Application Polish Demo'
$previousStatusFile = $env:GUIDEXOS_TEST_STATUS_FILE
$previousTooltipFile = $env:GUIDEXOS_TEST_TOOLTIP_FILE

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $statusFile = Join-Path ([IO.Path]::GetTempPath()) (
        'guideXOS_PolishStatus_' + [Guid]::NewGuid().ToString('N') + '.txt')
    $tooltipFile = Join-Path ([IO.Path]::GetTempPath()) (
        'guideXOS_PolishToolTips_' + [Guid]::NewGuid().ToString('N') + '.txt')
    $env:GUIDEXOS_TEST_STATUS_FILE = $statusFile
    $env:GUIDEXOS_TEST_TOOLTIP_FILE = $tooltipFile
    $process = Start-Process -FilePath $resolvedExecutable -PassThru
    try {
        $window = [IntPtr]::Zero
        Wait-Until {
            $windows = @([GuiTestNative]::FindTopLevelWindows($title, $process.Id))
            if ($windows.Count -eq 1) {
                $script:PolishWindow = $windows[0]
                return $true
            }
            return $false
        } "Cycle ${cycle}: PolishApp window did not appear exactly once"
        $window = $script:PolishWindow

        Wait-Until {
            $editChildren = [GuiTestNative]::FindChildren($window, 'Edit')
            $buttonChildren = [GuiTestNative]::FindChildren($window, 'Button')
            $listChildren = [GuiTestNative]::FindChildren($window, 'ListBox')
            $statusChildren = [GuiTestNative]::FindChildren($window, 'msctls_statusbar32')
            $editChildren.Count -eq 1 -and $buttonChildren.Count -eq 3 -and
                $listChildren.Count -eq 1 -and $statusChildren.Count -eq 1
        } "Cycle ${cycle}: PolishApp controls/status bar were not realized"
        $edits = @([GuiTestNative]::FindChildren($window, 'Edit'))
        $buttons = @([GuiTestNative]::FindChildren($window, 'Button'))
        $lists = @([GuiTestNative]::FindChildren($window, 'ListBox'))
        $statusBars = @([GuiTestNative]::FindChildren($window, 'msctls_statusbar32'))
        if ($edits.Count -ne 1 -or $buttons.Count -ne 3 -or
            $lists.Count -ne 1 -or $statusBars.Count -ne 1) {
            throw "Cycle ${cycle}: unexpected PolishApp control/status-bar counts: edits=$($edits.Count) buttons=$($buttons.Count) lists=$($lists.Count) status=$($statusBars.Count)"
        }
        $apply = [GuiTestNative]::FindChildByText($window, 'Button', 'Apply')
        $clear = [GuiTestNative]::FindChildByText($window, 'Button', 'Clear')
        $feature = [GuiTestNative]::FindChildByText($window, 'Button', 'Enable feature')
        if ($apply -eq [IntPtr]::Zero -or $clear -eq [IntPtr]::Zero -or
            $feature -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: PolishApp action controls were not found"
        }

        Wait-Until { (Get-TestStatusText $statusFile) -eq 'Ready' } `
            "Cycle ${cycle}: initial status-bar text was not Ready"
        foreach ($tip in @('Enter a display name', 'Apply the current values',
                           'Clear the form', 'Enable or disable the feature',
                           'Select an item')) {
            if (-not (Has-Tooltip $tooltipFile $tip)) {
                throw "Cycle ${cycle}: initial tooltip '$tip' was not registered"
            }
        }

        [GuiTestNative]::NativeButton($apply)
        Wait-Until {
            (Get-TestStatusText $statusFile) -eq 'Applied' -and
                (Has-Tooltip $tooltipFile 'Display name applied')
        } "Cycle ${cycle}: Apply did not update status and tooltip registration"

        [GuiTestNative]::ClickControl($feature)
        Wait-Until {
            (Get-TestStatusText $statusFile) -eq 'Feature enabled' -and
                (Has-Tooltip $tooltipFile 'Feature is enabled')
        } "Cycle ${cycle}: checkbox did not update status and tooltip"

        $list = $lists[0]
        [GuiTestNative]::NativeListSelect($window, $list, 1)
        Wait-Until {
            (Get-TestStatusText $statusFile) -eq 'Selected: Second item'
        } "Cycle ${cycle}: ListBox selection did not update status"

        [GuiTestNative]::Resize($window, 900, 650)
        [GuiTestNative]::Resize($window, 420, 300)
        Wait-Until {
            $client = [GuiTestNative]::ClientSize($window)
            $status = [GuiTestNative]::ChildPosition($window, $statusBars[0])
            $status[1] + $status[3] -eq $client[1] -and $status[3] -gt 0
        } "Cycle ${cycle}: status bar was not bottom-aligned after resize"
        $listPosition = [GuiTestNative]::ChildPosition($window, $list)
        $statusPosition = [GuiTestNative]::ChildPosition($window, $statusBars[0])
        if ($listPosition[1] + $listPosition[3] -gt $statusPosition[1]) {
            throw "Cycle ${cycle}: content overlapped the status bar"
        }

        [GuiTestNative]::NativeButton($clear)
        Wait-Until {
            (Get-TestStatusText $statusFile) -eq 'Cleared' -and
            -not (Has-Tooltip $tooltipFile 'Display name applied')
        } "Cycle ${cycle}: Clear did not clear tooltip registration"
        [GuiTestNative]::NativeButton($apply)
        Wait-Until {
            Has-Tooltip $tooltipFile 'Display name applied'
        } "Cycle ${cycle}: tooltip reassignment was not registered"

        [GuiTestNative]::Close($window)
        Wait-Until { $process.HasExited } "Cycle ${cycle}: PolishApp did not close"
        Write-Output "Cycle ${cycle}: status bar, dynamic tooltips, resize, and close/reopen coverage passed."
    } finally {
        if ($process -and -not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        if ($process) { $process.Dispose() }
        if (Test-Path -LiteralPath $statusFile) {
            Remove-Item -LiteralPath $statusFile -Force
        }
        if (Test-Path -LiteralPath $tooltipFile) {
            Remove-Item -LiteralPath $tooltipFile -Force
        }
        if ($null -eq $previousStatusFile) {
            Remove-Item Env:GUIDEXOS_TEST_STATUS_FILE -ErrorAction SilentlyContinue
        } else {
            $env:GUIDEXOS_TEST_STATUS_FILE = $previousStatusFile
        }
        if ($null -eq $previousTooltipFile) {
            Remove-Item Env:GUIDEXOS_TEST_TOOLTIP_FILE -ErrorAction SilentlyContinue
        } else {
            $env:GUIDEXOS_TEST_TOOLTIP_FILE = $previousTooltipFile
        }
    }
}

Write-Output "PASS: PolishApp GUI smoke completed $Cycles cycles."

param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

function Wait-ProfileModal([int] $ProcessId, [string] $Title, [int] $Cycle) {
    Wait-Until {
        $windows = @([GuiTestNative]::FindTopLevelWindowsByClass('#32770', $Title, $ProcessId) |
            Where-Object { [GuiTestNative]::IsVisible($_) })
        if ($windows.Count -eq 1) {
            $script:CloseModal = $windows[0]
            return $true
        }
        $fallback = @([GuiTestNative]::FindTopLevelWindowsByClassAnyTitle('#32770', $ProcessId) |
            Where-Object { [GuiTestNative]::IsVisible($_) })
        if ($fallback.Count -eq 1) {
            $script:CloseModal = $fallback[0]
            return $true
        }
        return $false
    } "Cycle ${Cycle}: modal '$Title' did not appear"
    return $script:CloseModal
}

function Click-ProfileModalButton([IntPtr] $Dialog, [string] $Text, [int] $Cycle) {
    $button = [GuiTestNative]::FindChildByText($Dialog, 'Button', $Text)
    if ($button -eq [IntPtr]::Zero) {
        throw "Cycle ${Cycle}: modal button '$Text' was not found"
    }
    [GuiTestNative]::NativeButton($button)
}

function Set-ProfileFileDialogPath([IntPtr] $Dialog, [string] $Path, [int] $Cycle) {
    Wait-Until {
        @([GuiTestNative]::VisibleChildren($Dialog, 'Edit')).Count -gt 0
    } "Cycle ${Cycle}: Save As filename control did not appear"
    $edit = @([GuiTestNative]::VisibleChildren($Dialog, 'Edit'))[0]
    [GuiTestNative]::NativeText($edit, $Path)
    if ([GuiTestNative]::GetText($edit) -ne $Path) {
        throw "Cycle ${Cycle}: Save As rejected '$Path'"
    }
}

function Start-Profile([string] $ResolvedExecutable, [string] $Directory, [int] $Cycle) {
    $process = Start-Process -FilePath $ResolvedExecutable -WorkingDirectory $Directory -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $windows = @([GuiTestNative]::FindTopLevelWindowsByClass(
                'guideXOS.AppModel.Windows.Foundation', 'Profile Manager - Untitled', $process.Id))
            if ($windows.Count -eq 1) {
                $script:CloseWindow = $windows[0]
                return $true
            }
            return $false
        } "Cycle ${Cycle}: ProfileManager close-test window did not appear"
        $window = $script:CloseWindow
        $edits = @([GuiTestNative]::FindChildren($window, 'Edit'))
        if ($edits.Count -lt 2) { throw "Cycle ${Cycle}: ProfileManager editor controls were not realized" }
        return @($process, $window, $edits[0], $edits[1])
    } catch {
        if ($process -and -not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        $process.Dispose()
        throw
    }
}

function Set-ProfileText([IntPtr] $Control, [string] $Text, [int] $Cycle) {
    [GuiTestNative]::NativeText($Control, $Text)
    if ([GuiTestNative]::GetText($Control) -ne $Text) {
        throw "Cycle ${Cycle}: editor text was not applied"
    }
}

function Finish-Profile([System.Diagnostics.Process] $Process, [IntPtr] $Window, [int] $Cycle) {
    if ($Process -and -not $Process.HasExited) {
        if ($Window -ne [IntPtr]::Zero -and [GuiTestNative]::IsAlive($Window)) {
            [GuiTestNative]::Close($Window)
        }
        if (-not $Process.WaitForExit(5000)) {
            $Process.Kill()
            $Process.WaitForExit()
        }
    }
    if ($Process -and -not $Process.HasExited) {
        throw "Cycle ${Cycle}: ProfileManager close-test process remained"
    }
    $Process.Dispose()
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$root = Join-Path ([IO.Path]::GetTempPath()) 'guideXOS_AppModel_Windows_ClosePolicyTests'
if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
New-Item -ItemType Directory -Path $root | Out-Null

try {
    for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
        $directory = Join-Path $root "cycle-$cycle"
        New-Item -ItemType Directory -Path $directory | Out-Null

        # Clean native close: no callback dialog is shown and the process exits.
        $parts = Start-Profile $resolvedExecutable $directory $cycle
        $process, $window, $name, $description = $parts
        try {
            [GuiTestNative]::Close($window)
            Wait-Until { $process.HasExited } "Cycle ${cycle}: clean native close did not exit"
        } finally { Finish-Profile $process $window $cycle }

        # Dirty Cancel, duplicate close request, then Dirty No. The editor and
        # window must remain usable after cancellation.
        $parts = Start-Profile $resolvedExecutable $directory $cycle
        $process, $window, $name, $description = $parts
        try {
            Set-ProfileText $name "Cancel then discard $cycle" $cycle
            [GuiTestNative]::Close($window)
            [GuiTestNative]::Close($window)
            $dialog = Wait-ProfileModal $process.Id 'Unsaved Changes' $cycle
            Click-ProfileModalButton $dialog 'Cancel' $cycle
            Wait-Until {
                -not $process.HasExited -and [GuiTestNative]::IsAlive($window) -and
                [GuiTestNative]::GetText($window) -like '* *'
            } "Cycle ${cycle}: Cancel did not preserve the dirty window"
            if ([GuiTestNative]::GetText($name) -ne "Cancel then discard $cycle") {
                throw "Cycle ${cycle}: Cancel changed the pending editor"
            }
            [GuiTestNative]::Close($window)
            $dialog = Wait-ProfileModal $process.Id 'Unsaved Changes' $cycle
            Click-ProfileModalButton $dialog 'No' $cycle
            Wait-Until { $process.HasExited } "Cycle ${cycle}: No did not close the dirty document"
        } finally { Finish-Profile $process $window $cycle }

        # Dirty Yes with an existing path updates the file before closure.
        $existingPath = Join-Path $directory "existing-$cycle.gxprofiles"
        $parts = Start-Profile $resolvedExecutable $directory $cycle
        $process, $window, $name, $description = $parts
        try {
            $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
            $saveAs = [GuiTestNative]::FindMenuCommandByText($file, 'Save As...')
            [GuiTestNative]::PostMenuCommand($window, $saveAs)
            $dialog = Wait-ProfileModal $process.Id 'Save Profile Set' $cycle
            Set-ProfileFileDialogPath $dialog $existingPath $cycle
            Click-ProfileModalButton $dialog 'Save' $cycle
            Wait-Until { Test-Path -LiteralPath $existingPath } "Cycle ${cycle}: initial Save As failed"
            Set-ProfileText $description "saved by native close $cycle" $cycle
            [GuiTestNative]::Close($window)
            $dialog = Wait-ProfileModal $process.Id 'Unsaved Changes' $cycle
            Click-ProfileModalButton $dialog 'Yes' $cycle
            Wait-Until { $process.HasExited } "Cycle ${cycle}: Yes did not close the existing document"
            if ([IO.File]::ReadAllText($existingPath) -notlike '*saved by native close*') {
                throw "Cycle ${cycle}: Yes did not save the existing document"
            }
        } finally { Finish-Profile $process $window $cycle }

        # Dirty Yes with no current path invokes Save As and then closes.
        $saveAsPath = Join-Path $directory "close-save-as-$cycle.gxprofiles"
        $parts = Start-Profile $resolvedExecutable $directory $cycle
        $process, $window, $name, $description = $parts
        try {
            Set-ProfileText $name "untitled close $cycle" $cycle
            [GuiTestNative]::Close($window)
            $dialog = Wait-ProfileModal $process.Id 'Unsaved Changes' $cycle
            Click-ProfileModalButton $dialog 'Yes' $cycle
            $dialog = Wait-ProfileModal $process.Id 'Save Profile Set' $cycle
            Set-ProfileFileDialogPath $dialog $saveAsPath $cycle
            Click-ProfileModalButton $dialog 'Save' $cycle
            Wait-Until { $process.HasExited -and (Test-Path -LiteralPath $saveAsPath) } `
                "Cycle ${cycle}: Yes Save As did not save and close"
        } finally { Finish-Profile $process $window $cycle }

        # Save As cancellation during close must keep the editor and window.
        $cancelPath = Join-Path $directory "cancelled-$cycle.gxprofiles"
        $parts = Start-Profile $resolvedExecutable $directory $cycle
        $process, $window, $name, $description = $parts
        try {
            Set-ProfileText $description "remain editable after cancel $cycle" $cycle
            [GuiTestNative]::Close($window)
            $dialog = Wait-ProfileModal $process.Id 'Unsaved Changes' $cycle
            Click-ProfileModalButton $dialog 'Yes' $cycle
            $dialog = Wait-ProfileModal $process.Id 'Save Profile Set' $cycle
            Set-ProfileFileDialogPath $dialog $cancelPath $cycle
            Click-ProfileModalButton $dialog 'Cancel' $cycle
            Wait-Until {
                -not $process.HasExited -and [GuiTestNative]::IsAlive($window) -and
                [GuiTestNative]::GetText($description) -eq "remain editable after cancel $cycle"
            } "Cycle ${cycle}: Save As cancellation did not preserve the editor"
            [GuiTestNative]::Close($window)
            $dialog = Wait-ProfileModal $process.Id 'Unsaved Changes' $cycle
            Click-ProfileModalButton $dialog 'No' $cycle
            Wait-Until { $process.HasExited } "Cycle ${cycle}: final close after Save As cancellation failed"
        } finally { Finish-Profile $process $window $cycle }

        # Existing-path save failure: replace the saved file with a directory,
        # then verify the close request remains canceled after the error dialog.
        $failurePath = Join-Path $directory "failure-$cycle.gxprofiles"
        $parts = Start-Profile $resolvedExecutable $directory $cycle
        $process, $window, $name, $description = $parts
        try {
            $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
            [GuiTestNative]::PostMenuCommand($window, [GuiTestNative]::FindMenuCommandByText($file, 'Save As...'))
            $dialog = Wait-ProfileModal $process.Id 'Save Profile Set' $cycle
            Set-ProfileFileDialogPath $dialog $failurePath $cycle
            Click-ProfileModalButton $dialog 'Save' $cycle
            Wait-Until { Test-Path -LiteralPath $failurePath } "Cycle ${cycle}: failure setup Save As failed"
            Remove-Item -LiteralPath $failurePath -Force
            New-Item -ItemType Directory -Path $failurePath | Out-Null
            Set-ProfileText $name "save failure $cycle" $cycle
            [GuiTestNative]::Close($window)
            $dialog = Wait-ProfileModal $process.Id 'Unsaved Changes' $cycle
            Click-ProfileModalButton $dialog 'Yes' $cycle
            $dialog = Wait-ProfileModal $process.Id 'Profile Manager Error' $cycle
            Click-ProfileModalButton $dialog 'OK' $cycle
            Wait-Until {
                -not $process.HasExited -and [GuiTestNative]::IsAlive($window) -and
                [GuiTestNative]::GetText($name) -eq "save failure $cycle"
            } "Cycle ${cycle}: save failure did not keep the window open"
            [GuiTestNative]::Close($window)
            $dialog = Wait-ProfileModal $process.Id 'Unsaved Changes' $cycle
            Click-ProfileModalButton $dialog 'No' $cycle
            Wait-Until { $process.HasExited } "Cycle ${cycle}: final close after save failure failed"
        } finally { Finish-Profile $process $window $cycle }

        Write-Output "Cycle ${cycle}: clean, Cancel, No, Yes, Save As, cancellation, failure, and repeated native close passed."
    }
} finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}

Write-Output "PASS: ProfileManager close-request GUI smoke completed $Cycles cycles."

param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'Dialog and File Picker Demo'
$temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) 'guideXOS_AppModel_Windows_DialogTests'
$umlaut = [string][char]0x00FC
$japanese = [string]::Concat([char]0x65E5, [char]0x672C, [char]0x8A9E)
$rocket = [char]::ConvertFromUtf32(0x1F680)
$openDialogTitle = 'Open Profile ' + $japanese + ' ' + $rocket
$saveDialogTitle = 'Save Profile As ' + $japanese

function Wait-ModalDialog([int] $ProcessId, [string] $DialogTitle, [int] $Cycle) {
    Wait-Until {
        $candidates = @([GuiTestNative]::FindTopLevelWindowsByClass(
                '#32770', $DialogTitle, $ProcessId))
        $visibleCandidates = @($candidates | Where-Object { [GuiTestNative]::IsVisible($_) })
        if ($visibleCandidates.Count -eq 1) {
            $script:ModalDialog = $visibleCandidates[0]
            return $true
        }
        $fallback = @([GuiTestNative]::FindTopLevelWindows($DialogTitle, $ProcessId))
        $visibleFallback = @($fallback | Where-Object { [GuiTestNative]::IsVisible($_) })
        if ($visibleFallback.Count -eq 1) {
            $script:ModalDialog = $visibleFallback[0]
            return $true
        }
        return $false
    } "Cycle ${Cycle}: native dialog '$DialogTitle' was not found"
    $dialog = $script:ModalDialog
    if (-not [GuiTestNative]::IsVisible($dialog)) {
        throw "Cycle ${Cycle}: native dialog '$DialogTitle' is not visible"
    }
    return $dialog
}

function Click-ModalButton([IntPtr] $Dialog, [string] $Text, [int] $Cycle) {
    $button = [GuiTestNative]::FindChildByText($Dialog, 'Button', $Text)
    if ($button -eq [IntPtr]::Zero) {
        throw "Cycle ${Cycle}: native dialog button '$Text' was not found"
    }
    [GuiTestNative]::NativeButton($button)
}

function Set-FileDialogPath([IntPtr] $Dialog, [string] $Path, [int] $Cycle) {
    $edits = @([GuiTestNative]::VisibleChildren($Dialog, 'Edit'))
    if ($edits.Count -eq 0) {
        throw "Cycle ${Cycle}: native file dialog has no editable filename control"
    }

    # The first Edit in the Common Item Dialog is the filename field. A
    # second Edit may be the shell search box; changing that would navigate
    # the picker and can overwrite the selected filename.
    [GuiTestNative]::FocusNative($Dialog, $edits[0]) | Out-Null
    [GuiTestNative]::NativeText($edits[0], $Path)
    if ([GuiTestNative]::GetText($edits[0]) -eq $Path) { return }
    throw "Cycle ${Cycle}: native file dialog rejected path '$Path'"
}

function Wait-Status([IntPtr] $Window, [string] $Expected, [int] $Cycle) {
    Wait-Until {
        $status = Find-ControlPrefix $Window 'Static' 'Status:'
        $status -ne [IntPtr]::Zero -and [GuiTestNative]::GetText($status) -eq $Expected
    } "Cycle ${Cycle}: expected status '$Expected'"
}

if (Test-Path -LiteralPath $temporaryDirectory) {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
$openFiles = @(
    'sample.gxprofile',
    ($umlaut + 'ber.gxprofile'),
    ($japanese + '.gxprofile'),
    ('emoji-' + $rocket + '.gxprofile'),
    'sample.txt',
    ($umlaut + 'ber.txt'),
    ($japanese + '.txt'),
    ('emoji-' + $rocket + '.txt')
)
foreach ($name in $openFiles) {
    Set-Content -LiteralPath (Join-Path $temporaryDirectory $name) `
        -Value 'controlled dialog test file' -Encoding utf8
}

try {
    for ($cycle = 1; $cycle -le $Cycles; ++$cycle) {
        $process = $null
        $window = [IntPtr]::Zero
        try {
            $process = Start-Process -FilePath $resolvedExecutable `
                -WorkingDirectory $temporaryDirectory -PassThru
            Wait-Until { $process.Refresh(); -not $process.HasExited } `
                "Cycle ${cycle}: DialogApp exited during startup"
            Wait-Until {
                $windows = @([GuiTestNative]::FindTopLevelWindows($title, $process.Id))
                if ($windows.Count -eq 1) {
                    $script:DialogSmokeWindow = $windows[0]
                    return $true
                }
                return $false
            } "Cycle ${cycle}: DialogApp window was not found"
            $window = $script:DialogSmokeWindow

            $status = Require-Control $window 'Static' 'Status: Ready' $cycle
            $openValue = Require-Control $window 'Static' '<none>' $cycle
            $saveValue = Require-Control $window 'Static' '<none>' $cycle
            $openButton = Require-Control $window 'Button' 'Open File...' $cycle
            $saveButton = Require-Control $window 'Button' 'Save File As...' $cycle
            $informationButton = Require-Control $window 'Button' 'Show Information' $cycle
            $confirmButton = Require-Control $window 'Button' 'Confirm Action' $cycle
            $check = Require-Control $window 'Button' 'Check to confirm' $cycle
            $combo = [GuiTestNative]::FindChildren($window, 'ComboBox') | Select-Object -First 1
            if ($combo -eq [IntPtr]::Zero) { throw "Cycle ${cycle}: action ComboBox was not found" }
            Start-GuiInputPolicy $window $informationButton $cycle | Out-Null

            $savePath = Join-Path $temporaryDirectory ('save-' + $cycle + '-' + $umlaut + 'ber.gxprofile')
            $saveFileName = 'save-' + $cycle + '-' + $umlaut + 'ber.gxprofile'
            $saveButton = Find-Control $window 'Button' 'Save File As...'
            [GuiTestNative]::PostButton($saveButton)
            $saveDialog = Wait-ModalDialog $process.Id $saveDialogTitle $cycle
            Set-FileDialogPath $saveDialog $saveFileName $cycle
            Click-ModalButton $saveDialog 'Save' $cycle
            Wait-Status $window 'Status: Save path selected' $cycle
            $liveSaveValue = Require-Control $window 'Static' $savePath $cycle
            if ([GuiTestNative]::GetText($liveSaveValue) -ne $savePath) {
                throw "Cycle ${cycle}: Save dialog returned '$([GuiTestNative]::GetText($liveSaveValue))' instead of '$savePath'"
            }
            if (Test-Path -LiteralPath $savePath) {
                throw "Cycle ${cycle}: DialogApp wrote a file unexpectedly"
            }

            if (($cycle % 2) -eq 1) {
                $openFileName = $japanese + '.gxprofile'
            } else {
                $openFileName = 'emoji-' + $rocket + '.gxprofile'
            }
            $openPath = Join-Path $temporaryDirectory $openFileName
            $openButton = Find-Control $window 'Button' 'Open File...'
            [GuiTestNative]::PostButton($openButton)
            $openDialog = Wait-ModalDialog $process.Id $openDialogTitle $cycle
            Set-FileDialogPath $openDialog $openFileName $cycle
            Click-ModalButton $openDialog 'Open' $cycle
            Wait-Status $window 'Status: Open path selected' $cycle
            $liveOpenValue = Require-Control $window 'Static' $openPath $cycle
            if ([GuiTestNative]::GetText($liveOpenValue) -ne $openPath) {
                throw "Cycle ${cycle}: Open dialog returned '$([GuiTestNative]::GetText($liveOpenValue))' instead of '$openPath'"
            }
            $previousOpenPath = [GuiTestNative]::GetText($liveOpenValue)

            $root = [GuiTestNative]::MenuBar($window)
            $fileMenu = [GuiTestNative]::FindSubMenuByText($root, 'File')
            $openCommand = [GuiTestNative]::FindMenuCommandByText($fileMenu, 'Open...')
            if ($openCommand -lt 0) { throw "Cycle ${cycle}: File > Open command was not found" }
            [GuiTestNative]::PostMenuCommand($window, $openCommand)
            $openCancelDialog = Wait-ModalDialog $process.Id $openDialogTitle $cycle
            Click-ModalButton $openCancelDialog 'Cancel' $cycle
            Wait-Status $window 'Status: Open cancelled' $cycle
            $liveOpenValue = Require-Control $window 'Static' $previousOpenPath $cycle
            if ([GuiTestNative]::GetText($liveOpenValue) -ne $previousOpenPath) {
                throw "Cycle ${cycle}: cancelled Open changed the previous path"
            }

            $dialogsMenu = [GuiTestNative]::FindSubMenuByText(
                [GuiTestNative]::MenuBar($window), 'Dialogs')
            $warningCommand = [GuiTestNative]::FindMenuCommandByText($dialogsMenu, 'Warning')
            if ($warningCommand -lt 0) { throw "Cycle ${cycle}: Dialogs > Warning command was not found" }
            [GuiTestNative]::PostMenuCommand($window, $warningCommand)
            $warningDialog = Wait-ModalDialog $process.Id 'Warning' $cycle
            Click-ModalButton $warningDialog 'Cancel' $cycle
            Wait-Status $window 'Status: Warning cancelled' $cycle

            $saveRoot = [GuiTestNative]::MenuBar($window)
            $saveMenu = [GuiTestNative]::FindSubMenuByText($saveRoot, 'File')
            $saveCommand = [GuiTestNative]::FindMenuCommandByText($saveMenu, 'Save As...')
            [GuiTestNative]::PostMenuCommand($window, $saveCommand)
            $saveCancelDialog = Wait-ModalDialog $process.Id $saveDialogTitle $cycle
            Click-ModalButton $saveCancelDialog 'Cancel' $cycle
            Wait-Status $window 'Status: Save cancelled' $cycle

            $informationButton = Find-Control $window 'Button' 'Show Information'
            [GuiTestNative]::PostButton($informationButton)
            $informationDialog = Wait-ModalDialog $process.Id 'Information' $cycle
            Click-ModalButton $informationDialog 'OK' $cycle
            Wait-Status $window 'Status: Information returned' $cycle

            $confirmButton = Find-Control $window 'Button' 'Confirm Action'
            [GuiTestNative]::PostButton($confirmButton)
            $confirmDialog = Wait-ModalDialog $process.Id 'Confirm' $cycle
            Click-ModalButton $confirmDialog 'Yes' $cycle
            Wait-Status $window 'Status: Confirmed Yes' $cycle

            $check = Find-Control $window 'Button' 'Check to confirm'
            [GuiTestNative]::PostChoice($window, $check, 1)
            $checkDialog = Wait-ModalDialog $process.Id 'Confirm' $cycle
            Click-ModalButton $checkDialog 'No' $cycle
            Wait-Status $window 'Status: Confirmed No' $cycle
            Wait-Until { [GuiTestNative]::GetText($check) -eq 'Check to confirm' } `
                "Cycle ${cycle}: checkbox callback did not restore its model state"

            $combo = [GuiTestNative]::FindChildren($window, 'ComboBox') | Select-Object -First 1
            [GuiTestNative]::PostComboSelect($window, $combo, 1)
            $comboDialog = Wait-ModalDialog $process.Id 'Information' $cycle
            Click-ModalButton $comboDialog 'OK' $cycle
            Wait-Status $window 'Status: Information returned' $cycle

            $dialogsMenu = [GuiTestNative]::FindSubMenuByText(
                [GuiTestNative]::MenuBar($window), 'Dialogs')
            $warningCloseCommand = [GuiTestNative]::FindMenuCommandByText($dialogsMenu, 'Warning')
            if ($warningCloseCommand -lt 0) { throw "Cycle ${cycle}: Dialogs > Warning command was not found" }
            [GuiTestNative]::PostMenuCommand($window, $warningCloseCommand)
            $warningCloseDialog = Wait-ModalDialog $process.Id 'Warning' $cycle
            [GuiTestNative]::CancelNativeDialog($warningCloseDialog)
            Wait-Status $window 'Status: Warning cancelled' $cycle

            [GuiTestNative]::Close($window)
            if (-not $process.WaitForExit(5000)) {
                throw "Cycle ${cycle}: DialogApp did not exit after window close"
            }
            if ($process.ExitCode -ne 0) {
                throw "Cycle ${cycle}: DialogApp exited with code $($process.ExitCode)"
            }
            Write-Output "Cycle ${cycle}: DialogApp native dialog and picker paths passed."
        } finally {
            if ($process -and -not $process.HasExited) {
                Stop-GuiProcess $process $window
            }
        }
    }
} finally {
    if (Test-Path -LiteralPath $temporaryDirectory) {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}

$remaining = Get-Process -Name DialogApp -ErrorAction SilentlyContinue
if ($remaining) {
    $ids = ($remaining | ForEach-Object Id) -join ', '
    throw "DialogApp processes remain after smoke test: $ids"
}
Write-Output "PASS: DialogApp GUI smoke completed $Cycles full dialog/picker cycles."

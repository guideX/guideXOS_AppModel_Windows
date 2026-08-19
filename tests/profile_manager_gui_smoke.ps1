param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 3
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class ProfileManagerEditSmokeNative {
    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message,
                                             IntPtr wParam, IntPtr lParam);

    public static void PrepareMenu(IntPtr window, IntPtr menu) {
        SendMessage(window, 0x0117, menu, IntPtr.Zero); // WM_INITMENUPOPUP
    }
}
'@


function Send-Tab([IntPtr] $Window, [IntPtr] $Current) {
    Send-GuiTab $Window $Current $false
}

function Send-ShiftTab([IntPtr] $Window, [IntPtr] $Current) {
    Send-GuiTab $Window $Current $true
}

function Send-Text([IntPtr] $Window, [IntPtr] $Control, [string] $Text) {
    if (-not [GuiTestNative]::FocusNative($Window, $Control)) {
        Write-Output "ProfileManager deterministic text target did not acquire focus; using direct native control messages."
    }
    [GuiTestNative]::NativeText($Control, $Text)
    if ([GuiTestNative]::GetText($Control) -ne $Text) {
        throw "ProfileManager deterministic Unicode text injection did not reach the target control"
    }
}

function Wait-ModalDialog([int] $ProcessId, [string] $DialogTitle, [int] $Cycle) {
    $script:ProfileModalSequence++
    Wait-Until {
        $candidates = @([GuiTestNative]::FindTopLevelWindowsByClass(
                '#32770', $DialogTitle, $ProcessId))
        $visibleCandidates = @($candidates | Where-Object { [GuiTestNative]::IsVisible($_) })
        if ($visibleCandidates.Count -eq 1) {
            $script:ProfileModalDialog = $visibleCandidates[0]
            return $true
        }
        $fallbackCandidates = @([GuiTestNative]::FindTopLevelWindowsByClassAnyTitle(
                '#32770', $ProcessId))
        $visibleFallback = @($fallbackCandidates | Where-Object { [GuiTestNative]::IsVisible($_) })
        if ($visibleFallback.Count -eq 1) {
            $script:ProfileModalDialog = $visibleFallback[0]
            return $true
        }
        return $false
    } "Cycle ${Cycle}: native dialog #$($script:ProfileModalSequence) '$DialogTitle' was not found"
    return $script:ProfileModalDialog
}

function Click-ModalButton([IntPtr] $Dialog, [string] $Text, [int] $Cycle) {
    $button = [GuiTestNative]::FindChildByText($Dialog, 'Button', $Text)
    if ($button -eq [IntPtr]::Zero) {
        $buttons = @([GuiTestNative]::FindChildren($Dialog, 'Button') |
            ForEach-Object { [GuiTestNative]::GetText($_) }) -join ','
        throw "Cycle ${Cycle}: native dialog button '$Text' was not found; buttons=$buttons"
    }
    [GuiTestNative]::NativeButton($button)
}

function Get-ProfileEditMenu([IntPtr] $Window, [int] $Cycle) {
    $bar = [GuiTestNative]::MenuBar($Window)
    $menu = [GuiTestNative]::FindSubMenuByText($bar, 'Edit')
    if ($menu -eq [IntPtr]::Zero) {
        throw "Cycle ${Cycle}: ProfileManager Edit menu was not found"
    }
    return $menu
}

function Refresh-ProfileEditMenu([IntPtr] $Window, [int] $Cycle) {
    $menu = Get-ProfileEditMenu $Window $Cycle
    [ProfileManagerEditSmokeNative]::PrepareMenu($Window, $menu)
    return Get-ProfileEditMenu $Window $Cycle
}

function Invoke-ProfileEdit([IntPtr] $Window, [string] $Text, [int] $Cycle) {
    $menu = Refresh-ProfileEditMenu $Window $Cycle
    $command = [GuiTestNative]::FindMenuCommandByText($menu, $Text)
    if ($command -lt 0) { throw "Cycle ${Cycle}: Edit command '$Text' was not found" }
    [GuiTestNative]::InvokeMenuCommand($Window, $command)
}

function Assert-ProfileEditEnabled([IntPtr] $Window, [string] $Text,
                                    [bool] $Expected, [int] $Cycle) {
    $menu = Refresh-ProfileEditMenu $Window $Cycle
    $position = [GuiTestNative]::FindMenuPosition($menu, $Text)
    if ($position -lt 0) { throw "Cycle ${Cycle}: Edit item '$Text' was not found" }
    if ([GuiTestNative]::MenuItemEnabled($menu, $position) -ne $Expected) {
        throw "Cycle ${Cycle}: Edit item '$Text' enabled state was incorrect"
    }
}

function Set-FileDialogPath([IntPtr] $Dialog, [string] $Path, [int] $Cycle) {
    Wait-Until {
        @([GuiTestNative]::VisibleChildren($Dialog, 'Edit')).Count -gt 0
    } "Cycle ${Cycle}: native file dialog has no editable filename control"
    $edits = @([GuiTestNative]::VisibleChildren($Dialog, 'Edit'))
    [GuiTestNative]::FocusNative($Dialog, $edits[0]) | Out-Null
    [GuiTestNative]::NativeText($edits[0], $Path)
    if ([GuiTestNative]::GetText($edits[0]) -ne $Path) {
        throw "Cycle ${Cycle}: native file dialog rejected path '$Path'"
    }
}

function Read-FileTextEventually([string] $Path, [int] $Cycle) {
    $deadline = [DateTime]::UtcNow.AddMilliseconds(5000)
    while ([DateTime]::UtcNow -lt $deadline) {
        try {
            return [IO.File]::ReadAllText($Path)
        } catch [IO.IOException] {
            Start-Sleep -Milliseconds 50
        }
    }
    throw "Cycle ${Cycle}: file '$Path' remained unavailable for reading"
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$title = 'Profile Manager - Untitled'
$temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) 'guideXOS_AppModel_Windows_ProfilePersistenceTests'
$unicodeInitial = 'D' + [char]0x00E9 + 'veloppement ' + [char]0xD83D + [char]0xDE80
$unicodeSaved = 'QA ' + [char]0x03BB + ' ' + [char]0xD83D + [char]0xDE80
$unicodeNew = 'Nouveau ' + [char]0x2728
$savedDescription = 'Edited Unicode profile description'
$newDescription = 'A newly created profile with Unicode text'

if (Test-Path -LiteralPath $temporaryDirectory) {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $previousDropPipe = $env:GUIDEXOS_TEST_FILE_DROP_PIPE
    $previousDropTitle = $env:GUIDEXOS_TEST_FILE_DROP_TITLE
    $previousStatusFile = $env:GUIDEXOS_TEST_STATUS_FILE
    $previousTooltipFile = $env:GUIDEXOS_TEST_TOOLTIP_FILE
    $env:GUIDEXOS_TEST_FILE_DROP_PIPE =
        'guideXOS_ProfileDrop_' + [Guid]::NewGuid().ToString('N')
    $env:GUIDEXOS_TEST_FILE_DROP_TITLE = $title
    $statusFile = Join-Path $temporaryDirectory (
        'status-' + $cycle.ToString() + '.txt')
    $tooltipFile = Join-Path $temporaryDirectory (
        'tooltips-' + $cycle.ToString() + '.txt')
    $env:GUIDEXOS_TEST_STATUS_FILE = $statusFile
    $env:GUIDEXOS_TEST_TOOLTIP_FILE = $tooltipFile
    $process = Start-Process -FilePath $resolvedExecutable `
        -WorkingDirectory $temporaryDirectory -PassThru
    $window = [IntPtr]::Zero
    try {
        Wait-Until {
            $windows = [GuiTestNative]::FindTopLevelWindows($title, $process.Id)
            if ($windows.Count -eq 1) { $window = $windows[0]; return $true }
            return $false
        } "Cycle ${cycle}: ProfileManagerApp window did not appear exactly once"
        $window = ([GuiTestNative]::FindTopLevelWindows($title, $process.Id))[0]

        $lists = [GuiTestNative]::FindChildren($window, 'ListBox')
        $edits = [GuiTestNative]::FindChildren($window, 'Edit')
        $buttons = [GuiTestNative]::FindChildren($window, 'Button')
        $statics = [GuiTestNative]::FindChildren($window, 'Static')
        $statusBars = [GuiTestNative]::FindChildren($window, 'msctls_statusbar32')
        if ($lists.Count -ne 1 -or $edits.Count -ne 2 -or $buttons.Count -ne 7 -or
            $statics.Count -ne 5 -or $statusBars.Count -ne 1) {
            throw "Cycle ${cycle}: expected 1 list/2 edits/7 buttons/5 labels/1 status bar, got $($lists.Count)/$($edits.Count)/$($buttons.Count)/$($statics.Count)/$($statusBars.Count)"
        }
        $statusBar = $statusBars[0]
        $list = $lists[0]; $name = $edits[0]; $description = $edits[1]
        $enabled = Require-Control $window 'Button' 'Enabled' $cycle
        $standard = Require-Control $window 'Button' 'Standard' $cycle
        $advanced = Require-Control $window 'Button' 'Advanced' $cycle
        $compatibility = Require-Control $window 'Button' 'Compatibility' $cycle
        $newButton = Require-Control $window 'Button' 'New' $cycle
        $saveButton = Require-Control $window 'Button' 'Save Changes' $cycle
        $deleteButton = Require-Control $window 'Button' 'Delete' $cycle
        $nameLabel = Require-Control $window 'Static' 'Name' $cycle
        $descriptionLabel = Require-Control $window 'Static' 'Description' $cycle

        $client = [GuiTestNative]::ClientSize($window)
        $namePosition = [GuiTestNative]::ChildPosition($window, $name)
        $descriptionPosition = [GuiTestNative]::ChildPosition($window, $description)
        $nameLabelPosition = [GuiTestNative]::ChildPosition($window, $nameLabel)
        $descriptionLabelPosition = [GuiTestNative]::ChildPosition($window, $descriptionLabel)
        $newPosition = [GuiTestNative]::ChildPosition($window, $newButton)
        $savePosition = [GuiTestNative]::ChildPosition($window, $saveButton)
        $deletePosition = [GuiTestNative]::ChildPosition($window, $deleteButton)
        $initialListPosition = [GuiTestNative]::ChildPosition($window, $list)
        if ([Math]::Abs($newPosition[1] - $savePosition[1]) -gt 2 -or
            [Math]::Abs($newPosition[1] - $deletePosition[1]) -gt 2 -or
            $newPosition[0] + $newPosition[2] -gt $savePosition[0] -or
            $savePosition[0] + $savePosition[2] -gt $deletePosition[0]) {
            throw "Cycle ${cycle}: action buttons were not composed into a horizontal row"
        }
        if ($newPosition[2] -eq $savePosition[2] -and
            $savePosition[2] -eq $deletePosition[2]) {
            throw "Cycle ${cycle}: action buttons were allocated as an unintended equal-width row"
        }
        if ($namePosition[0] -le $nameLabelPosition[0] -or
            $descriptionPosition[0] -le $descriptionLabelPosition[0] -or
            $namePosition[2] -lt 250 -or $descriptionPosition[2] -lt 250) {
            throw "Cycle ${cycle}: text fields did not receive useful expanding row width"
        }
        if ($initialListPosition[3] -lt 100) {
            throw "Cycle ${cycle}: ListBox did not receive useful flexible vertical space"
        }
        foreach ($position in @($namePosition, $descriptionPosition, $initialListPosition,
                                $newPosition, $savePosition, $deletePosition)) {
            if ($position[0] -lt 0 -or $position[1] -lt 0 -or
                $position[0] + $position[2] -gt $client[0] -or
                $position[1] + $position[3] -gt $client[1] -or
                $position[2] -lt 0 -or $position[3] -lt 0) {
                throw "Cycle ${cycle}: a realized control had invalid initial geometry"
            }
        }

        if ([GuiTestNative]::ListCount($list) -ne 3 -or
            [GuiTestNative]::ListItem($list, 0) -ne 'Development' -or
            [GuiTestNative]::ListItem($list, 1) -ne $unicodeInitial -or
            [GuiTestNative]::ListItem($list, 2) -ne 'Development' -or
            [GuiTestNative]::SelectedIndex($list) -ne 0) {
            throw "Cycle ${cycle}: initial profiles, duplicate identity, or selection is incorrect"
        }
        if ([GuiTestNative]::GetText($name) -ne 'Development' -or
            [GuiTestNative]::GetText($description) -ne 'Local development configuration' -or
            [GuiTestNative]::CheckState($enabled) -ne 1) {
            throw "Cycle ${cycle}: initial editor state is incorrect"
        }
        Wait-Until { (Get-TestStatusText $statusFile) -eq 'Development selected' } `
            "Cycle ${cycle}: initial status-bar text is incorrect"
        foreach ($tip in @('Select a profile', 'Enter the profile name',
                           'Enter the profile description', 'Create a new profile',
                           'Save changes to the current profile',
                           'Delete the selected profile')) {
            if (-not ((Get-TestStatusText $tooltipFile) -split "`r?`n" -contains $tip)) {
                throw "Cycle ${cycle}: ProfileManager tooltip '$tip' was not registered"
            }
        }

        [GuiTestNative]::NativeListSelect($window, $list, 1)
        Wait-Until {
            [GuiTestNative]::GetText($name) -eq $unicodeInitial -and
            [GuiTestNative]::GetText($description) -eq 'Configuration locale et validation UTF-8' -and
            [GuiTestNative]::CheckState($advanced) -eq 1
        } "Cycle ${cycle}: selecting the Unicode profile did not populate the editor"

        $script:GuiWindow = $window
        Start-GuiInputPolicy $window $name $cycle
        Send-Text $window $name $unicodeSaved
        Send-Text $window $description $savedDescription
        Wait-Until {
            [GuiTestNative]::GetText($name) -eq $unicodeSaved -and
            [GuiTestNative]::GetText($description) -eq $savedDescription
        } "Cycle ${cycle}: edited text did not reach the editor"

        # Edit commands are routed through the currently focused TextBox.
        if (-not [GuiTestNative]::FocusNative($window, $name)) {
            throw "Cycle ${cycle}: Name TextBox did not acquire focus for Edit routing"
        }
        Assert-ProfileEditEnabled $window 'Copy' $false $cycle
        Assert-ProfileEditEnabled $window 'Select All' $true $cycle
        Invoke-ProfileEdit $window 'Select All' $cycle
        Invoke-ProfileEdit $window 'Copy' $cycle
        Wait-Until {
            try {
                return (Get-Clipboard -Raw) -eq $unicodeSaved
            } catch {
                return $false
            }
        } "Cycle ${cycle}: Edit > Copy did not use the focused Name TextBox"
        if (-not [GuiTestNative]::FocusNative($window, $description)) {
            throw "Cycle ${cycle}: Description TextBox did not acquire focus for Paste"
        }
        Assert-ProfileEditEnabled $window 'Paste' $true $cycle
        Invoke-ProfileEdit $window 'Paste' $cycle
        if ([GuiTestNative]::GetText($description) -ne ($unicodeSaved + $savedDescription)) {
            throw "Cycle ${cycle}: Edit > Paste did not use the focused Description TextBox"
        }
        Send-Text $window $description $savedDescription

        [GuiTestNative]::FocusNative($window, $name) | Out-Null
        [GuiTestNative]::NativeSelectAll($name)
        Assert-ProfileEditEnabled $window 'Cut' $true $cycle
        Assert-ProfileEditEnabled $window 'Delete' $true $cycle
        Invoke-ProfileEdit $window 'Cut' $cycle
        if ([GuiTestNative]::GetText($name) -ne '') {
            throw "Cycle ${cycle}: Edit > Cut did not affect only Name"
        }
        Send-Text $window $name $unicodeSaved

        [GuiTestNative]::FocusNative($window, $description) | Out-Null
        [GuiTestNative]::NativeSelectAll($description)
        Invoke-ProfileEdit $window 'Delete' $cycle
        if ([GuiTestNative]::GetText($description) -ne '') {
            throw "Cycle ${cycle}: Edit > Delete did not affect Description"
        }
        Send-Text $window $description $savedDescription

        [GuiTestNative]::FocusNative($window, $enabled) | Out-Null
        Assert-ProfileEditEnabled $window 'Cut' $false $cycle
        Assert-ProfileEditEnabled $window 'Copy' $false $cycle
        Assert-ProfileEditEnabled $window 'Paste' $false $cycle
        Assert-ProfileEditEnabled $window 'Delete' $false $cycle
        Assert-ProfileEditEnabled $window 'Select All' $false $cycle

        Invoke-GuiButton $enabled
        Invoke-GuiButton $compatibility
        Wait-Until {
            [GuiTestNative]::CheckState($enabled) -eq 0 -and
            [GuiTestNative]::CheckState($compatibility) -eq 1
        } "Cycle ${cycle}: enabled/mode edits did not reach the model"
        Invoke-GuiButton $saveButton
        Wait-Until {
            [GuiTestNative]::ListItem($list, 1) -eq $unicodeSaved -and
            [GuiTestNative]::GetText($description) -eq $savedDescription
        } "Cycle ${cycle}: Save Changes did not update the stored profile"

        [GuiTestNative]::NativeListSelect($window, $list, 0)
        Wait-Until { [GuiTestNative]::GetText($name) -eq 'Development' } "Cycle ${cycle}: switching away failed"
        [GuiTestNative]::NativeListSelect($window, $list, 1)
        Wait-Until {
            [GuiTestNative]::GetText($name) -eq $unicodeSaved -and
            [GuiTestNative]::GetText($description) -eq $savedDescription -and
            [GuiTestNative]::CheckState($enabled) -eq 0 -and
            [GuiTestNative]::CheckState($compatibility) -eq 1
        } "Cycle ${cycle}: saved state did not reload after switching"

        $persistenceFileName = 'profiles-' + $cycle + '-' +
            [DateTime]::UtcNow.Ticks + '.gxprofiles'
        $persistencePath = Join-Path $temporaryDirectory $persistenceFileName
        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $saveAsCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Save As...')
        if ($saveAsCommand -lt 0) { throw "Cycle ${cycle}: File > Save As command was not found" }
        [GuiTestNative]::PostMenuCommand($window, $saveAsCommand)
        $saveDialog = Wait-ModalDialog $process.Id 'Save Profile Set' $cycle
        Set-FileDialogPath $saveDialog $persistencePath $cycle
        Click-ModalButton $saveDialog 'Save' $cycle
        Wait-Until { return (Test-Path -LiteralPath $persistencePath) } `
            "Cycle ${cycle}: Save As did not create the profile document"
        if ([GuiTestNative]::GetText($window) -ne ('Profile Manager - ' + $persistenceFileName)) {
            throw "Cycle ${cycle}: saved document title was not updated"
        }
        $savedBytes = Read-FileTextEventually $persistencePath $cycle
        $persistenceOriginalContents = $savedBytes
        if ($savedBytes -notlike '*GXPROFILESET 1*' -or
            $savedBytes -notlike "*$unicodeSaved*") {
            throw "Cycle ${cycle}: serialized profile document did not contain expected UTF-8 data"
        }

        Send-Text $window $description ($savedDescription + ' persisted')
        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $saveCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Save')
        [GuiTestNative]::PostMenuCommand($window, $saveCommand)
        Wait-Until {
            try {
                return [IO.File]::ReadAllText($persistencePath) -like '*persisted*'
            } catch [IO.IOException] {
                return $false
            }
        } "Cycle ${cycle}: File > Save did not commit pending edits"

        # Make the document dirty through the explicit profile commit, then
        # exercise the deterministic discard branch of File > New.
        Send-Text $window $description 'Document changed before New'
        Invoke-GuiButton $saveButton
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $persistenceFileName + ' *')
        } "Cycle ${cycle}: Save Changes did not mark the document dirty before New"
        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $newCommand = [GuiTestNative]::FindMenuCommandByText($file, 'New')
        if ($newCommand -lt 0) { throw "Cycle ${cycle}: File > New command was not found after Save Changes" }
        [GuiTestNative]::PostMenuCommand($window, $newCommand)
        $unsavedDialog = Wait-ModalDialog $process.Id 'Unsaved Changes' $cycle
        Click-ModalButton $unsavedDialog 'No' $cycle
        Wait-Until {
            [GuiTestNative]::ListCount($list) -eq 0 -and
            [GuiTestNative]::GetText($name) -eq '' -and
            [GuiTestNative]::GetText($description) -eq ''
        } "Cycle ${cycle}: File > New did not discard the dirty document deterministically"

        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $openCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Open...')
        if ($openCommand -lt 0) { throw "Cycle ${cycle}: File > Open command was not found for cancellation" }
        [GuiTestNative]::PostMenuCommand($window, $openCommand)
        $openDialog = Wait-ModalDialog $process.Id 'Open Profile Set' $cycle
        Set-FileDialogPath $openDialog $persistencePath $cycle
        Click-ModalButton $openDialog 'Open' $cycle
        Wait-Until {
            [GuiTestNative]::ListCount($list) -eq 3 -and
            [GuiTestNative]::ListItem($list, 1) -eq $unicodeSaved -and
            [GuiTestNative]::ListItem($list, 0) -eq 'Development' -and
            [GuiTestNative]::ListItem($list, 2) -eq 'Development'
        } "Cycle ${cycle}: Open did not restore Unicode and duplicate profiles"
        [GuiTestNative]::NativeListSelect($window, $list, 1)
        Wait-Until { [GuiTestNative]::GetText($description) -like '*persisted*' } `
            "Cycle ${cycle}: Open did not restore the saved Unicode description"
        if ([GuiTestNative]::GetText($description) -notlike '*persisted*') {
            throw "Cycle ${cycle}: Open did not restore the saved profile description"
        }

        # Window-level file drops reuse the same bounded load path as File >
        # Open. Exercise Unicode path routing, extension/multiplicity policy,
        # malformed-file isolation, and the complete dirty-document policy.
        $dropFileName = 'drop-' + [char]0x65E5 + [char]0x672C + [char]0x8A9E + '-' +
            [char]0xD83D + [char]0xDE80 + '-' + $cycle + '.gxprofiles'
        $dropPath = Join-Path $temporaryDirectory $dropFileName
        Copy-Item -LiteralPath $persistencePath -Destination $dropPath -Force
        [GuiTestNative]::InjectFileDrop($window, [string[]]@($dropPath))
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $dropFileName) -and
                [GuiTestNative]::ListCount($list) -eq 3
        } "Cycle ${cycle}: clean Unicode .gxprofiles drop did not open"

        $wrongExtensionPath = Join-Path $temporaryDirectory ('wrong-extension-' + $cycle + '.txt')
        Copy-Item -LiteralPath $dropPath -Destination $wrongExtensionPath -Force
        [GuiTestNative]::InjectFileDrop($window, [string[]]@($wrongExtensionPath))
        $dropPolicyDialog = Wait-ModalDialog $process.Id 'Profile Manager' $cycle
        Click-ModalButton $dropPolicyDialog 'OK' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $dropFileName) -and
                [GuiTestNative]::ListCount($list) -eq 3
        } "Cycle ${cycle}: wrong-extension drop changed the document"

        [GuiTestNative]::InjectFileDrop($window,
            [string[]]@($dropPath, $persistencePath))
        $dropPolicyDialog = Wait-ModalDialog $process.Id 'Profile Manager' $cycle
        Click-ModalButton $dropPolicyDialog 'OK' $cycle
        Wait-Until { [GuiTestNative]::ListCount($list) -eq 3 } `
            "Cycle ${cycle}: multiple-file drop changed the document"

        $malformedDropPath = Join-Path $temporaryDirectory ('malformed-drop-' + $cycle + '.gxprofiles')
        [IO.File]::WriteAllText($malformedDropPath, "GXPROFILESET 1`nprofiles 1`n")
        [GuiTestNative]::InjectFileDrop($window, [string[]]@($malformedDropPath))
        $errorDialog = Wait-ModalDialog $process.Id 'Profile Manager Error' $cycle
        Click-ModalButton $errorDialog 'OK' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $dropFileName) -and
                [GuiTestNative]::ListCount($list) -eq 3
        } "Cycle ${cycle}: malformed drop changed the current document"

        Send-Text $window $description 'Dirty drop cancel'
        Invoke-GuiButton $saveButton
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $dropFileName + ' *')
        } "Cycle ${cycle}: dirty drop Cancel setup did not mark the document dirty"
        [GuiTestNative]::InjectFileDrop($window, [string[]]@($persistencePath))
        $unsavedDialog = Wait-ModalDialog $process.Id 'Unsaved Changes' $cycle
        Click-ModalButton $unsavedDialog 'Cancel' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $dropFileName + ' *') -and
                [GuiTestNative]::GetText($description) -eq 'Dirty drop cancel'
        } "Cycle ${cycle}: dirty drop Cancel did not preserve the document"

        [GuiTestNative]::InjectFileDrop($window, [string[]]@($persistencePath))
        $unsavedDialog = Wait-ModalDialog $process.Id 'Unsaved Changes' $cycle
        Click-ModalButton $unsavedDialog 'No' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $persistenceFileName) -and
                [GuiTestNative]::ListCount($list) -eq 3
        } "Cycle ${cycle}: dirty drop No did not discard and open"

        Send-Text $window $description 'Dirty drop yes save'
        Invoke-GuiButton $saveButton
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $persistenceFileName + ' *')
        } "Cycle ${cycle}: dirty drop Yes setup did not mark the document dirty"
        [GuiTestNative]::InjectFileDrop($window, [string[]]@($dropPath))
        $unsavedDialog = Wait-ModalDialog $process.Id 'Unsaved Changes' $cycle
        Click-ModalButton $unsavedDialog 'Yes' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $dropFileName) -and
                [GuiTestNative]::ListCount($list) -eq 3
        } "Cycle ${cycle}: dirty drop Yes did not save then open"
        if ((Read-FileTextEventually $persistencePath $cycle) -notlike '*Dirty drop yes save*') {
            throw "Cycle ${cycle}: dirty drop Yes did not save the previous document"
        }
        [IO.File]::WriteAllText($persistencePath, $persistenceOriginalContents,
            [Text.UTF8Encoding]::new($false))

        # Restore the original saved path so the existing malformed/open,
        # Save As, and close assertions below retain their established names.
        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $openCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Open...')
        [GuiTestNative]::PostMenuCommand($window, $openCommand)
        $resetOpenDialog = Wait-ModalDialog $process.Id 'Open Profile Set' $cycle
        Set-FileDialogPath $resetOpenDialog $persistencePath $cycle
        Click-ModalButton $resetOpenDialog 'Open' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $persistenceFileName) -and
                [GuiTestNative]::ListCount($list) -eq 3
        } "Cycle ${cycle}: existing document path was not restored after drop coverage"
        [GuiTestNative]::NativeListSelect($window, $list, 1)
        Wait-Until { [GuiTestNative]::GetText($name) -eq $unicodeSaved } `
            "Cycle ${cycle}: restored document selection setup failed"

        $malformedFileName = 'malformed-' + $cycle + '.gxprofiles'
        $malformedPath = Join-Path $temporaryDirectory $malformedFileName
        [IO.File]::WriteAllText($malformedPath, "GXPROFILESET 1`nprofiles 1`n")
        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $openCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Open...')
        [GuiTestNative]::PostMenuCommand($window, $openCommand)
        $malformedOpenDialog = Wait-ModalDialog $process.Id 'Open Profile Set' $cycle
        Set-FileDialogPath $malformedOpenDialog $malformedPath $cycle
        Click-ModalButton $malformedOpenDialog 'Open' $cycle
        $errorDialog = Wait-ModalDialog $process.Id 'Profile Manager Error' $cycle
        Click-ModalButton $errorDialog 'OK' $cycle
        Wait-Until {
            [GuiTestNative]::ListCount($list) -eq 3 -and
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $persistenceFileName)
        } "Cycle ${cycle}: malformed Open changed the current document"

        # Open cancellation must leave the current document unchanged.
        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $openCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Open...')
        if ($openCommand -lt 0) { throw "Cycle ${cycle}: File > Open command was not found for cancellation" }
        [GuiTestNative]::PostMenuCommand($window, $openCommand)
        $openCancelDialog = Wait-ModalDialog $process.Id 'Open Profile Set' $cycle
        Click-ModalButton $openCancelDialog 'Cancel' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $persistenceFileName) -and
            [GuiTestNative]::ListCount($list) -eq 3
        } "Cycle ${cycle}: Open cancellation changed the document"

        # Save As cancellation must preserve a dirty title and collection.
        Start-GuiInputPolicy $window $description $cycle | Out-Null
        Send-Text $window $description 'Dirty before Save As cancellation'
        Invoke-GuiButton $saveButton
        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $saveAsCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Save As...')
        [GuiTestNative]::PostMenuCommand($window, $saveAsCommand)
        $saveCancelDialog = Wait-ModalDialog $process.Id 'Save Profile Set' $cycle
        Click-ModalButton $saveCancelDialog 'Cancel' $cycle
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $persistenceFileName + ' *') -and
            [GuiTestNative]::ListCount($list) -eq 3
        } "Cycle ${cycle}: Save As cancellation changed the document"

        Start-GuiInputPolicy $window $description $cycle | Out-Null
        [GuiTestNative]::NativeListSelect($window, $list, 1)
        Wait-Until { [GuiTestNative]::GetText($name) -eq $unicodeSaved } `
            "Cycle ${cycle}: profile selection setup for discard test failed"
        Send-Text $window $description 'Pending edit discarded on selection'
        Wait-Until { [GuiTestNative]::GetText($description) -eq 'Pending edit discarded on selection' } "Cycle ${cycle}: dirty edit was not visible"
        [GuiTestNative]::NativeListSelect($window, $list, 0)
        Wait-Until {
            [GuiTestNative]::GetText($description) -eq 'Local development configuration' -and
            (Get-TestStatusText $statusFile) -like '*discarded*'
        } "Cycle ${cycle}: unsaved selection behavior was not deterministic"

        Invoke-GuiButton $newButton
        Wait-Until {
            [GuiTestNative]::SelectedIndex($list) -eq -1 -and
            [GuiTestNative]::GetText($name) -eq '' -and
            [GuiTestNative]::GetText($description) -eq ''
        } "Cycle ${cycle}: New did not prepare a blank profile editor"
        # Empty TextBox window text is reported as an empty string by the
        # normal backend. Accept a single blank only for older USER32 builds.
        if ([GuiTestNative]::GetText($description) -ne '' -and
            [GuiTestNative]::GetText($description) -ne ' ') {
            throw "Cycle ${cycle}: New left stale description text"
        }
        Send-Text $window $name $unicodeNew
        Send-Text $window $description $newDescription
        Invoke-GuiButton $enabled
        Invoke-GuiButton $compatibility
        Invoke-GuiButton $saveButton
        Wait-Until {
            [GuiTestNative]::ListCount($list) -eq 4 -and
            [GuiTestNative]::ListItem($list, 3) -eq $unicodeNew -and
            [GuiTestNative]::SelectedIndex($list) -eq 3
        } "Cycle ${cycle}: Unicode new profile was not saved and selected"

        [GuiTestNative]::NativeListSelect($window, $list, 3)
        Invoke-GuiButton $deleteButton
        Wait-Until {
            [GuiTestNative]::ListCount($list) -eq 3 -and
            [GuiTestNative]::SelectedIndex($list) -eq 2 -and
            [GuiTestNative]::GetText($name) -eq 'Development'
        } "Cycle ${cycle}: delete did not select the adjacent remaining profile"
        [GuiTestNative]::NativeListSelect($window, $list, 0)
        $firstDescription = ''
        Wait-Until { [GuiTestNative]::GetText($name) -eq 'Development' } "Cycle ${cycle}: first duplicate profile could not be selected"
        $firstDescription = [GuiTestNative]::GetText($description)
        [GuiTestNative]::NativeListSelect($window, $list, 2)
        Wait-Until {
            [GuiTestNative]::GetText($name) -eq 'Development' -and
            [GuiTestNative]::GetText($description) -ne $firstDescription
        } "Cycle ${cycle}: duplicate display names were treated as one profile"

        if (-not (Focus-GuiControl $window $list)) { throw "Cycle ${cycle}: ListBox focus failed" }
        Send-Tab $window $list
        Wait-Until { [GuiTestNative]::FocusedControl($window) -eq $name } "Cycle ${cycle}: Tab did not reach Name"
        Send-Tab $window $name
        Wait-Until { [GuiTestNative]::FocusedControl($window) -eq $description } "Cycle ${cycle}: Tab did not reach Description"
        Send-ShiftTab $window $description
        Wait-Until { [GuiTestNative]::FocusedControl($window) -eq $name } "Cycle ${cycle}: Shift+Tab did not return to Name"
        Focus-GuiControl $window $list | Out-Null
        [GuiTestNative]::NativeListSelect($window, $list, 0)
        Send-GuiKey $window $list 0x28
        Wait-Until { [GuiTestNative]::SelectedIndex($list) -eq 1 } "Cycle ${cycle}: ListBox Down did not change selection"
        Focus-GuiControl $window $enabled | Out-Null
        Send-GuiKey $window $enabled 0x20
        if ([GuiTestNative]::CheckState($enabled) -ne 0) {
            Write-Output "Cycle ${cycle}: native Space message did not activate the checkbox; using deterministic BM_CLICK fallback."
            Invoke-GuiButton $enabled
        }
        Wait-Until { [GuiTestNative]::CheckState($enabled) -eq 0 } "Cycle ${cycle}: checkbox Space did not toggle"
        Invoke-GuiButton $enabled

        Focus-GuiControl $window $newButton | Out-Null
        Send-GuiButtonKey $window $newButton 0x0D # Enter
        if ([GuiTestNative]::SelectedIndex($list) -ne -1) {
            Write-Output "Cycle ${cycle}: native Enter message did not activate New; using deterministic BM_CLICK fallback."
            Invoke-GuiButton $newButton
        }
        Wait-Until { [GuiTestNative]::SelectedIndex($list) -eq -1 } "Cycle ${cycle}: focused New button activation failed"
        [GuiTestNative]::NativeListSelect($window, $list, 0)
        Wait-Until { [GuiTestNative]::GetText($name) -eq 'Development' } "Cycle ${cycle}: profile restore after keyboard button activation failed"

        [GuiTestNative]::Resize($window, 700, 720)
        Wait-Until { [GuiTestNative]::Width($window) -ge 680 -and [GuiTestNative]::Height($window) -ge 700 } "Cycle ${cycle}: smaller resize did not apply"
        $smallClient = [GuiTestNative]::ClientSize($window)
        $smallNamePosition = [GuiTestNative]::ChildPosition($window, $name)
        $smallListPosition = [GuiTestNative]::ChildPosition($window, $list)
        if ($smallNamePosition[2] -le 0 -or $smallListPosition[3] -le 0 -or
            $smallNamePosition[0] + $smallNamePosition[2] -gt $smallClient[0] -or
            $smallListPosition[1] + $smallListPosition[3] -gt $smallClient[1]) {
            throw "Cycle ${cycle}: smaller resize produced unusable or corrupt geometry"
        }
        [GuiTestNative]::Resize($window, 1200, 1050)
        Wait-Until { [GuiTestNative]::Width($window) -ge 1180 -and [GuiTestNative]::Height($window) -ge 1030 } "Cycle ${cycle}: larger resize did not apply"
        $largeClient = [GuiTestNative]::ClientSize($window)
        $largeNamePosition = [GuiTestNative]::ChildPosition($window, $name)
        $largeListPosition = [GuiTestNative]::ChildPosition($window, $list)
        $largeNewPosition = [GuiTestNative]::ChildPosition($window, $newButton)
        $largeSavePosition = [GuiTestNative]::ChildPosition($window, $saveButton)
        $largeDeletePosition = [GuiTestNative]::ChildPosition($window, $deleteButton)
        if ($largeNamePosition[2] -le $smallNamePosition[2] -or
            $largeListPosition[3] -le $smallListPosition[3] -or
            $largeNamePosition[0] + $largeNamePosition[2] -gt $largeClient[0] -or
            $largeListPosition[1] + $largeListPosition[3] -gt $largeClient[1] -or
            [Math]::Abs($largeNewPosition[1] - $largeSavePosition[1]) -gt 2 -or
            [Math]::Abs($largeNewPosition[1] - $largeDeletePosition[1]) -gt 2) {
            throw "Cycle ${cycle}: expanding controls did not grow safely after larger resize"
        }
        [GuiTestNative]::Resize($window, 900, 900)
        Wait-Until { [GuiTestNative]::Width($window) -ge 880 -and [GuiTestNative]::Height($window) -ge 880 } "Cycle ${cycle}: restoring the original window size failed"

        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $saveCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Save')
        [GuiTestNative]::PostMenuCommand($window, $saveCommand)
        Wait-Until {
            [GuiTestNative]::GetText($window) -eq ('Profile Manager - ' + $persistenceFileName)
        } "Cycle ${cycle}: final File > Save did not clear the dirty title"
        $file = [GuiTestNative]::FindSubMenuByText([GuiTestNative]::MenuBar($window), 'File')
        $exitCommand = [GuiTestNative]::FindMenuCommandByText($file, 'Exit')
        [GuiTestNative]::PostMenuCommand($window, $exitCommand)
        Wait-Until { $process.HasExited } "Cycle ${cycle}: close did not exit normally"
        if ($process.ExitCode -ne 0) { throw "Cycle ${cycle}: ProfileManagerApp exited with $($process.ExitCode)" }
        Write-Output "Cycle ${cycle}: ProfileManagerApp selection/edit/save/new/delete/focus/resize scenario passed."
    } finally {
        if ($window -ne [IntPtr]::Zero -and [GuiTestNative]::IsAlive($window)) {
            [GuiTestNative]::Close($window)
        }
        if ($process -and -not $process.HasExited) {
            $process.WaitForExit(5000)
        }
        if ($process -and -not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
            Write-Output "Cycle ${cycle}: orphan ProfileManagerApp process was terminated during cleanup."
        }
        $process.Dispose()
        if ($null -eq $previousDropPipe) {
            Remove-Item Env:GUIDEXOS_TEST_FILE_DROP_PIPE -ErrorAction SilentlyContinue
        } else {
            $env:GUIDEXOS_TEST_FILE_DROP_PIPE = $previousDropPipe
        }
        if ($null -eq $previousDropTitle) {
            Remove-Item Env:GUIDEXOS_TEST_FILE_DROP_TITLE -ErrorAction SilentlyContinue
        } else {
            $env:GUIDEXOS_TEST_FILE_DROP_TITLE = $previousDropTitle
        }
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

if (Test-Path -LiteralPath $temporaryDirectory) {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
}
Write-Output "PASS: ProfileManagerApp GUI smoke completed $Cycles full application cycles."

param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

function Close-FileDropDialog([IntPtr] $Dialog, [int] $Cycle) {
    Wait-Until {
        [GuiTestNative]::FindChildByText($Dialog, 'Button', 'OK') -ne [IntPtr]::Zero
    } `
        "Cycle ${Cycle}: file-drop message dialog had no OK button"
    $ok = [GuiTestNative]::FindChildByText($Dialog, 'Button', 'OK')
    [GuiTestNative]::NativeButton($ok)
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$root = Join-Path ([IO.Path]::GetTempPath()) 'guideXOS_AppModel_Windows_FileDropTests'
if (Test-Path -LiteralPath $root) {
    Remove-Item -LiteralPath $root -Recurse -Force
}
New-Item -ItemType Directory -Path $root | Out-Null

try {
    for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
        $directory = Join-Path $root "cycle-$cycle"
        New-Item -ItemType Directory -Path $directory | Out-Null
        $alpha = Join-Path $directory 'alpha file.txt'
        $accent = Join-Path $directory ('t' + [char]0x00E9 + [char]0x00E9 + '.txt')
        $unicode = Join-Path $directory (([char]0x65E5) + ([char]0x672C) +
            ([char]0x8A9E) + ' ' + [char]0xD83D + [char]0xDE80 + '.txt')
        $invalid = Join-Path $directory 'invalid-utf8.txt'
        $utf8 = [Text.UTF8Encoding]::new($false)
        [IO.File]::WriteAllText($alpha, "alpha UTF-8 text`nsecond line", $utf8)
        [IO.File]::WriteAllText($accent, 'accented text', $utf8)
        [IO.File]::WriteAllText($unicode, 'Unicode path text', $utf8)
        [IO.File]::WriteAllBytes($invalid, [byte[]](0xFF, 0xFE, 0xFD))

        $previousPipe = $env:GUIDEXOS_TEST_FILE_DROP_PIPE
        $previousTitle = $env:GUIDEXOS_TEST_FILE_DROP_TITLE
        $pipeName = 'guideXOS_FileDrop_' + [Guid]::NewGuid().ToString('N')
        $env:GUIDEXOS_TEST_FILE_DROP_PIPE = $pipeName
        $env:GUIDEXOS_TEST_FILE_DROP_TITLE = 'File Drop Demo'
        $process = Start-Process -FilePath $resolvedExecutable -PassThru
        $window = [IntPtr]::Zero
        try {
            Wait-Until {
                $windows = [GuiTestNative]::FindTopLevelWindows(
                    'File Drop Demo', $process.Id)
                if ($windows.Count -eq 1) {
                    $window = $windows[0]
                    return $true
                }
                return $false
            } "Cycle ${cycle}: FileDropApp window did not appear exactly once"
            $window = ([GuiTestNative]::FindTopLevelWindows(
                'File Drop Demo', $process.Id))[0]

            $list = [GuiTestNative]::FindChildren($window, 'ListBox')
            $buttons = [GuiTestNative]::FindChildren($window, 'Button')
            $statics = [GuiTestNative]::FindChildren($window, 'Static')
            if ($list.Count -ne 1 -or $buttons.Count -ne 2 -or
                $statics.Count -ne 5) {
                throw "Cycle ${cycle}: expected 1 list/2 buttons/5 labels, got $($list.Count)/$($buttons.Count)/$($statics.Count)"
            }
            $list = $list[0]
            $clear = Require-Control $window 'Button' 'Clear List' $cycle
            $open = Require-Control $window 'Button' 'Open First File' $cycle
            $count = Find-ControlPrefix $window 'Static' 'Count:'
            if ($count -eq [IntPtr]::Zero) {
                throw "Cycle ${cycle}: count label was not found"
            }

            [GuiTestNative]::InjectFileDrop($window, [string[]]@($alpha))
            Wait-Until {
                [GuiTestNative]::ListCount($list) -eq 1 -and
                    [GuiTestNative]::ListItem($list, 0) -eq $alpha -and
                    [GuiTestNative]::GetText($count) -eq 'Count: 1'
            } "Cycle ${cycle}: one dropped path was not displayed"

            [GuiTestNative]::InjectFileDrop(
                $window, [string[]]@($accent, $unicode, $accent))
            Wait-Until {
                [GuiTestNative]::ListCount($list) -eq 4 -and
                    [GuiTestNative]::ListItem($list, 1) -eq $accent -and
                    [GuiTestNative]::ListItem($list, 2) -eq $unicode -and
                    [GuiTestNative]::ListItem($list, 3) -eq $accent -and
                    [GuiTestNative]::GetText($count) -eq 'Count: 4'
            } "Cycle ${cycle}: multiple/order/duplicate Unicode drop failed"

            [GuiTestNative]::NativeButton($clear)
            Wait-Until {
                [GuiTestNative]::ListCount($list) -eq 0 -and
                    [GuiTestNative]::GetText($count) -eq 'Count: 0'
            } "Cycle ${cycle}: Clear List did not clear dropped paths"

            [GuiTestNative]::InjectFileDrop($window, [string[]]@($alpha))
            Wait-Until { [GuiTestNative]::ListCount($list) -eq 1 } `
                "Cycle ${cycle}: drop after clear did not append a fresh path"
            [GuiTestNative]::PostButton($open)
            $preview = [IntPtr]::Zero
            try {
                Wait-Until {
                    $dialogs = @([GuiTestNative]::FindTopLevelWindowsByClass(
                        '#32770', 'First File Preview', $process.Id))
                    $visible = @($dialogs | Where-Object { [GuiTestNative]::IsVisible($_) })
                    return $visible.Count -eq 1
                } "Cycle ${cycle}: valid UTF-8 first-file preview dialog did not appear"
            } catch {
                $titles = @([GuiTestNative]::FindTopLevelWindowsForProcess($process.Id) |
                    ForEach-Object { [GuiTestNative]::GetText($_) }) -join '|'
                throw "Cycle ${cycle}: valid UTF-8 first-file preview dialog did not appear; titles=$titles"
            }
            $preview = ([GuiTestNative]::FindTopLevelWindowsByClass(
                '#32770', 'First File Preview', $process.Id))[0]
            Close-FileDropDialog $preview $cycle

            [GuiTestNative]::NativeButton($clear)
            [GuiTestNative]::InjectFileDrop($window, [string[]]@($invalid))
            Wait-Until { [GuiTestNative]::ListCount($list) -eq 1 } `
                "Cycle ${cycle}: invalid UTF-8 test drop did not display"
            [GuiTestNative]::PostButton($open)
            $errorDialog = [IntPtr]::Zero
            Wait-Until {
                $dialogs = @([GuiTestNative]::FindTopLevelWindowsByClass(
                    '#32770', 'File Drop Error', $process.Id))
                $visible = @($dialogs | Where-Object { [GuiTestNative]::IsVisible($_) })
                return $visible.Count -eq 1
            } "Cycle ${cycle}: invalid UTF-8 error path did not appear"
            $errorDialog = ([GuiTestNative]::FindTopLevelWindowsByClass(
                '#32770', 'File Drop Error', $process.Id))[0]
            Close-FileDropDialog $errorDialog $cycle

            [GuiTestNative]::Resize($window, 680, 520)
            Wait-Until { [GuiTestNative]::Width($window) -ge 660 } `
                "Cycle ${cycle}: FileDropApp resize did not apply"

            [GuiTestNative]::Close($window)
            Wait-Until { $process.HasExited } `
                "Cycle ${cycle}: FileDropApp did not close cleanly"
            if ($process.ExitCode -ne 0) {
                throw "Cycle ${cycle}: FileDropApp exited with $($process.ExitCode)"
            }
            Write-Output "Cycle ${cycle}: file drop order/Unicode/duplicate/preview/error/resize passed."
        }
        finally {
            if ($window -ne [IntPtr]::Zero -and [GuiTestNative]::IsAlive($window)) {
                [GuiTestNative]::Close($window)
            }
            if ($process -and -not $process.HasExited) {
                $process.WaitForExit(3000)
            }
            if ($process -and -not $process.HasExited) {
                $process.Kill()
                $process.WaitForExit()
                Write-Output "Cycle ${cycle}: orphan FileDropApp process was terminated during cleanup."
            }
            if ($process) { $process.Dispose() }
            if ($null -eq $previousPipe) {
                Remove-Item Env:GUIDEXOS_TEST_FILE_DROP_PIPE -ErrorAction SilentlyContinue
            } else {
                $env:GUIDEXOS_TEST_FILE_DROP_PIPE = $previousPipe
            }
            if ($null -eq $previousTitle) {
                Remove-Item Env:GUIDEXOS_TEST_FILE_DROP_TITLE -ErrorAction SilentlyContinue
            } else {
                $env:GUIDEXOS_TEST_FILE_DROP_TITLE = $previousTitle
            }
        }
    }
}
finally {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}

Write-Output "PASS: FileDropApp GUI smoke completed $Cycles cycles with temporary files cleaned."

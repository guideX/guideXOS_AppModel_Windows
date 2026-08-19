param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int] $Cycles = 5
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'gui_test_helpers.ps1')

$unicodeText = [GuiTestNative]::Cafe() + ' / ' +
    [GuiTestNative]::Japanese() + ' / ' + [GuiTestNative]::Rocket()

for ($cycle = 1; $cycle -le $Cycles; ++$cycle) {
    $process = $null
    $window = [IntPtr]::Zero
    try {
        $process = Start-Process -FilePath $Executable -PassThru
        Wait-Until { $process.Refresh(); -not $process.HasExited } `
            "Cycle ${cycle}: ClipboardApp exited during startup"
        Wait-Until {
            $candidates = [GuiTestNative]::FindTopLevelWindows(
                'guideXOS Clipboard Demo', $process.Id)
            if ($candidates.Count -eq 1) {
                $script:ClipboardSmokeWindow = $candidates[0]
                return $true
            }
            return $false
        } "Cycle ${cycle}: ClipboardApp window was not found"
        $window = $script:ClipboardSmokeWindow

        $inputBox = Require-Control $window 'Edit' 'Initial text' $cycle
        $copy = Require-Control $window 'Button' 'Copy Input' $cycle
        $paste = Require-Control $window 'Button' 'Paste To Input' $cycle
        $clear = Require-Control $window 'Button' 'Clear Clipboard' $cycle
        $read = Require-Control $window 'Button' 'Read Clipboard' $cycle
        $clipboard = Find-ControlPrefix $window 'Static' 'Clipboard:'
        if ($clipboard -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: clipboard status label was not found"
        }
        Start-GuiInputPolicy $window $inputBox $cycle | Out-Null

        # Establish deterministic initial state. Clear intentionally affects
        # all native clipboard formats under the public Clear contract.
        if ([GuiTestNative]::IsEnabled($clear)) { Invoke-GuiButton $clear }
        Wait-Until { [GuiTestNative]::GetText($clipboard) -eq 'Clipboard: no supported text' } `
            "Cycle ${cycle}: initial clipboard clear did not update the status"

        $ascii = 'Hello from ClipboardApp'
        Send-GuiText $window $inputBox $ascii
        Invoke-GuiButton $copy
        Wait-Until { [GuiTestNative]::GetText($clipboard) -eq ('Clipboard: ' + $ascii) } `
            "Cycle ${cycle}: Copy Input did not publish exact ASCII text"
        Invoke-GuiButton $read
        Wait-Until { [GuiTestNative]::GetText($clipboard) -eq ('Clipboard: ' + $ascii) } `
            "Cycle ${cycle}: Read Clipboard did not display exact ASCII text"

        Send-GuiText $window $inputBox 'temporary replacement'
        Invoke-GuiButton $paste
        Wait-Until { [GuiTestNative]::GetText($inputBox) -eq $ascii } `
            "Cycle ${cycle}: Paste To Input did not reproduce exact text"

        Send-GuiText $window $inputBox 'replacement clipboard value'
        Invoke-GuiButton $copy
        Wait-Until {
            [GuiTestNative]::GetText($clipboard) -eq 'Clipboard: replacement clipboard value'
        } "Cycle ${cycle}: replacing clipboard text failed"

        Send-GuiText $window $inputBox $unicodeText
        $root = [GuiTestNative]::MenuBar($window)
        $edit = [GuiTestNative]::FindSubMenuByText($root, 'Edit')
        if ($edit -eq [IntPtr]::Zero) {
            throw "Cycle ${cycle}: Edit menu was not realized"
        }
        $copyCommand = [GuiTestNative]::FindMenuCommandByText($edit, 'Copy Input')
        $pasteCommand = [GuiTestNative]::FindMenuCommandByText($edit, 'Paste To Input')
        $readCommand = [GuiTestNative]::FindMenuCommandByText($edit, 'Read Clipboard')
        $clearCommand = [GuiTestNative]::FindMenuCommandByText($edit, 'Clear Clipboard')
        if ($copyCommand -lt 0 -or $pasteCommand -lt 0 -or
            $readCommand -lt 0 -or $clearCommand -lt 0) {
            throw "Cycle ${cycle}: Edit menu command discovery failed"
        }

        [GuiTestNative]::InvokeMenuCommand($window, $copyCommand)
        Wait-Until { [GuiTestNative]::GetText($clipboard) -eq ('Clipboard: ' + $unicodeText) } `
            "Cycle ${cycle}: menu Copy Input did not preserve Unicode/emoji text"
        $root = [GuiTestNative]::MenuBar($window)
        $edit = [GuiTestNative]::FindSubMenuByText($root, 'Edit')
        $readCommand = [GuiTestNative]::FindMenuCommandByText($edit, 'Read Clipboard')
        [GuiTestNative]::InvokeMenuCommand($window, $readCommand)
        Wait-Until { [GuiTestNative]::GetText($clipboard) -eq ('Clipboard: ' + $unicodeText) } `
            "Cycle ${cycle}: menu Read Clipboard did not preserve Unicode/emoji text"

        Send-GuiText $window $inputBox 'menu paste target'
        $root = [GuiTestNative]::MenuBar($window)
        $edit = [GuiTestNative]::FindSubMenuByText($root, 'Edit')
        $pasteCommand = [GuiTestNative]::FindMenuCommandByText($edit, 'Paste To Input')
        [GuiTestNative]::InvokeMenuCommand($window, $pasteCommand)
        Wait-Until { [GuiTestNative]::GetText($inputBox) -eq $unicodeText } `
            "Cycle ${cycle}: menu Paste To Input did not reproduce Unicode/emoji text"

        Send-GuiText $window $inputBox ''
        Invoke-GuiButton $copy
        Wait-Until { [GuiTestNative]::GetText($clipboard) -eq 'Clipboard: <empty>' } `
            "Cycle ${cycle}: empty clipboard text was not preserved as text"
        Invoke-GuiButton $paste
        Wait-Until { [GuiTestNative]::GetText($inputBox) -eq '' } `
            "Cycle ${cycle}: empty clipboard text did not paste"

        $root = [GuiTestNative]::MenuBar($window)
        $edit = [GuiTestNative]::FindSubMenuByText($root, 'Edit')
        $clearCommand = [GuiTestNative]::FindMenuCommandByText($edit, 'Clear Clipboard')
        [GuiTestNative]::InvokeMenuCommand($window, $clearCommand)
        Wait-Until { [GuiTestNative]::GetText($clipboard) -eq 'Clipboard: no supported text' } `
            "Cycle ${cycle}: menu Clear Clipboard did not clear the clipboard"
        if ([GuiTestNative]::IsEnabled($paste) -or
            [GuiTestNative]::IsEnabled($read) -or
            [GuiTestNative]::IsEnabled($clear)) {
            throw "Cycle ${cycle}: text-dependent commands remained enabled after Clear"
        }

        [GuiTestNative]::Resize($window, 700, 380)
        Wait-Until {
            [GuiTestNative]::Width($window) -ge 680 -and
            [GuiTestNative]::Height($window) -ge 360
        } "Cycle ${cycle}: resize did not apply to ClipboardApp"
        if (-not [GuiTestNative]::IsVisible($inputBox) -or
            -not [GuiTestNative]::IsVisible($copy)) {
            throw "Cycle ${cycle}: ClipboardApp controls were not visible after resize"
        }

        [GuiTestNative]::Close($window)
        if (-not $process.WaitForExit(3000)) {
            throw "Cycle ${cycle}: ClipboardApp did not close normally"
        }
        Write-Output "Cycle ${cycle}: ClipboardApp text, Unicode, menu, clear, resize, and close scenario passed."
    } finally {
        Stop-GuiProcess $process $window
    }
}

$remaining = Get-Process -Name ClipboardApp -ErrorAction SilentlyContinue
if ($remaining) {
    $ids = ($remaining | ForEach-Object { $_.Id }) -join ', '
    throw "ClipboardApp processes remain after smoke test: $ids"
}

Write-Output "PASS: ClipboardApp GUI smoke completed $Cycles serial text clipboard cycles."

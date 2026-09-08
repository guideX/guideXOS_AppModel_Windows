# Shared GUI smoke-test policy and native helpers.
#
# The foreground path is probed once per cycle. Text correctness and all
# control-targeted input use synchronous Unicode Win32 messages, so smoke-test
# correctness never depends on the desktop foreground or keyboard layout.

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class GuiTestNative {
    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    private struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)]
    private struct SCROLLINFO {
        public uint cbSize, fMask; public int nMin, nMax; public uint nPage;
        public int nPos, nTrackPos;
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct GUITHREADINFO {
        public int cbSize; public int flags; public IntPtr hwndActive, hwndFocus;
        public IntPtr hwndCapture, hwndMenuOwner, hwndMoveSize, hwndCaret;
        public RECT rcCaret;
    }
    private const uint WM_CLOSE = 0x0010;
    private const uint WM_SETTEXT = 0x000C;
    private const uint WM_GETTEXT = 0x000D;
    private const uint WM_KEYDOWN = 0x0100;
    private const uint WM_KEYUP = 0x0101;
    private const uint WM_SYSKEYDOWN = 0x0104;
    private const uint WM_SYSKEYUP = 0x0105;
    private const uint WM_CHAR = 0x0102;
    private const uint WM_COMMAND = 0x0111;
    private const uint WM_ESCAPE = 0x001B;
    private const uint EM_SETSEL = 0x00B1;
    private const uint BM_CLICK = 0x00F5;
    private const uint SW_SHOWNORMAL = 5;
    private const uint SWP_NOSENDCHANGING = 0x0400;
    private const uint SWP_NOZORDER = 0x0004;
    private const uint SWP_NOACTIVATE = 0x0010;
    private const uint MF_BYPOSITION = 0x0400;
    private const uint MF_DISABLED = 0x0002;
    private const uint MF_GRAYED = 0x0001;
    private const uint MF_CHECKED = 0x0008;
    private const uint IDCANCEL = 2;

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr hwnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")]
    private static extern bool IsWindow(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern bool IsWindowEnabled(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")]
    private static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")]
    private static extern bool ScreenToClient(IntPtr hwnd, ref POINT point);
    [DllImport("user32.dll")]
    private static extern bool SetWindowPos(IntPtr hwnd, IntPtr insertAfter, int x, int y,
                                            int width, int height, uint flags);
    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr hwnd, int command);
    [DllImport("user32.dll")]
    private static extern bool BringWindowToTop(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")]
    private static extern IntPtr SetActiveWindow(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern bool GetGUIThreadInfo(uint threadId, ref GUITHREADINFO info);
    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")]
    private static extern bool AttachThreadInput(uint source, uint target, bool attach);
    [DllImport("user32.dll")]
    private static extern IntPtr GetNextDlgTabItem(IntPtr parent, IntPtr child, bool previous);
    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    private static extern int GetScrollInfo(IntPtr hwnd, int bar, ref SCROLLINFO info);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wParam, StringBuilder lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wParam, string lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern bool SetWindowText(IntPtr hwnd, string text);
    [DllImport("user32.dll")]
    private static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    private static extern IntPtr GetMenu(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern IntPtr GetSubMenu(IntPtr menu, int position);
    [DllImport("user32.dll")]
    private static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetMenuString(IntPtr menu, uint item,
                                            StringBuilder text, int maxCount,
                                            uint flags);
    [DllImport("user32.dll")]
    private static extern uint GetMenuItemID(IntPtr menu, int position);
    [DllImport("user32.dll")]
    private static extern uint GetMenuState(IntPtr menu, uint item, uint flags);

    public static List<IntPtr> FindTopLevelWindows(string title, int processId) {
        var result = new List<IntPtr>();
        EnumWindows((hwnd, unused) => {
            uint candidatePid;
            GetWindowThreadProcessId(hwnd, out candidatePid);
            if (candidatePid == processId && GetText(hwnd) == title) result.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static List<IntPtr> FindTopLevelWindowsByClass(
        string className, string title, int processId) {
        var result = new List<IntPtr>();
        EnumWindows((hwnd, unused) => {
            uint candidatePid;
            GetWindowThreadProcessId(hwnd, out candidatePid);
            if (candidatePid == processId && GetClass(hwnd) == className &&
                (title == null || GetText(hwnd) == title)) result.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static List<IntPtr> FindTopLevelWindowsByClassAnyTitle(
        string className, int processId) {
        var result = new List<IntPtr>();
        EnumWindows((hwnd, unused) => {
            uint candidatePid;
            GetWindowThreadProcessId(hwnd, out candidatePid);
            if (candidatePid == processId && GetClass(hwnd) == className) {
                result.Add(hwnd);
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static List<IntPtr> FindTopLevelWindowsForProcess(int processId) {
        var result = new List<IntPtr>();
        EnumWindows((hwnd, unused) => {
            uint candidatePid;
            GetWindowThreadProcessId(hwnd, out candidatePid);
            if (candidatePid == processId) result.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static List<IntPtr> FindChildren(IntPtr parent, string className) {
        var result = new List<IntPtr>();
        EnumChildWindows(parent, (hwnd, unused) => {
            if (GetClass(hwnd) == className) result.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static string GetText(IntPtr hwnd) {
        var text = new StringBuilder(4096);
        SendMessage(hwnd, WM_GETTEXT, (IntPtr)text.Capacity, text);
        return text.ToString();
    }

    public static void SetTextValue(IntPtr hwnd, string value) {
        SendMessage(hwnd, WM_SETTEXT, IntPtr.Zero, value);
    }

    public static IntPtr FindChildByText(IntPtr parent, string className,
                                         string text) {
        foreach (var child in FindChildren(parent, className)) {
            var childText = GetText(child);
            var publicText = childText.Replace("&", "");
            if (childText == text || publicText == text) return child;
        }
        return IntPtr.Zero;
    }

    public static List<IntPtr> VisibleChildren(IntPtr parent, string className) {
        var result = new List<IntPtr>();
        foreach (var child in FindChildren(parent, className)) {
            if (IsVisible(child)) result.Add(child);
        }
        return result;
    }

    private static string GetClass(IntPtr hwnd) {
        var text = new StringBuilder(256);
        GetClassName(hwnd, text, text.Capacity);
        return text.ToString();
    }

    public static bool IsAlive(IntPtr hwnd) { return IsWindow(hwnd); }
    public static bool IsVisible(IntPtr hwnd) { return IsWindowVisible(hwnd); }
    public static bool IsEnabled(IntPtr hwnd) { return IsWindowEnabled(hwnd); }
    public static bool ForegroundIs(IntPtr hwnd) { return GetForegroundWindow() == hwnd; }
    public static IntPtr ForegroundWindow() { return GetForegroundWindow(); }

    public static IntPtr MenuBar(IntPtr window) { return GetMenu(window); }
    public static int MenuCount(IntPtr menu) {
        return menu == IntPtr.Zero ? 0 : GetMenuItemCount(menu);
    }
    public static IntPtr SubMenu(IntPtr menu, int position) {
        return GetSubMenu(menu, position);
    }
    public static string MenuText(IntPtr menu, int position) {
        var text = new StringBuilder(4096);
        GetMenuString(menu, (uint)position, text, text.Capacity, MF_BYPOSITION);
        return text.ToString();
    }
    public static int MenuCommand(IntPtr menu, int position) {
        return unchecked((int)GetMenuItemID(menu, position));
    }
    public static bool MenuItemEnabled(IntPtr menu, int position) {
        var state = GetMenuState(menu, (uint)position, MF_BYPOSITION);
        return (state & (MF_DISABLED | MF_GRAYED)) == 0;
    }
    public static bool MenuItemChecked(IntPtr menu, int position) {
        return (GetMenuState(menu, (uint)position, MF_BYPOSITION) & MF_CHECKED) != 0;
    }
    private static string PublicMenuText(string value) {
        var shortcut = value.IndexOf('\t');
        if (shortcut >= 0) value = value.Substring(0, shortcut);
        var result = new StringBuilder(value.Length);
        for (var index = 0; index < value.Length; ++index) {
            if (value[index] == '&' && index + 1 < value.Length &&
                value[index + 1] == '&') {
                result.Append('&');
                ++index;
            } else if (value[index] != '&') {
                result.Append(value[index]);
            }
        }
        return result.ToString();
    }
    public static int FindMenuPosition(IntPtr menu, string text, int occurrence = 0) {
        var seen = 0;
        for (var index = 0; index < MenuCount(menu); ++index) {
            if (PublicMenuText(MenuText(menu, index)) == text) {
                if (seen == occurrence) return index;
                ++seen;
            }
        }
        return -1;
    }
    public static IntPtr FindSubMenuByText(IntPtr menu, string text) {
        var position = FindMenuPosition(menu, text);
        return position < 0 ? IntPtr.Zero : SubMenu(menu, position);
    }
    public static int FindMenuCommandByText(IntPtr menu, string text,
                                            int occurrence = 0) {
        var position = FindMenuPosition(menu, text, occurrence);
        return position < 0 ? -1 : MenuCommand(menu, position);
    }
    public static void InvokeMenuCommand(IntPtr window, int commandId) {
        SendMessage(window, WM_COMMAND, (IntPtr)commandId, IntPtr.Zero);
    }
    public static void PostMenuCommand(IntPtr window, int commandId) {
        PostMessage(window, WM_COMMAND, (IntPtr)commandId, IntPtr.Zero);
    }
    public static void PostMnemonic(IntPtr window, ushort key) {
        PostMessage(window, WM_SYSKEYDOWN, (IntPtr)key,
                    new IntPtr(0x20000001));
    }
    public static void SendMnemonic(IntPtr window, ushort key) {
        SendMessage(window, WM_SYSKEYDOWN, (IntPtr)key,
                    new IntPtr(0x20000001));
    }
    public static void CancelMenu(IntPtr window) {
        SendMessage(window, WM_KEYDOWN, (IntPtr)WM_ESCAPE, IntPtr.Zero);
        SendMessage(window, WM_SYSKEYUP, IntPtr.Zero, IntPtr.Zero);
    }
    public static bool MenuIsActive(IntPtr parent) {
        uint processId;
        var thread = GetWindowThreadProcessId(parent, out processId);
        var info = new GUITHREADINFO { cbSize = Marshal.SizeOf(typeof(GUITHREADINFO)) };
        return GetGUIThreadInfo(thread, ref info) && info.hwndMenuOwner != IntPtr.Zero;
    }

    public static IntPtr FocusedControl(IntPtr parent) {
        uint processId;
        var thread = GetWindowThreadProcessId(parent, out processId);
        var info = new GUITHREADINFO { cbSize = Marshal.SizeOf(typeof(GUITHREADINFO)) };
        return GetGUIThreadInfo(thread, ref info) ? info.hwndFocus : IntPtr.Zero;
    }

    // One bounded activation attempt. SetFocus can succeed after thread input
    // attachment even when Windows refuses foreground ownership; callers must
    // check both the focused child and foreground window.
    public static bool TryForegroundFocus(IntPtr parent, IntPtr child) {
        if (!IsWindow(parent) || !IsWindow(child)) return false;
        uint processId;
        var targetThread = GetWindowThreadProcessId(parent, out processId);
        var currentThread = GetCurrentThreadId();
        var attached = currentThread != targetThread &&
                       AttachThreadInput(currentThread, targetThread, true);
        try {
            ShowWindow(parent, (int)SW_SHOWNORMAL);
            BringWindowToTop(parent);
            SetActiveWindow(parent);
            SetForegroundWindow(parent);
            SetFocus(child);
            return FocusedControl(parent) == child && ForegroundIs(parent);
        }
        finally {
            if (attached) AttachThreadInput(currentThread, targetThread, false);
        }
    }

    public static bool FocusNative(IntPtr parent, IntPtr child) {
        if (!IsWindow(parent) || !IsWindow(child)) return false;
        uint processId;
        var targetThread = GetWindowThreadProcessId(parent, out processId);
        var currentThread = GetCurrentThreadId();
        var attached = currentThread != targetThread &&
                       AttachThreadInput(currentThread, targetThread, true);
        try {
            SetFocus(child);
            return FocusedControl(parent) == child;
        }
        finally {
            if (attached) AttachThreadInput(currentThread, targetThread, false);
        }
    }

    public static IntPtr NextTab(IntPtr parent, IntPtr child, bool previous) {
        return GetNextDlgTabItem(parent, child, previous);
    }

    public static void NativeTab(IntPtr parent, IntPtr current, bool previous) {
        var next = NextTab(parent, current, previous);
        if (next == IntPtr.Zero || !FocusNative(parent, next))
            throw new InvalidOperationException("native tab focus failed");
    }

    public static void NativeSelectAll(IntPtr edit) {
        SendMessage(edit, EM_SETSEL, IntPtr.Zero, new IntPtr(-1));
    }

    public static void NativeSelectRange(IntPtr edit, int start, int end) {
        SendMessage(edit, EM_SETSEL, (IntPtr)start, (IntPtr)end);
    }

    // WM_SETTEXT carries the complete UTF-16 string, including surrogate
    // pairs, and does not depend on the active keyboard layout or foreground.
    public static void NativeText(IntPtr edit, string value) {
        // WM_CHAR keeps the TextBox EN_CHANGE callback path covered. The
        // complete WM_SETTEXT write then makes the final UTF-16 state exact,
        // including supplementary-plane characters.
        SendMessage(edit, EM_SETSEL, IntPtr.Zero, new IntPtr(-1));
        foreach (var character in value) {
            SendMessage(edit, WM_CHAR, (IntPtr)character, IntPtr.Zero);
        }
        SetWindowText(edit, value);
        SendMessage(edit, WM_SETTEXT, IntPtr.Zero, value);
    }

    public static void NativeAppendText(IntPtr edit, string value) {
        foreach (var character in value) {
            SendMessage(edit, WM_CHAR, (IntPtr)character, IntPtr.Zero);
        }
    }

    public static void NativeChar(IntPtr edit, ushort character) {
        SendMessage(edit, WM_CHAR, (IntPtr)character, IntPtr.Zero);
    }

    public static void NativeKey(IntPtr control, ushort key) {
        if (key == 0x08) {
            SendMessage(control, WM_CHAR, (IntPtr)key, IntPtr.Zero);
            return;
        }
        SendMessage(control, WM_KEYDOWN, (IntPtr)key, IntPtr.Zero);
        SendMessage(control, WM_KEYUP, (IntPtr)key, IntPtr.Zero);
    }

    public static void NativeButton(IntPtr button) {
        SendMessage(button, BM_CLICK, IntPtr.Zero, IntPtr.Zero);
    }

    public static void PostButton(IntPtr button) {
        PostMessage(button, BM_CLICK, IntPtr.Zero, IntPtr.Zero);
    }

    public static void NativeButtonKey(IntPtr button, ushort key) {
        NativeKey(button, key);
        NativeButton(button);
    }

    // The in-process test agent creates the same UTF-16 DROPFILES payload that
    // the Windows shell uses. A named pipe carries only UTF-8 test data here;
    // no native payload crosses a process boundary.
    public static void InjectFileDrop(IntPtr window, string[] paths) {
        if (paths == null || paths.Length == 0)
            throw new ArgumentException("at least one path is required");
        var pipe = Environment.GetEnvironmentVariable("GUIDEXOS_TEST_FILE_DROP_PIPE");
        if (String.IsNullOrEmpty(pipe))
            throw new InvalidOperationException("file-drop test pipe is not configured");
        using (var client = new System.IO.Pipes.NamedPipeClientStream(
                   ".", pipe, System.IO.Pipes.PipeDirection.Out)) {
            client.Connect(5000);
            using (var writer = new System.IO.StreamWriter(
                       client, new UTF8Encoding(false), 1024, true)) {
                writer.NewLine = "\n";
                writer.WriteLine(paths.Length);
                foreach (var path in paths) writer.WriteLine(path);
                writer.Flush();
            }
        }
    }

    public static void ClickControl(IntPtr control) {
        SendMessage(control, BM_CLICK, IntPtr.Zero, IntPtr.Zero);
    }

    public static int Width(IntPtr hwnd) {
        RECT rect; if (!GetWindowRect(hwnd, out rect)) return 0;
        return rect.Right - rect.Left;
    }
    public static int Height(IntPtr hwnd) {
        RECT rect; if (!GetWindowRect(hwnd, out rect)) return 0;
        return rect.Bottom - rect.Top;
    }
    public static int VerticalScrollPosition(IntPtr hwnd) {
        var info = new SCROLLINFO { cbSize = (uint)Marshal.SizeOf<SCROLLINFO>(), fMask = 0x17 };
        GetScrollInfo(hwnd, 1, ref info);
        return info.nPos;
    }
    public static int VerticalScrollMaximum(IntPtr hwnd) {
        var info = new SCROLLINFO { cbSize = (uint)Marshal.SizeOf<SCROLLINFO>(), fMask = 0x17 };
        GetScrollInfo(hwnd, 1, ref info);
        return Math.Max(0, info.nMax - (int)info.nPage + 1);
    }
    public static void MouseWheel(IntPtr hwnd, int delta) {
        var wheel = ((long)(ushort)delta) << 16;
        SendMessage(hwnd, 0x020A, (IntPtr)wheel, IntPtr.Zero);
    }
    public static int[] ClientSize(IntPtr hwnd) {
        RECT rect; if (!GetClientRect(hwnd, out rect))
            throw new InvalidOperationException("GetClientRect failed");
        return new[] { rect.Right - rect.Left, rect.Bottom - rect.Top };
    }
    public static int[] ChildPosition(IntPtr parent, IntPtr child) {
        RECT rect; if (!GetWindowRect(child, out rect))
            throw new InvalidOperationException("GetWindowRect failed");
        var point = new POINT { X = rect.Left, Y = rect.Top };
        if (!ScreenToClient(parent, ref point))
            throw new InvalidOperationException("ScreenToClient failed");
        return new[] { point.X, point.Y, rect.Right - rect.Left, rect.Bottom - rect.Top };
    }
    public static void Resize(IntPtr hwnd, int width, int height) {
        SetWindowPos(hwnd, IntPtr.Zero, 0, 0, width, height,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
    }

    // App-specific native observations live here so every smoke script uses
    // the same cross-process Unicode/message plumbing.
    public static int ListCount(IntPtr list) {
        return SendMessage(list, 0x018B, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }
    public static int SelectedIndex(IntPtr list) {
        return SendMessage(list, 0x0188, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }
    public static string ListItem(IntPtr list, int index) {
        var text = new StringBuilder(4096);
        SendMessage(list, 0x0189, (IntPtr)index, text);
        return text.ToString();
    }
    public static int ItemHeight(IntPtr list) {
        return Math.Max(1, SendMessage(list, 0x01A1, IntPtr.Zero, IntPtr.Zero).ToInt32());
    }
    public static void NativeListSelect(IntPtr parent, IntPtr list, int index) {
        SendMessage(list, 0x0186, (IntPtr)index, IntPtr.Zero);
        SendMessage(parent, 0x0111, (IntPtr)(1 << 16), list);
    }
    public static int CheckState(IntPtr control) {
        return SendMessage(control, 0x00F0, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }
    public static void NativeChoice(IntPtr parent, IntPtr control, int state) {
        SendMessage(control, 0x00F1, (IntPtr)state, IntPtr.Zero);
        var id = GetWindowLongId(control);
        SendMessage(parent, 0x0111, (IntPtr)id, control);
    }
    public static void PostChoice(IntPtr parent, IntPtr control, int state) {
        PostMessage(control, 0x00F1, (IntPtr)state, IntPtr.Zero);
        var id = GetWindowLongId(control);
        PostMessage(parent, 0x0111, (IntPtr)id, control);
    }
    [DllImport("user32.dll")]
    private static extern int GetDlgCtrlID(IntPtr hwnd);
    private static int GetWindowLongId(IntPtr hwnd) { return GetDlgCtrlID(hwnd); }

    public static int Count(IntPtr combo) {
        return SendMessage(combo, 0x0146, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }
    public static int ComboSelectedIndex(IntPtr combo) {
        return SendMessage(combo, 0x0147, IntPtr.Zero, IntPtr.Zero).ToInt32();
    }
    public static string Item(IntPtr combo, int index) {
        var text = new StringBuilder(4096);
        SendMessage(combo, 0x0148, (IntPtr)index, text);
        return text.ToString();
    }
    public static bool Dropped(IntPtr combo) {
        return SendMessage(combo, 0x0157, IntPtr.Zero, IntPtr.Zero).ToInt32() != 0;
    }
    public static void Open(IntPtr combo, bool open) {
        SendMessage(combo, 0x014F, open ? new IntPtr(1) : IntPtr.Zero, IntPtr.Zero);
    }
    public static void NativeComboSelect(IntPtr parent, IntPtr combo, int index) {
        SendMessage(combo, 0x014E, (IntPtr)index, IntPtr.Zero);
        SendMessage(parent, 0x0111, (IntPtr)(1 << 16), combo);
    }
    public static void PostComboSelect(IntPtr parent, IntPtr combo, int index) {
        SendMessage(combo, 0x014E, (IntPtr)index, IntPtr.Zero);
        PostMessage(parent, 0x0111, (IntPtr)(1 << 16), combo);
    }
    public static void NativeComboKey(IntPtr parent, IntPtr combo, ushort key) {
        SendMessage(combo, WM_KEYDOWN, (IntPtr)key, IntPtr.Zero);
        SendMessage(combo, WM_KEYUP, (IntPtr)key, IntPtr.Zero);
        SendMessage(parent, 0x0111, (IntPtr)(1 << 16), combo);
    }

    public static string Cafe() { return "caf\u00E9.txt"; }
    public static string Japanese() { return "\u65E5\u672C\u8A9E"; }
    public static string Rocket() { return "rocket \U0001F680"; }
    public static string Spanish() { return "Espa" + new string(new[] { '\u00F1' }) + "ol"; }
    public static string UnicodeName() { return "Zo\u00EB \uD83D\uDE80"; }
    public static string UnicodeMessage() { return "\u3053\u3093\u306B\u3061\u306F \u4E16\u754C"; }
    public static string EmDash() { return "\u2014"; }

    public static void Close(IntPtr hwnd) {
        PostMessage(hwnd, WM_CLOSE, IntPtr.Zero, IntPtr.Zero);
    }

    public static void CancelNativeDialog(IntPtr hwnd) {
        SendMessage(hwnd, WM_COMMAND, (IntPtr)IDCANCEL, IntPtr.Zero);
    }
}
'@

function Wait-Until([scriptblock] $Condition, [string] $FailureMessage, [int] $TimeoutMs = 5000) {
    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 50
    }
    throw $FailureMessage
}

function Get-TestStatusText([string] $Path) {
    try {
        if (-not (Test-Path -LiteralPath $Path)) { return '' }
        return [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
    } catch [IO.IOException] {
        return ''
    }
}

function Find-Control([IntPtr] $Window, [string] $ClassName, [string] $Text) {
    foreach ($control in [GuiTestNative]::FindChildren($Window, $ClassName)) {
        if ([GuiTestNative]::GetText($control) -eq $Text) { return $control }
    }
    return [IntPtr]::Zero
}

function Find-ControlPrefix([IntPtr] $Window, [string] $ClassName, [string] $Prefix) {
    foreach ($control in [GuiTestNative]::FindChildren($Window, $ClassName)) {
        if ([GuiTestNative]::GetText($control).StartsWith($Prefix, [StringComparison]::Ordinal)) {
            return $control
        }
    }
    return [IntPtr]::Zero
}

function Require-Control([IntPtr] $Window, [string] $ClassName, [string] $Text, [int] $Cycle) {
    Wait-Until { return (Find-Control $Window $ClassName $Text) -ne [IntPtr]::Zero } `
        "Cycle ${Cycle}: missing $ClassName control '$Text'"
    $control = Find-Control $Window $ClassName $Text
    if (-not [GuiTestNative]::IsVisible($control)) {
        throw "Cycle ${Cycle}: control '$Text' is not visible"
    }
    return $control
}

function Start-GuiInputPolicy([IntPtr] $Window, [IntPtr] $InitialControl, [int] $Cycle) {
    if ([GuiTestNative]::TryForegroundFocus($Window, $InitialControl)) {
        Write-Output "Cycle ${Cycle}: foreground activation was available; deterministic native-message input is authoritative."
    } else {
        Write-Output "Cycle ${Cycle}: foreground activation unavailable; using deterministic native-message input."
    }
}

function Focus-GuiControl([IntPtr] $Window, [IntPtr] $Control) {
    if ([GuiTestNative]::FocusNative($Window, $Control)) { return $true }
    return $false
}

function Send-GuiText([IntPtr] $Window, [IntPtr] $Control, [string] $Text) {
    # Text correctness is always native-message authoritative. This avoids
    # the known case where foreground SendInput reports success but leaves a
    # partial edit in a different foreground target.
    if (-not [GuiTestNative]::FocusNative($Window, $Control)) {
        throw "native focus failed before text injection"
    }
    [GuiTestNative]::NativeText($Control, $Text)
    if ([GuiTestNative]::GetText($Control) -ne $Text) {
        throw "native Unicode text injection did not reach the target control"
    }
}

function Send-GuiSelectAll([IntPtr] $Window, [IntPtr] $Control) {
    if (-not [GuiTestNative]::FocusNative($Window, $Control)) {
        throw "native focus failed before select-all"
    }
    [GuiTestNative]::NativeSelectAll($Control)
}

function Send-GuiKey([IntPtr] $Window, [IntPtr] $Control, [uint16] $Key) {
    # Key messages target the control directly. This avoids keyboard-layout
    # conversion and keeps key-state assertions independent of the desktop.
    if (-not [GuiTestNative]::FocusNative($Window, $Control)) {
        throw "native focus failed before key injection"
    }
    [GuiTestNative]::NativeKey($Control, $Key)
}

function Send-GuiTab([IntPtr] $Window, [IntPtr] $Current, [bool] $Previous = $false) {
    [GuiTestNative]::NativeTab($Window, $Current, $Previous)
}

function Send-GuiButtonKey([IntPtr] $Window, [IntPtr] $Control, [uint16] $Key) {
    if (-not [GuiTestNative]::FocusNative($Window, $Control)) {
        throw "native focus failed before button key injection"
    }
    [GuiTestNative]::NativeButtonKey($Control, $Key)
}

function Invoke-GuiButton([IntPtr] $Button) {
    [GuiTestNative]::NativeButton($Button)
}

function Stop-GuiProcess([System.Diagnostics.Process] $Process, [IntPtr] $Window) {
    if ($Process -and -not $Process.HasExited) {
        if ($Window -ne [IntPtr]::Zero -and [GuiTestNative]::IsAlive($Window)) {
            [GuiTestNative]::Close($Window)
        }
        if (-not $Process.WaitForExit(3000)) {
            $Process.Kill()
            $Process.WaitForExit()
        }
    }
}

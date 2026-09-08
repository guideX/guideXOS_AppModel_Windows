# guideXOS App Model for Windows

This repository is a small native Windows backend for the guideXOS App Model. Application code creates `Application`, `Window`, `Label`, `Button`, single-line `TextBox` controls, multiline `TextArea` controls, `ListBox`, `ComboBox`, `CheckBox`, `RadioButton`, `RadioGroup`, `ProgressBar`, horizontal `Slider`, portable file-backed `Image` controls, vertical `ScrollView` viewports, `TabView` pages, `Layout`, `MenuBar`, `Menu`, `MenuItem`, repeating `Timer`s, and process-wide text `Clipboard` access through a public C++ API. The backend realizes them with native desktop windows, controls, menus, timers, decoded raster surfaces, and the Windows Unicode clipboard while keeping platform handles and messages private.

## Current milestone

The current vertical slice demonstrates:

- two independent App Model windows in one `Application`
- a secondary window opened from a button event
- controls dispatching to the correct window model
- independent close, reopen, and destruction order
- mutable labels updated across windows
- deterministic callback cleanup and reentrant lifecycle operations
- an explicit, public application shutdown policy
- single-line editable text boxes with Unicode text and text-change callbacks
- portable multiline text areas with normalized newlines, selection, clipboard commands, read-only mode, scrolling, and optional word wrapping
- native keyboard editing, focus, and creation-order Tab navigation
- dynamic single-selection list boxes with UTF-8 items and selection callbacks
- non-editable single-selection combo boxes with UTF-8 items, drop-down interaction, and selection callbacks
- native-backed checkboxes and explicitly grouped radio buttons
- portable determinate and indeterminate progress bars with clamped integer ranges
- portable horizontal sliders with signed clamped integer ranges and user-change callbacks
- portable file-backed raster Images with PNG/JPEG decoding, alpha, and Fit/Fill/Stretch modes
- portable TabView pages with nested layouts, selected-page callbacks, and preserved page state
- portable vertical ScrollView viewports with independent content extent, native clipping, wheel input, and scrollbar synchronization
- deterministic choice callbacks across reentrancy, multiple windows, and close/reopen
- nested vertical and horizontal layouts with natural, expanding, and spacer items
- platform-neutral spacing, padding, geometry calculation, and deterministic resize behavior
- explicit natural-size and minimum-size measurement for controls and nested layouts
- native Windows text/font measurement kept behind the backend boundary
- platform-neutral hierarchical menu bars with duplicate-safe command identity
- enabled/checked menu state, UTF-8 labels, native mnemonics, and small Ctrl-letter shortcuts
- per-window native menu routing that survives close/reopen and reentrant callbacks
- synchronous native `MessageDialog`, `OpenFileDialog`, and `SaveFileDialog` foundations
- bounded UTF-8 whole-file read/write support with safe replacement
- cancelable, platform-neutral window close requests shared by native and programmatic close
- a process-wide, text-only UTF-8 Clipboard API with bounded Windows Unicode conversion
- platform-neutral TextBox and TextArea scalar caret/selection queries and Copy/Cut/Paste/Delete commands
- platform-neutral window-scoped focus identity, programmatic control focus, and routed text-edit commands
- window-level shell file drops with bounded, ordered UTF-8 filesystem paths
- shared UTF-8 ToolTips for controls with safe live registration and close/reopen rebuilding
- window-level native-backed StatusBar chrome with dynamic UTF-8 text and content-size preservation
- repeating application timers dispatched on the App Model event-loop thread

`ProfileManagerApp` is the first cohesive example application. It manages a
profile collection through a small application-level document/controller and
demonstrates selection, editable fields, pending changes, enabled state, radio
modes, Unicode, duplicate display names, document New/Open/Save/Save As, dirty
state, safe file replacement, malformed-file errors, and deterministic
selection behavior.

`HelloApp` remains the small one-window sample. `MultiWindowApp` is the focused lifecycle sample.
`TextInputApp` is the focused editable-input sample.
`MultilineTextApp` is the focused multiline editing, read-only, clipboard, and
resize sample.
`ListBoxApp` is the focused collection and selection sample.
`ChoiceControlsApp` is the focused checkbox and radio-group sample.
`ComboBoxApp` is the focused non-editable drop-down, mutation, and measurement sample.
`ProgressBarApp` is the focused determinate/indeterminate progress sample; it
uses the portable `Timer` to drive dynamic updates.
`SliderApp` is the focused horizontal Slider sample; it composes a Slider,
ProgressBar, Label, Button, and Layout without native application code.
`MenuApp` is the focused hierarchical menu, command, shortcut, mnemonic, and
window-chrome sample. `DialogApp` is the focused native message-dialog and
single-file picker sample; it does not read or write files. `MultiWindowApp` also includes identical per-window
Actions menus to exercise cross-window routing. `ClipboardApp` is the focused
text clipboard sample; it demonstrates programmatic copy/read/paste/clear
commands. `TextEditingApp` is the focused selection, caret, Unicode, control-
level clipboard command, menu-routing, and close/reopen sample.
`FocusCommandApp` is the focused platform-neutral focus identity, routed Edit
menu, multi-window, and close/reopen sample. `ProfileManagerApp` uses the same
focus target for its Name and Description edit commands.
`FileDropApp` is the focused shell filesystem-drop sample; it appends dropped
paths, preserves order and duplicates, previews a bounded UTF-8 text file, and
reports invalid-file errors without executing or opening anything through the
shell.
`PolishApp` is the focused ToolTip, StatusBar, nested-form, and resize sample.
`TimerApp` is the focused repeating timer, stop/restart, and explicit-shutdown
sample.
`TabViewApp` is the focused multi-page settings sample; it composes nested page
layouts from existing controls and demonstrates state preservation while
switching pages. `ImageApp` is the focused portable raster-image sample; it
loads deterministic PNG/JPEG fixtures, changes scale mode, clears/replaces its
source, and uses the same Image inside ordinary layouts.
`ScrollViewApp` is the focused scrollable-settings sample; it nests a
ScrollView inside a TabView page, composes existing controls and an Image,
and intentionally gives the content a larger extent than its viewport.

## Composable layouts

`Layout` is a small tree of controls, child layouts, and optional spacers. Its
direction is fixed at construction with `Orientation::Vertical` or
`Orientation::Horizontal`; the existing default is a padded vertical stack.
Each item is `LayoutSizing::Natural` by default or can be marked
`LayoutSizing::Expand` to consume remaining space. `ListBox` defaults to
`Expand` for compatibility with the earlier vertical layout; pass
`LayoutSizing::Natural` for a content-sized list. `SetSpacing()` and
`SetPadding()` use platform-neutral logical integer units. Geometry is clipped
to the supplied logical bounds even when a tiny container cannot fit its
padding.

```cpp
Layout root(Orientation::Vertical, 24, 8);
Layout nameRow(Orientation::Horizontal, 0, 8);
Label nameLabel("Name");
TextBox name;
nameRow.Add(nameLabel);
nameRow.Add(name, LayoutSizing::Expand);
root.Add(nameRow);

ListBox profiles;
root.Add(profiles, LayoutSizing::Expand);

Layout actions(Orientation::Horizontal, 0, 8);
actions.Add(saveButton);
actions.Add(deleteButton);
actions.AddSpacer();
root.Add(actions);
window.SetContent(root);
```

Natural controls receive their preferred content size on the layout direction's
main axis and stretch across the other axis. `LayoutSize` exposes a layout's
recursive `GetNaturalSize()` and `GetMinimumSize()`; controls use the same
internal measurement contract without adding measurement methods to every
public control. Natural size is the preferred child composition. Minimum size
is the smallest meaningful composition before below-minimum clipping is
unavoidable. Both use non-negative platform-neutral logical integer units.

`LayoutSizing::Expand` means natural size plus an equal share of remaining
main-axis space. After padding and spacing are reserved, every child starts at
its natural size. If the region is constrained, expanding children shrink
toward minimum first, followed by natural children. If all minimums still do
not fit, sizes are clipped in insertion order. The allocator never returns a
negative dimension or relies on floating-point or DPI-specific units. A
spacer measures zero by default and consumes extra space only when marked
`Expand`.

Measurement is recalculated on every geometry request and backend layout pass.
Changing label, button, checkbox, or radio text therefore updates its natural
width immediately; single-line text boxes keep a normal preferred height,
multiline text areas keep a useful multi-row preferred viewport, and list boxes
keep a useful viewport size rather than measuring all items.

The contract boundary is:

```text
Control state
   ↓
internal platform-neutral measurement contract
   ↓
backend measurement provider (native metrics when realized)
   ↓
recursive layout natural/minimum calculation
   ↓
platform-neutral geometry allocation
   ↓
backend native positioning
```

`Layout::CalculateGeometry()` uses the deterministic neutral provider, so
geometry tests and future backends do not require an `HWND`. The Windows
backend privately measures the assigned native font and text extent where it
adds value, with conventional bounded sizes for text boxes, list boxes, and
combo boxes. ComboBox is natural-sized by default and expands only when
explicitly marked `LayoutSizing::Expand`; it never consumes vertical space by
default.

Nested layouts are owned by their parent layout's shared model state, so a
temporary child remains valid after `Add()`. A control can belong to only one
layout, and a child layout can have only one parent. Direct and indirect cycles,
cross-application layouts or controls, and using one root layout for multiple
windows are rejected with `std::logic_error`. Closing and reopening the same
window preserves the layout tree and recalculates it from the new client size.

## Menus and commands

Menus are window chrome. They are attached with `Window::SetMenuBar()` and are
not `Layout` children, so the content layout still receives the requested
client rectangle. `Window::ClearMenuBar()` removes the bar; a window has zero or
one logical menu bar.

The public model is small and hierarchical:

```cpp
MenuBar menuBar;
Menu file("&File");
MenuItem newItem("&New");
MenuItem exitItem("E&xit");

newItem.SetShortcut(KeyShortcut::Ctrl('N'));
newItem.OnInvoked([&]() { status.SetText("New selected"); });
exitItem.OnInvoked([&]() { window.Close(); });

file.Add(newItem);
file.AddSeparator();
file.Add(exitItem);
menuBar.Add(file);
window.SetMenuBar(menuBar);
```

`Menu` and `MenuBar` are shared model handles like `Layout`; parents retain
inserted child state, including a temporary or copied menu structure. A menu
has one parent location and a menu item has one menu location. Cycles,
multi-parent insertion, cross-application use, and attaching one bar to two
windows are rejected with `std::logic_error`. Duplicate labels are allowed
because command identity is the item state, not its text.

`MenuItem` defaults to enabled and supports `SetText`, `SetEnabled`,
`SetChecked`, `SetShortcut`, `ClearShortcut`, `OnInvoked`, and `Invoke`.
Checked state is explicit model state; invocation does not toggle it
automatically. `OnInvoked` replaces the callback and `OnInvoked({})` clears it.
Dispatch is synchronous, model-first, reentrant, and suppressed for disabled or
detached items. Destroying the public item clears its callback even if a
parent retains the item model.

`Menu::OnOpening` replaces a synchronous callback that runs immediately before
that menu is shown. The callback may update enabled or checked state,
including rebuilding entries; an empty callback clears it. This narrow refresh
hook is used by dynamic Edit commands.

The documented mnemonic convention is ampersand markup: `&File` and `E&xit`
mark native mnemonics, while `&&` displays a literal ampersand. Text is still
strictly validated UTF-8; the ampersand convention is interpreted only by the
native menu boundary. `KeyShortcut::Ctrl('S')` and
`KeyShortcut::CtrlShift('S')` provide the intentionally small accelerator
subset. Duplicate shortcuts within one bar are rejected; separate windows may
use the same shortcut.

Changing item text, enabled/checked state, or shortcut updates the live native
menu immediately. Adding/removing/clearing entries is supported and rebuilds
that bar. Rebuilds use fresh private per-window command IDs, so test helpers
should rediscover an ID after a callback mutates menu state. Closing and
reopening retains the menu model and callback state while creating fresh native
handles and bindings. The Windows backend uses ordinary native menus and
accelerator tables; no native type appears in public headers or normal sample
code.

## ToolTips and StatusBar

ToolTips are a shared control property. The supported controls are `Button`,
`TextBox`, `TextArea`, `ListBox`, `CheckBox`, `RadioButton`, `ComboBox`, and
`ProgressBar`, and `Slider`; `Label` also
supports the same property. Text is UTF-8, empty text clears it, invalid UTF-8
throws `std::invalid_argument`, and a tooltip is limited to 8 KiB. ToolTip text
does not participate in measurement or layout.

```cpp
button.SetToolTip("Create a new profile");
textBox.SetToolTip("Enter the profile name");
button.SetToolTip({}); // clear
```

The Windows backend keeps one private common-controls tooltip host per realized
`Window` and registers each realized control that has non-empty text. Setting
text before `Show()`, changing it after realization, and clearing it all update
the model and native registration. Detached or destroyed controls are removed;
closing and reopening rebuilds fresh registrations. Disabled controls do not
receive hover input, so their tooltip is not shown. No tooltip timing, style,
balloon, or rich-content API is exposed. Automated tests inspect registration
in-process; actual mouse hover appearance, delay, placement, and theme/DPI
polish remain recommended manual checks.

`StatusBar` is window chrome rather than a `Layout` child. A window has zero or
one attached status bar:

```cpp
StatusBar status("Ready");
window.SetStatusBar(status);
status.SetText("Changes saved");
window.ClearStatusBar();
```

`StatusBar::SetText()` and `GetText()` use UTF-8. A status bar can be attached
to only one window and application; replacing it detaches the old bar, while
cross-window or cross-application attachment throws `std::logic_error`.
Text and attachment state survive close/reopen, while native handles are
destroyed and rebuilt. The native status control is private and has one text
part only: panes, progress, icons, clickable sections, and owner draw are not
implemented.

The chrome relationship is:

```text
Window
├── MenuBar
├── content Layout
└── StatusBar
```

`Window::SetSize(width, height)` continues to describe the usable content
client area. When menu or status chrome is present, the Windows outer window
grows to accommodate it; the content layout is measured and positioned in the
remaining region, and the status bar spans the bottom edge. Common-controls
initialization is performed once privately by the Windows backend. No native
class names, handles, messages, or geometry structures appear in public headers
or normal sample code.

## Native dialogs and file pickers

The dialog foundation is synchronous and owner-based. Application code passes a
realized, shown `Window`; it never receives an `HWND`, COM interface, Windows
dialog flag, or UTF-16 buffer.

```cpp
const auto result = MessageDialog::Show(
    window, "Delete this profile?", "Confirm",
    MessageDialogButtons::YesNo, MessageDialogIcon::Question);
if (result == MessageDialogResult::Yes) DeleteProfile();

OpenFileDialog open;
open.SetTitle("Open Profile");
open.AddFilter("guideXOS Profile", {"*.gxprofile"});
open.AddFilter("All Files", {"*.*"});
const std::optional<std::string> selected = open.Show(window);

SaveFileDialog save;
save.SetTitle("Save As");
save.SetSuggestedFileName("profile.gxprofile");
save.AddFilter("guideXOS Profile", {"*.gxprofile"});
const std::optional<std::string> destination = save.Show(window);
```

`MessageDialogButtons` supports `Ok`, `OkCancel`, `YesNo`, and `YesNoCancel`.
Results are `Ok`, `Cancel`, `Yes`, or `No`; a native `IDCANCEL` result,
including a close-X result when Windows reports it as cancellation, maps to
`Cancel`. An OK-only Windows message box reports its native `IDOK` close/ESC
behavior as `Ok`. Icons are `None`, `Information`, `Warning`, `Error`,
and `Question`. `MessageDialog::Show()` blocks until the native message box
returns, disables its owner according to normal Windows modal behavior, and
may be called from a button, menu, checkbox, or combo selection callback.

`OpenFileDialog` and `SaveFileDialog` support a title, one selected filesystem
path, cancellation, and `AddFilter(description, patterns)`. A filter requires
a non-empty UTF-8 description and at least one non-empty UTF-8 wildcard
pattern; wildcard grammar is otherwise left to Windows. `SetInitialDirectory()`
is optional and accepts a non-empty UTF-8 filesystem path. Save also supports a
UTF-8 suggested filename. The App Model does not append extensions or apply
overwrite policy and never reads or writes the selected file.

All dialog text and returned paths use UTF-8 `std::string`. Windows returns an
absolute native filesystem path where the shell provides one; path
interpretation remains application responsibility. Invalid UTF-8 throws
`std::invalid_argument`. A non-realized, closed, detached, or shutdown owner
throws `std::logic_error`. User cancellation returns `std::nullopt`; native
failure throws `std::runtime_error` without exposing Windows error codes.

The Windows backend uses `MessageBoxW` for messages and the Common Item Dialog
(`IFileOpenDialog`/`IFileSaveDialog`) for file selection. It initializes an
apartment with `CoInitializeEx(COINIT_APARTMENTTHREADED)` for each picker call,
accepts `S_OK` and `S_FALSE`, balances successful initialization with
`CoUninitialize`, and rejects an incompatible existing apartment with a
platform-neutral runtime error. COM policy is scoped to the picker call rather
than imposed globally on the application. Native COM objects, HRESULT values,
filters, and UTF-16 buffers remain private.

The dialog scope intentionally excludes multi-file selection, folder pickers,
custom or asynchronous dialogs, and default-extension policy. File I/O and
ProfileManager persistence are described below; generic document frameworks,
recent files, autosave, and server/Linux backends remain out of scope.

## Text clipboard

The process-wide `Clipboard` service exchanges only UTF-8 text and does not
require an `Application` or a shown `Window`:

```cpp
#include <guidexos/appmodel/appmodel.hpp>

using namespace guidexos::appmodel;

Clipboard::SetText("Hello from guideXOS");
if (Clipboard::HasText()) {
    const std::string text = Clipboard::GetText();
}
```

`SetText` rejects invalid UTF-8, embedded NUL bytes, and text larger than
`kMaximumClipboardTextBytes` (8 MiB) before native allocation. An empty
string is valid text: after `SetText({})`, `HasText()` is true and
`GetText()` returns an empty string. `HasText()` is false when the clipboard
has no supported Unicode text. `GetText()` throws `ClipboardNoTextError` in
that case; it may also throw a platform-neutral unavailable, malformed-text,
allocation, operation, or size error. Windows Unicode text is preferred and
is currently the only supported native format; ANSI-only `CF_TEXT` is not
decoded as a fallback.

`Clear()` clears all clipboard formats, not only App Model text. Clipboard
opens use three private attempts with a 1 ms delay between attempts. The API
never waits indefinitely when another process temporarily owns the clipboard.
The Windows backend converts UTF-8 to UTF-16 privately, publishes
`CF_UNICODETEXT`, validates native termination and UTF-16 conversion on read,
and uses RAII for clipboard open/close, global locks, and allocation ownership.
No Windows clipboard type or error code appears in public headers or sample
source. The service is process/system-wide, so all windows in one process see
the same current clipboard and closing a window does not affect it.

`TextBox` and `TextArea` control commands build on this service. `Copy()` is a no-op for an
empty selection; `Cut()` copies then removes the selection; `Paste()` is a
no-op when no supported clipboard text exists and replaces the current
selection otherwise; and `DeleteSelection()` removes selected text without
changing the clipboard. Direct `Clipboard::GetText()` retains its existing
throwing `ClipboardNoTextError` contract, while control-level `Paste()` treats
missing text as a no-op. Native physical Ctrl+C/Ctrl+X/Ctrl+V and Ctrl+A remain
provided by the Windows edit control; application command handlers can call the
same text-control methods explicitly. TextArea commands honor read-only mode;
copy and selection remain available while Cut, Paste, and DeleteSelection are
blocked.

`ClipboardApp` exercises repeated button and `Edit` menu operations, exact
Unicode/emoji and empty-text round trips, replacement, clear state, resize,
and close behavior. Clipboard tests restore the original supported text when
possible. If the original clipboard has no supported text, cleanup clears the
clipboard; arbitrary non-text formats cannot be backed up by this text-only
test harness, and that limitation is reported here rather than hidden.

## Bounded UTF-8 file I/O and ProfileManager persistence

The public whole-file utility is deliberately small:

```cpp
#include <guidexos/appmodel/appmodel.hpp>

const std::string text = File::ReadAllText(path);
File::WriteAllText(path, text);
```

`path`, file contents, and returned text are UTF-8. Paths are converted to
Windows UTF-16 only inside the backend. `ReadAllText` accepts one leading
UTF-8 BOM and returns text without it. `WriteAllText` emits plain UTF-8 without
adding a BOM; a caller-provided leading BOM is treated as a signature and is
removed. Invalid UTF-8 is rejected rather than reinterpreted as ANSI/ACP.

Whole-file operations are bounded to 16 MiB. The public API reports
platform-neutral `FileNotFoundError`, `FileAccessError`, `FileTooLargeError`,
`InvalidUtf8Error`, `FileWriteError`, and `FileReplacementError` subclasses of
`FileError`. Messages may include the UTF-8 path but never expose Win32 error
codes or handles.

Writes create a temporary file in the destination directory, write and flush
it, close it, then privately use Windows replacement/rename semantics. A
failure attempts to remove the temporary file. This avoids the obvious
truncate-then-partial-write case but is not a crash-durability or locking
promise. The backend boundary is intentionally just these operations so a
future guideXOS Server filesystem layer can provide the same contract without
introducing a virtual filesystem.

ProfileManager uses `.gxprofiles` and the format in
[`docs/profile-manager-format.md`](docs/profile-manager-format.md). Its File
menu is `New`, `Open...`, `Save`, `Save As...`, and `Exit`. `Save Changes`
commits the selected profile editor into the in-memory collection; `File >
Save` persists the complete collection. Save and Save As commit valid pending
editor changes before serialization. A Save As cancellation occurs before
that commit, and Open cancellation or malformed input leaves the current
document unchanged. Open/New discard pending editor text according to the
sample policy. New uses a Yes/No save confirmation. File > Exit and every
normal native close request use the same Yes/No/Cancel policy; Open
intentionally does not prompt yet and only replaces state after a successful
load.

The title is `Profile Manager - Untitled`, the filename, or the filename plus
` *` while the document or pending editor is dirty. A close-time Yes commits
pending editor changes before saving. Save As cancellation, save failure, and
Cancel leave the window and editor usable.

## Cancelable window closing

`Window::OnClosing` observes a close request before native destruction. The
callback is platform-neutral and is used by `Window::Close()`, the caption X,
Alt+F4, and the native system Close command:

```cpp
window.OnClosing([&](WindowClosingEvent& event) {
    if (!document.IsDirty()) return;
    const auto result = MessageDialog::Show(
        window, "Save changes before closing?", "Profile Manager",
        MessageDialogButtons::YesNoCancel, MessageDialogIcon::Question);
    if (result == MessageDialogResult::Cancel ||
        (result == MessageDialogResult::Yes && !SaveDocument())) {
        event.Cancel();
    }
});
```

`OnClosing` replaces the previous callback; `OnClosing({})` clears it. The
callback runs synchronously. A request for the same window received while its
callback is executing is ignored, so recursive self-close and duplicate native
requests cannot recurse. After the callback returns, Cancel leaves the window
open; an allowed request commits destruction exactly once. A retained
`Window` can be shown again and its callback remains installed.
This milestone does not add a public `OnClosed` callback; existing closure is
observable through `IsShown()` and application live-window accounting.

## Shutdown policy

`Application` defaults to `ShutdownMode::WhenLastWindowCloses`. There is no designated main window. The event loop exits when the last shown App Model window is closed; secondary windows therefore keep the application alive after another window closes. `ShutdownMode::Explicit` is also available and requires `Application::Quit()` to end the loop.

`Window::Close()` is a normal cancelable close request rather than an
unconditional destruction call. With no callback, or after the callback
allows the request, it destroys the native realization rather than merely
hiding it. The `Window` object and its model remain valid, and `Show()` may
create a fresh realization again until the application has shut down.
Repeated requests are harmless. Runtime teardown uses a private forced cleanup
path and does not repeatedly invoke user close callbacks. Under
`WhenLastWindowCloses`, cancellation leaves the live-window count and event
loop unchanged; only actual final destruction ends the loop. Under `Explicit`,
close cancellation does not issue or clear a `Quit()` request. An explicit
`Quit()` request ends the loop regardless of the number of live windows and
carries its requested exit code.

## Prerequisites

- Windows 10/11 desktop
- Visual Studio 2022/2026 with the MSVC C++ workload and Windows SDK
- CMake 3.25 or newer
- x64 support

The commands below use the Visual Studio 18 generator installed on the current development machine. If your installation exposes a different generator name, use the corresponding `cmake -G` entry while retaining `-A x64`.

## Configure, build, run, and test

From the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The sample executables are:

```text
build\Debug\HelloApp.exe
build\Debug\MultiWindowApp.exe
build\Debug\TextInputApp.exe
build\Debug\MultilineTextApp.exe
build\Debug\ListBoxApp.exe
build\Debug\ChoiceControlsApp.exe
build\Debug\ComboBoxApp.exe
build\Debug\ProgressBarApp.exe
build\Debug\SliderApp.exe
build\Debug\TabViewApp.exe
build\Debug\ImageApp.exe
build\Debug\ScrollViewApp.exe
build\Debug\ProfileManagerApp.exe
build\Debug\MenuApp.exe
build\Debug\DialogApp.exe
build\Debug\ClipboardApp.exe
build\Debug\TextEditingApp.exe
build\Debug\FocusCommandApp.exe
build\Debug\FileDropApp.exe
build\Debug\PolishApp.exe
build\Debug\TimerApp.exe
```

Run the GUI validations directly when iterating on a sample:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\gui_smoke.ps1 -Executable .\build\Debug\HelloApp.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\multi_window_gui_smoke.ps1 -Executable .\build\Debug\MultiWindowApp.exe -Cycles 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\text_input_gui_smoke.ps1 -Executable .\build\Debug\TextInputApp.exe -Cycles 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\multiline_text_gui_smoke.ps1 -Executable .\build\Debug\MultilineTextApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\listbox_gui_smoke.ps1 -Executable .\build\Debug\ListBoxApp.exe -Cycles 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\choice_controls_gui_smoke.ps1 -Executable .\build\Debug\ChoiceControlsApp.exe -Cycles 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\combobox_gui_smoke.ps1 -Executable .\build\Debug\ComboBoxApp.exe -Cycles 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\progress_bar_gui_smoke.ps1 -Executable .\build\Debug\ProgressBarApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\slider_gui_smoke.ps1 -Executable .\build\Debug\SliderApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\profile_manager_gui_smoke.ps1 -Executable .\build\Debug\ProfileManagerApp.exe -Cycles 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\profile_manager_close_gui_smoke.ps1 -Executable .\build\Debug\ProfileManagerApp.exe -Cycles 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\menu_gui_smoke.ps1 -Executable .\build\Debug\MenuApp.exe -Cycles 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\dialog_app_gui_smoke.ps1 -Executable .\build\Debug\DialogApp.exe -Cycles 3
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\clipboard_gui_smoke.ps1 -Executable .\build\Debug\ClipboardApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\text_editing_gui_smoke.ps1 -Executable .\build\Debug\TextEditingApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\focus_command_gui_smoke.ps1 -Executable .\build\Debug\FocusCommandApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\file_drop_gui_smoke.ps1 -Executable .\build\Debug\FileDropApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\polish_gui_smoke.ps1 -Executable .\build\Debug\PolishApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\timer_gui_smoke.ps1 -Executable .\build\Debug\TimerApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\tab_view_gui_smoke.ps1 -Executable .\build\Debug\TabViewApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\image_gui_smoke.ps1 -Executable .\build\Debug\ImageApp.exe -Cycles 5
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\scroll_view_gui_smoke.ps1 -Executable .\build\Debug\ScrollViewApp.exe -Cycles 5
```

These scripts drive the real executables and real native windows. The
DialogApp script creates and cleans a temporary Unicode test directory outside
the source tree, verifies message-box OK/Yes/No/Cancel paths, callback
reentrancy from buttons, a checkbox, a combo selection, and a menu, selects
Unicode open/save paths, verifies open/save cancellation and repeated use, and
confirms that no file is written. Its native-message Cancel fallback is
authoritative when caption-button interaction is not deterministic; physical
close-X activation remains a recommended manual check. The other scripts
retain their existing coverage: multi-window routing, text/list/choice/combo
behavior, ProfileManager workflow plus persistence round trips, malformed-file
rejection, cancellation, Unicode/duplicate preservation, repeated cycles, and
process/temp-file cleanup. The focused ProfileManager close suite drives a
genuine native close request through clean, Cancel, No, Yes-existing-file,
Yes-Save-As, Save As cancellation, save-failure, and repeated-close scenarios.
MenuApp covers hierarchy, state, shortcuts, Unicode, repeated cycles, and
process cleanup. `ClipboardApp` runs serially because it changes the shared
system clipboard; its five Debug cycles cover exact ASCII, replacement,
Unicode/emoji, empty text, button and menu commands, clear, resize, close,
and orphan-process cleanup. Release validation runs at least three cycles.
`FocusCommandApp` runs five Debug and three Release cycles covering focus
identity across TextBox, Button, ListBox, CheckBox, RadioButton, and ComboBox,
Tab and Shift+Tab, routed copy/paste/cut/select-all, menu state refresh,
multiple windows, close/reopen, and orphan-process cleanup.
`FileDropApp` runs five Debug and three Release cycles with controlled UTF-8
files, ordered multi-path drops, Unicode and duplicate paths, clear/re-drop,
bounded valid-file preview, invalid-UTF-8 error handling, resize, close, and
temporary-file/process cleanup. Its deterministic native injection does not
replace the recommended physical Explorer validation.
`MultilineTextApp` runs five Debug and five Release cycles covering native
multiline typing and Enter, AppModel newline synchronization, selection/copy,
paste, read-only focus and edit blocking, runtime layout resizing, and process
cleanup.

### GUI input automation policy

The shared [`tests/gui_test_helpers.ps1`](tests/gui_test_helpers.ps1) module uses one bounded policy for all GUI smoke scripts:

1. It makes one normal foreground/focus attempt when the desktop permits it.
2. It verifies both the focused child HWND and foreground ownership; thread-input attachment can make `SetFocus` succeed even when Windows refuses foreground ownership.
3. Text correctness uses synchronous, control-targeted UTF-16 messages. `WM_CHAR` preserves the TextBox notification path, followed by `WM_SETTEXT` to make the complete value exact, including surrogate pairs. This does not depend on the active keyboard layout.
4. Key, tab, selection, and button fallback operations target the native control directly and are followed by state polling or immediate state verification.

`SendInput` can be unavailable or unreliable in a desktop session because Windows foreground-stealing rules, session/window-station state, focus changes, or another foreground application can intercept delivery. It can also report that an input packet was accepted without proving that the intended control changed. The fallback proves the App Model callback, native control, Unicode conversion, and state-mutation contract for the targeted scenario. It does not prove physical mouse hit testing, real keyboard scan-code/layout behavior, accessibility behavior, or foreground desktop policy. Those remain manual release/checkpoint validations: click each interactive control, type with a physical keyboard (including the active layout and IME where relevant), exercise list/combo drop-down mouse behavior, and validate minimized/remote/locked desktop sessions as applicable.

## Public API example

The following uses two windows without exposing platform types:

```cpp
#include <guidexos/appmodel/appmodel.hpp>

using namespace guidexos::appmodel;

Application app("com.guidexos.samples.two-windows");
Window primary(app);
Window secondary(app);

Label primaryLabel("Primary");
Label secondaryLabel("Secondary");
Button openSecondary("Open Secondary");
Button closeSecondary("Close Secondary");

openSecondary.OnClick([&]() {
    secondary.Show();
    secondaryLabel.SetText("Opened by the primary window");
});
closeSecondary.OnClick([&]() { secondary.Close(); });

Layout primaryContent;
primaryContent.Add(primaryLabel);
primaryContent.Add(openSecondary);
primary.SetContent(primaryContent);

Layout secondaryContent;
secondaryContent.Add(secondaryLabel);
secondaryContent.Add(closeSecondary);
secondary.SetContent(secondaryContent);

primary.Show();
return app.Run();
```

Public headers and sample sources use UTF-8 `std::string` values and contain no native handle, message, or platform API types.

## Editable text input

`TextBox` is a single-line editable control. Its public text is UTF-8 in a
`std::string`; Windows UTF-16 conversion is private to the backend. Valid
Unicode, including supplementary-plane characters such as emoji, round-trips
through `GetText()`. Invalid UTF-8 is rejected with `std::invalid_argument`
before the logical value changes.

TextBox caret and selection indexes count Unicode scalar values. They do not
count UTF-8 bytes, UTF-16 code units, or grapheme clusters. `TextRange` is an
ordered half-open range `[start, start + length)`. Invalid caret indexes and
ranges throw `std::out_of_range`, and public code never sees native edit
messages or UTF-16 positions.

```cpp
TextBox nameInput("Initial value");
nameInput.OnTextChanged([&](const std::string& text) {
    previewLabel.SetText(text);
});

nameInput.SetText("Updated value");
const std::string current = nameInput.GetText();

nameInput.SetSelection({0, 7});
nameInput.Copy();
nameInput.Cut();
nameInput.Paste();
```

`SetText` is safe before or after a window is shown. It always resets the
selection to empty and places the caret at the end of the new text, including
when the text value is unchanged; an unchanged value still does not generate a
`TextChanged` event. A changed programmatic assignment and a user edit each
generate one `OnTextChanged` event. The model is updated before callback entry,
and caret/selection state reflects the post-operation state. `SetSelection`
places the caret at the range end, `SelectAll()` selects the entire text, and
`ClearSelection()` collapses to the selection end. Cut/delete collapse to the
selection start, while paste leaves the caret after inserted text. Empty
selection copy is a no-op. Native keyboard, mouse, Home/End, Shift-selection,
and Ctrl+A changes are synchronized into the model for later queries, but no
selection-changed event is emitted.

Callbacks run synchronously on the UI thread. A callback may call `GetText`, normalize its own `TextBox`, update another control or text box, close its own or another window, or issue duplicate close requests. A normalization callback can therefore receive another event for a different value, but identical assignments stop recursion. Native detachment does not erase logical text or callbacks: closing and reopening a window recreates the edit with the retained model value. Destroying the public `TextBox` clears its callback even if a layout still retains the model state.

Text boxes and buttons are created in layout order. Labels are skipped by native dialog-style navigation. With the sample layout, Tab moves from the Name box to the Message box, then the two buttons; Shift+Tab reverses that order. Windows supplies ordinary single-line editing, including click-to-focus, arrows, Backspace, Delete, Enter, and Space behavior.

## Portable multiline text editing

`TextArea` is the dedicated multiline editor. It shares the TextBox text,
selection, caret, callback, focus, and clipboard semantics without adding a
multiline mode to `TextBox`, so ordinary TextBox controls remain single-line.

```cpp
TextArea editor("line one\r\nline two");
editor.SetWordWrap(true);
editor.SetReadOnly(false);
editor.OnTextChanged([&](const std::string& text) {
    status.SetText("Characters: " + std::to_string(text.size()));
});

const std::string normalized = editor.GetText(); // "line one\nline two"
editor.SelectAll();
editor.Copy();
```

The public text is UTF-8 and uses `\n` as its only newline convention.
`TextArea` normalizes CRLF and lone CR input to LF on construction, `SetText`,
and native Windows edits; Windows CRLF storage remains private. User typing,
Enter/newline, navigation, selection, replacement, deletion, and native
clipboard editing update the model and produce at most one `OnTextChanged`
callback per changed value. Programmatic `SetText` resets the caret to the
scalar end and does not notify for an unchanged value.

`SetReadOnly(true)` prevents user and control-level modifying commands while
leaving focus, selection, and Copy available. `SetWordWrap(true)` is the
default; `false` enables private native horizontal scrolling while vertical
scrolling remains available in both modes. Both properties can be changed
before or after realization. A TextArea has a multi-row natural/minimum
measurement and can be marked `LayoutSizing::Expand` to consume available
vertical space. Rich text, undo/redo, grapheme-aware selection, and a
selection changed event remain out of scope.

## Platform-neutral focus and routed edit commands

`Window::GetFocusedControl()` reports the control that currently owns native
keyboard focus in that App Model window. It returns a weak, state-backed
`ControlRef`, not a native handle or control ID. The reference is safe to keep
across native detachment and close; while the logical state is retained it
remains an identity, but `HasFocus()`/`Focus()` report no current native focus
while closed. When the logical state is destroyed, it becomes invalid and its
queries and operations fail safely. `ControlRef::AsTextBox()` and
`ControlRef::AsTextArea()` provide narrow text-editing capability views for
routed commands.

```cpp
const ControlRef focused = window.GetFocusedControl();
if (auto textBox = focused.AsTextBox()) {
    if (textBox->HasSelection()) textBox->Copy();
}

nameInput.Focus();
if (nameInput.GetControlRef().HasFocus()) {
    nameInput.SelectAll();
}
```

`Focus()` is available on interactive controls and returns `false` for an
unrealized, detached, disabled, or closed control. Showing a window does not
invent a focus target; native click, Tab, and Shift+Tab transitions establish
focus. Focus is window-scoped: only the window with the current native child
focus reports it, while inactive windows report an invalid reference. Closing
clears current focus; reopening creates fresh native associations without
promising automatic focus restoration. A synchronous native modal dialog
temporarily clears the owner's current child focus, and Windows restoration is
reported when it returns.

The Edit menus in `TextEditingApp`, `ProfileManagerApp`, and `FocusCommandApp`
query this reference at command time. ProfileManager enables Cut, Copy, and
Delete only for a focused TextBox or TextArea with a selection, Paste only for a
focused text editor when the text clipboard is supported, and Select All only
for a focused text editor with text. `Menu::OnOpening` refreshes those states
immediately before the menu is shown. Ctrl+X/C/V/A accelerators route to the
focused text editor
before normal native edit dispatch, so each shortcut performs one logical
operation; global Delete is intentionally not an accelerator.

## ListBox and selection

`ListBox` owns a copied collection of UTF-8 `std::string` items and an optional zero-based selected index. Duplicate strings are allowed; selection identity is always the index, never the item text. `AddItem`, `InsertItem`, and `SetItem` validate UTF-8 before changing the model. Invalid UTF-8 throws `std::invalid_argument`, and invalid item or selection indexes throw `std::out_of_range` without changing the model.

```cpp
ListBox files;
files.AddItem("README.md");
files.AddItem("src");
files.AddItem("docs");

files.OnSelectionChanged([&](std::optional<std::size_t> index) {
    if (index) status.SetText(files.GetItem(*index));
});
files.SetSelectedIndex(1);
```

The default selection is empty. Appending, replacing text, and inserting after the selected index preserve the selection without an event. Inserting at or before the selected index increments its index so the same logical item stays selected. Removing an item before the selected index decrements the index without an event. Removing the selected item clears selection and emits one event; `ClearItems` does the same when a selection exists. Clearing an already-empty selection and selecting the current index are no-ops. `ClearItems` with no selection and item text changes do not emit selection events.

Both `SetSelectedIndex` and native mouse/keyboard selection emit `OnSelectionChanged` for a changed logical selection. Initial native creation never emits an event. The model selection is updated before callback entry, so `GetSelectedIndex()` and `GetItem()` are current inside the callback. Callbacks are copied before invocation; replacing or clearing a callback affects later dispatches. Nested selection changes are synchronous and depth-first, while assigning the current selection suppresses redundant recursion. Item mutations, other-control updates, closing either window, and native detachment are safe from the callback. Closing and reopening a window retains the collection, selection, and callback in the App Model.

In a vertical `Layout`, a ListBox receives a useful minimum height and consumes remaining vertical space; neighboring controls retain their normal horizontal resizing and Tab order. It is a native single-selection list control, so clicking, Up/Down, Home/End, scrolling, keyboard focus, and normal Tab traversal use standard Windows behavior.

## ComboBox and drop-down selection

`ComboBox` owns a copied collection of UTF-8 `std::string` items and an
optional zero-based selected index. It is a non-editable, single-selection
control: item text is displayed and selected through the native drop-down, but
there is no public text-entry value. Duplicate strings are allowed and are
distinguished by index.

```cpp
ComboBox mode;
mode.AddItem("Standard");
mode.AddItem("Advanced");
mode.AddItem("Compatibility");

mode.OnSelectionChanged([&](std::optional<std::size_t> index) {
    if (index) status.SetText(mode.GetItem(*index));
});
mode.SetSelectedIndex(0);
```

The full item API is `AddItem`, `InsertItem`, `RemoveItem`, `ClearItems`,
`GetItemCount`, `GetItem`, and `SetItem`. `SetSelectedIndex` accepts an
optional index, and `GetSelectedIndex` reports the authoritative model state.
Items and selection use the same zero-based, UTF-8-validating, deterministic
out-of-range rules as `ListBox`. Appending, replacing text, and inserting
after the selected index preserve selection without an event. Inserting at or
before it increments the selected index; removing before it decrements the
index; removing the selected item or clearing a non-empty selection clears it
and emits one event. Selecting the current index and clearing an already-empty
selection are no-ops.

The callback is replaced by `OnSelectionChanged({})`, runs synchronously after
the model is current, and receives a copy of the optional index. Programmatic
selection and native mouse/keyboard selection share the same dispatch path.
Callbacks may update labels, text boxes, checkboxes, radio groups, another
ComboBox, item collections, or either window. Nested changes are synchronous
and depth-first; native synchronization is guarded so backend writes do not
create feedback events. Closing and reopening a window retains items,
selection, enabled state, and the callback. Destroying the public ComboBox
clears its callback while a retained layout model remains inert.

The Windows realization is an ordinary `CBS_DROPDOWNLIST` control. Click opens
the popup; native Up/Down, Home/End where supplied by Windows, Alt+Down,
keyboard focus, and Tab participate in normal native behavior. The popup uses
the native control's bounded behavior; its height is not part of the layout
rectangle. The closed control uses a bounded natural width of 220 logical units
and a native-font-derived closed height (28 units in the neutral fallback),
with a 112-unit minimum width. Item strings intentionally do not widen the
form to the longest item, so adding or replacing a very long or Unicode item
does not create an absurd layout. `LayoutSizing::Expand` may widen the closed
control horizontally, while a natural vertical placement keeps its closed
height.

## CheckBox and RadioButton choices

Choice controls use the same UTF-8 `std::string` text contract as the other controls. Invalid UTF-8 throws `std::invalid_argument` before the logical value changes. Empty, accented, non-Latin, and supplementary-plane labels are supported.

```cpp
CheckBox notifications("Enable notifications", true);
RadioGroup theme;
RadioButton system("System");
RadioButton dark("Dark");

theme.Add(system);
theme.Add(dark);
theme.Select(system);

notifications.OnCheckedChanged([&](bool checked) {
    status.SetText(checked ? "Enabled" : "Disabled");
});
dark.OnSelectedChanged([&](bool selected) {
    if (selected) status.SetText("Dark selected");
});

notifications.SetChecked(false);
theme.Select(dark);
```

`CheckBox` starts unchecked unless its optional constructor argument is `true`. `SetChecked` changes the model even while disabled, dispatches one synchronous `OnCheckedChanged` callback for each real transition, and suppresses unchanged assignments. The model is current before callback entry. `OnCheckedChanged` replaces the callback; an empty function clears it. Native mouse and Space activation follow the same path, while a disabled native checkbox rejects activation.

`RadioGroup` starts with no selection, and no selection remains legal. Membership is explicit: `Add` rejects duplicates and rejects a `RadioButton` already in another group; remove it from the old group before moving it. A group stores non-owning model membership, while the controls and group state use safe shared/weak lifetime links. `Select` accepts only a member, allows programmatic selection while disabled, and suppresses selecting the current member. `ClearSelection` and removal of a selected member clear selection; explicit removal emits that member's `false` callback, while public-object destruction silently detaches it. Destroying a group clears its members without callbacks.

For an A-to-B selection, the group index and both member states are updated atomically first, then A receives `false`, followed by B receiving `true`. Both callbacks can query the group and observe B as selected. Nested changes are synchronous and depth-first; callbacks may select another member, update any other control, close either window, or clear/replace callbacks. A radio cannot be selected without membership in a `RadioGroup`.

`RadioGroup` and all its members must belong to one `Application`. A group cannot be used across applications. Independent groups remain independent even when their controls share a window or appear in different windows. Closing a window detaches native realizations but retains text, enabled state, selection, membership, and callbacks; reopening creates fresh native controls without initial selection events.

`Button`, `TextBox`, `TextArea`, `ListBox`, `ComboBox`, `CheckBox`, `RadioButton`, `ProgressBar`, and `Slider` expose `SetEnabled(bool)` and `IsEnabled()`. Disabled native controls reject native interaction. Programmatic model mutations remain allowed, including `Button::Click`, text/item changes, list/combo selection, checkbox state, radio-group selection, and progress/slider range/value changes. Labels do not currently expose enabled state.

Choice controls receive the normal vertical layout height, stretch horizontally, and participate in creation-order Tab navigation. Native buttons provide mouse activation, Space activation, focus, and standard radio arrow behavior where the native group segment permits it; the App Model `RadioGroup` remains authoritative and refreshes every realization after a selection.

## ProgressBar

`ProgressBar` is a non-focusable indicator with portable determinate and
indeterminate semantics. Its default range is `0..100`, its default value is
`0`, and it starts in determinate mode.

```cpp
ProgressBar progress;
progress.SetMinimum(0);
progress.SetMaximum(100);
progress.SetValue(35);

progress.SetIndeterminate(true);  // ongoing work with unknown completion
progress.SetValue(70);            // stored while indeterminate
progress.SetIndeterminate(false); // resumes at 70
```

`GetMinimum()`, `GetMaximum()`, and `GetValue()` report the authoritative
model state. `SetValue()` clamps to the current range. `SetMinimum()` clamps a
requested minimum to the current maximum, then clamps the value upward;
`SetMaximum()` clamps a requested maximum to the current minimum, then clamps
the value downward. This keeps `minimum <= value <= maximum` deterministic,
including zero-width ranges and endpoint changes such as `SetMinimum(100)`
when the maximum is `50`. Range and value setters continue to work while the
bar is indeterminate, and switching modes never discards the stored range or
value.

`SetEnabled()` and `IsEnabled()` follow the ordinary control model. The bar
participates in `Layout`; its neutral natural/minimum measurement is 220/96
logical units wide and 22 units high, and `LayoutSizing::Expand` gives it
remaining space on the layout's main axis. It is intentionally not a focus or
keyboard-navigation target. All properties work before realization, after
realization, and after close/reopen. `ControlRef::GetType()` reports
`ControlType::ProgressBar`, and `AsProgressBar()` provides a safe state view.

`ProgressBarApp` composes `Window`, `Label`, `ProgressBar`, `Button`,
`CheckBox`, `Layout`, and the existing repeating `Timer`. Start resets to the
minimum and advances to the maximum; Reset stops and returns to the minimum;
the checkbox switches determinate and indeterminate presentation. Application
code does not call Windows APIs, and the native bar does not embed percentage
text; the sample uses a separate label.

## Slider

`Slider` is a horizontal, focusable control for bounded integer input. Its
default range is `0..100`, its default value is `0`, and it accepts signed
minimums and maximums.

```cpp
Slider volume;
volume.SetMinimum(0);
volume.SetMaximum(100);
volume.SetValue(50);

volume.OnChanged([&]() {
    volumeLabel.SetText(std::to_string(volume.GetValue()));
});
```

The model preserves `minimum <= value <= maximum`. `SetValue()` clamps to the
range. Endpoint setters clamp a crossing endpoint to the other endpoint and
clamp the current value into the resulting range, matching `ProgressBar`.
Zero-width and signed ranges are valid. The model remains authoritative before
realization, after realization, and across close/reopen.

Programmatic and user-originated value changes share one synchronous
`OnChanged()` path. A callback runs after the model value is current; assigning
the effective current value emits no event. Native mouse clicks, thumb drags,
arrow keys, Page Up/Page Down, Home, and End are routed through the private
Windows trackbar and produce at most one logical callback per effective value.
Reentrant callbacks may change the value or enabled state, update other
controls, or close the containing window. The callback is copied at each
dispatch boundary and is cleared when the public Slider is destroyed.

`Slider` participates in `Layout`, expands horizontally, and has a non-zero
natural/minimum height. `ControlRef::GetType()` reports `ControlType::Slider`,
and `AsSlider()` provides a narrow range/value view. Explicit stepping and
vertical orientation are deferred; the current implementation is horizontal
only and uses the native integer line/page behavior for keyboard interaction.
`SliderApp` demonstrates direct Slider → `ProgressBar` and Label composition.

## TabView

`TabView` is a portable container control. Each `AddTab()` call returns a
stable `TabPage` handle whose `GetLayout()` is an ordinary nested `Layout`;
the page can therefore reuse existing controls without tab-specific classes:

```cpp
TabView tabs;
auto general = tabs.AddTab("General");
auto network = tabs.AddTab("Network");

TextBox name("guideXOS user");
general.GetLayout().Add(name);

TextBox server("example.com");
network.GetLayout().Add(server);

tabs.OnSelectionChanged([&](std::optional<std::size_t> index) {
    status.SetText(index ? tabs.GetTab(*index).GetTitle()
                         : "No page selected");
});

Layout content;
content.Add(tabs, LayoutSizing::Expand);
window.SetContent(content);
```

The first page is selected automatically; adding later pages preserves the
current selection. `GetSelectedIndex()` is optional: an empty TabView has no
selection, and `SetSelectedIndex(std::nullopt)` clears selection. A nonempty
index outside the page range throws `std::out_of_range`; assigning the
effective current selection is a no-op. Programmatic and native user
selection share one synchronous callback path and emit exactly one event per
effective change. `TabViewRef` is available through
`ControlRef::AsTabView()`, while `TabPageRef` provides stable page identity
and title/index queries.

Only the selected page is visible. Page controls remain logically owned by
their page layouts and their model state survives switching, close/reopen,
and native realization changes. Switching away from a focused page moves
focus to the TabView, so hidden page controls do not continue receiving
keyboard input. TabView expands in both layout axes, uses the native tab
control's reported content rectangle so headers are not overlapped, supports
UTF-8 titles and runtime page addition, and retains normal enabled/tool-tip
control behavior. Tab removal, icons, reordering, close buttons, and custom
tab painting are not currently supported.

## Image

`Image` is a non-interactive, portable raster control. Its source is an
immutable UTF-8 file description, so source identity and lifetime do not
depend on a native bitmap or decoder object:

```cpp
Image logo;
logo.SetSource(ImageSource::FromFile("assets/logo.png"));
logo.SetScaleMode(ImageScaleMode::Fit);

Layout content;
content.Add(logo, LayoutSizing::Expand);
window.SetContent(content);
```

`ImageSource::FromFile()` rejects empty, invalid UTF-8, and overlong paths.
Assignment is valid before or after `Window::Show()`. Before realization the
Image reports `Pending`; the backend then reports `Loaded` with decoded pixel
dimensions or `Failed` with an error string. A failed replacement clears the
previously displayed pixels, retains the requested source for inspection, and
never reports a successful load. `ClearSource()` returns the control to the
deterministic empty state. Reassigning an effective loaded source is suppressed;
reassigning a failed source retries decoding.

PNG and JPEG are the formally supported formats for this milestone. BMP is
also accepted by the Windows decoder when available. PNG pixels are converted
to a premultiplied alpha representation, so opaque, partially transparent,
and fully transparent pixels compose against the Image/window background
without an opaque black rectangle. Animated images, SVG, video, network
loading, asynchronous decoding, and image editing are intentionally out of
scope.

The default scale mode is `Fit`: the complete source preserves its aspect
ratio, is centered, and is letterboxed or pillarboxed as needed. `Stretch`
fills the destination rectangle and may change the aspect ratio. `Fill`
preserves the aspect ratio, covers the complete destination, and applies a
centered crop to the overflow. Geometry is bounded and handles empty or
zero-sized destinations without division by zero.

An Image uses a 220 × 140 logical fallback natural size before a successful
decode; after decoding, its natural size is the source pixel size and its
minimum size is 1 × 1. It participates in nested and horizontal/vertical
layouts and invalidates its native child when the source, mode, resize, or
window expose state changes. It is not focusable, a tab stop, or a keyboard
input target, although the generic enabled property is retained across
close/reopen.

ToolTips use the existing generic control mechanism. `ControlType::Image`,
`ControlRef::AsImage()`, and `ImageRef` expose only portable state and safe
weak lifetime behavior. The Windows backend keeps COM initialization, WIC
decoder interfaces, decoded pixel validation, private DIB storage, and alpha
blending entirely inside `src/platform/windows`; no public header contains a
Windows image type. Image controls also compose with TabView pages: inactive
page children are hidden and the decoded model state survives switching and
close/reopen.

## ScrollView

`ScrollView` is a portable vertical viewport. Its `ContentLayout()` is an
ordinary nested `Layout`, so existing controls, child layouts, Images, and
TabViews retain their normal APIs and model state while the native viewport
moves:

```cpp
TabView tabs;
auto settings = tabs.AddTab("Settings");
ScrollView scroll;
Label heading("Profile");
TextBox profile("guideXOS");
Image preview;
TextArea notes("Persistent notes");

scroll.ContentLayout().Add(heading);
scroll.ContentLayout().Add(profile);
scroll.ContentLayout().Add(preview);
scroll.ContentLayout().Add(notes, LayoutSizing::Expand);
settings.GetLayout().Add(scroll, LayoutSizing::Expand);

Layout root;
root.Add(tabs, LayoutSizing::Expand);
window.SetContent(root);
```

The viewport and content extent are distinct. `GetViewportSize()` reports the
space assigned by the parent layout, while `GetContentSize()` reports the
measured nested content. `GetMaximumVerticalOffset()` is
`max(0, content.height - viewport.height)`. `SetVerticalOffset()` clamps to
that range after realization and retains a pre-realization request until the
viewport is known. `GetControlRef().AsScrollView()` provides a safe
`ScrollViewRef` view.

The Windows backend realizes one private child host with real clipping and a
private vertical scrollbar. Wheel input over the viewport or its child
controls, line/page/thumb scrollbar actions, programmatic offsets, and resize
all synchronize back to AppModel state. Content controls are native children
of the host, so offscreen controls do not paint over neighboring UI and remain
interactive when they become visible. Closing, reopening, switching TabView
pages, and replacing an Image preserve logical content state.

Horizontal scrolling, inertial/touch scrolling, and automatic keyboard
scroll-to-focused-child are deferred. Child controls retain their own keyboard
semantics; the ScrollView does not consume their arrow-key navigation.

## Application timers

`Timer` is a repeating, application-owned event source. It uses only
platform-neutral time and callback semantics in application code:

```cpp
Timer refresh(app, std::chrono::milliseconds(250));
refresh.OnTick([&]() {
    status.SetText("Refreshed");
});
refresh.Start();
```

`Start()` and `Stop()` are idempotent. `SetInterval()` accepts a positive
`std::chrono::milliseconds` value and restarts a running timer with the new
interval. `OnTick()` replaces the callback; the callback runs synchronously on
the application event-loop thread after the timer fires and may stop or
reconfigure the timer. Destruction, `Stop()`, and backend shutdown remove the
native schedule. Timers are repeating only in this milestone; they do not
provide background-thread execution, cross-thread dispatch, or a one-shot
mode. With `ShutdownMode::WhenLastWindowCloses`, a timer does not keep an
application alive after its last window closes; use `ShutdownMode::Explicit`
when the application owns its shutdown request. Windows may clamp or
coalesce very short intervals, so `Timer` is not a real-time scheduler.

## Lifetime and supported operations

- `Application` must outlive its `Window` objects. `Window` and control objects are non-copyable.
- A closed `Window` remains a valid App Model object and can be shown again before `Application::Run()` shuts down the backend.
- Controls and layouts may remain as C++ objects after their parent window closes. Their model state is detached from native controls while closed and is realized again on reopen.
- A root `Layout` belongs to one `Window`; a closed window may reuse it when reopened. A child layout belongs to one parent layout, and a control belongs to one layout. Reusing a layout or control across different `Application` objects is rejected with `std::logic_error`.
- Adding the same control or child layout to one layout twice is rejected with `std::logic_error`; direct and indirect layout cycles are rejected as well.
- `Button::OnClick` replaces the prior callback. Destroying a `Button` clears its stored callback even if a layout still retains its model state.
- `TextBox::OnTextChanged` and `TextArea::OnTextChanged` replace the prior callback. Destroying either text control clears its stored callback even if a layout still retains its model state.
- `ListBox::OnSelectionChanged` replaces the prior callback. Destroying a `ListBox` clears its stored callback even if a layout still retains its model state.
- `ComboBox::OnSelectionChanged` replaces the prior callback. Destroying a `ComboBox` clears its stored callback even if a layout still retains its model state.
- `CheckBox::OnCheckedChanged` and `RadioButton::OnSelectedChanged` replace their prior callbacks. Destroying a choice control clears its callback even if a layout retains its model state.
- `ProgressBar` retains its range, value, indeterminate state, and enabled state across native detachment; its `ControlRef` remains safe while the layout retains the model state.
- `Slider::OnChanged` replaces the prior callback. Destroying a Slider clears its callback even if a layout retains its model state; its signed range, value, and enabled state remain retained across native detachment.
- `RadioGroup` retains weak membership links and rejects duplicate or cross-group membership. It must not be inferred from visual adjacency or creation order.
- Callbacks run synchronously on the creating/UI thread. A callback may close its own window, close another window, open or reopen a window, and update a control in another live window. These operations have deterministic state transitions; native dispatch does not retain an iterator across the callback.
- `Window::Close()` is idempotent. Model reads, writes, and programmatic `Button::Click()` remain ordinary C++ operations after a window closes. `Show()` returns `false` after application shutdown.
- `Window::OnClosing` replaces the synchronous close-request callback;
  `OnClosing({})` clears it. `WindowClosingEvent::Cancel()` keeps the realized
  window open. The callback is copied before invocation, so replacement or
  clearing during dispatch affects later requests only.
- Close callbacks may update controls, show synchronous dialogs, close another
  window, request application shutdown, or request their own close again. The
  same-window request is ignored while the callback is active; native teardown
  and runtime forced cleanup never invoke a callback after the model is closed.
- `Timer` belongs to one `Application`, retains its interval and callback across
  window close/reopen, and clears its native schedule on `Stop()`, destruction,
  or application shutdown.

Threading, background dispatch, one-shot timers, rich/password editing, grapheme or
word selection, selection-changed events, undo/redo, context menus, drag/drop,
editable or autocomplete combo boxes, tri-state checkboxes, toggle switches,
visual group boxes, multi-selection, item payloads, sorting, virtualization,
validation frameworks, clipboard image/HTML/rich-text/custom-format contracts,
custom focus scopes, focus-change notifications, generic command objects,
command bubbling, and global command registries, additional
collection controls, custom drawing, theming, packaging, Linux, C#,
Server integration, and multiple applications within one process remain
outside this milestone.

Vertical progress bars, embedded progress text, custom progress rendering,
taskbar/notification-area progress, and worker or asynchronous job
abstractions remain outside this milestone. The sample intentionally simulates
work with the existing event-loop `Timer`.

See [docs/architecture.md](docs/architecture.md) for the backend boundary and lifecycle details, and [docs/server-app-model-findings.md](docs/server-app-model-findings.md) for the local Server investigation.

## Window-level shell file drops

`Window::OnFilesDropped()` is the first drag/drop foundation. It accepts files
from the Windows shell onto the entire top-level Window content area while
keeping Windows messages and handles private:

```cpp
window.OnFilesDropped([&](const FileDropEvent& event) {
    for (const std::string& path : event.GetFiles()) {
        status.SetText(path);
    }
});
```

`FileDropEvent::GetFiles()` returns an immutable `std::vector<std::string>` of
UTF-8 paths in shell order. Windows supplies absolute native filesystem paths;
directories are surfaced as paths too, without existence or regular-file
validation. Duplicates are retained. Registration enables shell file drops for
that Window; passing an empty callback clears the callback and disables native
acceptance. The callback remains registered across `Close()`/`Show()` and is
copied before synchronous UI-thread dispatch, so it may update controls, show
dialogs, read files, replace itself, or close either Window safely.

The Windows backend uses the narrow `WM_DROPFILES`/`DragAcceptFiles` mechanism,
not OLE drag targets. Accepted input is bounded to 1,024 paths, 256 KiB per
UTF-8 path, and 4 MiB total UTF-8 path bytes. Invalid UTF-16/UTF-8 or empty
entries are ignored; entries beyond the count or aggregate bound are ignored
deterministically. The native payload is fully copied and finished before the
App Model callback starts. No `HWND`, `HDROP`, `WM_DROPFILES`, OLE interface,
UTF-16 buffer, screen coordinate, URI, image, text, or arbitrary clipboard
format is part of the public API.

`ProfileManagerApp` accepts exactly one dropped `.gxprofiles` path and reuses
the same parse-before-replace `OpenDocumentFromPath()` used by File > Open.
Malformed input preserves the current document. A dirty document uses the
existing Yes/No/Cancel policy: Yes saves first, No discards, and Cancel aborts.
`FileDropApp` appends repeated drops, keeps duplicates, and uses the bounded
`File::ReadAllText()` API for its first-file preview. The GUI smoke test uses a
test-only in-process native agent to create real `DROPFILES` payloads; it does
not claim to automate physical Explorer dragging. Explorer/desktop dragging,
multi-select cursor feedback, elevation boundaries, Remote Desktop, and other
physical shell behavior remain recommended manual validation.

Future drag/drop work may add child targets, drag-over feedback, custom drag
sources, effects, URI/text/image formats, or OLE negotiation; none is implied
by this file-path-only foundation.

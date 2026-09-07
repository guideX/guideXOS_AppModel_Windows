# Windows App Model architecture

## Boundary

```text
HelloApp / MultiWindowApp / TextInputApp / TextEditingApp / ListBoxApp / ComboBoxApp / ChoiceControlsApp / DialogApp / ClipboardApp / FileDropApp / PolishApp / TimerApp
          |
          v
Public guideXOS App Model API (include/guidexos/appmodel)
          |
          v
Platform-neutral runtime state and backend contract (src/appmodel, src/platform)
          |
          +--> Private Windows window/control backend (src/platform/windows)
          |
          +--> Private Windows clipboard backend (src/platform/windows)
          |
          +--> Private Windows shell file-drop backend (src/platform/windows)
          +--> Private Windows common-controls chrome backend (src/platform/windows)
          +--> Private Windows event-loop timer backend (src/platform/windows)
          |
          +--> Native desktop windows, controls, session clipboard, and shell file drops
```

The samples know only `Application`, `Window`, `Label`, `Button`, `TextBox`, `ListBox`, `ComboBox`, `CheckBox`, `RadioButton`, `RadioGroup`, `Layout`, `MenuBar`, `Menu`, `MenuItem`, `StatusBar`, `Timer`, `KeyShortcut`, `Clipboard`, `File`, `FileDropEvent`, and the synchronous dialog/file-picker contracts. Public headers contain no Windows SDK include, native handle, message parameter, COM type, clipboard handle, UTF-16 buffer, Win32 error code, or platform callback type.

## Process-wide text clipboard

Clipboard is deliberately a separate service from `ApplicationState` and
`WindowState`:

```text
Application code
      |
      v
Clipboard::HasText / GetText / SetText / Clear
      |
      v
platform-neutral ClipboardBackend contract
      |
      +--> Windows Unicode clipboard backend
      +--> future guideXOS Server/session backend
```

The public `Clipboard` API is static and requires neither an `Application` nor
a shown `Window`. A process-wide backend instance represents the current
Windows session clipboard, so two windows in one process observe the same
contents. Closing or reopening a window does not detach, cache, or destroy
clipboard state. The service is synchronous and follows the current App Model
UI-thread usage model; no background monitoring or clipboard history is
introduced.

The public contract is UTF-8 text only. `SetText` validates UTF-8, rejects
embedded NUL bytes, and rejects values larger than the public 8 MiB
`kMaximumClipboardTextBytes` bound before native allocation. Empty text is a
valid value. `HasText` means supported Unicode text can be opened, read, and
converted under this contract; it returns false only when no `CF_UNICODETEXT`
format is available. `GetText` returns the UTF-8 value, including an empty
value, and throws `ClipboardNoTextError` when no supported text exists. Native
open/read/conversion/allocation failures use the small platform-neutral
`ClipboardError` family; malformed native UTF-16 is not replaced or silently
decoded. ANSI-only `CF_TEXT` is intentionally not a supported fallback.

`Clear` calls the neutral clear operation but documents Windows semantics: it
removes all clipboard formats, not only App Model text. Opening the clipboard
uses three private attempts with a 1 ms delay between attempts, so contention
is bounded and surfaces as `ClipboardUnavailableError` rather than an
unbounded wait. The Windows implementation validates the native global block
size before locking, requires a NUL terminator, uses strict UTF-16/UTF-8
conversion, and bounds the native block to the amount needed for an 8 MiB
UTF-8 value plus its UTF-16 terminator.

`SetText` allocates a moveable native block, copies the private UTF-16 value
and terminator, and publishes it only after `EmptyClipboard` succeeds. RAII
closes the clipboard and frees the block on every pre-publication failure;
after `SetClipboardData` accepts the block, ownership is released to Windows
and it is not freed by the App Model. No public header exposes
`OpenClipboard`, `CloseClipboard`, `CF_UNICODETEXT`, `HGLOBAL`, UTF-16, or
Win32 error values. A future Server backend can implement the same
`ClipboardBackend` contract using an App Model, shell, compositor, or session
clipboard service without inheriting HWND ownership semantics.

## TextBox text positions and editing commands

`TextBox` exposes caret and selection state through a platform-neutral scalar
index model:

```cpp
TextRange selection = editor.GetSelection(); // [start, start + length)
editor.SetCaretIndex(2);
editor.SetSelection({0, 5});
editor.SelectAll();
editor.Copy();
editor.Cut();
editor.Paste();
editor.DeleteSelection();
```

Every public index counts Unicode scalar values in the UTF-8 model string.
UTF-8 byte offsets and Windows UTF-16 code-unit offsets are private. A
supplementary-plane emoji therefore occupies one public index and two native
UTF-16 units. `TextRange` is ordered and half-open; invalid indexes and
ranges throw `std::out_of_range`, while native reversed selections are
normalized to an ordered range. Grapheme clusters are intentionally not
counted in this milestone.

The default caret is at the end of the constructor text with an empty
selection. `SetText()` always resets the caret to the new scalar end and
clears selection; a same-value assignment does not emit `OnTextChanged`.
`SetSelection()` places the caret at the range end, `SelectAll()` selects the
whole string, and `ClearSelection()` collapses to the selection end. Cut and
delete collapse to the selection start; paste replaces the selection and leaves
the caret after inserted text. Copy with an empty selection and Paste without
supported clipboard text are no-ops. `GetSelectedText()` returns the exact
UTF-8 substring.

The Windows backend converts public scalar indexes to UTF-16 only while
synchronizing `EM_SETSEL`/`EM_GETSEL` at the private edit-control boundary. A
private edit subclass observes native keyboard, mouse, selection, and native
clipboard-edit messages after the native procedure returns. Text-changing
notifications read the native text and selection before dispatching the model
callback. This keeps queries current after typing, arrows, Shift-selection,
mouse selection, Home/End, Ctrl+A, and native Ctrl+C/Ctrl+X/Ctrl+V paths without
adding a selection-changed event. Closing/reopening destroys and recreates only
the native control; model text, caret, and selection remain attached to the
logical TextBox.

Control-level Copy/Cut/Paste use the existing process-wide UTF-8 Clipboard API,
so clipboard size and conversion policy are not duplicated. They dispatch at
most one logical TextChanged event for one text mutation. At callback entry,
GetText(), GetCaretIndex(), GetSelection(), and GetSelectedText() reflect the
post-operation model state. Callbacks may query or change selection, invoke
clipboard commands on this or another TextBox, set text, or close either
window; backend dispatch retains only shared model state across that user code.

Native physical Ctrl+C/Ctrl+X/Ctrl+V and Ctrl+A remain native edit behavior.
`KeyShortcut::Ctrl('A')`, `Ctrl('C')`, `Ctrl('X')`, and `Ctrl('V')` are also
available to application menus. The focus layer below lets those menu
commands target the currently focused TextBox without exposing native focus.

## Platform-neutral focus and routed editing

Each control has a private shared `ControlState`; the public `ControlRef` is a
weak identity reference to that state. `Window::GetFocusedControl()` returns
an invalid `ControlRef` when there is no current child focus. `ControlRef`
queries lock the state before use. A reference remains a safe logical identity
across native teardown, layout detachment, and window close while its model
state is retained; `HasFocus()` and `Focus()` report no current native focus
while closed. When the logical state is destroyed, the weak reference becomes
invalid rather than dangling. `AsTextBox()` is the one narrow capability query
currently needed for edit routing and returns a safe `TextBoxRef` with the same
weak lifetime behavior.

The backend observes private `WM_SETFOCUS` and `WM_KILLFOCUS` transitions for
every realized interactive control. The runtime records the focused control
and owning window only after the native transition is accepted. A control is
reported only while its window is shown, its state is enabled, and its window
is the window containing current native child focus. Labels are not focusable.
No focus is invented on first `Show()`; native click, Tab, and Shift+Tab
establish it. `Control::Focus()` is exposed as `Focus()` on the interactive
public control types. It activates the shown window and requests native focus,
returning `false` for an unrealized, detached, disabled, or closed control.

The focus contract is child-focus state, not foreground process or top-level
activation state. When another shown window receives native focus, the former
window reports no current focused control. Closing a window clears its focus;
reopening realizes fresh child bindings and does not promise focus restoration.
During a synchronous native modal dialog the owner normally reports no child
focus after `WM_KILLFOCUS`; a later `WM_SETFOCUS` restoration is reported when
Windows restores the child. The public API never stores or returns HWNDs,
native IDs, or focus messages.

`Menu::OnOpening` is a synchronous, replaceable callback invoked immediately
before a popup becomes interactive. It may update menu state or entries; the
Windows backend defers a rebuild safely until the callback returns. Nested
submenus receive the same treatment. ProfileManager uses this hook to enable
Cut, Copy, Delete, Paste, and Select All from the current focused TextBox.
The handlers query focus again and no-op when their target is gone, so menu
state is advisory and command execution remains safe under reentrancy.

The event loop chooses the native window containing the current child focus,
translates that window's accelerator table before normal dialog/message
translation, and consumes the Ctrl+X/C/V/A command after invoking the routed
handler. Thus the native edit control does not receive a second dispatch for
the same accelerator. Delete is a menu command without a global accelerator.
There is intentionally no public focus-changed event, command object hierarchy,
command bubbling, or custom traversal policy in this milestone.

## Application and shutdown

`Application` owns the identity, neutral runtime state, one backend instance, and event-loop lifetime. Its default `ShutdownMode::WhenLastWindowCloses` has no main-window designation: the application remains alive while any shown App Model window remains. `ShutdownMode::Explicit` is supported for applications that call `Quit()` themselves.

The backend only posts its platform quit signal after a window binding that belongs to this backend has been removed and `ShouldQuitAfterWindowClosed()` confirms the configured neutral policy. An arbitrary native destruction cannot end another application or a non-final window. `Application::Quit()` is the separate explicit exit path and preserves its requested exit code.

With the default policy, `Run()` also completes deterministically if called after there are no shown windows. With explicit shutdown, `Run()` requires a `Quit()` request. `Run()` and callbacks are UI-thread operations; this milestone has no cross-thread dispatch.

### Application timers

`Timer` is a repeating, application-owned event source. It stores a positive
`std::chrono::milliseconds` interval and a replaceable `OnTick()` callback.
`Start()` and `Stop()` are idempotent; changing the interval of a running timer
restarts its schedule. Each callback is copied before synchronous dispatch, so
it may stop or reconfigure the timer. Destruction, `Stop()`, and backend
shutdown clear the native schedule. Timers do not create a background thread,
do not keep `WhenLastWindowCloses` applications alive, and have no one-shot
mode in this milestone.
Windows may clamp or coalesce very short intervals, so this is an event-loop
timer rather than a real-time scheduler.

## Close-request contract

`Window::OnClosing(std::function<void(WindowClosingEvent&)>)` is the public,
platform-neutral interception point for normal window close requests. The
event has only `Cancel()` and `IsCanceled()` in this milestone. The callback is
used for `Window::Close()` and for every normal native close mechanism; callers
do not receive a native close reason or handle. Passing an empty function
clears the callback and installing another function replaces it.

The private per-window state follows this sequence:

```text
Closed -> Open -> CloseRequested -> Closing -> Closed
                    |                  |
                    +-- Cancel -> Open  +-- native destruction
```

Only an `Open` window accepts a new request. While `CloseRequested`, the
callback is executing synchronously and another request for that same window
is ignored. The callback is copied before entry, so replacing or clearing it
from inside the callback affects a later request. When it returns, cancellation
restores `Open`; an allowed request enters `Closing` and commits native
destruction exactly once. `WM_CLOSE`, `DestroyWindow`, and other Windows
details remain in `src/platform/windows` and do not occur in public headers or
samples.

The runtime has a private forced-cleanup path for application destruction. It
destroys remaining native realizations without user callbacks, so teardown
cannot be held open by a dialog or a callback. Native destruction marks the
model closed before live-window accounting. Consequently, under
`WhenLastWindowCloses`, a canceled request leaves the live count unchanged and
does not quit; an allowed final destruction posts the normal quit signal.
`Explicit` shutdown is unchanged: cancellation does not alter an existing
explicit shutdown request, and closing a window does not request one.
There is intentionally no new public `OnClosed` callback in this milestone;
the existing `IsShown()` and application live-window state cover the required
post-closure behavior.

## Per-window realization

Every `WindowsBackend` instance owns a registry keyed by its own native top-level handles. Each entry contains:

- the strong `WindowState` for that App Model window
- that window's native top-level handle
- a separate child-control binding collection
- weak model references and command IDs for its own controls

There is no global current-window pointer. A `WM_COMMAND`-equivalent notification is first resolved through the owning top-level binding and then through that binding's child handle. A command ID is never used to search another window. Destroying one top-level binding removes only its own children and registry entry; reopening the logical window creates a new binding and new native child handles.

The child binding collection includes private `EDIT` realizations for `TextBox`, ordinary single-selection `LISTBOX` realizations for `ListBox`, non-editable `COMBOBOX` realizations for `ComboBox`, and native `BUTTON` realizations for buttons, checkboxes, and radio buttons. Button notifications use the backend's private command IDs; edit, list, combo, and choice notifications are routed by the originating child handle and do not need a public control ID. `EN_CHANGE`, native edit selection/caret reads and writes, `LBN_SELCHANGE`, `CBN_SELCHANGE`, `BN_CLICKED`, `BM_GETCHECK`, `BM_SETCHECK`, `WM_GETTEXT`, list/combo item messages, UTF-16 conversion, and native synchronization guards remain entirely inside the Windows backend, so multiple controls, groups, and windows cannot cross-route their events.

Process-level class registration is the only intentionally shared native concern. The backend, native handles, control collections, layout realization, and destruction state are per application/window instance.

## Menu and command realization

Menus are window chrome, not content controls:

```text
Window chrome
  └─ MenuBar

Window client/content
  └─ Layout
```

`MenuBar`, `Menu`, and `MenuItem` are platform-neutral shared model handles.
`MenuBar::Add(Menu&)`, `Menu::Add(Menu&)`, `Menu::Add(MenuItem&)`, and
`Menu::AddSeparator()` build a hierarchical tree. A menu or item has one
logical parent location; direct/indirect cycles, duplicate attachment, and
cross-application/window reuse throw `std::logic_error`. Parent state retains
children, so a copied or temporary menu structure remains structurally valid
after insertion. Destroying a public `MenuItem` clears its callback while its
retained command state may remain displayed.

`Window::SetMenuBar()` attaches at most one bar to a window. Replacing or
clearing a bar detaches its model affinity so the bar can be reused. Closing a
window does not detach its bar: the logical menu and callbacks remain valid for
`Show()` recreation. A native realization receives fresh private command IDs,
and the previous per-window command map is discarded on close, removal, or
rebuild. Command identity is therefore state identity, never display text;
duplicate labels route independently.

The Windows backend privately creates ordinary `HMENU` trees and, when a
`KeyShortcut` is present, a per-window accelerator table. `WM_COMMAND` menu
notifications resolve only through the originating top-level window's command
map. A disabled item is gray and suppressed in both native and logical
dispatch. Checked state is model state and does not auto-toggle; application
callbacks make the transition explicitly. Menu text remains UTF-8 in the
public model, is strictly validated, and is converted to UTF-16 only at this
boundary. A single ampersand marks a native mnemonic and `&&` represents a
literal ampersand.

Text and enabled/checked/shortcut mutations rebuild the small native menu
realization immediately. Structural add/remove/clear mutations are also
supported and rebuild the affected bar. Rebuilds allocate new IDs, so callers
should rediscover native command IDs in test tooling after a callback mutates
menu state. The model callback is copied and invoked synchronously after the
enabled/parent state check; it may update controls or menus, close either
window, or request application shutdown. Detached items and disabled items are
no-ops, and native menu writes do not create feedback callbacks.

Attaching a menu does not change the public window-size contract. Width and
height remain requested client dimensions; the Windows backend includes the
native menu height when calculating the outer frame and lays out content from
the resulting client rectangle.

## Dialog and file-picker boundary

The dialog API follows the same boundary as the controls and menus:

```text
Application code
      |
      v
MessageDialog / OpenFileDialog / SaveFileDialog
      |
      v
platform-neutral dialog contracts
      |
      v
Windows dialog backend
      +--> MessageBoxW
      +--> Common Item Dialog
```

`dialogs.hpp` contains only enums, UTF-8 strings, optional UTF-8 paths, and a
small `FileDialogFilter` value. `Window` exposes no native owner handle; the
runtime accepts a dialog owner only while its App Model state is attached to a
live application and its native realization is shown. A detached, closed,
unrealized, or shutdown owner throws `std::logic_error`.

Dialog calls are synchronous on the creating/UI thread. The native owner is
passed privately to `MessageBoxW` or `IFileDialog::Show()`, and normal Windows
modal behavior disables the owner while the nested native loop runs. No global
dialog owner is stored. Other windows in the same backend retain independent
bindings, and callbacks may update or close the owner after `Show()` returns.
The backend keeps only model references across callback dispatch; it does not
use a window-binding pointer after a reentrant callback or modal return.

`MessageDialog` maps neutral button and icon enums to `MessageBoxW` flags and
maps `IDOK`, `IDYES`, `IDNO`, and `IDCANCEL` to the neutral results. Native
an `IDCANCEL` cancellation, including a close-X result when Windows reports
one, is `MessageDialogResult::Cancel`; an OK-only native box follows Windows'
`IDOK` close/ESC behavior. No raw `MB_*` value is public. UTF-8 input is
validated before conversion and invalid UTF-8 throws `std::invalid_argument`.

File dialogs use `IFileOpenDialog` and `IFileSaveDialog` with
`FOS_FORCEFILESYSTEM`, one-result selection, optional title/initial folder,
and private `COMDLG_FILTERSPEC` arrays. Public filters are copied as a UTF-8
description plus one or more patterns, then joined with semicolons only inside
the Windows backend. A user cancellation (`HRESULT_FROM_WIN32(ERROR_CANCELLED)`
or `E_ABORT`) returns `std::nullopt`; any other native failure becomes a
platform-neutral `std::runtime_error`. The selected `SIGDN_FILESYSPATH` value
is converted to UTF-8 and returned as the native absolute filesystem path.

COM is initialized per file-picker call with
`CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)`. `S_OK` and `S_FALSE` are
both accepted and balanced with `CoUninitialize`; `RPC_E_CHANGED_MODE` and
other failures are reported as a runtime error. This is a scoped, balanced
requirement rather than a process-wide COM policy, and all COM interfaces and
task-allocator memory are released by local RAII before the call returns.

The intentionally small dialog contract excludes multi-file selection, folder
pickers, extension policy, custom/async dialogs, and document semantics. A
future Server backend can replace the final layer with the same synchronous
neutral operations without implementing Windows shell concepts.

## Bounded file I/O and ProfileManager documents

The public `file.hpp` contract contains only `File::ReadAllText` and
`File::WriteAllText`, UTF-8 `std::string` paths/content, a 16 MiB whole-file
bound, and platform-neutral `FileError` subclasses. No public header contains
`HANDLE`, `CreateFile`, `std::wstring`, Win32 error codes, or sharing flags.

The private backend contract is byte-oriented and intentionally small:

```text
Application/sample
       |
       v
guidexos::appmodel::File
       |
       v
private filesystem backend contract
       |
       +-- Windows UTF-8 path conversion + bounded Win32 I/O
       +-- future guideXOS Server filesystem implementation
```

The Windows implementation checks the reported size before allocation, reads
in bounded chunks, and validates UTF-8 at the public boundary. It accepts one
leading BOM on read, returns text without it, and writes without adding a BOM.
Save writes to a same-directory `.guidexos-tmp-*` file, flushes and closes it,
then uses private replacement/rename semantics. Cleanup is best effort; the
guarantee is avoidance of obvious truncate-then-partial-write corruption, not
crash durability, locking, or recovery.

ProfileManager keeps document meaning out of the generic App Model. Its
`ProfileManagerDocument` owns the collection, current UTF-8 path, overall
document dirty state, selected profile, and pending editor state. The
application-level serializer uses the bounded deterministic format documented
in [`profile-manager-format.md`](profile-manager-format.md). Parsing is
temporary-state-first, so unsupported or malformed files cannot partially
replace the live document. `Save Changes` changes the collection; `File >
Save` writes the entire collection. Open/New discard pending editor text
according to the sample policy, New and File > Exit ask whether a dirty
collection should be saved, Open does not yet prompt for a dirty collection,
and `ProfileManagerWindow` installs one `OnClosing` policy for both File > Exit
and native close. `ProfileManagerDocument::IsDirty()` includes pending editor
changes. A close-time Yes commits those edits before serialization; No
discards them with the closing document; Cancel, Save As cancellation, and
save failure keep the window open.

## ToolTip and StatusBar chrome

ToolTip text is stored directly on `ControlState` and is intentionally not a
layout input. `Button`, `TextBox`, `ListBox`, `CheckBox`, `RadioButton`, and
`ComboBox` expose `SetToolTip()`/`GetToolTip()`; `Label` follows the same small
surface. Setters validate UTF-8 and reject values over 8 KiB before changing
the model. Empty text means no registration. The state remains useful while a
control is detached, and a later realization registers its current text.

Each realized `WindowBinding` owns one private Windows common-controls tooltip
host and private records for its non-empty control tips:

```text
WindowBinding
├── child control HWNDs and subclass state
└── ToolTip host HWND
    └── TOOLINFO records owned by this binding
```

The backend destroys the host and unregisters every record before rebuilding
controls or registrations. Child subclass state relays the relevant mouse
messages to that window's host; this avoids exposing a native subclass or
requiring one tooltip window per control. A live setter therefore replaces the
registration synchronously. Close/reopen creates a fresh host and records, and
detachment or child destruction cannot leave a record associated with an old
HWND. Disabled controls retain their model text but do not receive hover input.
No public native pointer is retained.

`StatusBar` is a separate shared model state attached to `WindowState`, not a
`LayoutState` item. The attachment is one-to-zero-or-one per window. The state
records weak application/window affinity, so attaching a bar to a second
window or application is rejected. Replacement and `ClearStatusBar()` detach
the previous affinity; close/reopen retains the logical object but destroys
and recreates its native realization. Text updates synchronize the one native
status part immediately, including updates made during callbacks.

The private native shape is:

```text
Window
├── menu bar (optional native menu)
├── content layout (native child controls)
└── status bar (optional STATUSCLASSNAME control)
```

`Window::SetSize()` continues to mean the usable content-client width and
height. When a status bar is attached, the backend measures its native height,
adds that height to the requested native client rectangle, and subtracts it
from the region passed to the existing layout allocator. Menu height remains
native menu chrome in the same contract. Thus existing content is not silently
shrunk by the requested size; the outer window grows to fit chrome, and resize
reflows the layout above a bottom-aligned status bar.

The backend calls `InitCommonControlsEx` once per backend process path through a
private `std::call_once` guard for the common-control bar classes used by
tooltips and status bars. Failure leaves the public model valid but prevents
native chrome creation in the same way as other backend realization failures.
The Windows class names, `TOOLINFO`, status messages, native IDs, HWNDs, and
geometry structures remain in `src/platform/windows` and test-only native
integration code. The PowerShell GUI smoke uses an in-process test report for
text because pointer-bearing common-control messages cannot marshal a buffer
across processes; the C++ native test inspects registration and pane text in
the owning process.

## Layout composition and geometry

`LayoutState` is a platform-neutral tree. Each item is a control, a child
`LayoutState`, or a spacer, and carries `LayoutSizing::Natural` or
`LayoutSizing::Expand`. Controls are natural by default, except `ListBox`,
which remains expanding by default to preserve the earlier vertical behavior.
`Orientation::Vertical` allocates the main-axis
height; `Orientation::Horizontal` allocates the main-axis width. Children
stretch across the cross axis, which preserves the original vertical sample
behavior while allowing a horizontal label/field row.

The model asks each direct child for a `LayoutMeasurement` containing a natural
and minimum `LayoutSize`. Control measurement is an internal App Model
contract; the public API exposes only `LayoutSize` plus
`Layout::GetNaturalSize()`/`GetMinimumSize()` for recursive layouts. Public
controls do not need implementation-oriented measurement methods. A spacer
reports zero for both sizes. The neutral provider uses deterministic logical
fallbacks: labels measure text-like width and preserve normal text height,
buttons include a bounded label/padding width and remain at least usable,
text boxes use a normal single-line height and a reasonable preferred width,
choices include indicator/gap space, and list boxes use a useful viewport
instead of total item content.

ComboBox uses a bounded closed-control natural width and native-font-derived
closed height. Its item text is intentionally excluded from preferred-width
calculation, so a long or Unicode item does not make a form unreasonably wide.
The native popup owns its own bounded list height, independent of the logical
closed-control rectangle. A ComboBox expands horizontally only when a layout
item requests `LayoutSizing::Expand` and remains natural-sized by default.

Layout measurement composes recursively. For a vertical layout, natural width
is the maximum child natural width plus horizontal padding, natural height is
the sum of child natural heights plus spacing and vertical padding, and minimum
size uses the same rules with child minimums. Horizontal layouts transpose the
axes. Empty layouts contain only their padding in their reported size; a
zero-padding empty layout reports zero.

The model calculates logical `LayoutRect` values before realization. Padding is
removed on both axes and spacing is reserved between every direct child. Each
child starts at natural size. `Natural` keeps that size when possible;
`Expand` receives natural size plus an equal share of remaining main-axis
space, with integer remainders assigned in insertion order. Under pressure,
expanding children shrink toward minimum first, then natural children. If all
minimums still exceed the available region, the remaining below-minimum clip is
assigned in insertion order. Every rectangle is clipped to its logical bounds,
has non-negative dimensions, and uses saturating integer arithmetic. The
Windows backend does not reimplement this policy or expose Win32 geometry
types.

The public `Layout::CalculateGeometry(LayoutRect)` method exposes direct-child
logical rectangles for platform-neutral tests and future backends. A nested
layout's assigned rectangle can be passed to that child's method. Internally,
`LayoutMeasurementProvider` lets a realized backend supply native information
without changing the allocator. The Windows backend recursively flattens only
actual controls for native creation and supplies a provider that uses the
assigned native font and text extent for labels/buttons/choices while retaining
bounded conventional dimensions for text boxes and list boxes. Resize,
refresh, close/reopen, and nested realization all remeasure from current state.

The measurement flow is:

```text
Control state
   ↓
platform-neutral measurement contract
   ↓
backend measurement implementation
   ↓
layout natural/minimum calculation (recursive)
   ↓
platform-neutral geometry allocation
   ↓
backend native positioning
```

## Model ownership and lifetime

- `Application` owns `ApplicationState` and its backend. The application must outlive its windows.
- `Window` owns a shared `WindowState` and registers a weak entry with its application. A committed close marks the model not shown but does not destroy the C++ object.
- `Window::Close()` requests a close through `OnClosing`; it is idempotent and cancelable. If allowed, it destroys the native realization. `Show()` can realize the same model again until application shutdown.
- `WindowState` retains its shared `LayoutState`; layouts retain shared control model state. Native child bindings retain weak control references, so destroyed public controls cannot keep callbacks alive.
- `Button` destruction clears its callback. A control model retained by a layout may still display text after the public control object is gone, but its event is inert.
- `TextBox` stores the authoritative UTF-8 logical value in its neutral model state. `SetText` validates the value, ignores identical assignments, refreshes a live native edit when required, and dispatches only after the model is current. Native `EN_CHANGE` reads the edit once, converts UTF-16 to UTF-8 with strict error handling, updates the model, and then invokes the copied callback.
- `ListBox` stores copied UTF-8 items and an optional index-based selection in its neutral model state. Item mutations validate before changing the collection, preserve the same logical selected item across index shifts, clear selection when its item is removed, and refresh every live realization. `SetSelectedIndex` and native selection both update the model before invoking a copied callback; unchanged logical selections are suppressed.
- `ComboBox` stores the same copied UTF-8 item collection and optional index-based selection model as `ListBox`. The two controls share private item/index adjustment helpers, while keeping separate public control types and native realization paths. A ComboBox is non-editable, retains selection across native detachment/recreation, and uses the same enabled and callback ownership rules.
- `CheckBox` stores UTF-8 text, a checked bit, and an enabled bit. Its programmatic and native transitions share one changed-value dispatcher; unchanged values are suppressed and the model is current before the copied callback runs.
- `RadioButton` stores UTF-8 text, a selected bit, and an enabled bit. `RadioGroup` stores weak member links, a weak selected member, and one application affinity. A radio belongs to at most one group; membership is explicit and is never inferred from native styles, adjacency, creation order, or parent relationships.
- Radio group selection changes the group and all affected member bits before native refresh or callbacks. The group is authoritative when the Windows button class also applies native auto-radio behavior.
- A root layout belongs to one window; a child layout belongs to one parent, and a control belongs to one layout. Layouts own nested layout state through shared ownership, while parent and window links are weak to avoid cycles. A closed window retains its root and can realize it again.
- A MenuBar belongs to at most one window, while a Menu belongs to one MenuBar or parent Menu and a MenuItem belongs to one Menu. Parent menus retain child state. Replacing/clearing a bar or removing an entry detaches the child affinity; close/reopen retains the attached bar and callbacks.
- A StatusBar is a shared model handle with one Window/application owner. Its UTF-8 text remains authoritative across close/reopen; replacement, clearing, or window destruction releases the window affinity and native status binding. Cross-window and cross-application attachment is rejected.
- A Timer belongs to one Application. Its interval, running state, and callback are neutral model state; a running timer is scheduled by the backend and is stopped on public destruction or application shutdown.
- ToolTip text belongs to the control model, has an 8 KiB UTF-8 bound, and is independent of measurement. The native per-window host and records are rebuilt from live controls, so setting text before realization, changing it while shown, clearing it, detaching a layout, and recreating a window cannot retain stale native records.
- Menu callbacks are replaced by `OnInvoked`, cleared with `OnInvoked({})`, and cleared when the public `MenuItem` is destroyed. `Invoke()` and native selection dispatch only when the item is attached to a MenuBar and enabled. Duplicate labels are valid; duplicate shortcuts within one bar are rejected.
- Direct self-insertion, indirect layout cycles, a second layout parent, a second window content owner, cross-application binding, and control insertion into a second layout are rejected with `std::logic_error`.
- A model mutation refreshes every shown window that contains the affected control or layout. Layout spacing/padding changes are ordinary model operations and do not create callback loops.
- After backend shutdown, `Show()` returns `false`; model reads and writes remain safe C++ operations and do not attempt to recreate native state.

The sample ownership pattern declares the application first, then windows, controls, and layouts, and enters `Run()` before those objects leave scope. Callbacks may capture those references under that pattern. Capturing references to objects that leave scope while a callback remains installed is unsupported.

## Reentrant events

`Button::Click()` and native button activation use the same neutral dispatch function. Dispatch copies the callback before invoking it. Native command handling resolves and copies the control model before calling user code, so a callback can:

- close its own window;
- close another window;
- open or reopen another window;
- update a label in another live window; or
- request application exit.

Close dispatch follows the same reentrant boundary. A close callback may update
a Label or TextBox, show a synchronous MessageDialog or file dialog, write a
file, close another window, call `Window::Close()` again, replace or clear its
own callback, or request application shutdown. The same-window recursive close
is ignored while the callback is active. User code is not called after native
destruction commits; teardown cleanup is private and callback-free.

`TextBox::SetText()` and native edit changes use the same changed-value rule: identical values do not dispatch. The model text and post-edit caret/selection are updated before `OnTextChanged` entry, and the callback receives a stable copy of the new UTF-8 value. A callback can normalize its own box or update another box; each distinct resulting value produces its own synchronous event. Native writes are marked as synchronization work, so the resulting `EN_CHANGE` cannot recursively re-dispatch the programmatic assignment. Control-level Cut, Paste, and DeleteSelection each produce one logical event when text changes; selection-only changes produce no text event. The callback may query or change selection, copy or edit another TextBox, set its own text, close its own or another window, or request shutdown; the backend does not use a child-binding pointer after returning from user code.

`ListBox::SetSelectedIndex()` and native list changes use the same changed-selection rule: identical indexes do not dispatch. The model is updated before `OnSelectionChanged` entry, and the callback receives a copy of the optional index. Item insertion/removal adjusts the index of the same logical item without a redundant event; removing that item or clearing a non-empty list emits one empty-selection event. List item and selection synchronization is guarded against native notifications generated by backend writes, so programmatic changes cannot recursively re-dispatch. Nested user selection changes are synchronous and depth-first. The callback may mutate this or another list, update labels/text boxes, or close either window; the backend does not use a child-binding pointer after returning from user code.

`ComboBox::SetSelectedIndex()` and native combo changes use that same dispatcher
and mutation contract. `CBN_SELCHANGE` is resolved through the originating
child handle, then the model is updated before the copied callback runs. The
closed selection is retained when the drop-down closes, and re-opening or
recreating the window does not generate an initial callback. The callback may
mutate another collection, clear or remove its own selection, update a label,
text box, checkbox, radio group, another ComboBox, or close either window.

`CheckBox::SetChecked()` and native button activation use the same changed-value rule. Programmatic state changes remain legal while disabled; disabled native notifications are rejected. `RadioGroup::Select()` updates the selected index and each affected `RadioButton` before dispatching A's `false` callback and B's `true` callback. Selecting the current member is a no-op. Nested group changes are synchronous and depth-first, and callbacks are looked up/copied at each dispatch boundary so replacement or clearing affects later callbacks. Removing a selected member clears the group; destruction detaches silently to avoid invoking user code from a destructor.

The dispatcher does not retain a registry or child-binding iterator across user code. A close removes only the affected binding, updates live-window accounting, and applies the shutdown policy after the destruction transition. A final-window close posts the normal policy-driven exit; a non-final close leaves the message loop running.

`MenuItem::Invoke()` and native menu selection share the same command dispatcher.
The item remains logically current before callback entry, the callback is copied
once, and nested text/control/menu changes are synchronous and depth-first.
Native accelerator delivery invokes that same dispatcher; disabled or stale
per-window command IDs are ignored.

`Timer` ticks use the same event-loop boundary. A native timer message resolves
only to the weak App Model timer state; the callback is copied before it runs,
and a callback may stop, restart, change the interval, update controls, or
request application shutdown without the backend retaining a native timer
binding across user code.

## Backend contract

`PlatformBackend` contains only neutral state pointers and operations to show, refresh windows/menus, resize, close, start/stop timers, run, request quit, and shut down. The Windows implementation alone converts UTF-8 to UTF-16, creates native controls and ordinary menus, handles native messages and accelerators, calculates client-preserving frames, owns native handle association, synchronizes list mutations with narrow native operations plus a reset fallback, synchronizes combo items and selection through private drop-down messages, routes thread-owned timer messages, and calls `IsDialogMessageW` for narrow dialog-style Tab navigation. Shutdown destroys remaining bindings, clears timer/menu/accelerator state, and unregisters the private class idempotently.

The Windows backend realizes the model's recursive placements without owning a
second layout policy. Controls use the calculated rectangle in flattened
depth-first insertion order, which also preserves native Tab order. `EnableWindow`
synchronization is shared by all interactive controls that expose enabled state.

## Measurement boundary and future Server readiness

Measurement is intentionally a combination of three layers:

- The public App Model owns `LayoutSize` and layout-level natural/minimum
  queries. These values are logical non-negative integers, not pixels, device
  units, or DPI-scaled public types.
- The internal App Model contract owns the meaning of natural size, minimum
  usable size, recursive layout composition, spacer semantics, and the
  constrained allocation policy. It accepts a backend-neutral
  `LayoutMeasurementProvider`.
- A platform backend may provide realized-control information. Windows uses
  the actual font assigned to each native child for text extent and font
  height, adds private native indicator/padding conventions, and falls back to
  bounded neutral values when a native metric is unavailable. ComboBox uses a
  fixed bounded preferred width and a native-font-derived closed height; item
  text does not widen the form. No public header
  includes a Windows SDK type.

A future guideXOS Server backend would need to provide, for each realized
control, a preferred width/height and a minimum usable width/height in the
same logical integer units. It would also need a text measurement primitive or
font metrics for the current compositor font, plus bounded conventions for
button padding, choice indicators, edit height, and list viewport size. It does
not need to expose or emulate `HWND`, `HDC`, `HFONT`, `SIZE`, `RECT`, Win32
messages, or Windows DPI structures. The read-only Server review found
compositor text drawing plus bitmap/system-font helpers in its existing
branches, so supplying these inputs is plausible, but it found no shared
preferred-size API; shaping, fallback, and control padding would need to be
made explicit by that future backend. The repository does not implement or
modify Server in this milestone.

The contract is therefore plausibly portable: a Server implementation can
feed the same recursive measurement and allocation functions with synthetic or
compositor measurements. Before that backend is started, it should settle its
font fallback and text shaping policy, ensure its logical units match the
contract, and decide whether its list viewport convention is the same bounded
default. None of those assumptions are currently Win32-visible in App Model
headers or sample sources.

## Validation coverage

`appmodel_model_test` covers neutral control and callback behavior, including UTF-8 validation, identical-value semantics, self-normalization, cross-box updates, cleanup, and cross-application layout rejection. `appmodel_layout_test` covers compatible default vertical behavior, horizontal/nested geometry, natural and expanding items, minimum-aware shrink/clipping, spacing, undersized and empty layouts, resize recalculation, ownership, cycle rejection, cross-application rejection, and close/reopen. `appmodel_layout_measurement_test` feeds deterministic synthetic measurements into the neutral allocator and covers zero/invalid sizes, natural/minimum composition, both orientations, expansion, minimum-aware shrinking, below-minimum clipping, padding/spacing, nested layouts, dynamic remeasurement, stability, and extreme bounds without HWNDs. `appmodel_lifecycle_test` covers live-window accounting, idempotent show/close, text callbacks that update labels and other boxes, duplicate close requests during text dispatch, retained text across native recreation, backend detachment, and final-window policy exit. `appmodel_listbox_model_test` covers collection indexes, duplicate and Unicode items, selection shifts, invalid indexes, event suppression, callback replacement, reentrant selection, list mutation from callbacks, and selected-item removal/clear behavior. `appmodel_listbox_lifecycle_test` covers multiple list boxes in one and multiple windows, cross-control updates, close-own/close-other callbacks, retained state, and native recreation. `gui_smoke.ps1` preserves the HelloApp regression coverage. `multi_window_gui_smoke.ps1` drives three complete real-window cycles covering both-direction control updates, duplicate-open prevention, button and native close paths, reopen, primary-close-with-secondary-alive, final-window exit code, and orphan-process checks. `text_input_gui_smoke.ps1` drives three TextInputApp cycles covering visible edit controls, initial text, native edit notifications, preview updates, Unicode, Backspace/Delete, Tab and Shift+Tab, button keyboard paths, clear, close-after-edit, and process cleanup. `listbox_gui_smoke.ps1` drives three ListBoxApp cycles covering visible native list content, Unicode and duplicates, native selection notifications, mouse/message selection, Up/Down, callback output, Tab focus, Unicode insertion, indexed removal, clear, repeated mutations, close-after-selection, and process cleanup. The GUI scripts report when the desktop requires deterministic native-message fallback; physical keyboard validation should then be checked manually.

`appmodel_focus_model_test` covers invalid initial focus, programmatic focus
for every interactive control, control identity and TextBox capability,
disabled/unrealized/detached controls, callback reentrancy, close/reopen,
multiple-window focus transfer, and weak-reference invalidation. The
`focus_command_gui_smoke.ps1` suite drives five Debug and three Release cycles
covering routed TextBox commands, menu-open state refresh, all supported
interactive control identities, Tab/Shift+Tab, multiple windows,
close/reopen, and orphan-process cleanup. It uses deterministic native-message
fallbacks where desktop focus injection is unavailable; physical mouse and
keyboard validation remains recommended.

`appmodel_clipboard_test` uses the real Windows Unicode clipboard for ASCII,
accented Latin, non-Latin text, emoji, empty text, bounded long text,
replacement, invalid UTF-8, embedded NUL, oversize rejection, no-text
behavior, button/menu/check/combo/close callbacks, multi-window visibility,
close/reopen independence, and text backup/restore. Its separate-process
contention probe is skipped when the desktop session cannot expose the helper
as clipboard owner; the backend policy remains bounded in all cases. The
`clipboard_gui_smoke.ps1` suite runs five serial Debug cycles and at least
three Release cycles, covering exact button/menu copy/read/paste/clear,
Unicode/emoji, empty text, replacement, enabled-state changes, resize, normal
close, and orphan-process cleanup. Tests can restore only an original
supported text value; arbitrary non-text clipboard formats are not preserved.

`appmodel_choice_model_test` covers default and initial checkbox state, changed/unchanged transitions, callback replacement/removal, enabled state, self/cross-control changes, Unicode, explicit group membership, exclusivity, callback order, group queries during callbacks, independent groups, reentrant selection, removal/destruction, and cross-application rejection. `appmodel_choice_lifecycle_test` covers two-window native routing, disabled native rejection, programmatic disabled updates, independent groups, close/reopen, callback close of both windows, and retained state. `choice_controls_gui_smoke.ps1` drives three ChoiceControlsApp cycles covering visible and initial choices, native and programmatic checkbox changes, focus/Tab and Space paths, native/programmatic radio selection, independent groups, UTF-8 labels, disabled interaction, repeated cycles, close-after-dispatch, and process cleanup. The GUI scripts report when deterministic native-message/focus fallback is used; physical mouse and keyboard validation should then be checked manually.
`profile_manager_model_test` covers serialization/deserialization, Unicode,
newlines, duplicate names, all modes and enabled states, version and malformed
input rejection, parser bounds, parse-failure isolation, dirty transitions,
and current-path behavior. `appmodel_file_api_test` covers ASCII/Unicode/emoji
paths, empty files, BOM handling, invalid UTF-8, missing/oversized files,
replacement, and temporary cleanup. `profile_manager_gui_smoke.ps1` adds
three Debug persistence cycles covering Save As/Save/Open, Unicode and
duplicate preservation, dirty New confirmation, malformed-file errors,
cancellation, overwrite, and no orphan process or leftover persistence
directory. The GUI scripts report when deterministic native-message/focus
fallback is used; physical mouse and keyboard validation should then be
checked manually.

`appmodel_tooltip_statusbar_test` covers empty/default state, UTF-8 and
invalid-text rejection, the 8 KiB tooltip bound, live replacement and clear,
detachment, control/layout recreation, native tooltip enumeration, dynamic
callback updates, status replacement/clearing, menu coexistence, content-size
preservation, bottom geometry, two-window separation, and close/reopen. The
`polish_gui_smoke.ps1` suite drives five Debug and three Release cycles for
native control/status realization, model-reported status transitions,
tooltip assignment/clear/reassignment, larger/smaller resize, bottom
alignment, content non-overlap, close, and orphan-process cleanup. Its
in-process status/tooltip report is test-only; native pointer-message
inspection remains in the same-process C++ integration test.

`appmodel_window_closing_test` covers the neutral close contract: allow/cancel,
replacement/clearing, recursive self-close, close-other-window, close/reopen,
live-window accounting, explicit shutdown, and callback-driven quit.
`profile_manager_close_gui_smoke.ps1` drives three native close cycles covering
clean close, Cancel, No, Yes with an existing path, Yes with Save As, Save As
cancellation, save failure, and duplicate requests. Physical caption-X,
Alt+F4, and system-menu activation remain recommended manual validation in
addition to deterministic native-message automation.

`appmodel_combobox_model_test` covers the shared collection/selection contract,
UTF-8, duplicate indexes, callback replacement/removal, model-first
reentrancy, mutation from callbacks, enabled state, and selection clearing.
`appmodel_combobox_lifecycle_test` proves per-window native combo routing,
cross-control updates, close/reopen retention, and close-own/close-other
behavior. `combobox_gui_smoke.ps1` covers popup open/close, native and keyboard
selection, focus/Tab, programmatic selection, insertion, Unicode and duplicate
indexes, removal, clearing, disabled interaction, resizing, close-after-
selection, and process cleanup.

`appmodel_menu_model_test` covers empty bars, hierarchy and separators,
duplicate labels, cycle and multi-parent rejection, cross-window/application
affinity, UTF-8 validation, callback replacement/clearing, enabled/checked and
shortcut state, duplicate-shortcut rejection, reentrant control/menu changes,
detachment suppression, close-own/close-other callbacks, shutdown, and
close/reopen retention. `menu_gui_smoke.ps1` covers native File/Edit/Help
menus, nested submenus, duplicate-label routing, dynamic enabled state,
check/ComboBox/status updates, UTF-8 realization, mnemonic markup, accelerator
display/registration, close-after-command, three Debug cycles, and process
cleanup. `multi_window_gui_smoke.ps1` additionally proves identical Actions
menu shapes route independently to both windows and that the surviving window
remains functional after the other closes.

`appmodel_dialog_model_test` covers UTF-8 validation, empty filter rejection,
unrealized and closed owner rejection, and post-shutdown owner rejection.
`dialog_app_gui_smoke.ps1` controls a temporary directory outside the source
tree and verifies MessageDialog OK, Yes, No, and Cancel outcomes; button,
menu, checkbox, and ComboBox callback invocation; Unicode open selection;
Unicode save selection; open/save cancellation; repeated invocation; exact
returned UTF-8 paths; no application file write; parent usability; and clean
process exit. Deterministic native close-message automation covers the
App Model close-request path; physical caption-button, Alt+F4, and system-menu
activation remain recommended manual validation.

`appmodel_text_box_test` covers scalar indexing for ASCII, accented Latin,
Japanese, and supplementary-plane emoji; ordered/empty/full Unicode ranges;
selected-text extraction; SetText reset behavior; Copy/Cut/Paste/DeleteSelection;
empty-clipboard behavior; one-event mutation semantics; reentrant callback
changes; close/reopen retention; and multi-window copy after source closure.
`text_editing_gui_smoke.ps1` runs five deterministic native GUI cycles for
programmatic caret/selection display, exact selected clipboard text, Unicode
replacement, menu commands, targeted native selection synchronization,
multi-TextBox copy/paste, close/reopen, and orphan-process cleanup. It does not
claim to automate physical mouse dragging or keyboard-layout-dependent Ctrl
input.

## Current limitations

The implementation targets Windows desktop x64 and one UI thread. File I/O is
limited to bounded complete UTF-8 text reads/writes for small documents; there
is no directory enumeration, copy/move/delete API, stream/async/watch API,
locking, autosave, recovery, recent-files system, or generic document
framework. Layout is
intentionally limited to vertical/horizontal stacks, natural/expand sizing,
equal expansion, spacing, padding, minimum-aware clipping, and spacers. It has
no grid/table, weights, percentages, anchors, public min/max constraints,
scroll container, designer metadata, or responsive breakpoint system. Neutral
text widths remain bounded logical fallbacks, while the Windows backend can use
the assigned native font and text extent; there is no public text-measurement
or DPI abstraction. `TextBox` is intentionally single-line and exposes no
multiline document, rich text, password mode, grapheme-cluster or word
selection, selection-changed event, undo/redo, context menu, drag/drop,
clipboard history, image clipboard, or formatting contract. Focus is limited
to the current window-scoped child query and programmatic focus request; there
is no focus scope, custom traversal order, focus-changed event, or generic
command framework.
`ListBox` and `ComboBox` are intentionally single-selection and expose no
multi-select, custom item payload, sorting, virtualization, owner draw, icons,
checkbox, or drag-and-drop contract. ComboBox is intentionally non-editable and
has no autocomplete or custom popup contract. Choice controls intentionally exclude tri-state
behavior, toggle switches, visual group boxes, custom drawing, and inferred or
 cross-application grouping. The menu foundation intentionally excludes
context menus, toolbars, icons, owner draw, recent-files behavior, global
application menus, tree views, tables, modal behavior beyond the synchronous
native dialog contract,
 theming, accessibility work beyond native controls, packaging, threading, background dispatch, multiple
applications within one process, C# bindings, Linux, and production ABI
stability.

## Window-level shell file drops

File drops are deliberately a Window event, not a control or general-purpose
drag/drop subsystem:

```text
Windows shell
    |
    | WM_DROPFILES / HDROP (private)
    v
WindowsBackend::WindowBinding
    |
    | copy, UTF-16 -> UTF-8, bounds, DragFinish
    v
WindowState::onFilesDropped
    |
    v
FileDropEvent { const vector<string>& GetFiles() }
```

The public `FileDropEvent` contains only UTF-8 filesystem paths. Windows paths
are absolute native filesystem paths under the same contract as the file API
and dialogs. The event does not promise that a path exists or is a regular
file; directories are surfaced when the shell supplies them. Shell order is
preserved, duplicates are retained, and no sort or deduplication occurs.

The first backend uses `WM_DROPFILES` because it is a narrow, synchronous
filesystem-path mechanism and avoids introducing COM/OLE `IDataObject` and
`IDropTarget` infrastructure before child targets or richer formats are in
scope. `DragAcceptFiles` is enabled only while a callback is registered on a
realized Window. Clearing the callback disables it. Closing destroys the
native binding and detaches the native acceptance; the logical callback stays
on `WindowState`, so a later `Show()` creates a fresh association if the
callback remains installed. Routing begins with the originating top-level
binding, so two windows cannot cross-dispatch a drop.

Before dispatch, the backend copies every accepted path and calls
`DragFinish`. The current bounds are 1,024 accepted paths, 256 KiB per UTF-8
path, and 4 MiB aggregate UTF-8 path bytes. Empty paths, invalid conversion,
or paths over the individual bound are ignored; once the aggregate bound would
be exceeded, later paths are ignored; count processing is capped in source
order. The callback is copied before invocation and runs synchronously on the
UI thread with a fully immutable event payload. It may update controls, open
synchronous dialogs, read files, replace/clear its callback, or close either
Window. No native pointer is retained after callback entry, and no App Model
focus change is invented by a drop.

`FileDropApp` demonstrates append semantics, duplicate/order preservation,
Unicode display, bounded `File::ReadAllText` preview, and invalid-UTF-8 error
handling. `ProfileManagerApp` accepts exactly one `.gxprofiles` path and calls
the same application-level `OpenDocumentFromPath` used by File > Open. Its
parse-before-replace behavior preserves the current document on malformed
input; dirty drops use Yes/No/Cancel with Yes-save, No-discard, and Cancel-
abort semantics. The native integration test sends a real in-process
`DROPFILES` payload. The GUI smoke suite uses a test-only in-process agent for
deterministic injection and therefore does not claim physical Explorer drag
validation. Explorer, desktop, multi-select, cursor feedback, elevation,
Remote Desktop, and other physical shell behavior remain manual validation.

This milestone intentionally excludes child-control targets, custom drag
sources, drag-over events, effects, move/copy/link negotiation, URI/text/image
drops, arbitrary clipboard formats, drag images, context menus, and OLE data
objects. Those are future expansions rather than hidden assumptions of the
Window path-drop event.

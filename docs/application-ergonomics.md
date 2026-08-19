# ProfileManagerApp application ergonomics

ProfileManagerApp keeps its business model separate from App Model objects:
`ProfileManagerModel` owns profiles, IDs, drafts, and selection. The small
`ProfileManagerDocument` wrapper adds the current path, overall document dirty
state, serialization, and parse-before-replace behavior. `ProfileManagerWindow`
translates that state into public controls, menus, dialogs, and callbacks.

The improved layout API materially improves the application code. The form no
longer encodes visual order as one long vertical list: two horizontal rows pair
`Name` and `Description` labels with expanding text boxes, the ListBox receives
explicit flexible vertical space, and the three commands plus a spacer form a
single action row.

```cpp
Layout nameRow(Orientation::Horizontal, 0, 8);
nameRow.Add(nameLabel);
nameRow.Add(name, LayoutSizing::Expand);

Layout actions(Orientation::Horizontal, 0, 8);
actions.Add(newButton);
actions.Add(saveButton);
actions.Add(deleteButton);
actions.AddSpacer();

content.Add(nameRow);
content.Add(profiles, LayoutSizing::Expand);
content.Add(actions);
```

This is a small, visible improvement rather than a new application framework:
the controller still owns explicit refresh methods and a `loading_` guard,
because those are business-state synchronization decisions and not layout
responsibilities.

## ToolTips and StatusBar evaluation

ToolTips improve discoverability for the six profile-facing actions and fields
without adding another explanatory row to the form. The concise messages are
available on demand and do not change natural sizes, so the ProfileManager
layout remains as readable when tips are absent. The shared control property is
pleasant at the call site:

```cpp
profiles.SetToolTip("Select a profile");
saveButton.SetToolTip("Save changes to the current profile");
```

`StatusBar` removes the redundant in-content status `Label` from
ProfileManager. That makes status feedback independent of the form's vertical
composition and gives the ListBox and editor rows the available content space.
The attachment is also readable and lifecycle-safe:

```cpp
StatusBar status("Ready");
window.SetStatusBar(status);
status.SetText("Changes saved");
```

Attaching window chrome objects is pleasant because `SetStatusBar()` and
`ClearStatusBar()` match the existing `SetMenuBar()`/`ClearMenuBar()` pattern,
while the object retains its text across close/reopen. The main friction is
intentional: a status bar is one text part only, and tooltip timing/style and
native hover placement are not configurable yet. GUI automation can prove
native registration and model updates, but actual hover appearance still needs
manual validation.

## File/document workflow

ProfileManager uses the menu foundation for the complete document workflow:

```text
File
  New
  Open...
  Save
  Save As...
  Exit
```

The visible `New`, `Save Changes`, and `Delete` buttons remain. `Save Changes`
commits the selected editor into memory; `File > Save` writes the complete
collection. `File > New` asks whether to save a dirty collection with a
Yes/No/Cancel message dialog. `File > Exit` simply requests `window.Close()`.
The window's `OnClosing` callback supplies the same Yes/No/Cancel policy to
File > Exit and caption X, Alt+F4, and system-menu Close.
`ProfileManagerDocument::IsDirty()` includes pending editor changes, so a
Yes response commits the editor before serialization. No discards the
current document; Cancel, picker cancellation, and save failure leave the
window and editor usable. `File > Open...` and Save As use the native pickers,
and Open calls the same `OpenDocumentFromPath` routine used by file drops.
Open cancellation and malformed input leave the current document unchanged.

The close policy is an ordinary App Model event rather than a ProfileManager-
specific native message handler:

```cpp
window.OnClosing([&](WindowClosingEvent& event) {
    if (!ConfirmDocumentCanClose()) event.Cancel();
});

exitItem.OnInvoked([&] { window.Close(); });
```

`Window::Close()` invokes the callback. The callback is synchronous and may
show the existing synchronous dialogs. A recursive close request for the same
window is ignored while the callback is running; an allowed request destroys
the native realization once, while a canceled request leaves it shown.

## Window-level file drops

The application can opt into shell file drops without knowing anything about
Windows messages or native handles:

```cpp
window.OnFilesDropped([&](const FileDropEvent& event) {
    for (const auto& path : event.GetFiles()) {
        status.SetText(path);
    }
});
```

Registering a callback enables file drops for that realized top-level Window;
clearing it disables acceptance. The callback is synchronous on the UI thread,
receives an immutable event containing ordered UTF-8 absolute Windows paths,
and may update controls, show synchronous dialogs, read files, replace the
callback, or close either Window. The callback remains logically installed
across close/reopen, while each native realization gets a fresh association.
Empty or invalid paths are ignored. The bounds are 1,024 paths, 256 KiB per
path, and 4 MiB aggregate path bytes; order and duplicates are preserved.
Directories are surfaced if the shell supplies them, because the event
represents filesystem paths rather than validated regular files.

`ProfileManagerApp` accepts one dropped `.gxprofiles` file and reuses
`OpenDocumentFromPath` for its existing parse-before-replace and dirty-document
Yes/No/Cancel policy. Multiple drops and wrong extensions are rejected with a
concise dialog. `FileDropApp` appends dropped paths to a ListBox and exercises
bounded UTF-8 preview and error handling. The automated suite injects the
native payload through a test-only in-process agent; physical Explorer drag
validation remains a recommended manual check.

## Dialog and file evaluation

The new dialog foundation fits the next ProfileManager workflow without
requiring a business-model change:

```cpp
const auto result = MessageDialog::Show(
    window, "Delete this profile?", "Confirm",
    MessageDialogButtons::YesNo, MessageDialogIcon::Question);
if (result == MessageDialogResult::Yes) SaveDocument();

OpenFileDialog open;
open.SetTitle("Open Profile");
open.AddFilter("guideXOS Profile", {"*.gxprofile"});
const auto path = open.Show(window);
```

`MessageDialog`, `OpenFileDialog`, and `SaveFileDialog` are synchronous, use
UTF-8 strings and paths, and require a realized owner `Window`. Cancellation
is an ordinary `std::nullopt`/`MessageDialogResult::Cancel` outcome; native
failure is exceptional. Filters are copied as a description plus wildcard
patterns, and Windows Common Item Dialog/COM details remain private. This
keeps the future `File > Open...` and delete-confirmation commands readable
The new `File` utility keeps persistence equally small:

```cpp
const std::string contents = File::ReadAllText(path);
File::WriteAllText(path, contents);
```

Paths and contents are UTF-8; the utility accepts one BOM on read, writes
without a BOM, rejects invalid UTF-8, bounds whole-file operations to 16 MiB,
and privately writes/flushes a same-directory temporary before replacement.
ProfileManager's `.gxprofiles` format, version marker, parser bounds, and
length-prefixed text fields are documented in
[`profile-manager-format.md`](profile-manager-format.md). This is an
application-level document controller, not a generic document framework.

## Clipboard evaluation

`Window::GetFocusedControl()` now provides the small platform-neutral command
target that the multi-editor form needed. It returns a weak `ControlRef`; an
expired or detached reference is invalid rather than dangling. The focused
control can be recognized with `AsTextBox()` and the resulting `TextBoxRef`
supports only the existing edit operations. ProfileManager's Edit menu uses
that query for both the Name and Description fields, so the application has no
last-clicked-field workaround.

`Menu::OnOpening` refreshes the dynamic enabled state immediately before the
menu is shown. Cut, Copy, and Delete require a non-empty selection; Paste
requires a focused TextBox and supported text on the system clipboard; Select
All requires a focused TextBox with text. Handlers query the target again and
no-op safely if reentrancy closes or detaches it. `TextEditingApp` now uses the
same focus query, while `FocusCommandApp` demonstrates the contract across all
interactive control types and two windows.

## TextBox selection and command boundary

TextBox indexes use Unicode scalar values, matching the UTF-8 model string and
remaining independent of Windows UTF-16 code units. Ranges are ordered and
half-open:

```cpp
TextRange range = editor.GetSelection();
editor.SetSelection({range.start, range.length});
editor.Copy();
editor.Cut();
editor.Paste();
editor.DeleteSelection();
```

`SetText` resets the caret to the scalar end and clears selection. The default
caret is also the constructor-text end. `SetSelection` puts the caret at the
range end; `ClearSelection` collapses to that end; Cut and DeleteSelection
collapse to the selection start; and Paste leaves the caret after inserted
text. Empty Copy and Paste without supported clipboard text are no-ops.
`GetSelectedText` returns exact UTF-8 text, including supplementary-plane
characters. There is no selection-changed callback yet, and grapheme-cluster,
word, multiline, rich-text, undo, context-menu, drag/drop, and image-clipboard
behavior remain outside this milestone.

These commands preserve the existing synchronous `OnTextChanged` rule. One
logical Cut, Paste, or DeleteSelection mutation emits one event; callback entry
already sees updated text, caret, and selection. Native typing, arrows,
Shift-selection, mouse selection, Home/End, Ctrl+A, and native clipboard edit
paths synchronize into later model queries through the private Windows
backend. Closing and reopening a window retains logical TextBox state.

## What worked well

- The public API remains sufficient to build the complete utility without
  Win32 types in application code.
- Nested layouts make ordinary form relationships readable at the point where
  controls are composed.
- `Expand` expresses intent directly: text boxes fill their row and the ListBox
  grows with the window without application resize callbacks.
- A spacer gives the action row a simple toolbar-like trailing region without a
  native placeholder window.
- ListBox indexes combined with application-owned `ProfileId` values make
  duplicate display names straightforward and safe.
- Synchronous callbacks make the controller easy to reason about: the model is
  current when each callback starts, and delete/new/save operations can mutate
  the list from inside command callbacks.
- `SetText`, `SetChecked`, and `RadioGroup::Select` allow the controller to
  repopulate the editor naturally. The application-level `loading_` guard
  suppresses feedback from those programmatic assignments.
- UTF-8 text, enabled state, close/reopen, and native realization remain
  backend concerns rather than application code.
- TextBox scalar selection and control-level clipboard commands compose cleanly
  with `ControlRef::AsTextBox()`. A window-scoped weak focus identity keeps
  ProfileManager's routed Edit commands safe across close/reopen and callback
  reentrancy without adding a generic command framework.
- Native text measurement gives the Name/Description labels and action buttons
  preferred widths that follow their current labels, while the text boxes keep
  a stable normal edit width and the ListBox remains a useful expanding
  viewport.

## Awkward but acceptable

- The sample still has explicit `RefreshEditor`, `RefreshCommands`, and
  `loading_` methods. This is ordinary controller code for a pending-edit form,
  not a reason to add data binding.
- Natural widths remain backend-informed but bounded, and the application does
  not control per-control minimums or weights. The small sizing model is enough
  for this form but not for dense, highly constrained interfaces.
- Radio mode conversion is explicit application code (`ProfileMode` to a member
  button), which keeps the application model independent of control ordering.
- Pending editor text is separate from document dirty state. Selection still
  discards pending edits deterministically; file commands apply the explicit
  Open/New/Save policy documented above.

## App Model deficiencies discovered

The previous milestone exposed and corrected a private Windows-backend text
buffer sizing issue. This measurement milestone did not require a public
control measurement method or a business-model change. The layout now has an
explicit natural/minimum contract: labels, buttons, and choices may use the
realized Windows font/text extent; text boxes use a conventional single-line
preferred width and height; and ListBoxes use a bounded default viewport.
The neutral fallback remains deterministic for geometry tests and future
backends.

## Changes made during this milestone

- Added platform-neutral vertical/horizontal nested layout composition.
- Added natural/expand sizing, spacing, padding, deterministic geometry, and
  lightweight expanding spacers.
- Added recursive natural/minimum `LayoutSize` measurement, minimum-aware
  constrained allocation, and a private backend measurement provider.
- Rejected control multi-parenting, layout multi-parenting, direct/indirect
  cycles, cross-application contamination, and multiple window content owners.
- Refactored ProfileManagerApp into nested field and action rows.
- Extended the neutral layout tests and ProfileManager native smoke scenario to
  inspect resize geometry as well as business interaction.
- Added ProfileManager's File menu, bounded UTF-8 file utility, versioned
  `.gxprofiles` serializer, document state, dirty-title behavior, and native
  picker/error workflow while retaining the visible profile buttons.
- Added platform-neutral `ControlRef`/`TextBoxRef` focus identity and
  `Window::GetFocusedControl()`, with deterministic programmatic `Focus()` for
  interactive controls.
- Added synchronous `Menu::OnOpening` refresh, routed Edit commands to
  ProfileManager and TextEditingApp, and added the FocusCommandApp proof sample
  plus model/native focus coverage.
- Added shared UTF-8 ToolTip properties, one-part window StatusBar chrome, the
  PolishApp proof sample, and native geometry/lifecycle coverage.

## Measurement and resize behavior

The sample relies on the contract rather than hand-tuned widths. A natural
child receives its preferred main-axis size when the row has room. An expanding
TextBox receives that preferred width plus the remaining row space, while a
button keeps a bounded usable minimum if the window is constrained. The action
row's three buttons remain text-sized and its spacer receives only leftover
space; it does not force equal button widths. The ListBox's default `Expand`
sizing keeps it as the primary vertical consumer.

Measurements are recomputed during `CalculateGeometry()` and every live backend
refresh/layout pass. Changing a label, button, checkbox, or radio label cannot
leave its prior natural width cached. Resize therefore covers normal, larger,
and constrained windows through the same allocator. Closing and reopening the
window recreates native controls, reassigns their font, and measures again from
the retained model state. Unicode labels remain UTF-8 in the model and are
converted privately before Windows text measurement.

The current limitations are deliberate: no wrapping, multiline text, public
preferred-size setters, weighted expansion, per-control min/max API, DPI API,
theme API, or exact cross-platform font parity. A future Server backend would
provide equivalent text/font and bounded control metrics to the internal
provider without changing ProfileManagerApp.

## Ideas intentionally deferred

## ComboBox evaluation

The new non-editable `ComboBox` would be a compact alternative for a field
with several mutually exclusive modes such as the ProfileManager mode
selector. It is not an automatic replacement here. The existing
`ProfileManagerApp` deliberately keeps its `RadioGroup`: the three mode labels
are visible at once, the sample continues to provide useful RadioButton
coverage, and the current form does not have enough modes for the drop-down to
be an obvious usability improvement. ComboBox is therefore documented and
covered by its own sample, while ProfileManager remains unchanged until a
real mode list or form-density requirement makes the compact presentation
clearly preferable.

- Grid/table layout, weights, percentages, anchors, min/max expressions,
  responsive breakpoints, scroll containers, and designer metadata.
- Generic validation frameworks, data binding, observable collections, MVVM,
  and reusable document abstractions.
- Generic command objects, command bubbling, command registries, focus scopes,
  custom Tab order, and focus-change notifications.
- Context menus, toolbars, modal windows, background dispatch, threading, and
  cross-platform realization.

## Representative application behavior

The controller shape remains a small callback-driven translation layer:

```cpp
profiles.OnSelectionChanged([&](std::optional<std::size_t> index) {
    if (!index) {
        model.ClearSelection();
        RefreshEditor();
        status.SetText("Status: No profile selected");
        return;
    }

    const bool discarded = model.IsDirty();
    model.SelectIndex(*index);
    RefreshEditor();
    status.SetText("Status: " + model.GetDraft().name + " selected" +
                   (discarded ? "; unsaved changes discarded" : ""));
});
```

Layout composition improves the form's structure and resize behavior without
changing that business/controller boundary.

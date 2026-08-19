# guideXOS Server App Model investigation

Investigation date: 2026-08-06

All Server repositories below were inspected read-only. None was modified for this milestone.

## Locations inspected

- `D:\dev\guideXOSServer`
- `D:\dev\guideXOSServerV0.2`
- `D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO`
- `D:\dev\guideXOSServerV1.1_DOTNET_SUPPORT`

The current primary source was `D:\dev\guideXOSServer` on branch `main` at `89ba78b`. The compared branches were `v0.2` at `d43438c`, `v0.5_DEVELOPER_STUDIO` at `6799c18`, and `v1.1_DOTNET_SUPPORT` at `5ccd6c1`. `guideXOSServerV0.2` had pre-existing worktree edits; they were left untouched.

## Application registration, identity, and launch

The current hosted Server App Model is primarily an application discovery and launch model:

- `app_manifest.h` defines `AppManifest`, `AppKind`, `AppEntry`, `DefaultWindow`, permissions, file associations, and per-architecture entries.
- `app_manifest_loader.cpp` loads `app.json`; `app_manifest_validator.cpp` validates schema, identity, architectures, entries, and relative paths.
- `app_registry.h/.cpp` scans manifest sources and stores `RegisteredApp` records. The documented sources include `/system/apps`, `sdk/samples`, `examples/apps`, `/Apps`, and `/users/default/apps`.
- `built_in_app_metadata.h` supplies canonical built-in identities, display names, launch names, aliases, availability, and default window metadata. Hosted built-ins are mirrored as synthetic manifest records, but they still use hard-coded hosted dispatch.
- `app_launch_target.h` defines a typed diagnostic `LaunchTarget` with built-in, manifest, native ELF, GXApp package, shell action, alias, file-open, service, and other target kinds.
- `app_launch_resolver.h/.cpp` resolves a registered manifest to a strategy such as `BuiltIn`, `NativeElf`, `GXAppPackage`, `Service`, `HypervisorGuest`, or `Script`.
- `desktop_service.cpp` remains hybrid: built-ins dispatch through a concrete `if/else` launcher table, Native ELF is experimental, and manifest-driven GXApp package launch is still a placeholder.
- Bare-metal uses a separate `kernel::AppManager` and `registerKernelApps()` path rather than the hosted `AppRegistry`.

The current identity direction is recognizable and useful for Windows: stable application IDs, display names, canonical launch names, aliases, source metadata, supported architecture, and default window dimensions. It is not yet a shared implementation across hosted and bare-metal targets, so the Windows project uses only a small UTF-8 application ID value and does not link Server headers.

## Windows, window, control, layout, event, and lifecycle contracts found

Server has several related but tightly scoped GUI mechanisms rather than one public object App Model:

- `gui_protocol.h` defines compositor messages for create, close, move, resize, title, invalidation, input, widget add/event, and frame presentation.
- `compositor.h` defines private `WinInfo` window state and a `Widget` structure. The current widget enum contains a button, with integer coordinates, text, hover, and pressed state. `WinInfo` also contains title, geometry, text/draw lists, widget lists, minimized/maximized/visibility state, and persistence details.
- `compositor.cpp` performs hit testing, title/window management, layout and event emission. `WM_*` handling and native display details are server internals, not a reusable application-facing C++ contract.
- `sdk/include/guidexos/abi.h` defines C ABI event types for window close/focus/blur, key, mouse, and paint, plus host calls such as `request_window`, `draw_text`, `draw_rect`, `poll_event`, `wait_for_close`, and `exit`.
- `sdk/include/guidexos/ui.h` adds C helper functions such as `gx_draw_label`, `gx_draw_button`, and simple rectangle helpers. The ResourceViewer sample uses this low-level draw-and-poll ABI, not `Application`, `Window`, `Label`, `Button`, or `Layout` C++ objects.
- `lifecycle.h/.cpp` describes Server bootstrap phases (`ColdStart` through `Interactive`, `ShuttingDown`, and `Stopped`) and compositor/console service startup. It is an OS-server lifecycle, not a portable per-application UI loop.
- Individual hosted apps such as `calculator.h/.cpp` and `console_window.h/.cpp` expose static `Launch()` methods and implement their own IPC/message loops. Their button/event handling is app-specific and depends on `ipc_bus.h`, `process.h`, and `gui_protocol.h`.

Server therefore has recognizable concepts for this Windows slice—application identity, window metadata, controls, button click events, paint/close events, and lifecycle—but the actual window/control ownership and event processing are compositor/server internals.

## Reuse decision

No existing Server shared header or library is safe to use directly for this first native Windows object API:

- `app_manifest.h` and the registry headers include filesystem and Server launch policy details.
- `gui_protocol.h` includes IPC and serialized compositor messages.
- `compositor.h` includes Server services, IPC, image types, process state, persistence, and conditional Windows types.
- `guidexos/abi.h` and `guidexos/ui.h` are useful reference contracts for event names and drawing ideas, but they are a freestanding C ABI for Server's experimental Native ELF path, not a stable C++ UI object layer.
- `lifecycle.h` owns Server bootstrap services and cannot be used as an application runtime without importing unrelated server behavior.

The Windows repository consequently mirrors a stable subset in new public headers and documents the intentional difference. It has no source dependency on any sibling repository and no copied Server implementation structure. The first backend uses standard Win32 child controls so the App Model boundary is proven independently of the guideXOS compositor.

## Alignment and intentional differences

Aligned concepts:

- reverse-domain application identity
- application registration at the root of the application model
- window title and default/client dimensions
- label and button control concepts
- ordered layout/content relationship
- close and click event concepts
- explicit lifecycle and orderly shutdown
- future target-specific backend realization

Intentionally different for this foundation:

- one explicit `Application` object owns a local event loop instead of Server's global lifecycle and process table
- C++ value/model objects replace Server's serialized IPC widget messages
- Win32 owns native repaint, focus, resize, minimize, maximize, and close behavior instead of the guideXOS compositor
- UTF-8 public strings are converted to UTF-16 only inside the Windows backend
- no manifest scanning, package launch, ELF loading, permissions, sandboxing, C# binding, or Server dependency is introduced

This is a small stable subset rather than a claim that the Server App Model is already a cross-platform UI API.

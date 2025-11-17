# Openbox Developer Notes

## Directory Overview

* **openbox** — core window manager code
* **obrender** — `librender`, shared rendering routines
* **obt** — shared utility library (display, xml, keyboard, xqueue)
* **data** — default configuration, themes, menu files
* **tests** — X11 regression tests and unit tests
* **tools** — helper utilities (e.g. obxprop, themeupdate)
* **doc** — documentation (manpages, doxygen config)
* **themes** — shipped themes
* **po** — translations

---

## Coding Standards

* Use modern C and keep functions small and narrow in scope  
* Keep globals to a minimum; prefer passing context explicitly  
* Avoid macros where an inline function is clearer  
* Be strict with `const` correctness  
* Use early returns sparingly; centralize cleanup paths

---

## Error Handling

Favor a single cleanup path per function. When an early failure occurs, jump to a shared label (e.g. `goto out;`) that releases resources in the reverse order they were acquired. This keeps error paths short, avoids duplicate cleanup logic, and makes it obvious where new teardown work belongs. Reserve multiple exit labels for genuinely distinct teardown requirements.

---

## Core Architecture Notes

### Event Flow

The WM follows a straightforward flow:

1. X11 event arrives  
2. Event decoded into an internal enum  
3. Dispatched to subsystems: client, focus, stacking, actions, config, or render  
4. If geometry or stacking changed, layout and drawing layers update

Keep all X11 event intake inside a single dispatcher to avoid duplicated logic.

### Client Object Model

Clients are represented by the `Client` struct:

* `Client.area` tracks the *logical* client rectangle  
* `Client.frame` wraps decorations and tracks the rendered extents  
* `Client.transient` plus `parents`/`transients` track transient relationships  
* `Client.obwin` is the server-side representation of the frame window  
* `Client.desktop` indexes into the workspace manager

The WM should never assume a client is well-behaved; sanitize properties and types.

### Stacking and Focus

* Stacking is managed as a single array or list; keep it consistent  
* Focus is a state machine: normal → click → grab → revert  
* Never focus windows that are unmapped or withdrawn  
* Always check the client’s allowed focus model when handling EnterNotify

---

## Transient Hierarchy

Transience is tracked with explicit fields:

* `transient` — whether the client advertises a transient relationship
* `transient_for_group` — TRUE when the transient-for hint points at Root or when group-level transience is inferred
* `parents` / `transients` — GSLists that form the tree

Group transients must **never** be treated as a real parent pointer. `client_direct_parent()` intentionally returns `NULL` for group-level transients so higher-level code does not walk into invalid pointers or loops. When updating or traversing transient trees, always guard on `transient_for_group` before following parent chains and avoid letting group transients adopt other group transients.

---

## X11 and XCB Practices

Openbox uses Xlib internally with some XCB interop. Modern code should follow these rules:

* Prefer XCB requests where possible  
* Avoid implicit Xlib round-trips  
* Atom lookups must be cached  
* Window property fetches must validate lengths and types  
* Never trust client-sent `ConfigureRequest` blindly  
* Use `XSync` only when correctness requires it, not stylistically  

Whenever adding new low-level behavior, keep it in `openbox/x11/` instead of scattering helpers around unrelated modules.

---

## Window Coordinates and Sizes

When using coordinates or sizes of windows:

* `Client.area` — reference point and size of the *client* window  
  This is not what is visibly rendered; gravity is applied to transform it  
* `Client.frame.area` — position and size of the entire frame  
  This is usually the value to use, unless you are inside `client.c` adjusting or using the size from the client’s perspective

Never mix frame metrics and client metrics in the same calculation.

---

## Configuration System Notes

* Configuration XML is parsed into a typed config struct  
* Never directly read XML nodes from runtime code  
* Always validate parsed values before applying  
* Reloads build a new config object, validate it, then atomically swap it in  
* Keep default values well-documented in `data/` and in the parser tables

Actions, chain actions, and keybindings should live in the policy layer, not core window management.

---

## Rendering and Theme Engine Notes

`librender` is a shared component used by both the WM and tools such as obconf.

Guidelines:

* Do not block in render paths; these are called during expose storms  
* Draw everything using cached pixmaps where possible  
* Button states should be stateless: derive from client flags, not internal globals  
* Theme metrics (padding, title height, border widths) must never be queried ad hoc; store them once during theme load

HiDPI support requires resolution-independent geometry; avoid pixel assumptions when writing new code.

---

## Event Loop Efficiency

* Always integrate the X connection file descriptor into a `select()`/`poll()` (or GLib `GSource`) driven loop instead of calling `XPending`/`XNextEvent` in a busy cycle  
* Before painting, drain bursts of `Expose`/`ConfigureNotify` events for the same window  
* Old toolkits redraw once per discrete event, causing visible flicker  
* Use `XCheckTypedWindowEvent`/`XCheckWindowEvent` (or local xqueue helpers) to coalesce events and repaint only once after the queue is empty  
* Keep batching localized: drain only the event types that affect redraw state so unrelated button/key events continue to be delivered quickly

---

## Workspace and Desktop Handling

* Workspace objects should be independent; avoid storing cross-workspace state in the client  
* Moving a window across workspaces must update both the client and stacking layers  
* Keep sticky windows as simple flags; do not duplicate them across desktops  
* EWMH `_NET_NUMBER_OF_DESKTOPS`, `_NET_CURRENT_DESKTOP`, and `_NET_DESKTOP_VIEWPORT` must always reflect the internal state

---

## EWMH Coverage

Openbox advertises the following EWMH hints via `_NET_SUPPORTED` (see `openbox/screen.c`), with optional pieces noted.

| Category | Hints | Status | Notes |
| --- | --- | --- | --- |
| Root & WM metadata | `_NET_SUPPORTED`, `_NET_SUPPORTING_WM_CHECK`, `_NET_WM_FULL_PLACEMENT`, `_NET_ACTIVE_WINDOW`, `_NET_WM_HANDLED_ICONS` | Supported | Support window is created and lowered on startup. |
| Desktop management | `_NET_NUMBER_OF_DESKTOPS`, `_NET_DESKTOP_GEOMETRY`, `_NET_DESKTOP_VIEWPORT`, `_NET_CURRENT_DESKTOP`, `_NET_DESKTOP_LAYOUT`, `_NET_DESKTOP_NAMES`, `_NET_WORKAREA`, `_NET_SHOWING_DESKTOP` | Supported | Values are kept in sync with internal workspace state. |
| Client lists & stacking | `_NET_CLIENT_LIST`, `_NET_CLIENT_LIST_STACKING` | Supported | Updated whenever stacking or desktop membership changes. |
| Root messages | `_NET_CLOSE_WINDOW`, `_NET_MOVERESIZE_WINDOW`, `_NET_RESTACK_WINDOW`, `_NET_REQUEST_FRAME_EXTENTS` | Supported | Handled in the central event dispatcher. |
| Client identity & geometry | `_NET_WM_NAME`, `_NET_WM_VISIBLE_NAME`, `_NET_WM_ICON_NAME`, `_NET_WM_VISIBLE_ICON_NAME`, `_NET_WM_DESKTOP`, `_NET_WM_PID`, `_NET_FRAME_EXTENTS` | Supported | Applied during manage and on property changes. |
| Icons & struts | `_NET_WM_ICON`, `_NET_WM_ICON_GEOMETRY`, `_NET_WM_STRUT`, `_NET_WM_STRUT_PARTIAL` | Supported | `_NET_WM_HANDLED_ICONS` tells clients Openbox renders iconic windows itself. |
| Window types | `_NET_WM_WINDOW_TYPE` values: Desktop, Dock, Toolbar, Menu, Utility, Splash, Dialog, Normal, PopupMenu, Tooltip, Notification, Combo, DND | Supported | Popup-style helper types are managed as transient, undecorated helper windows and are not focusable. |
| Window states | `_NET_WM_STATE` values: Modal, Sticky, MaximizedVert, MaximizedHorz, Shaded, SkipTaskbar, SkipPager, Hidden, Fullscreen, Above, Below, DemandsAttention | Supported | All states are listed in `_NET_SUPPORTED`. |
| Allowed actions | `_NET_WM_ALLOWED_ACTIONS` values: Move, Resize, Minimize, Shade, Stick, MaximizeHorz, MaximizeVert, Fullscreen, ChangeDesktop, Close, Above, Below | Supported | Sent per window based on capabilities. |
| Focus/timing & compositor hints | `_NET_WM_USER_TIME`, `_NET_WM_USER_TIME_WINDOW`, `_NET_WM_BYPASS_COMPOSITOR`, `_NET_WM_WINDOW_OPACITY` | Supported | Opacity is widely used but outside the core spec. |
| Ping & moveresize | `_NET_WM_PING`, `_NET_WM_MOVERESIZE`, `_NET_MOVERESIZE_WINDOW` | Supported | Includes keyboard directions and cancel codes. |
| Startup notification | `_NET_STARTUP_ID` | Supported (optional) | Functional when built with `-Dstartup_notification`; otherwise stubbed. |
| Sync requests | `_NET_WM_SYNC_REQUEST`, `_NET_WM_SYNC_REQUEST_COUNTER` | Supported (optional) | Advertised and used when XSync is available (`-Dxsync`). |
| Not advertised | `_NET_VIRTUAL_ROOTS` | Not supported | `_NET_VIRTUAL_ROOTS` is intentionally not advertised; the WM sticks to the real root window. |

---

## Testing and Verification

Openbox historically has little automated testing. Developers should use:

* Unit tests for pure logic (geometry helpers, config parsers, state machines)  
* Xvfb or Xephyr to test client interactions  
* ASan and UBSan builds to catch lifetime and bounds errors  
* Logging wrappers around X11 property fetches  
* Reproducible test scripts for stacking, focus, and EWMH behavior  

Include a simple test runner in `tests/` rather than scattering ad-hoc scripts, and wire new binaries into `tests/meson.build` so `ninja -C build x11-regression-tests` can exercise them.

---

## TODO Conventions

Use `TODO(owner|tracker-id): short description` only for work that is known, scoped, and blocked on a follow-up. Prefer linking an issue (e.g. `TODO(#1234): handle transient loops`) over leaving the note anonymous. Avoid long-term TODOs; either fix the code or file an issue and remove the marker.

---

## Modernization TODO

* XCB-first X11 path — replace `XPending`/`XNextEvent`/`XGetWindowProperty` use in `obt/xqueue.c`, `openbox/event.c`, and `obt/prop.c` with `xcb_poll_for_event` plus bounded property replies to cut round-trips and oversized allocations.
* Event path profiling — add a small Xephyr-based perf harness under `tests/` to track event-to-frame latency and repaint batching, then use it to guide regression checks while pruning redundant buffering in `obt/xqueue`.
* Headless CI coverage — wire `ninja -C build x11-regression-tests` plus Xephyr into CI so X11 paths and EWMH behavior are exercised automatically, and gate merges on it.
* Keyboard stack refresh — port `obt/keyboard` to xkbcommon, drop xmodmap-style parsing, and harden layout change handling so focus and shortcuts survive VT and layout switches without Xlib round-trips.
* Atom/property cache consolidation — centralize atom lookups and property validation in `openbox/x11` helpers to remove duplicated Xlib fallbacks and enforce length/type checks consistently.
* HiDPI geometry audit — re-run frame/client conversions and theme metrics with DPI awareness, cleaning up pixel assumptions and adding regression coverage for high-scale setups.
* Render cache hygiene — audit `obrender` pixmap lifetime and reuse to avoid per-expose allocations and ensure cache invalidation is tied to theme reloads and frame resizes.
* Parser fuzzing — add libFuzzer harnesses for config/XML parsing and X11 property decode helpers to catch malformed input handling regressions early.
* Transient tree invariants — build targeted tests for parent/transient updates (group vs direct), guarding against cycles and invalid adoption in `client_transient_*` paths.

---

## Contributor Workflow

* Keep each commit scoped and readable  
* If refactoring, isolate mechanical changes from logic changes  
* Document newly added subsystems in this file  
* Use `clang-format` on touched code  
* Add comments describing invariants, not obvious restatements  
* Treat all client windows as untrusted input  
* Open a discussion on architectural changes before implementing them

---

## Original Authors and Contributors

| Name                                                                       | Role                                           | Notes |
| -------------------------------------------------------------------------- | ---------------------------------------------- | ----- |
| **Mikael Magnusson** [mikachu@icculus.org](mailto:mikachu@icculus.org)     | Developer                                      |       |
| **Dana Jansens** [danakj@orodu.net](mailto:danakj@orodu.net)               | Lead Developer                                 |       |
| **Derek Foreman** [manmower@openbox.org](mailto:manmower@openbox.org)      | Rendering Code                                 |       |
| **Tore Anderson** [tore@linpro.no](mailto:tore@linpro.no)                  | Directional focus, edge moving/growing actions |       |
| **Audun Hove** [audun@nlc.no](mailto:audun@nlc.no)                         | Actions code, move window to edge actions      |       |
| **Marius Nita** [marius@cs.pdx.edu](mailto:marius@cs.pdx.edu)              | Otk prototype code, design ideas               |       |



---

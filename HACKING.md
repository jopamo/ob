# Openbox Developer Notes

## Directory Overview

* **openbox** — core of the window manager
* **render** — `librender`, rendering routines for the WM and for apps
* **parser** — `libparser`, for parsing configuration files
* **obconf** — configuration tool, not part of the WM runtime
* **data** — default configuration, themes, menu files

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
* `Client.transient_for` links transient relationships  
* `Client.obwin` is the server-side representation of the frame window  
* `Client.desktop` indexes into the workspace manager

The WM should never assume a client is well-behaved; sanitize properties and types.

### Stacking and Focus

* Stacking is managed as a single array or list; keep it consistent  
* Focus is a state machine: normal → click → grab → revert  
* Never focus windows that are unmapped or withdrawn  
* Always check the client’s allowed focus model when handling EnterNotify

---

## `Client.transient_for`

Be cautious when using `Client.transient_for`.
It can be set to a non-NULL value of `TRAN_GROUP`, which is **not a valid pointer**.

Always check for `TRAN_GROUP` before following `transient_for`.
If a window is transient for the group, this **excludes**:

* other windows transient for the group, and
* windows that are children of the window (to avoid infinite loops)

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

## EWMH Compliance Document

The following lists all **NetWM (EWMH)** hints from freedesktop.org and Openbox’s current level of compliance.

### Legend

```

* = none
  / = partial

- = complete

* = compliant, but something else needs checking
  ? = unknown

```

### Compliance Table

| Hint                              | Version | Status | Notes                                 |
| --------------------------------- | ------- | ------ | ------------------------------------- |
| `_NET_SUPPORTED`                  | 1.3     | +      |                                       |
| `_NET_CLIENT_LIST`                | 1.3     | +      |                                       |
| `_NET_NUMBER_OF_DESKTOPS`         | 1.3     | +      |                                       |
| `_NET_DESKTOP_GEOMETRY`           | 1.3     | +      | Matches screen size only              |
| `_NET_DESKTOP_VIEWPORT`           | 1.3     | +      | Always (0,0); no large desktops       |
| `_NET_CURRENT_DESKTOP`            | 1.3     | +      |                                       |
| `_NET_DESKTOP_NAMES`              | 1.3     | +      |                                       |
| `_NET_ACTIVE_WINDOW`              | 1.3     | +      |                                       |
| `_NET_WORKAREA`                   | 1.3     | +      |                                       |
| `_NET_SUPPORTING_WM_CHECK`        | 1.3     | +      |                                       |
| `_NET_VIRTUAL_ROOTS`              | 1.3     | +      | Not used by Openbox                   |
| `_NET_DESKTOP_LAYOUT`             | 1.3     | +      |                                       |
| `_NET_SHOWING_DESKTOP`            | 1.3     | +      |                                       |
| `_NET_CLOSE_WINDOW`               | 1.3     | +      |                                       |
| `_NET_MOVERESIZE_WINDOW`          | 1.3     | +      |                                       |
| `_NET_WM_MOVERESIZE`              | 1.3     | +      |                                       |
| `_NET_WM_NAME`                    | 1.3     | +      |                                       |
| `_NET_WM_VISIBLE_NAME`            | 1.3     | +      |                                       |
| `_NET_WM_ICON_NAME`               | 1.3     | +      |                                       |
| `_NET_WM_VISIBLE_ICON_NAME`       | 1.3     | +      |                                       |
| `_NET_WM_DESKTOP`                 | 1.3     | +      |                                       |
| `_NET_WM_WINDOW_TYPE`             | 1.3     | +      | Cannot be changed post-mapping        |
| `_NET_WM_STATE`                   | 1.3     | +      |                                       |
| `_NET_WM_ALLOWED_ACTIONS`         | 1.3     | +      |                                       |
| `_NET_WM_STRUT`                   | 1.3     | +      |                                       |
| `_NET_WM_STRUT_PARTIAL`           | 1.3     | +      | Per-monitor struts in Xinerama setups |
| `_NET_WM_ICON_GEOMETRY`           | 1.3     | +      |                                       |
| `_NET_WM_ICON`                    | 1.3     | +      |                                       |
| `_NET_WM_PID`                     | 1.3     | -      | Openbox does not kill processes       |
| `_NET_WM_HANDLED_ICONS`           | 1.3     | -      | Icons for iconic windows not shown    |
| `_NET_WM_USER_TIME`               | 1.3     | +      |                                       |
| `_NET_WM_USER_TIME_WINDOW`        | 1.4     | +      | Helper-window timestamps respected    |
| `_NET_WM_PING`                    | 1.3     | -      | Does not check for hung processes     |
| `_NET_FRAME_EXTENTS`              | 1.3     | +      |                                       |
| `_NET_WM_STATE_DEMANDS_ATTENTION` | 1.3     | +      |                                       |
| `_NET_RESTACK_WINDOW`             | 1.3     | +      |                                       |
| `_NET_WM_SYNC_REQUEST`            | 1.3     | +      |                                       |
| `_NET_WM_FULL_PLACEMENT`          | 1.4     | +      |                                       |
| `_NET_WM_MOVERESIZE_CANCEL`       | 1.4     | +      |                                       |
| `_NET_REQUEST_FRAME_EXTENTS`      | 1.3     | +      |                                       |
| `_NET_WM_ACTION_MOVE`             | 1.3     | +      |                                       |
| `_NET_WM_ACTION_RESIZE`           | 1.3     | +      |                                       |
| `_NET_WM_ACTION_MINIMIZE`         | 1.3     | +      |                                       |
| `_NET_WM_ACTION_SHADE`            | 1.3     | +      |                                       |
| `_NET_WM_ACTION_STICK`            | 1.3     | -      | No large desktops → no sticky state   |
| `_NET_WM_ACTION_MAXIMIZE_HORZ`    | 1.3     | +      |                                       |
| `_NET_WM_ACTION_MAXIMIZE_VERT`    | 1.3     | +      |                                       |
| `_NET_WM_ACTION_FULLSCREEN`       | 1.3     | +      |                                       |
| `_NET_WM_ACTION_CHANGE_DESKTOP`   | 1.3     | +      |                                       |
| `_NET_WM_ACTION_CLOSE`            | 1.3     | +      |                                       |
| `_NET_WM_ACTION_ABOVE`            | 1.4?    | +      |                                       |
| `_NET_WM_ACTION_BELOW`            | 1.4?    | +      |                                       |

---

## Testing and Verification

Openbox historically has little automated testing. Developers should use:

* Unit tests for pure logic (geometry helpers, config parsers, state machines)  
* Xvfb or Xephyr to test client interactions  
* ASan and UBSan builds to catch lifetime and bounds errors  
* Logging wrappers around X11 property fetches  
* Reproducible test scripts for stacking, focus, and EWMH behavior  

Include a simple test runner in `test/` rather than scattering ad-hoc scripts.

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

## Authors and Contributors

| Name                                                                       | Role                                           | Notes |
| -------------------------------------------------------------------------- | ---------------------------------------------- | ----- |
| **Mikael Magnusson** [mikachu@icculus.org](mailto:mikachu@icculus.org)     | Developer                                      |       |
| **Dana Jansens** [danakj@orodu.net](mailto:danakj@orodu.net)               | Lead Developer                                 |       |
| **Derek Foreman** [manmower@openbox.org](mailto:manmower@openbox.org)      | Rendering Code                                 |       |
| **Tore Anderson** [tore@linpro.no](mailto:tore@linpro.no)                  | Directional focus, edge moving/growing actions |       |
| **Audun Hove** [audun@nlc.no](mailto:audun@nlc.no)                         | Actions code, move window to edge actions      |       |
| **Marius Nita** [marius@cs.pdx.edu](mailto:marius@cs.pdx.edu)              | Otk prototype code, design ideas               |       |
| **John McKnight** [jmcknight@gmail.com](mailto:jmcknight@gmail.com)        | Clearlooks themes                              |       |
| **David Barr** [david@chalkskeletons.com](mailto:david@chalkskeletons.com) | Bear2 & Clearlooks themes, icon                |       |
| **Brandon Cash**                                                           | SplitVertical gradient style                   |       |
| **Dan Rise**                                                               | Natura theme                                   |       |

---

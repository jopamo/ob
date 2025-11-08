# Openbox Developer Notes

## Directory Overview

* **openbox** — core of the window manager
* **render** — `librender`, rendering routines for the WM and for apps
* **parser** — `libparser`, for parsing configuration files

---

## Important Implementation Notes

### `Client.transient_for`

Be cautious when using `Client.transient_for`.
It can be set to a non-NULL value of `TRAN_GROUP`, which is **not a valid pointer**.

Always check for `TRAN_GROUP` before following `transient_for`.
If a window is transient for the group, this **excludes**:

* other windows transient for the group, and
* windows that are children of the window (to avoid infinite loops).

---

### Window Coordinates and Sizes

When using coordinates or sizes of windows:

* `Client.area` — reference point and size of the *client* window.
  This is not what is visibly rendered; gravity is applied to transform it.
* `Client.frame.area` — position and size of the entire frame.
  This is usually the value to use, unless you are inside `client.c` adjusting or using the size from the client’s perspective.

### Event Loop Efficiency

* Always integrate the X connection file descriptor into a `select()`/`poll()` (or GLib `GSource`) driven loop instead of calling `XPending`/`XNextEvent` in a busy cycle. The WM frequently has timers, pipes, and sockets in play; letting the kernel block until something is ready avoids needless wakeups.
* Before painting, drain bursts of `Expose`/`ConfigureNotify` events for the same window. Old toolkits redraw once per discrete event, which causes visible flicker. Use `XCheckTypedWindowEvent`/`XCheckWindowEvent` (or the local xqueue helpers) to coalesce those events and repaint only once after the queue is empty.
* Keep batching localized: drain only the event types that affect redraw state so that unrelated button/key events continue to be delivered with low latency.

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

## EWMH Compliance Document

The following lists all **NetWM (EWMH)** hints from freedesktop.org and Openbox’s current level of compliance.

### Legend

```
- = none
/ = partial
+ = complete
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
| `_NET_WM_USER_TIME_WINDOW`        | 1.4     | -      |                                       |
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

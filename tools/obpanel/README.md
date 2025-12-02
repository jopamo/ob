# obpanel

`obpanel` is a lightweight panel designed to pair with Openbox without pulling
in a full desktop shell. It speaks EWMH, renders via Cairo, and keeps a small
set of modules enabled by default:

* **Task list** — shows windows from the current desktop (sticky windows are
  always visible). Click to focus/raise.
* **Clock** — configurable `strftime` format, updates every second.
* **Volume** — ALSA-based readout with click-to-mute and scroll-to-adjust.
  Builds without ALSA too, the module simply stays hidden.
* **Status** — optional text pulled from a shell command (e.g. a script that
  prints battery state).

## Building

`obpanel` is part of the `tools/` Meson subtree, so rebuilding the top-level
project is enough:

```sh
meson setup build   # once
ninja -C build tools/obpanel/obpanel
```

The binary installs alongside the rest of the Openbox tools via
`ninja -C build install`.

## Usage

```
obpanel [--edge top|bottom]
        [--height N]
        [--font "Sans 11"]
        [--clock-format "%a %d %b %H:%M"]
        [--status-command "path/to/script"]
        [--status-interval N]
        [--tasks all|current]
        [--mixer-device hw:0]
        [--mixer-element Master]
```

* `--edge` picks whether the panel is glued to the top or bottom of the screen (default: top).
* `--status-command` runs synchronously at the configured interval and expects
  a single line on stdout. Keep the command short to avoid blocking redraws.
* `--tasks` controls whether the task list shows every desktop (`all`, default)
  or only the current workspace.
* Volume shortcuts require ALSA support; without it the volume module is
  suppressed automatically.

## Notes

* The task list filters out docks, splash screens, notifications, and any
  client that sets `_NET_WM_STATE_SKIP_TASKBAR`.
* `obpanel` reserves space using `_NET_WM_STRUT_PARTIAL` so tiled windows do not
  overlap the bar.
* Each module is re-drawn on demand; expensive status commands or ALSA calls
  will only impact their specific region.

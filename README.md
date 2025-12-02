# Openbox

Openbox is a lightweight, standards-compliant X11 window manager. The tree is organized into the core WM (`openbox`), shared rendering code (`obrender`), and utility/library code (`obt`). Default themes and data live under `data/` and `themes/`.

## Build
- Prereqs: Meson, Ninja, GLib, X11/XCB, Xext, Xrender, Xcursor, Xrandr, Pango, and related dev headers.
- Configure: `meson setup --buildtype=debugoptimized build` (add `-Dstartup_notification=disabled` or other options as needed).
- Rebuild: `ninja -C build`

## Tests
Headless/unit (safe for CI):
- `meson test -C build unit-prop obt-unittests` (or `meson test -C build --suite openbox:unit`).
- These run without a display server; set `G_DEBUG=fatal-warnings` when chasing UB/ASan issues.

X11 regression suite (needs nested display, not your main session):
- Ensure you are on a real display (e.g., `DISPLAY=:0`) and have `Xephyr` installed.
- Run: `meson test -C build x11-regressions`  
  The runner (`tests/run-x11-regressions.sh`) will refuse to use the current display and will start Xephyr on `:1` by default; override with `OPENBOX_TEST_DISPLAY` and `OPENBOX_TEST_SCREEN` if needed.

## Useful Targets
- `ninja -C build` — full build
- `meson test -C build` — run all registered tests (includes X11 suite; ensure Xephyr available)
- `meson test -C build --list` — see available tests/suites

## Code Navigation (ctags)
Generate tags for quick symbol lookup (Universal Ctags recommended):
- `ctags -R --languages=C --kinds-C=+p --fields=+iaS --extras=+q -f .git/tags .`
- Common excludes: add `--exclude=build --exclude=build-asan --exclude=tests/*.[ch]` if you only want core WM/library symbols.
Point your editor to `.git/tags` (keeps the repo root clean) and regenerate after changes.

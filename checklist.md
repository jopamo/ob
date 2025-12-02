# XCB migration checklist

## Runtime code still on Xlib
- [ ] `obt/xqueue.c` — event intake uses `XPending`/`XNextEvent`; replace with `xcb_poll_for_event` + bounded property replies (per HACKING.md).
- [ ] `openbox/event.c` — central dispatcher still consumes Xlib events; plan XCB intake and conversions.
- [x] `openbox/grab.c` — pointer/keyboard/server grabs via Xlib; migrate to XCB grabs/ungrabs.
- [x] `openbox/client.c`, `openbox/window.c` — window attribute/query, save-set, event mask changes via Xlib; move to XCB variants.
- [ ] `openbox/frame.c`, `openbox/popup.c`, `openbox/prompt.c`, `openbox/focus_cycle_popup.c`, `openbox/focus_cycle_indicator.c`, `openbox/menuframe.c` — frame/popup creation, map/moveresize, cursor attributes via Xlib; map to XCB.
- [x] `openbox/dock.c` — dockapp save-set, attribute queries, map/select via Xlib.
- [x] `openbox/screen.c` — support window setup and root selections via Xlib; convert to XCB.
- [x] `openbox/moveresize.c` — XSync alarms/events; evaluate XCB sync extension path or keep minimal Xlib interop.

## Tools using Xlib atoms/properties
- [x] `tools/obpanel/obpanel.c` — atom setup uses `XInternAtom`; migrate to `xcb_intern_atom` and adjust property/event calls accordingly.

## Tests still using Xlib atom/property helpers
- [x] `tests/oldfullscreen.c` — motif hints atoms via `XInternAtom`/`XChangeProperty`; port to XCB.
- [x] `tests/wmhints.c` — motif hints atoms via `XInternAtom`/`XChangeProperty`; port to XCB.
- [x] `tests/modal.c` / `modal2.c` / `modal3.c` / `groupmodal.c` — modal atoms via `XInternAtom`; move to XCB.
- [x] `tests/fullscreen.c` — `_NET_WM_STATE`/`_NET_WM_STATE_FULLSCREEN` via `XInternAtom` + `XSendEvent`; migrate to XCB.
- [x] `tests/skiptaskbar.c` / `skiptaskbar2.c` — skip-taskbar atoms via `XInternAtom`; migrate to XCB.
- [x] `tests/urgent.c` — fullscreen/state atoms via `XInternAtom`; port to XCB.
- [x] `tests/iconifydelay.c` — `WM_CHANGE_STATE` via `XInternAtom`; switch to XCB.
- [x] `tests/strut.c` — `_NET_WM_STRUT` via `XInternAtom`/`XChangeProperty`; port to XCB.
- [x] `tests/confignotifymax.c` — `_NET_WM_STATE` + maximized atoms via `XInternAtom`; migrate to XCB.
- [x] `tests/mapiconic.c` — `WM_STATE` via `XInternAtom`; use XCB equivalents.
- [x] `tests/restack.c` — `_NET_RESTACK_WINDOW` via `XInternAtom`/`XSendEvent`; port to XCB.
- [x] `tests/title.c` — `WM_NAME`/`STRING` via `XInternAtom`; use XCB for property set.
- [x] `tests/duplicatesession.c` — `SM_CLIENT_ID`/`STRING` via `XInternAtom`; port to XCB.
## Notes
- No remaining `XGetWindowProperty` callers found; property fetches now use shared helpers or XCB in touched paths.
- Recently migrated: `tests/icons.c`, `tests/extentsrequest.c`, `tests/usertimewin.c`, `tests/wmhints.c`, `tests/oldfullscreen.c`, `tests/fullscreen.c`, `tests/skiptaskbar.c`, `tests/skiptaskbar2.c`, `tests/modal*.c`, `tests/groupmodal.c`, `tests/urgent.c`, `tests/iconifydelay.c`, `tests/strut.c`, `tests/confignotifymax.c`, `tests/mapiconic.c`, `tests/restack.c`, `tests/title.c`, `tests/duplicatesession.c`, `tools/obxprop/obxprop.c`, `tools/obpanel/obpanel.c` now XCB-based.

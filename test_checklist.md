# Test Checklist

| Test File | Appropriate for CI | Notes |
| :--- | :--- | :--- |
| `./obt/tests/watchtest.c` | Yes | Fixed infinite loop |
| `./obt/tests/bstest.c` | Yes | Unit test |
| `./obt/tests/ddtest.c` | No | Not built (requires command line arguments) |
| `./obt/tests/linktest.c` | Yes | |
| `./tests/urgent.c` | Yes | Visual (Xvfb required) |
| `./tests/stacking.c` | Yes | Fixed infinite loop |
| `./tests/modal.c` | Yes | Visual (Xvfb required) |
| `./tests/restack.c` | Yes | Fixed infinite loop |
| `./tests/skiptaskbar.c` | Yes | Fixed infinite loop |
| `./tests/cursorio.c` | Yes | Visual (Xvfb required) |
| `./tests/mingrow.c` | Yes | Visual (Xvfb required) |
| `./tests/grouptrancircular.c` | Yes | Visual (Xvfb required) |
| `./tests/big.c` | Yes | Visual (Xvfb required) |
| `./tests/fallback.c` | Yes | Visual (Xvfb required) |
| `./tests/groupmodal.c` | Yes | Fixed infinite loop |
| `./tests/skiptaskbar2.c` | Yes | Fixed infinite loop |
| `./tests/title.c` | No | Disabled (requires args) |
| `./tests/showhide.c` | Yes | Visual (Xvfb required) |
| `./tests/mapiconic.c` | Yes | Fixed infinite loop |
| `./tests/override.c` | Yes | Visual (Xvfb required) |
| `./tests/usertimewin.c` | Yes | Visual (Xvfb required) |
| `./tests/resize.c` | Yes | Fixed infinite loop |
| `./tests/focusout.c` | Yes | Visual (Xvfb required) |
| `./tests/grouptrancircular2.c` | Yes | Visual (Xvfb required) |
| `./tests/modal3.c` | Yes | |
| `./tests/confignotifymax.c` | Yes | Visual (Xvfb required) |
| `./tests/noresize.c` | Yes | Visual (Xvfb required) |
| `./tests/iconifydelay.c` | Yes | |
| `./tests/icons.c` | No | Disabled (interactive/infinite loop) |
| `./tests/duplicatesession.c` | Yes | Visual (Xvfb required) |
| `./tests/strut.c` | Yes | Visual (Xvfb required) |
| `./tests/positioned.c` | Yes | Fixed infinite loop |
| `./tests/confignotify.c` | Yes | Fixed infinite loop |
| `./tests/grav.c` | Yes | Visual (Xvfb required) |
| `./tests/fullscreen.c` | Yes | Visual (Xvfb required) |
| `./tests/modal2.c` | Yes | Visual (Xvfb required) |
| `./tests/grouptran.c` | Yes | Visual (Xvfb required) |
| `./tests/wmhints.c` | Yes | Visual (Xvfb required) |
| `./tests/overrideinputonly.c` | Yes | Fixed infinite loop |
| `./tests/extentsrequest.c` | Yes | Visual (Xvfb required) |
| `./tests/aspect.c` | Yes | Visual (Xvfb required) |
| `./tests/oldfullscreen.c` | Yes | Fixed infinite loop |
| `./tests/stackabove.c` | Yes | Fixed infinite loop |
| `./tests/grouptran2.c` | Yes | Visual (Xvfb required) |
| `./tests/fakeunmap.c` | Yes | Visual (Xvfb required) |
| `./tests/borderchange.c` | Yes | Visual (Xvfb required) |
| `./tests/shape.c` | Yes | Fixed infinite loop |
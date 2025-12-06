# Test Checklist

| Test File | Appropriate for CI | Notes |
| :--- | :--- | :--- |
| `./obt/tests/watchtest.c` | No | Infinite Loop |
| `./obt/tests/bstest.c` | Yes | Unit test |
| `./obt/tests/ddtest.c` | No | Requires command line arguments |
| `./obt/tests/linktest.c` | Yes | |
| `./tests/urgent.c` | Yes | Visual (Xvfb required) |
| `./tests/stacking.c` | No | Infinite Loop |
| `./tests/modal.c` | Yes | Visual (Xvfb required) |
| `./tests/restack.c` | No | Infinite Loop |
| `./tests/skiptaskbar.c` | No | Infinite Loop |
| `./tests/cursorio.c` | Yes | Visual (Xvfb required) |
| `./tests/mingrow.c` | Yes | Visual (Xvfb required) |
| `./tests/grouptrancircular.c` | Yes | Visual (Xvfb required) |
| `./tests/big.c` | Yes | Visual (Xvfb required) |
| `./tests/fallback.c` | Yes | Visual (Xvfb required) |
| `./tests/groupmodal.c` | No | Infinite Loop |
| `./tests/skiptaskbar2.c` | No | Infinite Loop |
| `./tests/title.c` | No | Infinite Loop / Requires Args |
| `./tests/showhide.c` | Yes | Visual (Xvfb required) |
| `./tests/mapiconic.c` | No | Infinite Loop |
| `./tests/override.c` | Yes | Visual (Xvfb required) |
| `./tests/usertimewin.c` | Yes | Visual (Xvfb required) |
| `./tests/resize.c` | No | Infinite Loop |
| `./tests/focusout.c` | Yes | Visual (Xvfb required) |
| `./tests/grouptrancircular2.c` | Yes | Visual (Xvfb required) |
| `./tests/modal3.c` | No | Infinite Loop |
| `./tests/confignotifymax.c` | Yes | Visual (Xvfb required) |
| `./tests/noresize.c` | Yes | Visual (Xvfb required) |
| `./tests/iconifydelay.c` | No | Infinite Loop |
| `./tests/icons.c` | No | Interactive / Infinite Loop |
| `./tests/duplicatesession.c` | Yes | Visual (Xvfb required) |
| `./tests/strut.c` | Yes | Visual (Xvfb required) |
| `./tests/positioned.c` | No | Interactive / Infinite Loop |
| `./tests/confignotify.c` | No | Infinite Loop |
| `./tests/grav.c` | Yes | Visual (Xvfb required) |
| `./tests/fullscreen.c` | Yes | Visual (Xvfb required) |
| `./tests/modal2.c` | Yes | Visual (Xvfb required) |
| `./tests/grouptran.c` | Yes | Visual (Xvfb required) |
| `./tests/wmhints.c` | Yes | Visual (Xvfb required) |
| `./tests/overrideinputonly.c` | No | Infinite Loop / Broken |
| `./tests/extentsrequest.c` | Yes | Visual (Xvfb required) |
| `./tests/aspect.c` | Yes | Visual (Xvfb required) |
| `./tests/oldfullscreen.c` | No | Interactive / Infinite Loop |
| `./tests/stackabove.c` | No | Infinite Loop |
| `./tests/grouptran2.c` | Yes | Visual (Xvfb required) |
| `./tests/fakeunmap.c` | Yes | Visual (Xvfb required) |
| `./tests/borderchange.c` | Yes | Visual (Xvfb required) |
| `./tests/shape.c` | No | Infinite Loop |
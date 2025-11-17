// hidpi.c -- exercise scaling helpers for HiDPI setups

#include <math.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <glib.h>

#include "obrender/render.h"

static int expect_close(const char* label, gdouble got, gdouble expected, gdouble tolerance) {
  if (fabs(got - expected) <= tolerance)
    return 0;

  fprintf(stderr, "%s: expected %.2f (+/- %.2f) but got %.2f\n", label, expected, tolerance, got);
  return 1;
}

static int expect_int(const char* label, gint got, gint expected) {
  if (got == expected)
    return 0;

  fprintf(stderr, "%s: expected %d but got %d\n", label, expected, got);
  return 1;
}

int main(void) {
  Display* dpy = XOpenDisplay(NULL);
  if (!dpy)
    return 77;

  gint failures = 0;

  g_setenv("OPENBOX_SCALE", "2.0", TRUE);
  g_unsetenv("OPENBOX_DPI");

  RrInstance* inst = RrInstanceNew(dpy, DefaultScreen(dpy));
  failures += expect_close("scale override", RrScale(inst), 2.0, 0.01);
  failures += expect_int("scale positive value", RrScaleValue(inst, 3), 6);
  failures += expect_int("scale negative value", RrScaleValue(inst, -2), -4);
  failures += expect_int("scale zero passthrough", RrScaleValue(inst, 0), 0);
  RrInstanceFree(inst);

  g_unsetenv("OPENBOX_SCALE");
  g_setenv("OPENBOX_DPI", "192", TRUE);

  inst = RrInstanceNew(dpy, DefaultScreen(dpy));
  failures += expect_close("dpi-derived scale", RrScale(inst), 2.0, 0.05);
  failures += expect_close("dpi-x override", RrDpiX(inst), 192.0, 0.5);
  failures += expect_close("dpi-y override", RrDpiY(inst), 192.0, 0.5);
  RrInstanceFree(inst);

  XCloseDisplay(dpy);

  if (failures)
    fprintf(stderr, "%d hidpi expectation(s) failed\n", failures);

  return failures ? 1 : 0;
}

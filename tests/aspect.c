/* aspect.c for Openbox window manager */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define TOLERANCE 10  // tolerance for position check

// helper to check window position and size
static void test_window(Display* dpy, Window win, int exp_x, int exp_y, int exp_w, int exp_h) {
  XWindowAttributes attr;
  XGetWindowAttributes(dpy, win, &attr);

  if (abs(attr.x - exp_x) <= TOLERANCE && abs(attr.y - exp_y) <= TOLERANCE && attr.width == exp_w &&
      attr.height == exp_h) {
    printf("ok: pos (%d,%d) size %dx%d\n", attr.x, attr.y, attr.width, attr.height);
  }
  else {
    printf("fail: expected (%d,%d) %dx%d, got (%d,%d) %dx%d\n", exp_x, exp_y, exp_w, exp_h, attr.x, attr.y, attr.width,
           attr.height);
  }
}

// predicate for XIfEvent to wait for ConfigureNotify on our window
static Bool is_configure_for_window(Display* dpy, XEvent* ev, XPointer arg) {
  Window win = (Window)(uintptr_t)arg;
  return ev->type == ConfigureNotify && ev->xconfigure.window == win;
}

// predicate for XIfEvent to wait for MapNotify on our window
static Bool is_map_for_window(Display* dpy, XEvent* ev, XPointer arg) {
  Window win = (Window)(uintptr_t)arg;
  return ev->type == MapNotify && ev->xmap.window == win;
}

int main(void) {
  // init X11 display and create a simple window
  Display* dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "couldn't connect to X server\n");
    return 1;
  }

  int screen = DefaultScreen(dpy);
  int x = 10, y = 10, w = 400, h = 100;

  XSetWindowAttributes xswa;
  xswa.win_gravity = StaticGravity;  // keep server side position logic predictable

  Window win = XCreateWindow(dpy, RootWindow(dpy, screen), x, y, w, h, 0, CopyFromParent, InputOutput, CopyFromParent,
                             CWWinGravity, &xswa);

  XSetWindowBackground(dpy, win, WhitePixel(dpy, screen));

  // set strict aspect ratio 3:3 so WMs don't resize unexpectedly
  XSizeHints size;
  size.flags = PAspect;
  size.min_aspect.x = 3;
  size.min_aspect.y = 3;
  size.max_aspect.x = 3;
  size.max_aspect.y = 3;
  XSetWMNormalHints(dpy, win, &size);

  // select events we care about
  XSelectInput(dpy, win, ExposureMask | StructureNotifyMask);

  // map and wait until actually mapped
  XMapWindow(dpy, win);
  XEvent ev;
  XIfEvent(dpy, &ev, is_map_for_window, (XPointer)(uintptr_t)win);

  // place at 0,0 then wait for the ConfigureNotify that reflects the move
  XMoveWindow(dpy, win, 0, 0);
  XIfEvent(dpy, &ev, is_configure_for_window, (XPointer)(uintptr_t)win);
  test_window(dpy, win, 0, 0, w, h);

  // do a few random moves and validate after each ConfigureNotify
  srand((unsigned)time(NULL));
  for (int i = 0; i < 3; ++i) {
    int rx = rand() % 500;
    int ry = rand() % 500;
    XMoveWindow(dpy, win, rx, ry);
    XIfEvent(dpy, &ev, is_configure_for_window, (XPointer)(uintptr_t)win);
    printf("confignotify %d,%d-%ix%i\n", ev.xconfigure.x, ev.xconfigure.y, ev.xconfigure.width, ev.xconfigure.height);
    test_window(dpy, win, rx, ry, w, h);
    usleep(200000);
  }

  // clean up client resources and quiesce the connection
  XDestroyWindow(dpy, win);
  XSync(dpy, True);
  XCloseDisplay(dpy);

  return 0;
}

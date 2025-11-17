/* usertimewin.c ensures helper-window timestamps block unwanted focus */

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static Window read_active_window(Display* dpy, Atom active_atom) {
  Atom actual;
  int format;
  unsigned long nitems = 0, bytes = 0;
  unsigned char* data = NULL;
  Window active = None;

  if (XGetWindowProperty(dpy, RootWindow(dpy, DefaultScreen(dpy)), active_atom, 0, 1, False, XA_WINDOW, &actual,
                         &format, &nitems, &bytes, &data) == Success &&
      actual == XA_WINDOW && format == 32 && nitems == 1 && data) {
    active = *((Window*)data);
  }

  if (data)
    XFree(data);

  return active;
}

int main(void) {
  Display* display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "couldn't connect to X server\n");
    return EXIT_FAILURE;
  }

  const int screen = DefaultScreen(display);
  Atom atime = XInternAtom(display, "_NET_WM_USER_TIME", False);
  Atom atimewin = XInternAtom(display, "_NET_WM_USER_TIME_WINDOW", False);
  Atom active_atom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);

  Window focus_win =
      XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 200, 150, 0, BlackPixel(display, screen),
                          WhitePixel(display, screen));
  XMapWindow(display, focus_win);
  XFlush(display);
  sleep(1);

  XSetInputFocus(display, focus_win, RevertToPointerRoot, CurrentTime);
  XSync(display, False);

  Window helper = XCreateWindow(display, RootWindow(display, screen), 0, 0, 1, 1, 0, CopyFromParent, InputOnly,
                                CopyFromParent, 0, NULL);
  XMapWindow(display, helper);

  Window test_win =
      XCreateSimpleWindow(display, RootWindow(display, screen), 240, 10, 200, 150, 0, BlackPixel(display, screen),
                          WhitePixel(display, screen));

  /* Tell Openbox that helper carries timestamps for test_win */
  unsigned long helper_id = helper;
  XChangeProperty(display, test_win, atimewin, XA_WINDOW, 32, PropModeReplace, (unsigned char*)&helper_id, 1);

  /* Report "do not focus" by setting timestamp 0 on the helper window */
  unsigned long ts = 0;
  XChangeProperty(display, helper, atime, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&ts, 1);

  XMapWindow(display, test_win);
  XFlush(display);
  sleep(1);

  Window active = read_active_window(display, active_atom);

  XDestroyWindow(display, test_win);
  XDestroyWindow(display, helper);
  XDestroyWindow(display, focus_win);
  XCloseDisplay(display);

  if (active == test_win) {
    fprintf(stderr, "helper timestamps ignored; new window became active\n");
    return EXIT_FAILURE;
  }

  printf("helper timestamps respected; active window 0x%lx\n", active);
  return EXIT_SUCCESS;
}

/* usertimewin.c ensures helper-window timestamps block unwanted focus */

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>

static xcb_atom_t intern_atom(xcb_connection_t* conn, const char* name) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(name), name);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

static Window read_active_window(Display* dpy, xcb_connection_t* conn, xcb_atom_t active_atom) {
  const xcb_window_t root = RootWindow(dpy, DefaultScreen(dpy));
  xcb_get_property_cookie_t cookie = xcb_get_property(conn, 0, root, active_atom, XCB_ATOM_WINDOW, 0, 1);
  xcb_get_property_reply_t* reply = xcb_get_property_reply(conn, cookie, NULL);
  const uint8_t* raw;
  Window active = None;

  if (!reply)
    return None;

  if (reply->type != XCB_ATOM_WINDOW || reply->format != 32 || reply->value_len < 1)
    goto out;

  raw = xcb_get_property_value(reply);
  if (raw)
    active = (Window)((xcb_window_t*)raw)[0];

out:
  free(reply);
  return active;
}

int main(void) {
  Display* display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "couldn't connect to X server\n");
    return EXIT_FAILURE;
  }

  const int screen = DefaultScreen(display);
  xcb_connection_t* conn = XGetXCBConnection(display);
  xcb_atom_t atime = intern_atom(conn, "_NET_WM_USER_TIME");
  xcb_atom_t atimewin = intern_atom(conn, "_NET_WM_USER_TIME_WINDOW");
  xcb_atom_t active_atom = intern_atom(conn, "_NET_ACTIVE_WINDOW");

  Window focus_win = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 200, 150, 0,
                                         BlackPixel(display, screen), WhitePixel(display, screen));
  XMapWindow(display, focus_win);
  XFlush(display);
  sleep(1);

  XSetInputFocus(display, focus_win, RevertToPointerRoot, CurrentTime);
  XSync(display, False);

  Window helper = XCreateWindow(display, RootWindow(display, screen), 0, 0, 1, 1, 0, CopyFromParent, InputOnly,
                                CopyFromParent, 0, NULL);
  XMapWindow(display, helper);

  Window test_win = XCreateSimpleWindow(display, RootWindow(display, screen), 240, 10, 200, 150, 0,
                                        BlackPixel(display, screen), WhitePixel(display, screen));

  /* Tell Openbox that helper carries timestamps for test_win */
  uint32_t helper_id = (uint32_t)helper;
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, test_win, atimewin, XCB_ATOM_WINDOW, 32, 1, &helper_id);
  xcb_flush(conn);

  /* Report "do not focus" by setting timestamp 0 on the helper window */
  uint32_t ts = 0;
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, helper, atime, XCB_ATOM_CARDINAL, 32, 1, &ts);
  xcb_flush(conn);

  XMapWindow(display, test_win);
  XFlush(display);
  sleep(1);

  Window active = read_active_window(display, conn, active_atom);

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

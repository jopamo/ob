#define _DEFAULT_SOURCE
/* skiptaskbar.c for the Openbox window manager */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <xcb/xcb.h>

static xcb_atom_t intern_atom(xcb_connection_t* conn, const char* name) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(name), name);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

int main() {
  Display* display = NULL;
  xcb_connection_t* conn = NULL;
  Window win = None;
  XEvent report;
  xcb_atom_t state, skip;
  XClassHint classhint;
  int x = 10, y = 10, h = 400, w = 400;
  int ret = 1;

  display = XOpenDisplay(NULL);

  if (display == NULL) {
    fprintf(stderr, "couldn't connect to X server :0\n");
    return 1;
  }
  conn = XGetXCBConnection(display);
  if (conn == NULL) {
    fprintf(stderr, "couldn't get XCB connection\n");
    goto out;
  }

  state = intern_atom(conn, "_NET_WM_STATE");
  skip = intern_atom(conn, "_NET_WM_STATE_SKIP_TASKBAR");

  if (state == XCB_ATOM_NONE || skip == XCB_ATOM_NONE) {
    fprintf(stderr, "Failed to intern atoms\n");
    goto out;
  }

  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                      0, NULL);

  if (win == None) {
    fprintf(stderr, "Failed to create window\n");
    goto out;
  }

  XSetWindowBackground(display, win, WhitePixel(display, 0));

  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, state, XCB_ATOM_ATOM, 32, 1, &skip);
  xcb_flush(conn);

  classhint.res_name = "test";
  classhint.res_class = "Test";
  XSetClassHint(display, win, &classhint);

  XMapWindow(display, win);
  XFlush(display);

  ret = 0;
  while (1) {
    XNextEvent(display, &report);
    if (report.type == DestroyNotify)
      break;
  }

out:
  if (win != None)
    XDestroyWindow(display, win);
  if (display)
    XCloseDisplay(display);
  return ret;
}

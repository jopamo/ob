#define _DEFAULT_SOURCE
/* modal.c for the Openbox window manager */

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
  Window parent = None, child = None;
  XEvent report;
  xcb_atom_t state, modal;
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
  modal = intern_atom(conn, "_NET_WM_STATE_MODAL");

  if (state == XCB_ATOM_NONE || modal == XCB_ATOM_NONE) {
    fprintf(stderr, "Failed to intern atoms: _NET_WM_STATE or _NET_WM_STATE_MODAL\n");
    goto out;
  }

  printf("Atoms successfully interned: _NET_WM_STATE = %u, _NET_WM_STATE_MODAL = %u\n", state, modal);

  parent = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent,
                         CopyFromParent, 0, 0);
  child = XCreateWindow(display, RootWindow(display, 0), x, y, w / 2, h / 2, 10, CopyFromParent, CopyFromParent,
                        CopyFromParent, 0, 0);

  XSetWindowBackground(display, parent, WhitePixel(display, 0));
  XSetWindowBackground(display, child, BlackPixel(display, 0));

  // Set transient window hint
  XSetTransientForHint(display, child, parent);

  uint32_t modal_val = modal;
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, child, state, XCB_ATOM_ATOM, 32, 1, &modal_val);
  xcb_flush(conn);

  XMapWindow(display, parent);
  XMapWindow(display, child);
  XFlush(display);

  // Listen for events (simulate handling)
  XSelectInput(display, child, ExposureMask | StructureNotifyMask);

  while (1) {
    XNextEvent(display, &report);

    // Handle Expose and ConfigureNotify events
    switch (report.type) {
      case Expose:
        printf("Expose event received\n");
        break;
      case ConfigureNotify:
        printf("ConfigureNotify: window %lu moved to (%d, %d) and resized to %dx%d\n", report.xconfigure.window,
               report.xconfigure.x, report.xconfigure.y, report.xconfigure.width, report.xconfigure.height);
        break;
    }

    // Exit after handling the first event to avoid an infinite loop in CI
    if (report.type == Expose && report.xexpose.count == 0) {
      printf("Test completed. Closing the program.\n");
      break;
    }
  }

  ret = 0;

out:
  if (parent != None)
    XDestroyWindow(display, parent);
  if (child != None)
    XDestroyWindow(display, child);
  if (display)
    XCloseDisplay(display);
  return ret;
}

/* strut.c for Openbox window manager */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xatom.h>
#include <xcb/xcb.h>

static xcb_atom_t intern_atom(xcb_connection_t* conn, const char* name) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(name), name);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

void test_strut_property(Display* display, xcb_connection_t* conn, Window win, xcb_atom_t _net_strut) {
  uint32_t s[4];

  // Set top strut
  s[0] = 0;
  s[1] = 0;
  s[2] = 20;
  s[3] = 0;
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, _net_strut, XCB_ATOM_CARDINAL, 32, 4, s);
  xcb_flush(conn);
  sleep(2);

  // Remove strut
  xcb_delete_property(conn, win, _net_strut);
  xcb_flush(conn);
  sleep(2);
}

void handle_events(Display* display, Window win) {
  XEvent report;
  while (1) {
    XNextEvent(display, &report);

    switch (report.type) {
      case Expose:
        printf("Window exposed\n");
        break;
      case ConfigureNotify:
        printf("Window configuration changed: position (%i, %i), size (%ix%i)\n", report.xconfigure.x,
               report.xconfigure.y, report.xconfigure.width, report.xconfigure.height);
        break;
    }

    if (report.type == Expose && report.xexpose.count == 0) {
      printf("All events processed. Closing the program.\n");
      return;
    }
  }
}

int main() {
  Display* display = NULL;
  xcb_connection_t* conn = NULL;
  Window win = None;
  xcb_atom_t _net_strut = XCB_ATOM_NONE;
  int x = 10, y = 10, h = 100, w = 400;
  int ret = 1;

  // Connect to X server
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

  _net_strut = intern_atom(conn, "_NET_WM_STRUT");
  if (_net_strut == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern _NET_WM_STRUT\n");
    goto out;
  }

  // Create window
  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                      0, NULL);
  if (win == None) {
    fprintf(stderr, "failed to create window\n");
    goto out;
  }
  XSetWindowBackground(display, win, WhitePixel(display, 0));
  XMapWindow(display, win);
  XFlush(display);

  // Test strut property
  test_strut_property(display, conn, win, _net_strut);

  // Listen for events
  XSelectInput(display, win, ExposureMask | StructureNotifyMask);
  handle_events(display, win);

  ret = 0;

out:
  if (win != None)
    XDestroyWindow(display, win);
  if (display)
    XCloseDisplay(display);
  return ret;
}

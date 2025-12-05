#define _DEFAULT_SOURCE
/* confignotifymax.c for the Openbox window manager */

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

int main() {
  Display* display = NULL;
  xcb_connection_t* conn = NULL;
  Window win = None;
  XEvent report;
  xcb_atom_t _net_max[2], _net_state;
  int x = 10, y = 10, h = 100, w = 100;
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

  _net_state = intern_atom(conn, "_NET_WM_STATE");
  _net_max[0] = intern_atom(conn, "_NET_WM_STATE_MAXIMIZED_VERT");
  _net_max[1] = intern_atom(conn, "_NET_WM_STATE_MAXIMIZED_HORZ");
  if (_net_state == XCB_ATOM_NONE || _net_max[0] == XCB_ATOM_NONE || _net_max[1] == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern maximized atoms\n");
    goto out;
  }

  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 0, CopyFromParent, CopyFromParent, CopyFromParent, 0,
                      NULL);
  if (win == None) {
    fprintf(stderr, "failed to create window\n");
    goto out;
  }
  XSetWindowBackground(display, win, WhitePixel(display, 0));
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, _net_state, XCB_ATOM_ATOM, 32, 2, _net_max);
  xcb_flush(conn);

  XSelectInput(display, win, ExposureMask | StructureNotifyMask | GravityNotify);

  XMapWindow(display, win);
  XFlush(display);

  usleep(100000);  // Give Openbox some time to process the map request

  int configure_notify_count = 0;
  while (configure_notify_count < 5) {  // Wait for a few configure notifies after setting max state
    XNextEvent(display, &report);

    switch (report.type) {
      case MapNotify:
        printf("map notify\n");
        break;
      case Expose:
        printf("exposed\n");
        break;
      case GravityNotify:
        printf("gravity notify event 0x%lx window 0x%lx x %d y %d\n", (unsigned long)report.xgravity.event,
               (unsigned long)report.xgravity.window, report.xgravity.x, report.xgravity.y);
        break;
      case ConfigureNotify: {
        unsigned long se = (unsigned long)report.xconfigure.send_event;
        unsigned long event = (unsigned long)report.xconfigure.event;
        unsigned long wind = (unsigned long)report.xconfigure.window;
        int cx = report.xconfigure.x;
        int cy = report.xconfigure.y;
        int cw = report.xconfigure.width;
        int ch = report.xconfigure.height;
        int bw = report.xconfigure.border_width;
        unsigned long abv = (unsigned long)report.xconfigure.above;
        int ovrd = report.xconfigure.override_redirect;

        printf(
            "confignotify send %lu ev 0x%lx win 0x%lx %i,%i-%ix%i bw %i\n"
            "             above 0x%lx ovrd %d\n",
            se, event, wind, cx, cy, cw, ch, bw, abv, ovrd);
        configure_notify_count++;
        break;
      }
    }
  }

  printf("Test completed after receiving configure notifies. Closing the program.\n");

  XDestroyWindow(display, win);
  ret = 0;

out:
  if (display)
    XCloseDisplay(display);
  return ret;
}

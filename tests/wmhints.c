/* wmhints.c for the Openbox window manager */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>
#include <xcb/xcb.h>

typedef struct {
  uint32_t flags;
  uint32_t functions;
  uint32_t decorations;
  int32_t inputMode;
  uint32_t status;
} Hints;

static xcb_atom_t intern_atom(xcb_connection_t* conn, const char* name) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(name), name);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

int main(int argc, char** argv) {
  Display* display = NULL;
  xcb_connection_t* conn = NULL;
  Window win = None;
  XEvent report;
  int x = 200, y = 200, h = 100, w = 400, s;
  Hints hints;
  xcb_atom_t prop = XCB_ATOM_NONE;
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

  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 0, CopyFromParent, CopyFromParent, CopyFromParent, 0,
                      NULL);
  if (win == None) {
    fprintf(stderr, "failed to create window\n");
    goto out;
  }
  XSetWindowBackground(display, win, WhitePixel(display, 0));

  prop = intern_atom(conn, "_MOTIF_WM_HINTS");
  if (prop == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern _MOTIF_WM_HINTS\n");
    goto out;
  }
  hints.flags = 2;
  hints.decorations = 0;
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, prop, prop, 32, 5, &hints);
  xcb_flush(conn);

  XSelectInput(display, win, StructureNotifyMask | ButtonPressMask);

  XMapWindow(display, win);
  XFlush(display);

  sleep(1);
  hints.flags = 2;
  hints.decorations = (1u << 3) | (1u << 1);  // Set border and title
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, prop, prop, 32, 5, &hints);
  xcb_flush(conn);

  XMapWindow(display, win);
  XFlush(display);

  int event_processed = 0;
  while (!event_processed) {
    XNextEvent(display, &report);

    switch (report.type) {
      case ButtonPress:
        XUnmapWindow(display, win);
        event_processed = 1;
        break;
      case ConfigureNotify:
        x = report.xconfigure.x;
        y = report.xconfigure.y;
        w = report.xconfigure.width;
        h = report.xconfigure.height;
        s = report.xconfigure.send_event;
        printf("confignotify %i,%i-%ix%i (send: %d)\n", x, y, w, h, s);
        event_processed = 1;
        break;
    }
  }

  ret = 0;

out:
  if (win != None)
    XDestroyWindow(display, win);
  if (display)
    XCloseDisplay(display);
  return ret;
}

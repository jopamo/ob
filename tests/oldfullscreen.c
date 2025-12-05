#define _DEFAULT_SOURCE
/* oldfullscreen.c for the Openbox window manager */

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
  int x = 200, y = 200;
  unsigned int w = 400, h = 100, s = 0, depth = 0;
  XSizeHints* size;
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

  XGetGeometry(display, RootWindow(display, DefaultScreen(display)), &win, &x, &y, &w, &h, &s, &depth);

  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 0, CopyFromParent, CopyFromParent, CopyFromParent, 0,
                      NULL);
  if (win == None) {
    fprintf(stderr, "failed to create window\n");
    goto out;
  }
  XSetWindowBackground(display, win, WhitePixel(display, 0));

  size = XAllocSizeHints();
  size->flags = PPosition;
  XSetWMNormalHints(display, win, size);
  XFree(size);

  prop = intern_atom(conn, "_MOTIF_WM_HINTS");
  if (prop == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern _MOTIF_WM_HINTS\n");
    goto out;
  }
  hints.flags = 2;
  hints.decorations = 0;
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, prop, prop, 32, 5, &hints);
  xcb_flush(conn);

  XFlush(display);
  XMapWindow(display, win);

  XSelectInput(display, win, StructureNotifyMask | ButtonPressMask);

  usleep(100000);
  XResizeWindow(display, win, w + 5, h + 5);
  XMoveWindow(display, win, x, y);

  while (1) {
    XNextEvent(display, &report);

    switch (report.type) {
      case ButtonPress:
        // Simulate button press (window will be unmapped)
        XUnmapWindow(display, win);
        printf("Window unmapped due to ButtonPress\n");
        break;

      case ConfigureNotify:
        x = report.xconfigure.x;
        y = report.xconfigure.y;
        w = (unsigned int)report.xconfigure.width;
        h = (unsigned int)report.xconfigure.height;
        s = (unsigned int)report.xconfigure.send_event;
        printf("confignotify %i,%i-%ux%u (send: %u)\n", x, y, w, h, s);
        break;
    }

    // Exit after handling events
    if (report.type == ButtonPress) {
      printf("Test completed. Closing the program.\n");
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

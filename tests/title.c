#define _DEFAULT_SOURCE
// title.c for the Openbox window manager

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>
#include <xcb/xcb.h>

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
  int x = 10, y = 10, h = 100, w = 400;
  XTextProperty name;
  xcb_atom_t nameprop, nameenc;
  int ret = 1;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <window_title> [nameprop] [encoding]\n", argv[0]);
    return 0;
  }

  // Open connection to the X server
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

  // Use the provided arguments for window property names and encoding
  if (argc > 2)
    nameprop = intern_atom(conn, argv[2]);
  else
    nameprop = intern_atom(conn, "WM_NAME");

  if (argc > 3)
    nameenc = intern_atom(conn, argv[3]);
  else
    nameenc = intern_atom(conn, "STRING");
  if (nameprop == XCB_ATOM_NONE || nameenc == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern name atoms\n");
    goto out;
  }

  // Create a simple window
  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                      0, NULL);
  if (win == None) {
    fprintf(stderr, "failed to create window\n");
    goto out;
  }

  // Set the window background color to white
  XSetWindowBackground(display, win, WhitePixel(display, 0));

  // Set the window title using the provided argument (argv[1])
  XStringListToTextProperty(&argv[1], 1, &name);
  XSetWMName(display, win, &name);

  // Alternatively, you can set the window property directly (for example with XChangeProperty)
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, nameprop, nameenc, 8, strlen(argv[1]), argv[1]);
  xcb_flush(conn);

  // Flush the display to ensure changes take effect
  XFlush(display);

  // Map the window (make it visible)
  XMapWindow(display, win);

  // Set the event mask to handle exposure and configuration changes
  XSelectInput(display, win, ExposureMask | StructureNotifyMask);

  // Event loop
  while (1) {
    XNextEvent(display, &report);

    switch (report.type) {
      case Expose:
        // The window is exposed (redrawn)
        printf("Window exposed\n");
        break;

      case ConfigureNotify:
        // The window has been resized or moved
        x = report.xconfigure.x;
        y = report.xconfigure.y;
        w = report.xconfigure.width;
        h = report.xconfigure.height;
        printf("Window configured: %i,%i-%ix%i\n", x, y, w, h);
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

#define _DEFAULT_SOURCE
// groupmodal.c for the Openbox window manager

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
  Window one = None, two = None, group = None;
  XEvent report;
  XWMHints* wmhints;
  xcb_atom_t state, modal;
  int ret = 1;

  display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Couldn't connect to X server :0\n");
    return 1;  // Return failure if X server connection fails
  }
  conn = XGetXCBConnection(display);
  if (conn == NULL) {
    fprintf(stderr, "couldn't get XCB connection\n");
    goto out;
  }

  state = intern_atom(conn, "_NET_WM_STATE");
  modal = intern_atom(conn, "_NET_WM_STATE_MODAL");

  // Check if atoms are valid
  if (state == XCB_ATOM_NONE || modal == XCB_ATOM_NONE) {
    fprintf(stderr, "Failed to intern atoms\n");
    goto out;
  }
  printf("Atoms successfully interned: _NET_WM_STATE = %u, _NET_WM_STATE_MODAL = %u\n", state, modal);

  // Create windows
  group = XCreateWindow(display, RootWindow(display, 0), 0, 0, 1, 1, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                        0, 0);
  if (group == 0) {
    fprintf(stderr, "Failed to create group window\n");
    XCloseDisplay(display);  // Clean up and close display
    return 1;                // Return failure if window creation fails
  }

  one = XCreateWindow(display, RootWindow(display, 0), 0, 0, 300, 300, 10, CopyFromParent, CopyFromParent,
                      CopyFromParent, 0, 0);
  if (one == 0) {
    fprintf(stderr, "Failed to create window one\n");
    XCloseDisplay(display);  // Clean up and close display
    return 1;                // Return failure if window creation fails
  }

  two = XCreateWindow(display, RootWindow(display, 0), 0, 0, 100, 100, 10, CopyFromParent, CopyFromParent,
                      CopyFromParent, 0, 0);
  if (two == 0) {
    fprintf(stderr, "Failed to create window two\n");
    XCloseDisplay(display);  // Clean up and close display
    return 1;                // Return failure if window creation fails
  }

  // Set window background colors
  XSetWindowBackground(display, one, WhitePixel(display, 0));
  XSetWindowBackground(display, two, BlackPixel(display, 0));

  // Set transient and modal state
  XSetTransientForHint(display, two, RootWindow(display, 0));
  {
    uint32_t modal_val = modal;
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, two, state, XCB_ATOM_ATOM, 32, 1, &modal_val);
    xcb_flush(conn);
  }

  // Set window group for both windows
  wmhints = XAllocWMHints();
  if (wmhints == NULL) {
    fprintf(stderr, "Failed to allocate memory for window hints\n");
    XCloseDisplay(display);  // Clean up and close display
    return 1;                // Return failure if allocation fails
  }

  wmhints->flags = WindowGroupHint;
  wmhints->window_group = group;

  XSetWMHints(display, one, wmhints);
  XSetWMHints(display, two, wmhints);
  XFree(wmhints);  // Free memory after use

  // Map the windows
  XMapWindow(display, one);
  XFlush(display);
  usleep(100000);  // Sleep to ensure the first window is mapped before the second
  XMapWindow(display, two);
  XFlush(display);

  ret = 0;
  // Event loop
  while (1) {
    XNextEvent(display, &report);
    // Process events (currently does nothing, but could be extended)
  }

out:
  if (one != None)
    XDestroyWindow(display, one);
  if (two != None)
    XDestroyWindow(display, two);
  if (group != None)
    XDestroyWindow(display, group);
  if (display)
    XCloseDisplay(display);
  return ret;
}

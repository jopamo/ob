#define _DEFAULT_SOURCE
/* modal2.c for the Openbox window manager */

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

static void send_modal(xcb_connection_t* conn,
                       xcb_window_t root,
                       xcb_window_t win,
                       xcb_atom_t state,
                       xcb_atom_t modal) {
  xcb_client_message_event_t ev = {
      .response_type = XCB_CLIENT_MESSAGE,
      .format = 32,
      .window = win,
      .type = state,
  };

  ev.data.data32[0] = 1;  // add
  ev.data.data32[1] = modal;

  xcb_send_event(conn, 0, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                 (const char*)&ev);
  xcb_flush(conn);
}

int main() {
  Display* display = NULL;
  xcb_connection_t* conn = NULL;
  Window parent = None, child = None;
  XEvent report;
  xcb_atom_t state, modal;
  int x = 10, y = 10, h = 400, w = 400;
  xcb_window_t root = XCB_WINDOW_NONE;
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
  root = RootWindow(display, DefaultScreen(display));

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

  // Set background colors
  XSetWindowBackground(display, parent, WhitePixel(display, 0));
  XSetWindowBackground(display, child, BlackPixel(display, 0));

  // Set the child window as transient for the parent window
  XSetTransientForHint(display, child, parent);

  // Map the windows (make them visible)
  XMapWindow(display, parent);
  XMapWindow(display, child);
  XFlush(display);

  send_modal(conn, root, child, state, modal);

  // Wait for events (simulate handling)
  XSelectInput(display, child, ExposureMask | StructureNotifyMask);

  // Event loop
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

    // Exit the loop after handling the first event to avoid infinite loops in CI
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

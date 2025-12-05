#define _DEFAULT_SOURCE
/* modal3.c for the Openbox window manager */

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
                       xcb_atom_t modal,
                       uint32_t action) {
  xcb_client_message_event_t ev = {
      .response_type = XCB_CLIENT_MESSAGE,
      .format = 32,
      .window = win,
      .type = state,
  };

  ev.data.data32[0] = action;
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
    fprintf(stderr, "failed to intern atoms\n");
    goto out;
  }

  // Create parent and child windows
  parent = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent,
                         CopyFromParent, 0, 0);
  child = XCreateWindow(display, RootWindow(display, 0), x, y, w / 2, h / 2, 10, CopyFromParent, CopyFromParent,
                        CopyFromParent, 0, 0);

  // Set backgrounds for parent and child windows
  XSetWindowBackground(display, parent, WhitePixel(display, 0));
  XSetWindowBackground(display, child, BlackPixel(display, 0));

  // Set the child window as transient for the parent window
  XSetTransientForHint(display, child, parent);

  // Map the windows to make them visible
  XMapWindow(display, parent);
  XMapWindow(display, child);
  XFlush(display);

  // Send the modal state to the child window, then clear it
  send_modal(conn, root, child, state, modal, 1);
  send_modal(conn, root, child, state, modal, 0);

  ret = 0;
  // Event loop (wait for events to be processed)
  while (1) {
    XNextEvent(display, &report);
    // Process events here (e.g., handle modal or expose events)
    if (report.type == Expose) {
      printf("Expose event\n");
    }
  }

out:
  if (parent != None)
    XDestroyWindow(display, parent);
  if (child != None)
    XDestroyWindow(display, child);
  if (display)
    XCloseDisplay(display);
  return ret;
}

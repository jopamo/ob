/* urgent.c for the Openbox window manager */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

static void toggle_state(xcb_connection_t* conn,
                         xcb_window_t root,
                         xcb_window_t win,
                         xcb_atom_t state_atom,
                         xcb_atom_t value_atom) {
  xcb_client_message_event_t ev = {
      .response_type = XCB_CLIENT_MESSAGE,
      .format = 32,
      .window = win,
      .type = state_atom,
  };

  ev.data.data32[0] = 1;  // add
  ev.data.data32[1] = value_atom;

  xcb_send_event(conn, 0, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                 (const char*)&ev);
  xcb_flush(conn);
}

int main() {
  Display* display = NULL;
  xcb_connection_t* conn = NULL;
  Window win = None;
  XEvent report;
  xcb_atom_t _net_attn = XCB_ATOM_NONE, _net_state = XCB_ATOM_NONE;
  int x = 50, y = 50, h = 100, w = 400;
  XWMHints hint;
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

  _net_state = intern_atom(conn, "_NET_WM_STATE");
  _net_attn = intern_atom(conn, "_NET_WM_STATE_DEMANDS_ATTENTION");
  if (_net_state == XCB_ATOM_NONE || _net_attn == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern urgency atoms\n");
    goto out;
  }

  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                      0, NULL);
  if (win == None) {
    fprintf(stderr, "failed to create window\n");
    goto out;
  }

  XSetWindowBackground(display, win, WhitePixel(display, 0));

  XMapWindow(display, win);
  XFlush(display);

  sleep(1);

  // Set the urgency hint on the window
  printf("urgent on\n");
  hint.flags = XUrgencyHint;
  XSetWMHints(display, win, &hint);
  XFlush(display);
  toggle_state(conn, root, win, _net_state, _net_attn);

  XSelectInput(display, win, ExposureMask | StructureNotifyMask);

  while (1) {
    XNextEvent(display, &report);

    switch (report.type) {
      case Expose:
        printf("exposed\n");
        break;
      case ConfigureNotify:
        x = report.xconfigure.x;
        y = report.xconfigure.y;
        w = report.xconfigure.width;
        h = report.xconfigure.height;
        printf("confignotify %i,%i-%ix%i\n", x, y, w, h);
        break;
    }

    // Exit after processing the first event to avoid infinite loops in CI
    if (report.type == Expose && report.xexpose.count == 0) {
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

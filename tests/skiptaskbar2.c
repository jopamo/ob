// skiptaskbar2.c for the Openbox window manager

#include <stdint.h>
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

static void send_state(xcb_connection_t* conn,
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

  ev.data.data32[0] = 1;
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
  xcb_atom_t state, skip;
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
  skip = intern_atom(conn, "_NET_WM_STATE_SKIP_TASKBAR");
  if (state == XCB_ATOM_NONE || skip == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern atoms\n");
    goto out;
  }

  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                      0, 0);

  if (win == None) {
    fprintf(stderr, "failed to create window\n");
    goto out;
  }

  XSetWindowBackground(display, win, WhitePixel(display, 0));

  XMapWindow(display, win);
  XFlush(display);

  sleep(1);

  send_state(conn, root, win, state, skip);

  ret = 0;
  while (1) {
    XNextEvent(display, &report);
    if (report.type == DestroyNotify)
      break;
  }

out:
  if (win != None)
    XDestroyWindow(display, win);
  if (display)
    XCloseDisplay(display);
  return ret;
}

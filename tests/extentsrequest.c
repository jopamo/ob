/* extentsrequest.c for the Openbox window manager */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

void request(Display* display, Atom _request, Atom _extents, Window win) {
  XEvent msg;
  msg.xclient.type = ClientMessage;
  msg.xclient.message_type = _request;
  msg.xclient.display = display;
  msg.xclient.window = win;
  msg.xclient.format = 32;
  msg.xclient.data.l[0] = 0l;
  msg.xclient.data.l[1] = 0l;
  msg.xclient.data.l[2] = 0l;
  msg.xclient.data.l[3] = 0l;
  msg.xclient.data.l[4] = 0l;
  XSendEvent(display, RootWindow(display, 0), False, SubstructureNotifyMask | SubstructureRedirectMask, &msg);
  XFlush(display);
}

void reply(Display* display, xcb_connection_t* conn, xcb_atom_t _extents) {
  printf("  waiting for extents\n");
  while (1) {
    XEvent report;
    XNextEvent(display, &report);

    if (report.type == PropertyNotify && report.xproperty.atom == (Atom)_extents) {
      xcb_get_property_cookie_t cookie =
          xcb_get_property(conn, 0, report.xproperty.window, _extents, XCB_ATOM_CARDINAL, 0, 4);
      xcb_get_property_reply_t* r = xcb_get_property_reply(conn, cookie, NULL);
      if (r && r->type == XCB_ATOM_CARDINAL && r->format == 32 && r->value_len == 4) {
        const uint32_t* values = (const uint32_t*)xcb_get_property_value(r);
        printf("  got new extents %u, %u, %u, %u\n", values[0], values[1], values[2], values[3]);
      }
      free(r);
      break;
    }
  }
}

void test_request(Display* display,
                  xcb_connection_t* conn,
                  Window win,
                  xcb_atom_t request_atom,
                  xcb_atom_t extents_atom,
                  xcb_atom_t type_prop,
                  xcb_atom_t type_value,
                  xcb_atom_t state_prop,
                  xcb_atom_t state_value,
                  const char* state_name) {
  printf("requesting for type %s\n", state_name);
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, type_prop, XCB_ATOM_ATOM, 32, 1, &type_value);
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, state_prop, XCB_ATOM_ATOM, 32, 1, &state_value);
  xcb_flush(conn);
  request(display, request_atom, extents_atom, win);
  reply(display, conn, extents_atom);
}

int main() {
  Display* display;
  Window win;
  xcb_atom_t _request, _extents, _type, _normal, _desktop, _state;
  xcb_atom_t _state_fs, _state_mh, _state_mv;
  int x = 10, y = 10, h = 100, w = 400;

  display = XOpenDisplay(NULL);

  if (display == NULL) {
    fprintf(stderr, "couldn't connect to X server :0\n");
    return 1;
  }

  xcb_connection_t* conn = XGetXCBConnection(display);
  _type = intern_atom(conn, "_NET_WM_WINDOW_TYPE");
  _normal = intern_atom(conn, "_NET_WM_WINDOW_TYPE_NORMAL");
  _desktop = intern_atom(conn, "_NET_WM_WINDOW_TYPE_DESKTOP");
  _request = intern_atom(conn, "_NET_REQUEST_FRAME_EXTENTS");
  _extents = intern_atom(conn, "_NET_FRAME_EXTENTS");
  _state = intern_atom(conn, "_NET_WM_STATE");
  _state_fs = intern_atom(conn, "_NET_WM_STATE_FULLSCREEN");
  _state_mh = intern_atom(conn, "_NET_WM_STATE_MAXIMIZED_HORZ");
  _state_mv = intern_atom(conn, "_NET_WM_STATE_MAXIMIZED_VERT");

  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                      0, NULL);
  XSelectInput(display, win, PropertyChangeMask);

  // Test all the states
  test_request(display, conn, win, _request, _extents, _type, _normal, _state, _state_fs, "normal+fullscreen");
  test_request(display, conn, win, _request, _extents, _type, _normal, _state, _state_mv, "normal+maximized_vert");
  test_request(display, conn, win, _request, _extents, _type, _normal, _state, _state_mh, "normal+maximized_horz");
  test_request(display, conn, win, _request, _extents, _type, _desktop, _state, _desktop, "desktop");

  XCloseDisplay(display);  // Close the display properly

  return 0;
}

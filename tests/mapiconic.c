/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   urgent.c for the Openbox window manager
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

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

int main() {
  Display* display = NULL;
  xcb_connection_t* conn = NULL;
  Window win = None;
  XEvent report;
  xcb_atom_t wm_state;
  int x = 50, y = 50, h = 100, w = 400;
  uint32_t state[2];
  XWMHints* hints;
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

  state[0] = IconicState;
  state[1] = XCB_WINDOW_NONE;

  wm_state = intern_atom(conn, "WM_STATE");
  if (wm_state == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern WM_STATE\n");
    goto out;
  }

  win = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                      0, NULL);
  if (win == None) {
    fprintf(stderr, "failed to create window\n");
    goto out;
  }

  hints = XAllocWMHints();
  hints->flags = StateHint;
  hints->initial_state = IconicState;
  XSetWMHints(display, win, hints);
  XFree(hints);
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, wm_state, wm_state, 32, 2, state);
  xcb_flush(conn);

  XSetWindowBackground(display, win, WhitePixel(display, 0));

  XMapWindow(display, win);
  XFlush(display);

  ret = 0;
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
  }

out:
  if (win != None)
    XDestroyWindow(display, win);
  if (display)
    XCloseDisplay(display);
  return ret;
}

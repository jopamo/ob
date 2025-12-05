#define _DEFAULT_SOURCE
/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   iconifydelay.c for the Openbox window manager
   Copyright (c) 2009   Dana Jansens

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

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

static void send_change_state(xcb_connection_t* conn, xcb_window_t root, xcb_window_t win, xcb_atom_t change_state) {
  xcb_client_message_event_t ev = {
      .response_type = XCB_CLIENT_MESSAGE,
      .format = 32,
      .window = win,
      .type = change_state,
  };

  ev.data.data32[0] = IconicState;
  xcb_send_event(conn, 0, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                 (const char*)&ev);
  xcb_flush(conn);
}

static void sleep_for_ms(long ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
  }
}

int main() {
  Display* display = NULL;
  xcb_connection_t* conn = NULL;
  Window win = None;
  XEvent report;
  xcb_atom_t change_state = XCB_ATOM_NONE;
  int x = 50, y = 50, h = 100, w = 400;
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

  change_state = intern_atom(conn, "WM_CHANGE_STATE");
  if (change_state == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern WM_CHANGE_STATE\n");
    goto out;
  }

  win = XCreateWindow(display, RootWindow(display, DefaultScreen(display)), x, y, w, h, 10, CopyFromParent,
                      CopyFromParent, CopyFromParent, 0, NULL);
  XSetWindowBackground(display, win, WhitePixel(display, DefaultScreen(display)));

  sleep_for_ms(100);
  XMapWindow(display, win);
  XFlush(display);
  sleep_for_ms(100);

  send_change_state(conn, root, win, change_state);

  ret = 0;
  while (1) {
    XNextEvent(display, &report);
  }

out:
  if (win != None)
    XDestroyWindow(display, win);
  if (display)
    XCloseDisplay(display);
  return ret;
}

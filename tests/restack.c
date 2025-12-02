/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   extentsrequest.c for the Openbox window manager
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
#include <X11/Xatom.h>
#include <xcb/xcb.h>

static xcb_atom_t intern_atom(xcb_connection_t* conn, const char* name) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(name), name);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

static void send_restack(xcb_connection_t* conn,
                         xcb_window_t root,
                         xcb_window_t win,
                         xcb_atom_t restack,
                         uint32_t detail) {
  xcb_client_message_event_t ev = {
      .response_type = XCB_CLIENT_MESSAGE,
      .format = 32,
      .window = win,
      .type = restack,
  };

  ev.data.data32[0] = 2;
  ev.data.data32[1] = 0;
  ev.data.data32[2] = detail;

  xcb_send_event(conn, 0, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                 (const char*)&ev);
  xcb_flush(conn);
}

int main() {
  Display* display = NULL;
  xcb_connection_t* conn = NULL;
  Window win = None;
  XEvent report;
  xcb_atom_t _restack;
  int x = 10, y = 10, h = 100, w = 400;
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

  _restack = intern_atom(conn, "_NET_RESTACK_WINDOW");
  if (_restack == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern _NET_RESTACK_WINDOW\n");
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

  printf("requesting bottom in 3\n");
  sleep(3);

  send_restack(conn, root, win, _restack, Below);

  printf("requesting top in 3\n");
  sleep(3);

  send_restack(conn, root, win, _restack, Above);

  printf("requesting bottomif in 3\n");
  sleep(3);

  send_restack(conn, root, win, _restack, BottomIf);

  printf("requesting topif in 3\n");
  sleep(3);

  send_restack(conn, root, win, _restack, TopIf);

  printf("requesting opposite in 3\n");
  sleep(3);

  send_restack(conn, root, win, _restack, Opposite);

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

/* duplicatesession.c for the Openbox window manager */

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
  Window win1 = None, win2 = None;
  XEvent report;
  int x = 10, y = 10, h = 100, w = 400;
  xcb_atom_t sm_id, enc;
  int ret = 1;

  // Open the X display
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

  // Intern atoms
  sm_id = intern_atom(conn, "SM_CLIENT_ID");
  enc = intern_atom(conn, "STRING");
  if (sm_id == XCB_ATOM_NONE || enc == XCB_ATOM_NONE) {
    fprintf(stderr, "failed to intern SM_CLIENT_ID or STRING\n");
    goto out;
  }

  // Create two windows
  win1 = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                       0, NULL);
  win2 = XCreateWindow(display, RootWindow(display, 0), x, y, w, h, 10, CopyFromParent, CopyFromParent, CopyFromParent,
                       0, NULL);
  if (win1 == None || win2 == None) {
    fprintf(stderr, "failed to create windows\n");
    goto out;
  }

  // Set background colors for the windows
  XSetWindowBackground(display, win1, WhitePixel(display, 0));
  XSetWindowBackground(display, win2, BlackPixel(display, 0));

  // Change the properties for the windows
  const char* sid = "abcdefg";
  xcb_change_property(conn, XCB_PROP_MODE_APPEND, win1, sm_id, enc, 8, strlen(sid), sid);
  xcb_change_property(conn, XCB_PROP_MODE_APPEND, win2, sm_id, enc, 8, strlen(sid), sid);
  xcb_flush(conn);

  // Flush the display and map the windows
  XFlush(display);
  XMapWindow(display, win1);
  XMapWindow(display, win2);

  // Select the events to listen for
  XSelectInput(display, win1, ExposureMask | StructureNotifyMask);
  XSelectInput(display, win2, ExposureMask | StructureNotifyMask);

  // Event loop to process events
  while (1) {
    XNextEvent(display, &report);

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
  if (win1 != None)
    XDestroyWindow(display, win1);
  if (win2 != None)
    XDestroyWindow(display, win2);
  if (display)
    XCloseDisplay(display);

  return ret;
}

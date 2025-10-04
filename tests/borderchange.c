/* borderchange.c for Openbox window manager */

#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>

int main(void) {
  Display* display = NULL;
  Window win = 0;
  XEvent report;
  int x = 10, y = 10, h = 200, w = 200;

  display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "error: couldn't connect to X server :0\n");
    return 1;
  }

  int screen = DefaultScreen(display);

  // create window with initial 10px border
  win = XCreateWindow(display, RootWindow(display, screen), x, y, w, h, 10, CopyFromParent, CopyFromParent,
                      CopyFromParent, 0, NULL);
  if (!win) {
    fprintf(stderr, "error: XCreateWindow failed\n");
    XCloseDisplay(display);
    return 1;
  }

  XSetWindowBackground(display, win, WhitePixel(display, screen));
  XSelectInput(display, win, ExposureMask | StructureNotifyMask);

  XMapWindow(display, win);
  XFlush(display);

  // initial window events
  while (XPending(display)) {
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

  sleep(2);

  // set border width to 0
  printf("setting border to 0\n");
  XSetWindowBorderWidth(display, win, 0);
  XFlush(display);

  // events after border width change
  while (XPending(display)) {
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

  sleep(2);

  // set border width to 50
  printf("setting border to 50\n");
  XSetWindowBorderWidth(display, win, 50);
  XFlush(display);

  // events after border width change
  while (XPending(display)) {
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

  // cleanup so Valgrind shows no definite leak
  XDestroyWindow(display, win);
  XSync(display, False);
  XCloseDisplay(display);

  return 0;
}

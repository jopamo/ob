// fallback.c for the Openbox window manager
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

static void sleep_for_ms(long ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
  }
}

int main() {
  Display* display;
  Window one, two;
  XEvent report;

  display = XOpenDisplay(NULL);

  if (display == NULL) {
    fprintf(stderr, "couldn't connect to X server :0\n");
    return 1;  // Return failure if unable to connect to the display
  }

  one = XCreateWindow(display, RootWindow(display, 0), 0, 0, 200, 200, 10, CopyFromParent, CopyFromParent,
                      CopyFromParent, 0, 0);
  two = XCreateWindow(display, RootWindow(display, 0), 0, 0, 150, 150, 10, CopyFromParent, CopyFromParent,
                      CopyFromParent, 0, 0);

  if (!one || !two) {
    fprintf(stderr, "Failed to create windows\n");
    return 1;  // Return failure if windows are not created
  }

  XSetWindowBackground(display, one, WhitePixel(display, 0));
  XSetWindowBackground(display, two, BlackPixel(display, 0));

  XSetTransientForHint(display, two, one);

  // Map the first window and check the event
  XMapWindow(display, one);
  XFlush(display);
  XNextEvent(display, &report);
  if (report.type != MapNotify) {
    fprintf(stderr, "Failed to map the first window correctly\n");
    return 1;
  }

  // Map the second window and check the event
  XMapWindow(display, two);
  XFlush(display);
  XNextEvent(display, &report);
  if (report.type != MapNotify) {
    fprintf(stderr, "Failed to map the second window correctly\n");
    return 1;
  }

  // Destroy the second window
  XDestroyWindow(display, two);
  XFlush(display);
  sleep_for_ms(1);

  // Destroy the first window
  XDestroyWindow(display, one);
  XSync(display, False);  // Ensure all events are processed before exit

  // Check for proper closure
  XCloseDisplay(display);  // Close the display connection

  return 0;
}

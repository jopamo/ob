// icons.c for the Openbox window manager

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <assert.h>
#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>

static xcb_atom_t intern_atom(xcb_connection_t* conn, const char* name, gboolean only_if_exists) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_if_exists, strlen(name), name);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

static gboolean window_has_wm_state(xcb_connection_t* conn, Window win, xcb_atom_t state_atom) {
  xcb_get_property_cookie_t cookie = xcb_get_property(conn, 0, win, state_atom, state_atom, 0, 1);
  xcb_get_property_reply_t* reply = xcb_get_property_reply(conn, cookie, NULL);
  gboolean found = FALSE;

  if (reply && reply->type == state_atom && reply->format == 32 && reply->value_len >= 1)
    found = TRUE;

  free(reply);
  return found;
}

Window findClient(Display* d, xcb_connection_t* conn, Window win, xcb_atom_t state_atom) {
  Window r, *children;
  unsigned int n, i;
  Window found = None;

  XQueryTree(d, win, &r, &r, &children, &n);
  for (i = 0; i < n; ++i) {
    found = findClient(d, conn, children[i], state_atom);
    if (found)
      break;
  }
  if (children)
    XFree(children);

  if (found)
    return found;
  if (state_atom != XCB_ATOM_NONE && window_has_wm_state(conn, win, state_atom))
    return win;

  return None;
}

int main(int argc, char** argv) {
  Display* d = XOpenDisplay(NULL);
  if (!d) {
    fprintf(stderr, "couldn't connect to X server\n");
    return 1;
  }
  xcb_connection_t* conn = XGetXCBConnection(d);
  if (!conn) {
    fprintf(stderr, "failed to acquire XCB connection\n");
    return 1;
  }
  int s = DefaultScreen(d);
  xcb_atom_t net_wm_icon = intern_atom(conn, "_NET_WM_ICON", True);
  xcb_atom_t wm_state = intern_atom(conn, "WM_STATE", True);
  if (net_wm_icon == XCB_ATOM_NONE) {
    fprintf(stderr, "_NET_WM_ICON not available\n");
    return 1;
  }
  unsigned int winw = 0, winh = 0;
  const int MAX_IMAGES = 10;
  uint32_t* prop_return[MAX_IMAGES];
  XImage* i[MAX_IMAGES];
  uint32_t offset = 0;
  uint32_t bytes_left = 0;
  unsigned int image = 0;
  unsigned int j;  // loop counter
  Window id, win;
  Pixmap p;
  Cursor cur;
  XEvent ev;
  const unsigned int bs = sizeof(uint32_t);

  printf("Click on a window with an icon...\n");

  // int id = strtol(argv[1], NULL, 16);
  XUngrabPointer(d, CurrentTime);
  cur = XCreateFontCursor(d, XC_crosshair);
  XGrabPointer(d, RootWindow(d, s), False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, cur, CurrentTime);
  while (1) {
    XNextEvent(d, &ev);
    if (ev.type == ButtonPress) {
      XUngrabPointer(d, CurrentTime);
      id = findClient(d, conn, ev.xbutton.subwindow, wm_state);
      break;
    }
  }

  printf("Using window 0x%lx\n", id);

  do {
    uint32_t w, h;
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t* reply = NULL;
    const uint32_t* values;

    cookie = xcb_get_property(conn, 0, id, net_wm_icon, XCB_ATOM_CARDINAL, offset, 1);
    reply = xcb_get_property_reply(conn, cookie, NULL);
    if (!reply || reply->type == XCB_ATOM_NONE || reply->format != 32 || reply->value_len < 1) {
      printf("No icon found\n");
      free(reply);
      return 1;
    }
    values = (const uint32_t*)xcb_get_property_value(reply);
    if (!values) {
      free(reply);
      printf("No icon found\n");
      return 1;
    }
    w = values[0];
    offset += reply->value_len;
    free(reply);

    cookie = xcb_get_property(conn, 0, id, net_wm_icon, XCB_ATOM_CARDINAL, offset, 1);
    reply = xcb_get_property_reply(conn, cookie, NULL);
    if (!reply || reply->type == XCB_ATOM_NONE || reply->format != 32 || reply->value_len < 1) {
      printf("Failed to get height\n");
      free(reply);
      return 1;
    }
    values = (const uint32_t*)xcb_get_property_value(reply);
    if (!values) {
      free(reply);
      printf("Failed to get height\n");
      return 1;
    }
    h = values[0];
    offset += reply->value_len;
    free(reply);

    if (w == 0 || h == 0) {
      printf("Invalid icon size\n");
      return 1;
    }

    cookie = xcb_get_property(conn, 0, id, net_wm_icon, XCB_ATOM_CARDINAL, offset, w * h);
    reply = xcb_get_property_reply(conn, cookie, NULL);
    if (!reply || reply->type == XCB_ATOM_NONE || reply->format != 32 || reply->value_len < w * h) {
      printf("Failed to get image data\n");
      free(reply);
      return 1;
    }
    bytes_left = reply->bytes_after;
    values = (const uint32_t*)xcb_get_property_value(reply);
    if (!values) {
      free(reply);
      printf("Failed to get image data\n");
      return 1;
    }
    prop_return[image] = g_memdup2(values, reply->value_len * sizeof(uint32_t));
    offset += reply->value_len;
    free(reply);

    printf("Found icon with size %ux%u\n", w, h);

    i[image] = XCreateImage(d, DefaultVisual(d, s), DefaultDepth(d, s), ZPixmap, 0, NULL, w, h, 32, 0);
    assert(i[image]);
    i[image]->byte_order = LSBFirst;
    i[image]->data = (char*)prop_return[image];
    for (j = 0; j < w * h; j++) {
      unsigned char alpha = (unsigned char)i[image]->data[j * bs + 3];
      unsigned char r = (unsigned char)i[image]->data[j * bs + 0];
      unsigned char g = (unsigned char)i[image]->data[j * bs + 1];
      unsigned char b = (unsigned char)i[image]->data[j * bs + 2];

      // background color
      unsigned char bgr = 0;
      unsigned char bgg = 0;
      unsigned char bgb = 0;

      r = bgr + (r - bgr) * alpha / 256;
      g = bgg + (g - bgg) * alpha / 256;
      b = bgb + (b - bgb) * alpha / 256;

      i[image]->data[j * 4 + 0] = (char)r;
      i[image]->data[j * 4 + 1] = (char)g;
      i[image]->data[j * 4 + 2] = (char)b;
    }

    winw += w;
    if (h > winh)
      winh = h;

    ++image;
  } while (bytes_left > 0 && image < (unsigned int)MAX_IMAGES);

#define hashsize(n) ((guint32)1 << (n))
#define hashmask(n) (hashsize(n) - 1)
#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

#define mix(a, b, c) \
  {                  \
    a -= c;          \
    a ^= rot(c, 4);  \
    c += b;          \
    b -= a;          \
    b ^= rot(a, 6);  \
    a += c;          \
    c -= b;          \
    c ^= rot(b, 8);  \
    b += a;          \
    a -= c;          \
    a ^= rot(c, 16); \
    c += b;          \
    b -= a;          \
    b ^= rot(a, 19); \
    a += c;          \
    c -= b;          \
    c ^= rot(b, 4);  \
    b += a;          \
  }

#define final(a, b, c) \
  {                    \
    c ^= b;            \
    c -= rot(b, 14);   \
    a ^= c;            \
    a -= rot(c, 11);   \
    b ^= a;            \
    b -= rot(a, 25);   \
    c ^= b;            \
    c -= rot(b, 16);   \
    a ^= c;            \
    a -= rot(c, 4);    \
    b ^= a;            \
    b -= rot(a, 14);   \
    c ^= b;            \
    c -= rot(b, 24);   \
  }

  /* hash the images */
  for (j = 0; j < image; ++j) {
    unsigned int w, h, length;
    guint32 a, b, c;
    guint32 initval = 0xf00d;
    const guint32* k = (guint32*)i[j]->data;

    w = i[j]->width;
    h = i[j]->height;
    length = w * h;

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + (((guint32)length) << 2) + initval;

    /*---------------------------------------- handle most of the key */
    while (length > 3) {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 3;
      k += 3;
    }

    /*--------------------------------- handle the last 3 uint32_t's */
    switch (length) /* all the case statements fall through */
    {
      case 3:
        c += k[2];
        /* fall through */
      case 2:
        b += k[1];
        /* fall through */
      case 1:
        a += k[0];
        final(a, b, c);
        /* fall through */
      case 0: /* case 0: nothing left to add */
        break;
    }
    /*------------------------------------ report the result */
    printf("image[%d] %ux%u %u\n", j, w, h, c);
  }

  win = XCreateSimpleWindow(d, RootWindow(d, s), 0, 0, winw, winh, 0, 0, 0);
  assert(win);
  XMapWindow(d, win);

  p = XCreatePixmap(d, win, winw, winh, DefaultDepth(d, s));
  XFillRectangle(d, p, DefaultGC(d, s), 0, 0, winw, winh);

  for (j = 0; j < image; ++j) {
    static unsigned int x = 0;

    XPutImage(d, p, DefaultGC(d, s), i[j], 0, 0, x, 0, i[j]->width, i[j]->height);
    x += i[j]->width;
    XDestroyImage(i[j]);
  }

  XSetWindowBackgroundPixmap(d, win, p);
  XClearWindow(d, win);

  XFlush(d);

  getchar();

  XFreePixmap(d, p);
  XCloseDisplay(d);
}

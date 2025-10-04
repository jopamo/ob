/* mask.c for the Openbox window manager */

#include "render.h"
#include "color.h"
#include "mask.h"
#include <glib.h>

/* compute number of bytes for a 1bpp bitmap with width w and height h, rounded up to the nearest byte */
#define BITMAP_BYTES(w, h) (((gsize)((w) + 7u) / 8u) * (gsize)(h))

RrPixmapMask* RrPixmapMaskNew(const RrInstance* inst, gint w, gint h, const gchar* data) {
  g_return_val_if_fail(inst != NULL, NULL);
  g_return_val_if_fail(w > 0 && h > 0, NULL);
  g_return_val_if_fail(data != NULL, NULL);

  RrPixmapMask* m = g_slice_new(RrPixmapMask);
  m->inst = inst;
  m->width = w;
  m->height = h;

  /* round up to nearest byte */
  gsize bytes = BITMAP_BYTES(w, h);
  m->data = g_memdup2(data, bytes);

  m->mask = XCreateBitmapFromData(RrDisplay(inst), RrRootWindow(inst), data, (unsigned int)w, (unsigned int)h);

  if (!m->mask) {
    g_free(m->data);
    g_slice_free(RrPixmapMask, m);
    return NULL;
  }

  return m;
}

void RrPixmapMaskFree(RrPixmapMask* m) {
  if (!m)
    return;
  if (m->mask)
    XFreePixmap(RrDisplay(m->inst), m->mask);
  g_free(m->data);
  g_slice_free(RrPixmapMask, m);
}

void RrPixmapMaskDraw(Pixmap p, const RrTextureMask* m, const RrRect* area) {
  g_return_if_fail(m != NULL);
  g_return_if_fail(m->mask != NULL);
  g_return_if_fail(area != NULL);

  gint x = area->x + (area->width - m->mask->width) / 2;
  gint y = area->y + (area->height - m->mask->height) / 2;
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;

  Display* dpy = RrDisplay(m->mask->inst);
  GC gc = RrColorGC(m->color);

  /* set the clip region */
  XSetClipMask(dpy, gc, m->mask->mask);
  XSetClipOrigin(dpy, gc, x, y);

  /* fill in the clipped region */
  XFillRectangle(dpy, p, gc, x, y, (unsigned int)m->mask->width, (unsigned int)m->mask->height);

  /* unset the clip region */
  XSetClipMask(dpy, gc, None);
  XSetClipOrigin(dpy, gc, 0, 0);
}

RrPixmapMask* RrPixmapMaskCopy(const RrPixmapMask* src) {
  g_return_val_if_fail(src != NULL, NULL);
  g_return_val_if_fail(src->inst != NULL, NULL);
  g_return_val_if_fail(src->width > 0 && src->height > 0, NULL);
  g_return_val_if_fail(src->data != NULL, NULL);

  RrPixmapMask* m = g_slice_new(RrPixmapMask);
  m->inst = src->inst;
  m->width = src->width;
  m->height = src->height;

  /* round up to nearest byte */
  gsize bytes = BITMAP_BYTES(src->width, src->height);
  m->data = g_memdup2(src->data, bytes);

  m->mask = XCreateBitmapFromData(RrDisplay(m->inst), RrRootWindow(m->inst), (const char*)m->data,
                                  (unsigned int)m->width, (unsigned int)m->height);

  if (!m->mask) {
    g_free(m->data);
    g_slice_free(RrPixmapMask, m);
    return NULL;
  }

  return m;
}

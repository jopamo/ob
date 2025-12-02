/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   frame.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
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

#include "frame.h"
#include "client.h"
#include "openbox.h"
#include "grab.h"
#include "debug.h"
#include "config.h"
#include "framerender.h"
#include "focus_cycle.h"
#include "focus_cycle_indicator.h"
#include "moveresize.h"
#include "screen.h"
#include "obrender/theme.h"
#include "obt/display.h"
#include "obt/xqueue.h"
#include "obt/prop.h"

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#ifdef SHAPE
#include <xcb/shape.h>
#endif
#ifdef DAMAGE
#include <X11/extensions/Xdamage.h>
#endif

#define FRAME_EVENTMASK                                                                      \
  (XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_BUTTON_PRESS | \
   XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_FOCUS_CHANGE)
#define ELEMENT_EVENTMASK                                                                       \
  (XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION | \
   XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW)

#define FRAME_ANIMATE_ICONIFY_TIME 150000           /* .15 seconds */
#define FRAME_ANIMATE_ICONIFY_STEP_TIME (1000 / 60) /* 60 Hz */

#define FRAME_HANDLE_Y(f) (f->size.top + f->client->area.height + f->cbwidth_b)

static void flash_done(gpointer data);
static gboolean flash_timeout(gpointer data);

static void layout_title(ObFrame* self);
static void set_theme_statics(ObFrame* self);
static void free_theme_statics(ObFrame* self);
static gboolean frame_animate_iconify(gpointer self);
static void frame_adjust_cursors(ObFrame* self);
static void frame_publish_updates(ObFrame* self, gboolean force_extents, gboolean send_damage);
static void frame_apply_shape(ObFrame* self);

typedef struct {
  xcb_colormap_t cmap;
  guint refcount;
} FrameColormapEntry;

static GHashTable* frame_colormap_cache = NULL;

static xcb_colormap_t frame_colormap_acquire(xcb_visualid_t visual);
static void frame_colormap_release(xcb_visualid_t visual, xcb_colormap_t cmap);

static xcb_screen_t* get_screen(xcb_connection_t* conn, int screen_num) {
  xcb_screen_iterator_t iter;
  iter = xcb_setup_roots_iterator(xcb_get_setup(conn));
  for (; iter.rem; --screen_num, xcb_screen_next(&iter))
    if (screen_num == 0)
      return iter.data;
  return NULL;
}

static Window createWindow(Window parent, xcb_visualid_t visual, uint32_t mask, uint32_t* val) {
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  xcb_window_t win = xcb_generate_id(conn);
  uint8_t depth = 0; /* CopyFromParent */

  if (visual) {
    /* find the depth for the visual */
    xcb_depth_iterator_t d_it;
    xcb_screen_t* screen = get_screen(conn, ob_screen);
    if (screen) {
      d_it = xcb_screen_allowed_depths_iterator(screen);
      for (; d_it.rem; xcb_depth_next(&d_it)) {
        xcb_visualtype_iterator_t v_it = xcb_depth_visuals_iterator(d_it.data);
        for (; v_it.rem; xcb_visualtype_next(&v_it)) {
          if (v_it.data->visual_id == visual) {
            depth = d_it.data->depth;
            goto found_depth;
          }
        }
      }
    }
  found_depth:;
  }
  else {
    depth = RrDepth(ob_rr_inst);
    visual = RrVisual(ob_rr_inst)->visualid;
  }

  xcb_create_window(conn, depth, win, parent, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, visual, mask, val);
  return win;
}

static xcb_visualid_t check_32bit_client(ObClient* c) {
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  xcb_get_window_attributes_reply_t* attr;
  xcb_visualid_t visual = XCB_NONE;

  /* we're already running at 32 bit depth, yay. we don't need to use their
     visual */
  if (RrDepth(ob_rr_inst) == 32)
    return XCB_NONE;

  /* We need to find the depth of the visual. xcb_get_window_attributes returns visual, but not depth directly?
     Actually, xcb_get_geometry returns depth. */
  xcb_get_geometry_reply_t* geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, c->window), NULL);
  if (geom) {
    if (geom->depth == 32) {
      attr = xcb_get_window_attributes_reply(conn, xcb_get_window_attributes(conn, c->window), NULL);
      if (attr) {
        visual = attr->visual;
        free(attr);
      }
    }
    free(geom);
  }
  return visual;
}

static xcb_colormap_t frame_colormap_acquire(xcb_visualid_t visual) {
  FrameColormapEntry* entry;

  g_assert(visual != XCB_NONE);

  if (!frame_colormap_cache)
    frame_colormap_cache = g_hash_table_new(NULL, NULL);

  entry = g_hash_table_lookup(frame_colormap_cache, GUINT_TO_POINTER(visual));
  if (!entry) {
    xcb_connection_t* conn = XGetXCBConnection(obt_display);
    entry = g_new0(FrameColormapEntry, 1);
    entry->cmap = xcb_generate_id(conn);
    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, entry->cmap, obt_root(ob_screen), visual);
    entry->refcount = 1;
    g_hash_table_insert(frame_colormap_cache, GUINT_TO_POINTER(visual), entry);
  }
  else
    entry->refcount++;

  return entry->cmap;
}

static void frame_colormap_release(xcb_visualid_t visual, xcb_colormap_t cmap) {
  FrameColormapEntry* entry;

  if (!visual || !cmap || !frame_colormap_cache)
    return;

  entry = g_hash_table_lookup(frame_colormap_cache, GUINT_TO_POINTER(visual));
  if (!entry || entry->cmap != cmap)
    return;

  g_assert(entry->refcount > 0);
  entry->refcount--;
  if (entry->refcount == 0) {
    g_hash_table_remove(frame_colormap_cache, GUINT_TO_POINTER(visual));
    xcb_free_colormap(XGetXCBConnection(obt_display), entry->cmap);
    g_free(entry);
  }
}

ObFrame* frame_new(ObClient* client) {
  uint32_t mask;
  uint32_t val[4];
  ObFrame* self;
  xcb_visualid_t visual;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);

  self = g_slice_new0(ObFrame);
  self->client = client;

  visual = check_32bit_client(client);

  /* create the non-visible decor windows */

  mask = 0;
  int i = 0;
  if (visual) {
    /* client has a 32-bit visual */
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_COLORMAP;
    self->colormap_visual = visual;
    self->colormap = frame_colormap_acquire(visual);
    val[i++] = BlackPixel(obt_display, ob_screen); /* back pixel */
    val[i++] = BlackPixel(obt_display, ob_screen); /* border pixel */
    val[i++] = self->colormap;
  }
  self->window = createWindow(obt_root(ob_screen), visual, mask, val);

  /* create the visible decor windows */

  mask = 0;
  i = 0;
  if (visual) {
    /* client has a 32-bit visual */
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_COLORMAP;
    val[i++] = BlackPixel(obt_display, ob_screen); /* back pixel */
    val[i++] = BlackPixel(obt_display, ob_screen); /* border pixel */
    val[i++] = RrColormap(ob_rr_inst);
  }

  self->backback = createWindow(self->window, 0, mask, val);
  self->backfront = createWindow(self->backback, 0, mask, val);

  mask |= XCB_CW_EVENT_MASK;
  val[i] = ELEMENT_EVENTMASK; /* Append event mask */
  /* i is now index of event mask */

  self->innerleft = createWindow(self->window, 0, mask, val);
  self->innertop = createWindow(self->window, 0, mask, val);
  self->innerright = createWindow(self->window, 0, mask, val);
  self->innerbottom = createWindow(self->window, 0, mask, val);

  self->innerblb = createWindow(self->innerbottom, 0, mask, val);
  self->innerbrb = createWindow(self->innerbottom, 0, mask, val);
  self->innerbll = createWindow(self->innerleft, 0, mask, val);
  self->innerbrr = createWindow(self->innerright, 0, mask, val);

  self->title = createWindow(self->window, 0, mask, val);
  self->titleleft = createWindow(self->window, 0, mask, val);
  self->titletop = createWindow(self->window, 0, mask, val);
  self->titletopleft = createWindow(self->window, 0, mask, val);
  self->titletopright = createWindow(self->window, 0, mask, val);
  self->titleright = createWindow(self->window, 0, mask, val);
  self->titlebottom = createWindow(self->window, 0, mask, val);

  self->topresize = createWindow(self->title, 0, mask, val);
  self->tltresize = createWindow(self->title, 0, mask, val);
  self->tllresize = createWindow(self->title, 0, mask, val);
  self->trtresize = createWindow(self->title, 0, mask, val);
  self->trrresize = createWindow(self->title, 0, mask, val);

  self->left = createWindow(self->window, 0, mask, val);
  self->right = createWindow(self->window, 0, mask, val);

  self->label = createWindow(self->title, 0, mask, val);
  self->max = createWindow(self->title, 0, mask, val);
  self->close = createWindow(self->title, 0, mask, val);
  self->desk = createWindow(self->title, 0, mask, val);
  self->shade = createWindow(self->title, 0, mask, val);
  self->icon = createWindow(self->title, 0, mask, val);
  self->iconify = createWindow(self->title, 0, mask, val);

  self->handle = createWindow(self->window, 0, mask, val);
  self->lgrip = createWindow(self->handle, 0, mask, val);
  self->rgrip = createWindow(self->handle, 0, mask, val);

  self->handleleft = createWindow(self->handle, 0, mask, val);
  self->handleright = createWindow(self->handle, 0, mask, val);

  self->handletop = createWindow(self->window, 0, mask, val);
  self->handlebottom = createWindow(self->window, 0, mask, val);
  self->lgripleft = createWindow(self->window, 0, mask, val);
  self->lgriptop = createWindow(self->window, 0, mask, val);
  self->lgripbottom = createWindow(self->window, 0, mask, val);
  self->rgripright = createWindow(self->window, 0, mask, val);
  self->rgriptop = createWindow(self->window, 0, mask, val);
  self->rgripbottom = createWindow(self->window, 0, mask, val);

  self->focused = FALSE;

  /* the other stuff is shown based on decor settings */
  xcb_map_window(conn, self->label);
  xcb_map_window(conn, self->backback);
  xcb_map_window(conn, self->backfront);

  self->max_press = self->close_press = self->desk_press = self->iconify_press = self->shade_press = FALSE;
  self->max_hover = self->close_hover = self->desk_hover = self->iconify_hover = self->shade_hover = FALSE;

  /* make sure the size will be different the first time, so the extent hints
     will be set */
  STRUT_SET(self->oldsize, -1, -1, -1, -1);

  set_theme_statics(self);

  return self;
}

static void set_theme_statics(ObFrame* self) {
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  uint32_t mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
  uint32_t val[2];

  /* set colors/appearance/sizes for stuff that doesn't change */
  val[0] = geom->button_size;
  val[1] = geom->button_size;
  xcb_configure_window(conn, self->max, mask, val);
  xcb_configure_window(conn, self->iconify, mask, val);
  xcb_configure_window(conn, self->close, mask, val);
  xcb_configure_window(conn, self->desk, mask, val);
  xcb_configure_window(conn, self->shade, mask, val);

  val[0] = geom->button_size + 2;
  val[1] = geom->button_size + 2;
  xcb_configure_window(conn, self->icon, mask, val);

  val[0] = geom->grip_width;
  val[1] = geom->paddingy + 1;
  xcb_configure_window(conn, self->tltresize, mask, val);
  xcb_configure_window(conn, self->trtresize, mask, val);

  val[0] = geom->paddingx + 1;
  val[1] = geom->title_height;
  xcb_configure_window(conn, self->tllresize, mask, val);
  xcb_configure_window(conn, self->trrresize, mask, val);
}

static void free_theme_statics(ObFrame* self) {}

void frame_free(ObFrame* self) {
  free_theme_statics(self);

  xcb_destroy_window(XGetXCBConnection(obt_display), self->window);
  if (self->colormap)
    frame_colormap_release(self->colormap_visual, self->colormap);

  g_slice_free(ObFrame, self);
}

void frame_show(ObFrame* self) {
  if (!self->visible) {
    self->visible = TRUE;
    framerender_frame(self);
    /* Grab the server to make sure that the frame window is mapped before
       the client gets its MapNotify, i.e. to make sure the client is
       _visible_ when it gets MapNotify. */
    grab_server(TRUE);
    xcb_connection_t* conn = XGetXCBConnection(obt_display);
    xcb_map_window(conn, self->client->window);
    xcb_map_window(conn, self->window);
    grab_server(FALSE);
  }
}

void frame_hide(ObFrame* self) {
  if (self->visible) {
    self->visible = FALSE;
    xcb_connection_t* conn = XGetXCBConnection(obt_display);
    if (!frame_iconify_animating(self))
      xcb_unmap_window(conn, self->window);
    /* we unmap the client itself so that we can get MapRequest
       events, and because the ICCCM tells us to! */
    xcb_unmap_window(conn, self->client->window);
    self->client->ignore_unmaps += 1;
  }
}

void frame_adjust_theme(ObFrame* self) {
  free_theme_statics(self);
  set_theme_statics(self);
}

static void frame_publish_updates(ObFrame* self, gboolean force_extents, gboolean send_damage) {
  gboolean suppress_damage = self->suppress_damage_once;
  self->suppress_damage_once = FALSE;
#ifdef DAMAGE
  if (send_damage && obt_display_extension_damage && !suppress_damage)
    XDamageAdd(obt_display, self->window, None);
#else
  (void)send_damage;
  (void)suppress_damage;
#endif

  if (force_extents || !STRUT_EQUAL(self->size, self->oldsize)) {
    gulong vals[4];

    vals[0] = self->size.left;
    vals[1] = self->size.right;
    vals[2] = self->size.top;
    vals[3] = self->size.bottom;
    OBT_PROP_SETA32(self->client->window, NET_FRAME_EXTENTS, CARDINAL, vals, 4);
    self->oldsize = self->size;
  }
}

#ifdef SHAPE
void frame_adjust_shape_kind(ObFrame* self, int kind) {
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  gint num;
  xcb_rectangle_t xrect[2];
  gboolean shaped;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);

  shaped = (kind == XCB_SHAPE_SK_BOUNDING && self->client->shaped);
#ifdef ShapeInput
  shaped |= (kind == XCB_SHAPE_SK_INPUT && self->client->shaped_input);
#endif

  if (!shaped) {
    /* clear the shape on the frame window */
    xcb_shape_mask(conn, kind, XCB_SHAPE_SO_SET, self->window, self->size.left, self->size.top, XCB_NONE);
  }
  else {
    /* make the frame's shape match the clients */
    xcb_shape_combine(conn, kind, kind, XCB_SHAPE_SO_SET, self->window, self->size.left, self->size.top,
                      self->client->window);

    num = 0;
    if (self->decorations & OB_FRAME_DECOR_TITLEBAR) {
      xrect[0].x = 0;
      xrect[0].y = 0;
      xrect[0].width = self->area.width;
      xrect[0].height = self->size.top;
      ++num;
    }

    if (self->decorations & OB_FRAME_DECOR_HANDLE && geom->handle_height > 0) {
      xrect[1].x = 0;
      xrect[1].y = FRAME_HANDLE_Y(self);
      xrect[1].width = self->area.width;
      xrect[1].height = geom->handle_height + self->bwidth * 2;
      ++num;
    }

    xcb_shape_rectangles(conn, XCB_SHAPE_SO_UNION, kind, XCB_CLIP_ORDERING_UNSORTED, self->window, 0, 0, num, xrect);
  }
}
#endif

static void frame_apply_shape(ObFrame* self) {
#ifdef SHAPE
  frame_adjust_shape_kind(self, XCB_SHAPE_SK_BOUNDING);
#ifdef ShapeInput
  frame_adjust_shape_kind(self, XCB_SHAPE_SK_INPUT);
#endif
#endif
}

void frame_adjust_shape(ObFrame* self) {
  frame_apply_shape(self);
}

void frame_update_shape(ObFrame* self) {
  frame_apply_shape(self);
  frame_publish_updates(self, TRUE, TRUE);
}

void frame_adjust_area(ObFrame* self, gboolean moved, gboolean resized, gboolean fake) {
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  uint32_t mask, val[5];

  if (resized) {
    /* do this before changing the frame's status like max_horz max_vert */
    frame_adjust_cursors(self);

    self->functions = self->client->functions;
    self->decorations = self->client->decorations;
    self->max_horz = self->client->max_horz;
    self->max_vert = self->client->max_vert;
    self->shaded = self->client->shaded;

    if (self->decorations & OB_FRAME_DECOR_BORDER)
      self->bwidth = self->client->undecorated ? geom->ubwidth : geom->fbwidth;
    else
      self->bwidth = 0;

    if (self->decorations & OB_FRAME_DECOR_BORDER && !self->client->undecorated) {
      self->cbwidth_l = self->cbwidth_r = geom->cbwidthx;
      self->cbwidth_t = self->cbwidth_b = geom->cbwidthy;
    }
    else
      self->cbwidth_l = self->cbwidth_t = self->cbwidth_r = self->cbwidth_b = 0;

    if (self->max_horz) {
      self->cbwidth_l = self->cbwidth_r = 0;
      self->width = self->client->area.width;
      if (self->max_vert)
        self->cbwidth_b = 0;
    }
    else
      self->width = self->client->area.width + self->cbwidth_l + self->cbwidth_r;

    /* some elements are sized based of the width, so don't let them have
       negative values */
    self->width = MAX(self->width, (geom->grip_width + self->bwidth) * 2 + 1);

    STRUT_SET(self->size, self->cbwidth_l + (!self->max_horz ? self->bwidth : 0),
              self->cbwidth_t + (!self->max_horz || !self->max_vert ? self->bwidth : 0),
              self->cbwidth_r + (!self->max_horz ? self->bwidth : 0),
              self->cbwidth_b + (!self->max_horz || !self->max_vert ? self->bwidth : 0));

    if (self->decorations & OB_FRAME_DECOR_TITLEBAR)
      self->size.top += geom->title_height + self->bwidth;
    else if (self->max_horz && self->max_vert) {
      /* A maximized and undecorated window needs a border on the
         top of the window to let the user still undecorate/unmaximize the
         window via the client menu. */
      self->size.top += self->bwidth;
    }

    if (self->decorations & OB_FRAME_DECOR_HANDLE && geom->handle_height > 0) {
      self->size.bottom += geom->handle_height + self->bwidth;
    }

    /* position/size and map/unmap all the windows */

    if (!fake) {
      gint innercornerheight = geom->grip_width - self->size.bottom;
      mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

      if (self->cbwidth_l) {
        val[0] = self->size.left - self->cbwidth_l;
        val[1] = self->size.top;
        val[2] = self->cbwidth_l;
        val[3] = self->client->area.height;
        xcb_configure_window(conn, self->innerleft, mask, val);
        xcb_map_window(conn, self->innerleft);
      }
      else
        xcb_unmap_window(conn, self->innerleft);

      if (self->cbwidth_l && innercornerheight > 0) {
        val[0] = 0;
        val[1] = self->client->area.height - (geom->grip_width - self->size.bottom);
        val[2] = self->cbwidth_l;
        val[3] = geom->grip_width - self->size.bottom;
        xcb_configure_window(conn, self->innerbll, mask, val);
        xcb_map_window(conn, self->innerbll);
      }
      else
        xcb_unmap_window(conn, self->innerbll);

      if (self->cbwidth_r) {
        val[0] = self->size.left + self->client->area.width;
        val[1] = self->size.top;
        val[2] = self->cbwidth_r;
        val[3] = self->client->area.height;
        xcb_configure_window(conn, self->innerright, mask, val);
        xcb_map_window(conn, self->innerright);
      }
      else
        xcb_unmap_window(conn, self->innerright);

      if (self->cbwidth_r && innercornerheight > 0) {
        val[0] = 0;
        val[1] = self->client->area.height - (geom->grip_width - self->size.bottom);
        val[2] = self->cbwidth_r;
        val[3] = geom->grip_width - self->size.bottom;
        xcb_configure_window(conn, self->innerbrr, mask, val);
        xcb_map_window(conn, self->innerbrr);
      }
      else
        xcb_unmap_window(conn, self->innerbrr);

      if (self->cbwidth_t) {
        val[0] = self->size.left - self->cbwidth_l;
        val[1] = self->size.top - self->cbwidth_t;
        val[2] = self->client->area.width + self->cbwidth_l + self->cbwidth_r;
        val[3] = self->cbwidth_t;
        xcb_configure_window(conn, self->innertop, mask, val);
        xcb_map_window(conn, self->innertop);
      }
      else
        xcb_unmap_window(conn, self->innertop);

      if (self->cbwidth_b) {
        val[0] = self->size.left - self->cbwidth_l;
        val[1] = self->size.top + self->client->area.height;
        val[2] = self->client->area.width + self->cbwidth_l + self->cbwidth_r;
        val[3] = self->cbwidth_b;
        xcb_configure_window(conn, self->innerbottom, mask, val);

        val[0] = 0;
        val[1] = 0;
        val[2] = geom->grip_width + self->bwidth;
        val[3] = self->cbwidth_b;
        xcb_configure_window(conn, self->innerblb, mask, val);

        val[0] = self->client->area.width + self->cbwidth_l + self->cbwidth_r - (geom->grip_width + self->bwidth);
        val[1] = 0;
        /* val[2] and val[3] are same as above */
        xcb_configure_window(conn, self->innerbrb, mask, val);

        xcb_map_window(conn, self->innerbottom);
        xcb_map_window(conn, self->innerblb);
        xcb_map_window(conn, self->innerbrb);
      }
      else {
        xcb_unmap_window(conn, self->innerbottom);
        xcb_unmap_window(conn, self->innerblb);
        xcb_unmap_window(conn, self->innerbrb);
      }

      if (self->bwidth) {
        gint titlesides;

        /* height of titleleft and titleright */
        titlesides = (!self->max_horz ? geom->grip_width : 0);

        val[0] = geom->grip_width + self->bwidth;
        val[1] = 0;
        val[2] = self->width - geom->grip_width * 2;
        val[3] = self->bwidth;
        xcb_configure_window(conn, self->titletop, mask, val);

        val[0] = 0;
        val[1] = 0;
        val[2] = geom->grip_width + self->bwidth;
        val[3] = self->bwidth;
        xcb_configure_window(conn, self->titletopleft, mask, val);

        val[0] = self->client->area.width + self->size.left + self->size.right - geom->grip_width - self->bwidth;
        val[1] = 0;
        /* val[2], val[3] same */
        xcb_configure_window(conn, self->titletopright, mask, val);

        if (titlesides > 0) {
          val[0] = 0;
          val[1] = self->bwidth;
          val[2] = self->bwidth;
          val[3] = titlesides;
          xcb_configure_window(conn, self->titleleft, mask, val);

          val[0] = self->client->area.width + self->size.left + self->size.right - self->bwidth;
          /* val[1], val[2], val[3] same */
          xcb_configure_window(conn, self->titleright, mask, val);

          xcb_map_window(conn, self->titleleft);
          xcb_map_window(conn, self->titleright);
        }
        else {
          xcb_unmap_window(conn, self->titleleft);
          xcb_unmap_window(conn, self->titleright);
        }

        xcb_map_window(conn, self->titletop);
        xcb_map_window(conn, self->titletopleft);
        xcb_map_window(conn, self->titletopright);

        if (self->decorations & OB_FRAME_DECOR_TITLEBAR) {
          val[0] = (self->max_horz ? 0 : self->bwidth);
          val[1] = geom->title_height + self->bwidth;
          val[2] = self->width;
          val[3] = self->bwidth;
          xcb_configure_window(conn, self->titlebottom, mask, val);

          xcb_map_window(conn, self->titlebottom);
        }
        else
          xcb_unmap_window(conn, self->titlebottom);
      }
      else {
        xcb_unmap_window(conn, self->titlebottom);

        xcb_unmap_window(conn, self->titletop);
        xcb_unmap_window(conn, self->titletopleft);
        xcb_unmap_window(conn, self->titletopright);
        xcb_unmap_window(conn, self->titleleft);
        xcb_unmap_window(conn, self->titleright);
      }

      if (self->decorations & OB_FRAME_DECOR_TITLEBAR) {
        val[0] = (self->max_horz ? 0 : self->bwidth);
        val[1] = self->bwidth;
        val[2] = self->width;
        val[3] = geom->title_height;
        xcb_configure_window(conn, self->title, mask, val);

        xcb_map_window(conn, self->title);

        if (self->decorations & OB_FRAME_DECOR_GRIPS) {
          val[0] = geom->grip_width;
          val[1] = 0;
          val[2] = self->width - geom->grip_width * 2;
          val[3] = geom->paddingy + 1;
          xcb_configure_window(conn, self->topresize, mask, val);

          uint32_t move_mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
          uint32_t move_val[2] = {0, 0};
          xcb_configure_window(conn, self->tltresize, move_mask, move_val);
          xcb_configure_window(conn, self->tllresize, move_mask, move_val);

          move_val[0] = self->width - geom->grip_width;
          xcb_configure_window(conn, self->trtresize, move_mask, move_val);

          move_val[0] = self->width - geom->paddingx - 1;
          xcb_configure_window(conn, self->trrresize, move_mask, move_val);

          xcb_map_window(conn, self->topresize);
          xcb_map_window(conn, self->tltresize);
          xcb_map_window(conn, self->tllresize);
          xcb_map_window(conn, self->trtresize);
          xcb_map_window(conn, self->trrresize);
        }
        else {
          xcb_unmap_window(conn, self->topresize);
          xcb_unmap_window(conn, self->tltresize);
          xcb_unmap_window(conn, self->tllresize);
          xcb_unmap_window(conn, self->trtresize);
          xcb_unmap_window(conn, self->trrresize);
        }
      }
      else
        xcb_unmap_window(conn, self->title);
    }

    if ((self->decorations & OB_FRAME_DECOR_TITLEBAR))
      /* layout the title bar elements */
      layout_title(self);

    if (!fake) {
      gint sidebwidth = self->max_horz ? 0 : self->bwidth;

      if (self->bwidth && self->size.bottom) {
        val[0] = geom->grip_width + self->bwidth + sidebwidth;
        val[1] = self->size.top + self->client->area.height + self->size.bottom - self->bwidth;
        val[2] = self->width - (geom->grip_width + sidebwidth) * 2;
        val[3] = self->bwidth;
        xcb_configure_window(conn, self->handlebottom, mask, val);

        if (sidebwidth) {
          val[0] = 0;
          val[1] = self->size.top + self->client->area.height + self->size.bottom -
                   (!self->max_horz ? geom->grip_width : self->size.bottom - self->cbwidth_b);
          val[2] = self->bwidth;
          val[3] = (!self->max_horz ? geom->grip_width : self->size.bottom - self->cbwidth_b);
          xcb_configure_window(conn, self->lgripleft, mask, val);

          val[0] = self->size.left + self->client->area.width + self->size.right - self->bwidth;
          /* val[1], val[2], val[3] same */
          xcb_configure_window(conn, self->rgripright, mask, val);

          xcb_map_window(conn, self->lgripleft);
          xcb_map_window(conn, self->rgripright);
        }
        else {
          xcb_unmap_window(conn, self->lgripleft);
          xcb_unmap_window(conn, self->rgripright);
        }

        val[0] = sidebwidth;
        val[1] = self->size.top + self->client->area.height + self->size.bottom - self->bwidth;
        val[2] = geom->grip_width + self->bwidth;
        val[3] = self->bwidth;
        xcb_configure_window(conn, self->lgripbottom, mask, val);

        val[0] = self->size.left + self->client->area.width + self->size.right - self->bwidth - sidebwidth -
                 geom->grip_width;
        /* val[1], val[2], val[3] same */
        xcb_configure_window(conn, self->rgripbottom, mask, val);

        xcb_map_window(conn, self->handlebottom);
        xcb_map_window(conn, self->lgripbottom);
        xcb_map_window(conn, self->rgripbottom);

        if (self->decorations & OB_FRAME_DECOR_HANDLE && geom->handle_height > 0) {
          val[0] = geom->grip_width + self->bwidth + sidebwidth;
          val[1] = FRAME_HANDLE_Y(self);
          val[2] = self->width - (geom->grip_width + sidebwidth) * 2;
          val[3] = self->bwidth;
          xcb_configure_window(conn, self->handletop, mask, val);
          xcb_map_window(conn, self->handletop);

          if (self->decorations & OB_FRAME_DECOR_GRIPS) {
            val[0] = geom->grip_width;
            val[1] = 0;
            val[2] = self->bwidth;
            val[3] = geom->handle_height;
            xcb_configure_window(conn, self->handleleft, mask, val);

            val[0] = self->width - geom->grip_width - self->bwidth;
            /* val[1], val[2], val[3] same */
            xcb_configure_window(conn, self->handleright, mask, val);

            val[0] = sidebwidth;
            val[1] = FRAME_HANDLE_Y(self);
            val[2] = geom->grip_width + self->bwidth;
            val[3] = self->bwidth;
            xcb_configure_window(conn, self->lgriptop, mask, val);

            val[0] = self->size.left + self->client->area.width + self->size.right - self->bwidth - sidebwidth -
                     geom->grip_width;
            /* val[1], val[2], val[3] same */
            xcb_configure_window(conn, self->rgriptop, mask, val);

            xcb_map_window(conn, self->handleleft);
            xcb_map_window(conn, self->handleright);
            xcb_map_window(conn, self->lgriptop);
            xcb_map_window(conn, self->rgriptop);
          }
          else {
            xcb_unmap_window(conn, self->handleleft);
            xcb_unmap_window(conn, self->handleright);
            xcb_unmap_window(conn, self->lgriptop);
            xcb_unmap_window(conn, self->rgriptop);
          }
        }
        else {
          xcb_unmap_window(conn, self->handleleft);
          xcb_unmap_window(conn, self->handleright);
          xcb_unmap_window(conn, self->lgriptop);
          xcb_unmap_window(conn, self->rgriptop);

          xcb_unmap_window(conn, self->handletop);
        }
      }
      else {
        xcb_unmap_window(conn, self->handleleft);
        xcb_unmap_window(conn, self->handleright);
        xcb_unmap_window(conn, self->lgriptop);
        xcb_unmap_window(conn, self->rgriptop);

        xcb_unmap_window(conn, self->handletop);

        xcb_unmap_window(conn, self->handlebottom);
        xcb_unmap_window(conn, self->lgripleft);
        xcb_unmap_window(conn, self->rgripright);
        xcb_unmap_window(conn, self->lgripbottom);
        xcb_unmap_window(conn, self->rgripbottom);
      }

      if (self->decorations & OB_FRAME_DECOR_HANDLE && geom->handle_height > 0) {
        val[0] = sidebwidth;
        val[1] = FRAME_HANDLE_Y(self) + self->bwidth;
        val[2] = self->width;
        val[3] = geom->handle_height;
        xcb_configure_window(conn, self->handle, mask, val);
        xcb_map_window(conn, self->handle);

        if (self->decorations & OB_FRAME_DECOR_GRIPS) {
          val[0] = 0;
          val[1] = 0;
          val[2] = geom->grip_width;
          val[3] = geom->handle_height;
          xcb_configure_window(conn, self->lgrip, mask, val);

          val[0] = self->width - geom->grip_width;
          /* val[1], val[2], val[3] same */
          xcb_configure_window(conn, self->rgrip, mask, val);

          xcb_map_window(conn, self->lgrip);
          xcb_map_window(conn, self->rgrip);
        }
        else {
          xcb_unmap_window(conn, self->lgrip);
          xcb_unmap_window(conn, self->rgrip);
        }
      }
      else {
        xcb_unmap_window(conn, self->lgrip);
        xcb_unmap_window(conn, self->rgrip);

        xcb_unmap_window(conn, self->handle);
      }

      if (self->bwidth && !self->max_horz &&
          (self->client->area.height + self->size.top + self->size.bottom) > geom->grip_width * 2) {
        val[0] = 0;
        val[1] = self->bwidth + geom->grip_width;
        val[2] = self->bwidth;
        val[3] = self->client->area.height + self->size.top + self->size.bottom - geom->grip_width * 2;
        xcb_configure_window(conn, self->left, mask, val);

        xcb_map_window(conn, self->left);
      }
      else
        xcb_unmap_window(conn, self->left);

      if (self->bwidth && !self->max_horz &&
          (self->client->area.height + self->size.top + self->size.bottom) > geom->grip_width * 2) {
        val[0] = self->client->area.width + self->cbwidth_l + self->cbwidth_r + self->bwidth;
        val[1] = self->bwidth + geom->grip_width;
        val[2] = self->bwidth;
        val[3] = self->client->area.height + self->size.top + self->size.bottom - geom->grip_width * 2;
        xcb_configure_window(conn, self->right, mask, val);

        xcb_map_window(conn, self->right);
      }
      else
        xcb_unmap_window(conn, self->right);

      val[0] = self->size.left;
      val[1] = self->size.top;
      val[2] = self->client->area.width;
      val[3] = self->client->area.height;
      xcb_configure_window(conn, self->backback, mask, val);
    }
  }

  /* shading can change without being moved or resized */
  RECT_SET_SIZE(self->area, self->client->area.width + self->size.left + self->size.right,
                (self->client->shaded ? geom->title_height + self->bwidth * 2
                                      : self->client->area.height + self->size.top + self->size.bottom));

  if ((moved || resized) && !fake) {
    /* find the new coordinates, done after setting the frame.size, for
       frame_client_gravity. */
    self->area.x = self->client->area.x;
    self->area.y = self->client->area.y;
    frame_client_gravity(self, &self->area.x, &self->area.y);
  }

  if (!fake) {
    if (!frame_iconify_animating(self)) {
      /* move and resize the top level frame.
         shading can change without being moved or resized.

         but don't do this during an iconify animation. it will be
         reflected afterwards.
      */
      val[0] = self->area.x;
      val[1] = self->area.y;
      val[2] = self->area.width;
      val[3] = self->area.height;
      xcb_configure_window(conn, self->window, mask, val);
    }

    /* when the client has StaticGravity, it likes to move around.
       also this correctly positions the client when it maps.
       this also needs to be run when the frame's decorations sizes change!
    */
    uint32_t move_mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    uint32_t move_val[2] = {self->size.left, self->size.top};
    xcb_configure_window(conn, self->client->window, move_mask, move_val);

    if (resized) {
      self->need_render = TRUE;
      framerender_frame(self);
      self->suppress_damage_once = !self->need_render;
      frame_update_shape(self);
    }
    else {
      frame_publish_updates(self, FALSE, FALSE);
    }

    /* if this occurs while we are focus cycling, the indicator needs to
       match the changes */
    if (focus_cycle_target == self->client)
      focus_cycle_update_indicator(self->client);
  }
  if (resized && (self->decorations & OB_FRAME_DECOR_TITLEBAR) && self->label_width) {
    val[0] = self->label_width;
    val[1] = geom->label_height;
    xcb_configure_window(conn, self->label, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, val);
  }
}

static void frame_adjust_cursors(ObFrame* self) {
  if ((self->functions & OB_CLIENT_FUNC_RESIZE) != (self->client->functions & OB_CLIENT_FUNC_RESIZE) ||
      self->max_horz != self->client->max_horz || self->max_vert != self->client->max_vert ||
      self->shaded != self->client->shaded) {
    gboolean r =
        (self->client->functions & OB_CLIENT_FUNC_RESIZE) && !(self->client->max_horz && self->client->max_vert);
    gboolean topbot = !self->client->max_vert;
    gboolean sh = self->client->shaded;
    uint32_t mask = XCB_CW_CURSOR;
    uint32_t val;
    xcb_connection_t* conn = XGetXCBConnection(obt_display);

    /* these ones turn off when max vert, and some when shaded */
    val = ob_cursor(r && topbot && !sh ? OB_CURSOR_NORTH : OB_CURSOR_NONE);
    xcb_change_window_attributes(conn, self->topresize, mask, &val);
    xcb_change_window_attributes(conn, self->titletop, mask, &val);
    val = ob_cursor(r && topbot ? OB_CURSOR_SOUTH : OB_CURSOR_NONE);
    xcb_change_window_attributes(conn, self->handle, mask, &val);
    xcb_change_window_attributes(conn, self->handletop, mask, &val);
    xcb_change_window_attributes(conn, self->handlebottom, mask, &val);
    xcb_change_window_attributes(conn, self->innerbottom, mask, &val);

    /* these ones change when shaded */
    val = ob_cursor(r ? (sh ? OB_CURSOR_WEST : OB_CURSOR_NORTHWEST) : OB_CURSOR_NONE);
    xcb_change_window_attributes(conn, self->titleleft, mask, &val);
    xcb_change_window_attributes(conn, self->tltresize, mask, &val);
    xcb_change_window_attributes(conn, self->tllresize, mask, &val);
    xcb_change_window_attributes(conn, self->titletopleft, mask, &val);
    val = ob_cursor(r ? (sh ? OB_CURSOR_EAST : OB_CURSOR_NORTHEAST) : OB_CURSOR_NONE);
    xcb_change_window_attributes(conn, self->titleright, mask, &val);
    xcb_change_window_attributes(conn, self->trtresize, mask, &val);
    xcb_change_window_attributes(conn, self->trrresize, mask, &val);
    xcb_change_window_attributes(conn, self->titletopright, mask, &val);

    /* these ones are pretty static */
    val = ob_cursor(r ? OB_CURSOR_WEST : OB_CURSOR_NONE);
    xcb_change_window_attributes(conn, self->left, mask, &val);
    xcb_change_window_attributes(conn, self->innerleft, mask, &val);
    val = ob_cursor(r ? OB_CURSOR_EAST : OB_CURSOR_NONE);
    xcb_change_window_attributes(conn, self->right, mask, &val);
    xcb_change_window_attributes(conn, self->innerright, mask, &val);
    val = ob_cursor(r ? OB_CURSOR_SOUTHWEST : OB_CURSOR_NONE);
    xcb_change_window_attributes(conn, self->lgrip, mask, &val);
    xcb_change_window_attributes(conn, self->handleleft, mask, &val);
    xcb_change_window_attributes(conn, self->lgripleft, mask, &val);
    xcb_change_window_attributes(conn, self->lgriptop, mask, &val);
    xcb_change_window_attributes(conn, self->lgripbottom, mask, &val);
    xcb_change_window_attributes(conn, self->innerbll, mask, &val);
    xcb_change_window_attributes(conn, self->innerblb, mask, &val);
    val = ob_cursor(r ? OB_CURSOR_SOUTHEAST : OB_CURSOR_NONE);
    xcb_change_window_attributes(conn, self->rgrip, mask, &val);
    xcb_change_window_attributes(conn, self->handleright, mask, &val);
    xcb_change_window_attributes(conn, self->rgripright, mask, &val);
    xcb_change_window_attributes(conn, self->rgriptop, mask, &val);
    xcb_change_window_attributes(conn, self->rgripbottom, mask, &val);
    xcb_change_window_attributes(conn, self->innerbrr, mask, &val);
    xcb_change_window_attributes(conn, self->innerbrb, mask, &val);
  }
}

void frame_adjust_client_area(ObFrame* self) {
  /* adjust the window which is there to prevent flashing on unmap */
  uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
  uint32_t val[4] = {0, 0, self->client->area.width, self->client->area.height};
  xcb_configure_window(XGetXCBConnection(obt_display), self->backfront, mask, val);
}

void frame_adjust_state(ObFrame* self) {
  self->need_render = TRUE;
  framerender_frame(self);
}

void frame_adjust_focus(ObFrame* self, gboolean hilite) {
  ob_debug_type(OB_DEBUG_FOCUS, "Frame for 0x%x has focus: %d", self->client->window, hilite);
  self->focused = hilite;
  self->need_render = TRUE;
  framerender_frame(self);
  xcb_flush(XGetXCBConnection(obt_display));
}

void frame_adjust_title(ObFrame* self) {
  self->need_render = TRUE;
  framerender_frame(self);
}

void frame_adjust_icon(ObFrame* self) {
  self->need_render = TRUE;
  framerender_frame(self);
}

void frame_grab_client(ObFrame* self) {
  /* DO NOT map the client window here. we used to do that, but it is bogus.
     we need to set up the client's dimensions and everything before we
     send a mapnotify or we create race conditions.
  */
  xcb_connection_t* conn = XGetXCBConnection(obt_display);

  /* reparent the client to the frame */
  xcb_reparent_window(conn, self->client->window, self->window, 0, 0);

  /*
    When reparenting the client window, it is usually not mapped yet, since
    this occurs from a MapRequest. However, in the case where Openbox is
    starting up, the window is already mapped, so we'll see an unmap event
    for it.
  */
  if (ob_state() == OB_STATE_STARTING)
    ++self->client->ignore_unmaps;

  /* select the event mask on the client's parent (to receive config/map
     req's) the ButtonPress is to catch clicks on the client border */
  uint32_t val = FRAME_EVENTMASK;
  xcb_change_window_attributes(conn, self->window, XCB_CW_EVENT_MASK, &val);

  /* set all the windows for the frame in the window_map */
  window_add(&self->window, CLIENT_AS_WINDOW(self->client));
  window_add(&self->backback, CLIENT_AS_WINDOW(self->client));
  window_add(&self->backfront, CLIENT_AS_WINDOW(self->client));
  window_add(&self->innerleft, CLIENT_AS_WINDOW(self->client));
  window_add(&self->innertop, CLIENT_AS_WINDOW(self->client));
  window_add(&self->innerright, CLIENT_AS_WINDOW(self->client));
  window_add(&self->innerbottom, CLIENT_AS_WINDOW(self->client));
  window_add(&self->innerblb, CLIENT_AS_WINDOW(self->client));
  window_add(&self->innerbll, CLIENT_AS_WINDOW(self->client));
  window_add(&self->innerbrb, CLIENT_AS_WINDOW(self->client));
  window_add(&self->innerbrr, CLIENT_AS_WINDOW(self->client));
  window_add(&self->title, CLIENT_AS_WINDOW(self->client));
  window_add(&self->label, CLIENT_AS_WINDOW(self->client));
  window_add(&self->max, CLIENT_AS_WINDOW(self->client));
  window_add(&self->close, CLIENT_AS_WINDOW(self->client));
  window_add(&self->desk, CLIENT_AS_WINDOW(self->client));
  window_add(&self->shade, CLIENT_AS_WINDOW(self->client));
  window_add(&self->icon, CLIENT_AS_WINDOW(self->client));
  window_add(&self->iconify, CLIENT_AS_WINDOW(self->client));
  window_add(&self->handle, CLIENT_AS_WINDOW(self->client));
  window_add(&self->lgrip, CLIENT_AS_WINDOW(self->client));
  window_add(&self->rgrip, CLIENT_AS_WINDOW(self->client));
  window_add(&self->topresize, CLIENT_AS_WINDOW(self->client));
  window_add(&self->tltresize, CLIENT_AS_WINDOW(self->client));
  window_add(&self->tllresize, CLIENT_AS_WINDOW(self->client));
  window_add(&self->trtresize, CLIENT_AS_WINDOW(self->client));
  window_add(&self->trrresize, CLIENT_AS_WINDOW(self->client));
  window_add(&self->left, CLIENT_AS_WINDOW(self->client));
  window_add(&self->right, CLIENT_AS_WINDOW(self->client));
  window_add(&self->titleleft, CLIENT_AS_WINDOW(self->client));
  window_add(&self->titletop, CLIENT_AS_WINDOW(self->client));
  window_add(&self->titletopleft, CLIENT_AS_WINDOW(self->client));
  window_add(&self->titletopright, CLIENT_AS_WINDOW(self->client));
  window_add(&self->titleright, CLIENT_AS_WINDOW(self->client));
  window_add(&self->titlebottom, CLIENT_AS_WINDOW(self->client));
  window_add(&self->handleleft, CLIENT_AS_WINDOW(self->client));
  window_add(&self->handletop, CLIENT_AS_WINDOW(self->client));
  window_add(&self->handleright, CLIENT_AS_WINDOW(self->client));
  window_add(&self->handlebottom, CLIENT_AS_WINDOW(self->client));
  window_add(&self->lgripleft, CLIENT_AS_WINDOW(self->client));
  window_add(&self->lgriptop, CLIENT_AS_WINDOW(self->client));
  window_add(&self->lgripbottom, CLIENT_AS_WINDOW(self->client));
  window_add(&self->rgripright, CLIENT_AS_WINDOW(self->client));
  window_add(&self->rgriptop, CLIENT_AS_WINDOW(self->client));
  window_add(&self->rgripbottom, CLIENT_AS_WINDOW(self->client));
}

static gboolean find_reparent(XEvent* e, gpointer data) {
  const ObFrame* self = data;

  /* Find ReparentNotify events for the window that aren't being reparented into the
     frame, thus the client reparenting itself off the frame. */
  return e->type == ReparentNotify && e->xreparent.window == self->client->window &&
         e->xreparent.parent != self->window;
}

void frame_release_client(ObFrame* self) {
  /* if there was any animation going on, kill it */
  if (self->iconify_animation_timer)
    g_source_remove(self->iconify_animation_timer);

  /* check if the app has already reparented its window away */
  if (!xqueue_exists_local(find_reparent, self)) {
    /* according to the ICCCM - if the client doesn't reparent itself,
       then we will reparent the window to root for them */
    XReparentWindow(obt_display, self->client->window, obt_root(ob_screen), self->client->area.x, self->client->area.y);
  }

  /* remove all the windows for the frame from the window_map */
  window_remove(self->window);
  window_remove(self->backback);
  window_remove(self->backfront);
  window_remove(self->innerleft);
  window_remove(self->innertop);
  window_remove(self->innerright);
  window_remove(self->innerbottom);
  window_remove(self->innerblb);
  window_remove(self->innerbll);
  window_remove(self->innerbrb);
  window_remove(self->innerbrr);
  window_remove(self->title);
  window_remove(self->label);
  window_remove(self->max);
  window_remove(self->close);
  window_remove(self->desk);
  window_remove(self->shade);
  window_remove(self->icon);
  window_remove(self->iconify);
  window_remove(self->handle);
  window_remove(self->lgrip);
  window_remove(self->rgrip);
  window_remove(self->topresize);
  window_remove(self->tltresize);
  window_remove(self->tllresize);
  window_remove(self->trtresize);
  window_remove(self->trrresize);
  window_remove(self->left);
  window_remove(self->right);
  window_remove(self->titleleft);
  window_remove(self->titletop);
  window_remove(self->titletopleft);
  window_remove(self->titletopright);
  window_remove(self->titleright);
  window_remove(self->titlebottom);
  window_remove(self->handleleft);
  window_remove(self->handletop);
  window_remove(self->handleright);
  window_remove(self->handlebottom);
  window_remove(self->lgripleft);
  window_remove(self->lgriptop);
  window_remove(self->lgripbottom);
  window_remove(self->rgripright);
  window_remove(self->rgriptop);
  window_remove(self->rgripbottom);

  if (self->flash_timer)
    g_source_remove(self->flash_timer);
}

/* is there anything present between us and the label? */
static gboolean is_button_present(ObFrame* self, const gchar* lc, gint dir) {
  for (; *lc != '\0' && lc >= config_title_layout; lc += dir) {
    if (*lc == ' ')
      continue; /* it was invalid */
    if (*lc == 'N' && self->decorations & OB_FRAME_DECOR_ICON)
      return TRUE;
    if (*lc == 'D' && self->decorations & OB_FRAME_DECOR_ALLDESKTOPS)
      return TRUE;
    if (*lc == 'S' && self->decorations & OB_FRAME_DECOR_SHADE)
      return TRUE;
    if (*lc == 'I' && self->decorations & OB_FRAME_DECOR_ICONIFY)
      return TRUE;
    if (*lc == 'M' && self->decorations & OB_FRAME_DECOR_MAXIMIZE)
      return TRUE;
    if (*lc == 'C' && self->decorations & OB_FRAME_DECOR_CLOSE)
      return TRUE;
    if (*lc == 'L')
      return FALSE;
  }
  return FALSE;
}

static void
place_button(ObFrame* self, const char* lc, gint bwidth, gint left, gint i, gint* x, gint* button_on, gint* button_x) {
  if (!(*button_on = is_button_present(self, lc, i)))
    return;

  self->label_width -= bwidth;
  if (i > 0)
    *button_x = *x;
  *x += i * bwidth;
  if (i < 0) {
    if (self->label_x <= left || *x > self->label_x) {
      *button_x = *x;
    }
    else {
      /* the button would have been drawn on top of another button */
      *button_on = FALSE;
      self->label_width += bwidth;
    }
  }
}

static void layout_title(ObFrame* self) {
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  gchar* lc;
  gint i;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
  uint32_t val[2];

  const gint bwidth = geom->button_size + geom->paddingx + 1;
  /* position of the leftmost button */
  const gint left = geom->paddingx + 1;
  /* position of the rightmost button */
  const gint right = self->width;

  /* turn them all off */
  self->icon_on = self->desk_on = self->shade_on = self->iconify_on = self->max_on = self->close_on = self->label_on =
      FALSE;
  self->label_width = self->width - (geom->paddingx + 1) * 2;
  self->leftmost = self->rightmost = OB_FRAME_CONTEXT_NONE;

  /* figure out what's being shown, find each element's position, and the
     width of the label

     do the ones before the label, then after the label,
     i will be +1 the first time through when working to the left,
     and -1 the second time through when working to the right */
  for (i = 1; i >= -1; i -= 2) {
    gint x;
    ObFrameContext* firstcon;

    if (i > 0) {
      x = left;
      lc = config_title_layout;
      firstcon = &self->leftmost;
    }
    else {
      x = right;
      lc = config_title_layout + strlen(config_title_layout) - 1;
      firstcon = &self->rightmost;
    }

    /* stop at the end of the string (or the label, which calls break) */
    for (; *lc != '\0' && lc >= config_title_layout; lc += i) {
      if (*lc == 'L') {
        if (i > 0) {
          self->label_on = TRUE;
          self->label_x = x;
        }
        break; /* break the for loop, do other side of label */
      }
      else if (*lc == 'N') {
        if (firstcon)
          *firstcon = OB_FRAME_CONTEXT_ICON;
        /* icon is bigger than buttons */
        place_button(self, lc, bwidth + 2, left, i, &x, &self->icon_on, &self->icon_x);
      }
      else if (*lc == 'D') {
        if (firstcon)
          *firstcon = OB_FRAME_CONTEXT_ALLDESKTOPS;
        place_button(self, lc, bwidth, left, i, &x, &self->desk_on, &self->desk_x);
      }
      else if (*lc == 'S') {
        if (firstcon)
          *firstcon = OB_FRAME_CONTEXT_SHADE;
        place_button(self, lc, bwidth, left, i, &x, &self->shade_on, &self->shade_x);
      }
      else if (*lc == 'I') {
        if (firstcon)
          *firstcon = OB_FRAME_CONTEXT_ICONIFY;
        place_button(self, lc, bwidth, left, i, &x, &self->iconify_on, &self->iconify_x);
      }
      else if (*lc == 'M') {
        if (firstcon)
          *firstcon = OB_FRAME_CONTEXT_MAXIMIZE;
        place_button(self, lc, bwidth, left, i, &x, &self->max_on, &self->max_x);
      }
      else if (*lc == 'C') {
        if (firstcon)
          *firstcon = OB_FRAME_CONTEXT_CLOSE;
        place_button(self, lc, bwidth, left, i, &x, &self->close_on, &self->close_x);
      }
      else
        continue; /* don't set firstcon */
      firstcon = NULL;
    }
  }

  /* position and map the elements */
  if (self->icon_on) {
    xcb_map_window(conn, self->icon);
    val[0] = self->icon_x;
    val[1] = geom->paddingy;
    xcb_configure_window(conn, self->icon, mask, val);
  }
  else
    xcb_unmap_window(conn, self->icon);

  if (self->desk_on) {
    xcb_map_window(conn, self->desk);
    val[0] = self->desk_x;
    val[1] = geom->paddingy + 1;
    xcb_configure_window(conn, self->desk, mask, val);
  }
  else
    xcb_unmap_window(conn, self->desk);

  if (self->shade_on) {
    xcb_map_window(conn, self->shade);
    val[0] = self->shade_x;
    val[1] = geom->paddingy + 1;
    xcb_configure_window(conn, self->shade, mask, val);
  }
  else
    xcb_unmap_window(conn, self->shade);

  if (self->iconify_on) {
    xcb_map_window(conn, self->iconify);
    val[0] = self->iconify_x;
    val[1] = geom->paddingy + 1;
    xcb_configure_window(conn, self->iconify, mask, val);
  }
  else
    xcb_unmap_window(conn, self->iconify);

  if (self->max_on) {
    xcb_map_window(conn, self->max);
    val[0] = self->max_x;
    val[1] = geom->paddingy + 1;
    xcb_configure_window(conn, self->max, mask, val);
  }
  else
    xcb_unmap_window(conn, self->max);

  if (self->close_on) {
    xcb_map_window(conn, self->close);
    val[0] = self->close_x;
    val[1] = geom->paddingy + 1;
    xcb_configure_window(conn, self->close, mask, val);
  }
  else
    xcb_unmap_window(conn, self->close);

  if (self->label_on && self->label_width > 0) {
    xcb_map_window(conn, self->label);
    val[0] = self->label_x;
    val[1] = geom->paddingy;
    xcb_configure_window(conn, self->label, mask, val);
  }
  else
    xcb_unmap_window(conn, self->label);
}

gboolean frame_next_context_from_string(gchar* names, ObFrameContext* cx) {
  gchar *p, *n;

  if (!*names) /* empty string */
    return FALSE;

  /* find the first space */
  for (p = names; *p; p = g_utf8_next_char(p)) {
    const gunichar c = g_utf8_get_char(p);
    if (g_unichar_isspace(c))
      break;
  }

  if (p == names) {
    /* leading spaces in the string */
    n = g_utf8_next_char(names);
    if (!frame_next_context_from_string(n, cx))
      return FALSE;
  }
  else {
    n = p;
    if (*p) {
      /* delete the space with null zero(s) */
      while (n < g_utf8_next_char(p))
        *(n++) = '\0';
    }

    *cx = frame_context_from_string(names);

    /* find the next non-space */
    for (; *n; n = g_utf8_next_char(n)) {
      const gunichar c = g_utf8_get_char(n);
      if (!g_unichar_isspace(c))
        break;
    }
  }

  /* delete everything we just read (copy everything at n to the start of
     the string */
  for (p = names; *n; ++p, ++n)
    *p = *n;
  *p = *n;

  return TRUE;
}

ObFrameContext frame_context_from_string(const gchar* name) {
  if (!g_ascii_strcasecmp("Desktop", name))
    return OB_FRAME_CONTEXT_DESKTOP;
  else if (!g_ascii_strcasecmp("Root", name))
    return OB_FRAME_CONTEXT_ROOT;
  else if (!g_ascii_strcasecmp("Client", name))
    return OB_FRAME_CONTEXT_CLIENT;
  else if (!g_ascii_strcasecmp("Titlebar", name))
    return OB_FRAME_CONTEXT_TITLEBAR;
  else if (!g_ascii_strcasecmp("Frame", name))
    return OB_FRAME_CONTEXT_FRAME;
  else if (!g_ascii_strcasecmp("TLCorner", name))
    return OB_FRAME_CONTEXT_TLCORNER;
  else if (!g_ascii_strcasecmp("TRCorner", name))
    return OB_FRAME_CONTEXT_TRCORNER;
  else if (!g_ascii_strcasecmp("BLCorner", name))
    return OB_FRAME_CONTEXT_BLCORNER;
  else if (!g_ascii_strcasecmp("BRCorner", name))
    return OB_FRAME_CONTEXT_BRCORNER;
  else if (!g_ascii_strcasecmp("Top", name))
    return OB_FRAME_CONTEXT_TOP;
  else if (!g_ascii_strcasecmp("Bottom", name))
    return OB_FRAME_CONTEXT_BOTTOM;
  else if (!g_ascii_strcasecmp("Left", name))
    return OB_FRAME_CONTEXT_LEFT;
  else if (!g_ascii_strcasecmp("Right", name))
    return OB_FRAME_CONTEXT_RIGHT;
  else if (!g_ascii_strcasecmp("Maximize", name))
    return OB_FRAME_CONTEXT_MAXIMIZE;
  else if (!g_ascii_strcasecmp("AllDesktops", name))
    return OB_FRAME_CONTEXT_ALLDESKTOPS;
  else if (!g_ascii_strcasecmp("Shade", name))
    return OB_FRAME_CONTEXT_SHADE;
  else if (!g_ascii_strcasecmp("Iconify", name))
    return OB_FRAME_CONTEXT_ICONIFY;
  else if (!g_ascii_strcasecmp("Icon", name))
    return OB_FRAME_CONTEXT_ICON;
  else if (!g_ascii_strcasecmp("Close", name))
    return OB_FRAME_CONTEXT_CLOSE;
  else if (!g_ascii_strcasecmp("MoveResize", name))
    return OB_FRAME_CONTEXT_MOVE_RESIZE;
  else if (!g_ascii_strcasecmp("Dock", name))
    return OB_FRAME_CONTEXT_DOCK;

  return OB_FRAME_CONTEXT_NONE;
}

ObFrameContext frame_context(ObClient* client, Window win, gint x, gint y) {
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  ObFrame* self;
  ObWindow* obwin;

  if (moveresize_in_progress)
    return OB_FRAME_CONTEXT_MOVE_RESIZE;

  if (win == obt_root(ob_screen))
    return OB_FRAME_CONTEXT_ROOT;
  if ((obwin = window_find(win))) {
    if (WINDOW_IS_DOCK(obwin)) {
      return OB_FRAME_CONTEXT_DOCK;
    }
  }
  if (client == NULL)
    return OB_FRAME_CONTEXT_NONE;
  if (win == client->window) {
    /* conceptually, this is the desktop, as far as users are
       concerned */
    if (client->type == OB_CLIENT_TYPE_DESKTOP)
      return OB_FRAME_CONTEXT_DESKTOP;
    return OB_FRAME_CONTEXT_CLIENT;
  }

  self = client->frame;

  /* when the user clicks in the corners of the titlebar and the client
     is fully maximized, then treat it like they clicked in the
     button that is there */
  if (self->max_horz && self->max_vert &&
      (win == self->title || win == self->titletop || win == self->titleleft || win == self->titletopleft ||
       win == self->titleright || win == self->titletopright)) {
    /* get the mouse coords in reference to the whole frame */
    gint fx = x;
    gint fy = y;

    /* these windows are down a border width from the top of the frame */
    if (win == self->title || win == self->titleleft || win == self->titleright)
      fy += self->bwidth;

    /* title is a border width in from the edge */
    if (win == self->title)
      fx += self->bwidth;
    /* titletop is a bit to the right */
    else if (win == self->titletop)
      fx += geom->grip_width + self->bwidth;
    /* titletopright is way to the right edge */
    else if (win == self->titletopright)
      fx += self->area.width - (geom->grip_width + self->bwidth);
    /* titleright is even more way to the right edge */
    else if (win == self->titleright)
      fx += self->area.width - self->bwidth;

    /* figure out if we're over the area that should be considered a
       button */
    if (fy < self->bwidth + geom->paddingy + 1 + geom->button_size) {
      if (fx < (self->bwidth + geom->paddingx + 1 + geom->button_size)) {
        if (self->leftmost != OB_FRAME_CONTEXT_NONE)
          return self->leftmost;
      }
      else if (fx >= (self->area.width - (self->bwidth + geom->paddingx + 1 + geom->button_size))) {
        if (self->rightmost != OB_FRAME_CONTEXT_NONE)
          return self->rightmost;
      }
    }

    /* there is no resizing maximized windows so make them the titlebar
       context */
    return OB_FRAME_CONTEXT_TITLEBAR;
  }
  else if (self->max_vert && (win == self->titletop || win == self->topresize))
    /* can't resize vertically when max vert */
    return OB_FRAME_CONTEXT_TITLEBAR;
  else if (self->shaded && (win == self->titletop || win == self->topresize))
    /* can't resize vertically when shaded */
    return OB_FRAME_CONTEXT_TITLEBAR;

  if (win == self->window)
    return OB_FRAME_CONTEXT_FRAME;
  if (win == self->label)
    return OB_FRAME_CONTEXT_TITLEBAR;
  if (win == self->handle)
    return OB_FRAME_CONTEXT_BOTTOM;
  if (win == self->handletop)
    return OB_FRAME_CONTEXT_BOTTOM;
  if (win == self->handlebottom)
    return OB_FRAME_CONTEXT_BOTTOM;
  if (win == self->handleleft)
    return OB_FRAME_CONTEXT_BLCORNER;
  if (win == self->lgrip)
    return OB_FRAME_CONTEXT_BLCORNER;
  if (win == self->lgripleft)
    return OB_FRAME_CONTEXT_BLCORNER;
  if (win == self->lgriptop)
    return OB_FRAME_CONTEXT_BLCORNER;
  if (win == self->lgripbottom)
    return OB_FRAME_CONTEXT_BLCORNER;
  if (win == self->handleright)
    return OB_FRAME_CONTEXT_BRCORNER;
  if (win == self->rgrip)
    return OB_FRAME_CONTEXT_BRCORNER;
  if (win == self->rgripright)
    return OB_FRAME_CONTEXT_BRCORNER;
  if (win == self->rgriptop)
    return OB_FRAME_CONTEXT_BRCORNER;
  if (win == self->rgripbottom)
    return OB_FRAME_CONTEXT_BRCORNER;
  if (win == self->title)
    return OB_FRAME_CONTEXT_TITLEBAR;
  if (win == self->titlebottom)
    return OB_FRAME_CONTEXT_TITLEBAR;
  if (win == self->titleleft)
    return OB_FRAME_CONTEXT_TLCORNER;
  if (win == self->titletopleft)
    return OB_FRAME_CONTEXT_TLCORNER;
  if (win == self->titleright)
    return OB_FRAME_CONTEXT_TRCORNER;
  if (win == self->titletopright)
    return OB_FRAME_CONTEXT_TRCORNER;
  if (win == self->titletop)
    return OB_FRAME_CONTEXT_TOP;
  if (win == self->topresize)
    return OB_FRAME_CONTEXT_TOP;
  if (win == self->tltresize)
    return OB_FRAME_CONTEXT_TLCORNER;
  if (win == self->tllresize)
    return OB_FRAME_CONTEXT_TLCORNER;
  if (win == self->trtresize)
    return OB_FRAME_CONTEXT_TRCORNER;
  if (win == self->trrresize)
    return OB_FRAME_CONTEXT_TRCORNER;
  if (win == self->left)
    return OB_FRAME_CONTEXT_LEFT;
  if (win == self->right)
    return OB_FRAME_CONTEXT_RIGHT;
  if (win == self->innertop)
    return OB_FRAME_CONTEXT_TITLEBAR;
  if (win == self->innerleft)
    return OB_FRAME_CONTEXT_LEFT;
  if (win == self->innerbottom)
    return OB_FRAME_CONTEXT_BOTTOM;
  if (win == self->innerright)
    return OB_FRAME_CONTEXT_RIGHT;
  if (win == self->innerbll)
    return OB_FRAME_CONTEXT_BLCORNER;
  if (win == self->innerblb)
    return OB_FRAME_CONTEXT_BLCORNER;
  if (win == self->innerbrr)
    return OB_FRAME_CONTEXT_BRCORNER;
  if (win == self->innerbrb)
    return OB_FRAME_CONTEXT_BRCORNER;
  if (win == self->max)
    return OB_FRAME_CONTEXT_MAXIMIZE;
  if (win == self->iconify)
    return OB_FRAME_CONTEXT_ICONIFY;
  if (win == self->close)
    return OB_FRAME_CONTEXT_CLOSE;
  if (win == self->icon)
    return OB_FRAME_CONTEXT_ICON;
  if (win == self->desk)
    return OB_FRAME_CONTEXT_ALLDESKTOPS;
  if (win == self->shade)
    return OB_FRAME_CONTEXT_SHADE;

  return OB_FRAME_CONTEXT_NONE;
}

void frame_client_gravity(ObFrame* self, gint* x, gint* y) {
  /* horizontal */
  switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case SouthWestGravity:
    case WestGravity:
      break;

    case NorthGravity:
    case SouthGravity:
    case CenterGravity:
      /* the middle of the client will be the middle of the frame */
      *x -= (self->size.right - self->size.left) / 2;
      break;

    case NorthEastGravity:
    case SouthEastGravity:
    case EastGravity:
      /* the right side of the client will be the right side of the frame */
      *x -= self->size.right + self->size.left - self->client->border_width * 2;
      break;

    case ForgetGravity:
    case StaticGravity:
      /* the client's position won't move */
      *x -= self->size.left - self->client->border_width;
      break;
  }

  /* vertical */
  switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case NorthEastGravity:
    case NorthGravity:
      break;

    case CenterGravity:
    case EastGravity:
    case WestGravity:
      /* the middle of the client will be the middle of the frame */
      *y -= (self->size.bottom - self->size.top) / 2;
      break;

    case SouthWestGravity:
    case SouthEastGravity:
    case SouthGravity:
      /* the bottom of the client will be the bottom of the frame */
      *y -= self->size.bottom + self->size.top - self->client->border_width * 2;
      break;

    case ForgetGravity:
    case StaticGravity:
      /* the client's position won't move */
      *y -= self->size.top - self->client->border_width;
      break;
  }
}

void frame_frame_gravity(ObFrame* self, gint* x, gint* y) {
  /* horizontal */
  switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
      break;
    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
      /* the middle of the client will be the middle of the frame */
      *x += (self->size.right - self->size.left) / 2;
      break;
    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
      /* the right side of the client will be the right side of the frame */
      *x += self->size.right + self->size.left - self->client->border_width * 2;
      break;
    case StaticGravity:
    case ForgetGravity:
      /* the client's position won't move */
      *x += self->size.left - self->client->border_width;
      break;
  }

  /* vertical */
  switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case NorthGravity:
    case NorthEastGravity:
      break;
    case WestGravity:
    case CenterGravity:
    case EastGravity:
      /* the middle of the client will be the middle of the frame */
      *y += (self->size.bottom - self->size.top) / 2;
      break;
    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
      /* the bottom of the client will be the bottom of the frame */
      *y += self->size.bottom + self->size.top - self->client->border_width * 2;
      break;
    case StaticGravity:
    case ForgetGravity:
      /* the client's position won't move */
      *y += self->size.top - self->client->border_width;
      break;
  }
}

void frame_rect_to_frame(ObFrame* self, Rect* r) {
  r->width += self->size.left + self->size.right;
  r->height += self->size.top + self->size.bottom;
  frame_client_gravity(self, &r->x, &r->y);
}

void frame_rect_to_client(ObFrame* self, Rect* r) {
  r->width -= self->size.left + self->size.right;
  r->height -= self->size.top + self->size.bottom;
  frame_frame_gravity(self, &r->x, &r->y);
}

static void flash_done(gpointer data) {
  ObFrame* self = data;

  self->flash_timer = 0;
}

static gboolean flash_timeout(gpointer data) {
  ObFrame* self = data;
  GDateTime* now = g_date_time_new_now_local();

  if (g_date_time_compare(now, self->flash_end) >= 0)
    self->flashing = FALSE;

  g_date_time_unref(now);

  if (!self->flashing) {
    if (self->focused != self->flash_on)
      frame_adjust_focus(self, self->focused);

    return FALSE; /* we are done */
  }

  self->flash_on = !self->flash_on;
  if (!self->focused) {
    frame_adjust_focus(self, self->flash_on);
    self->focused = FALSE;
  }

  return TRUE; /* go again */
}

void frame_flash_start(ObFrame* self) {
  self->flash_on = self->focused;

  if (!self->flashing)
    self->flash_timer = g_timeout_add_full(G_PRIORITY_DEFAULT, 600, flash_timeout, self, flash_done);

  // Clean up previous end time if still set
  if (self->flash_end)
    g_date_time_unref(self->flash_end);

  // Set new end time: 5 seconds from now
  self->flash_end = g_date_time_add_seconds(g_date_time_new_now_local(), 5);

  self->flashing = TRUE;
}

void frame_flash_stop(ObFrame* self) {
  self->flashing = FALSE;
  if (self->flash_end) {
    g_date_time_unref(self->flash_end);
    self->flash_end = NULL;
  }
}

static gulong frame_animate_iconify_time_left(ObFrame* self, GDateTime* now) {
  gint64 usec_left = g_date_time_difference(self->iconify_animation_end, now);
  // g_date_time_difference returns microseconds (can be negative!)
  return usec_left > 0 ? (gulong)usec_left : 0;
}

static gboolean frame_animate_iconify(gpointer p) {
  ObFrame* self = p;
  gint x, y, w, h;
  gint iconx, icony, iconw;
  GDateTime* now;
  gulong time;
  gboolean iconifying;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
  uint32_t val[4];

  if (self->client->icon_geometry.width == 0) {
    /* there is no icon geometry set so just go straight down */
    const Rect* a;

    a = screen_physical_area_monitor(screen_find_monitor(&self->area));
    iconx = self->area.x + self->area.width / 2 + 32;
    icony = a->y + a->width;
    iconw = 64;
  }
  else {
    iconx = self->client->icon_geometry.x;
    icony = self->client->icon_geometry.y;
    iconw = self->client->icon_geometry.width;
  }

  iconifying = self->iconify_animation_going > 0;

  /* how far do we have left to go ? */
  now = g_date_time_new_now_local();
  time = frame_animate_iconify_time_left(self, now);
  g_date_time_unref(now);

  if ((time > 0 && iconifying) || (time == 0 && !iconifying)) {
    /* start where the frame is supposed to be */
    x = self->area.x;
    y = self->area.y;
    w = self->area.width;
    h = self->area.height;
  }
  else {
    /* start at the icon */
    x = iconx;
    y = icony;
    w = iconw;
    h = self->size.top; /* just the titlebar */
  }

  if (time > 0) {
    glong dx, dy, dw;
    glong elapsed;

    dx = self->area.x - iconx;
    dy = self->area.y - icony;
    dw = self->area.width - self->bwidth * 2 - iconw;
    /* if restoring, we move in the opposite direction */
    if (!iconifying) {
      dx = -dx;
      dy = -dy;
      dw = -dw;
    }

    elapsed = FRAME_ANIMATE_ICONIFY_TIME - time;
    x = x - (dx * elapsed) / FRAME_ANIMATE_ICONIFY_TIME;
    y = y - (dy * elapsed) / FRAME_ANIMATE_ICONIFY_TIME;
    w = w - (dw * elapsed) / FRAME_ANIMATE_ICONIFY_TIME;
    h = self->size.top; /* just the titlebar */
  }

  val[0] = x;
  val[1] = y;
  val[2] = w;
  val[3] = h;
  xcb_configure_window(conn, self->window, mask, val);
  xcb_flush(conn);

  return time > 0; /* repeat until we're out of time */
}

void frame_end_iconify_animation(gpointer data) {
  ObFrame* self = data;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  /* see if there is an animation going */
  if (self->iconify_animation_going == 0)
    return;

  if (!self->visible)
    xcb_unmap_window(conn, self->window);
  else {
    /* Send a ConfigureNotify when the animation is done to keep pagers
       in sync.  since the window is mapped at a different location and is then moved, we
       need to send the synthetic configurenotify, since apps may have
       read the position when the client mapped, apparently. */
    client_reconfigure(self->client, TRUE);
  }

  /* we're not animating any more ! */
  self->iconify_animation_going = 0;
  self->iconify_animation_timer = 0;

  uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
  uint32_t val[4] = {self->area.x, self->area.y, self->area.width, self->area.height};
  xcb_configure_window(conn, self->window, mask, val);

  /* we delay re-rendering until after we're done animating */
  framerender_frame(self);
  xcb_flush(conn);
}

void frame_begin_iconify_animation(ObFrame* self, gboolean iconifying) {
  gulong time = FRAME_ANIMATE_ICONIFY_TIME;  // in microseconds
  gboolean new_anim = FALSE;
  gboolean set_end = TRUE;
  GDateTime* now = g_date_time_new_now_local();

  // If there is no titlebar, just don't animate for now
  if (!(self->decorations & OB_FRAME_DECOR_TITLEBAR)) {
    g_date_time_unref(now);
    return;
  }

  if (self->iconify_animation_going) {
    if (!!iconifying != (self->iconify_animation_going > 0)) {
      // Animation was already going in the opposite direction
      gulong left = frame_animate_iconify_time_left(self, now);
      if (left < time)
        time = time - left;
      else
        time = 0;
    }
    else {
      // Animation was already going in the same direction
      set_end = FALSE;
    }
  }
  else {
    new_anim = TRUE;
  }

  self->iconify_animation_going = iconifying ? 1 : -1;

  if (set_end) {
    // Clean up any previous timer
    if (self->iconify_animation_end)
      g_date_time_unref(self->iconify_animation_end);

    // Add the new interval to the current time (seconds as double)
    self->iconify_animation_end = g_date_time_add_seconds(now, time / (double)G_USEC_PER_SEC);
  }

  g_date_time_unref(now);

  if (new_anim) {
    if (self->iconify_animation_timer)
      g_source_remove(self->iconify_animation_timer);

    self->iconify_animation_timer = g_timeout_add_full(G_PRIORITY_DEFAULT, FRAME_ANIMATE_ICONIFY_STEP_TIME,
                                                       frame_animate_iconify, self, frame_end_iconify_animation);

    // do the first step
    frame_animate_iconify(self);

    // show it during the animation even if it is not "visible"
    if (!self->visible)
      xcb_map_window(XGetXCBConnection(obt_display), self->window);
  }
}

/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   focus_cycle_indicator.c for the Openbox window manager
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

#include "focus_cycle.h"
#include "focus_cycle_indicator.h"
#include "client.h"
#include "openbox.h"
#include "frame.h"
#include "event.h"
#include "obrender/render.h"

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#include <glib.h>

#define FOCUS_INDICATOR_WIDTH 6

static struct {
  ObInternalWindow top;
  ObInternalWindow left;
  ObInternalWindow right;
  ObInternalWindow bottom;
} focus_indicator;

static RrAppearance* a_focus_indicator;
static RrColor* color_white;
static gboolean visible;

static Window create_window(Window parent, uint32_t mask, uint32_t* val) {
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  xcb_window_t win = xcb_generate_id(conn);
  uint8_t depth = RrDepth(ob_rr_inst);
  xcb_visualid_t visual = RrVisual(ob_rr_inst)->visualid;

  xcb_create_window(conn, depth, win, parent, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, visual, mask, val);
  return win;
}

void focus_cycle_indicator_startup(gboolean reconfig) {
  uint32_t mask;
  uint32_t val[2];
  xcb_connection_t* conn = XGetXCBConnection(obt_display);

  visible = FALSE;

  if (reconfig)
    return;

  focus_indicator.top.type = OB_WINDOW_CLASS_INTERNAL;
  focus_indicator.left.type = OB_WINDOW_CLASS_INTERNAL;
  focus_indicator.right.type = OB_WINDOW_CLASS_INTERNAL;
  focus_indicator.bottom.type = OB_WINDOW_CLASS_INTERNAL;

  mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_BACK_PIXEL;
  val[0] = 1; /* True */
  val[1] = BlackPixel(obt_display, ob_screen);

  focus_indicator.top.window = create_window(obt_root(ob_screen), mask, val);
  focus_indicator.left.window = create_window(obt_root(ob_screen), mask, val);
  focus_indicator.right.window = create_window(obt_root(ob_screen), mask, val);
  focus_indicator.bottom.window = create_window(obt_root(ob_screen), mask, val);

  stacking_add(INTERNAL_AS_WINDOW(&focus_indicator.top));
  stacking_add(INTERNAL_AS_WINDOW(&focus_indicator.left));
  stacking_add(INTERNAL_AS_WINDOW(&focus_indicator.right));
  stacking_add(INTERNAL_AS_WINDOW(&focus_indicator.bottom));
  window_add(&focus_indicator.top.window, INTERNAL_AS_WINDOW(&focus_indicator.top));
  window_add(&focus_indicator.left.window, INTERNAL_AS_WINDOW(&focus_indicator.left));
  window_add(&focus_indicator.right.window, INTERNAL_AS_WINDOW(&focus_indicator.right));
  window_add(&focus_indicator.bottom.window, INTERNAL_AS_WINDOW(&focus_indicator.bottom));

  color_white = RrColorNew(ob_rr_inst, 0xff, 0xff, 0xff);

  a_focus_indicator = RrAppearanceNew(ob_rr_inst, 4);
  a_focus_indicator->surface.grad = RR_SURFACE_SOLID;
  a_focus_indicator->surface.relief = RR_RELIEF_FLAT;
  a_focus_indicator->surface.primary = RrColorNew(ob_rr_inst, 0, 0, 0);
  a_focus_indicator->texture[0].type = RR_TEXTURE_LINE_ART;
  a_focus_indicator->texture[0].data.lineart.color = color_white;
  a_focus_indicator->texture[1].type = RR_TEXTURE_LINE_ART;
  a_focus_indicator->texture[1].data.lineart.color = color_white;
  a_focus_indicator->texture[2].type = RR_TEXTURE_LINE_ART;
  a_focus_indicator->texture[2].data.lineart.color = color_white;
  a_focus_indicator->texture[3].type = RR_TEXTURE_LINE_ART;
  a_focus_indicator->texture[3].data.lineart.color = color_white;
}

void focus_cycle_indicator_shutdown(gboolean reconfig) {
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  if (reconfig)
    return;

  RrColorFree(color_white);

  RrAppearanceFree(a_focus_indicator);

  window_remove(focus_indicator.top.window);
  window_remove(focus_indicator.left.window);
  window_remove(focus_indicator.right.window);
  window_remove(focus_indicator.bottom.window);

  stacking_remove(INTERNAL_AS_WINDOW(&focus_indicator.top));
  stacking_remove(INTERNAL_AS_WINDOW(&focus_indicator.left));
  stacking_remove(INTERNAL_AS_WINDOW(&focus_indicator.right));
  stacking_remove(INTERNAL_AS_WINDOW(&focus_indicator.bottom));

  xcb_destroy_window(conn, focus_indicator.top.window);
  xcb_destroy_window(conn, focus_indicator.left.window);
  xcb_destroy_window(conn, focus_indicator.right.window);
  xcb_destroy_window(conn, focus_indicator.bottom.window);
}

void focus_cycle_update_indicator(ObClient* c) {
  if (visible)
    focus_cycle_draw_indicator(c);
}

void focus_cycle_draw_indicator(ObClient* c) {
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
  uint32_t val[4];

  if (!c && visible) {
    gulong ignore_start;

    /* kill enter events cause by this unmapping */
    ignore_start = event_start_ignore_all_enters();

    xcb_unmap_window(conn, focus_indicator.top.window);
    xcb_unmap_window(conn, focus_indicator.left.window);
    xcb_unmap_window(conn, focus_indicator.right.window);
    xcb_unmap_window(conn, focus_indicator.bottom.window);

    event_end_ignore_all_enters(ignore_start);

    visible = FALSE;
  }
  else if (c) {
    /*
      if (c)
          frame_adjust_focus(c->frame, FALSE);
      frame_adjust_focus(c->frame, TRUE);
    */
    gint x, y, w, h;
    gint wt, wl, wr, wb;
    gulong ignore_start;

    wt = wl = wr = wb = FOCUS_INDICATOR_WIDTH;

    x = c->frame->area.x;
    y = c->frame->area.y;
    w = c->frame->area.width;
    h = wt;

    /* kill enter events cause by this moving */
    ignore_start = event_start_ignore_all_enters();

    val[0] = x;
    val[1] = y;
    val[2] = w;
    val[3] = h;
    xcb_configure_window(conn, focus_indicator.top.window, mask, val);
    a_focus_indicator->texture[0].data.lineart.x1 = 0;
    a_focus_indicator->texture[0].data.lineart.y1 = h - 1;
    a_focus_indicator->texture[0].data.lineart.x2 = 0;
    a_focus_indicator->texture[0].data.lineart.y2 = 0;
    a_focus_indicator->texture[1].data.lineart.x1 = 0;
    a_focus_indicator->texture[1].data.lineart.y1 = 0;
    a_focus_indicator->texture[1].data.lineart.x2 = w - 1;
    a_focus_indicator->texture[1].data.lineart.y2 = 0;
    a_focus_indicator->texture[2].data.lineart.x1 = w - 1;
    a_focus_indicator->texture[2].data.lineart.y1 = 0;
    a_focus_indicator->texture[2].data.lineart.x2 = w - 1;
    a_focus_indicator->texture[2].data.lineart.y2 = h - 1;
    a_focus_indicator->texture[3].data.lineart.x1 = (wl - 1);
    a_focus_indicator->texture[3].data.lineart.y1 = h - 1;
    a_focus_indicator->texture[3].data.lineart.x2 = w - wr;
    a_focus_indicator->texture[3].data.lineart.y2 = h - 1;
    RrPaint(a_focus_indicator, focus_indicator.top.window, w, h);

    x = c->frame->area.x;
    y = c->frame->area.y;
    w = wl;
    h = c->frame->area.height;

    val[0] = x;
    val[1] = y;
    val[2] = w;
    val[3] = h;
    xcb_configure_window(conn, focus_indicator.left.window, mask, val);
    a_focus_indicator->texture[0].data.lineart.x1 = w - 1;
    a_focus_indicator->texture[0].data.lineart.y1 = 0;
    a_focus_indicator->texture[0].data.lineart.x2 = 0;
    a_focus_indicator->texture[0].data.lineart.y2 = 0;
    a_focus_indicator->texture[1].data.lineart.x1 = 0;
    a_focus_indicator->texture[1].data.lineart.y1 = 0;
    a_focus_indicator->texture[1].data.lineart.x2 = 0;
    a_focus_indicator->texture[1].data.lineart.y2 = h - 1;
    a_focus_indicator->texture[2].data.lineart.x1 = 0;
    a_focus_indicator->texture[2].data.lineart.y1 = h - 1;
    a_focus_indicator->texture[2].data.lineart.x2 = w - 1;
    a_focus_indicator->texture[2].data.lineart.y2 = h - 1;
    a_focus_indicator->texture[3].data.lineart.x1 = w - 1;
    a_focus_indicator->texture[3].data.lineart.y1 = wt - 1;
    a_focus_indicator->texture[3].data.lineart.x2 = w - 1;
    a_focus_indicator->texture[3].data.lineart.y2 = h - wb;
    RrPaint(a_focus_indicator, focus_indicator.left.window, w, h);

    x = c->frame->area.x + c->frame->area.width - wr;
    y = c->frame->area.y;
    w = wr;
    h = c->frame->area.height;

    val[0] = x;
    val[1] = y;
    val[2] = w;
    val[3] = h;
    xcb_configure_window(conn, focus_indicator.right.window, mask, val);
    a_focus_indicator->texture[0].data.lineart.x1 = 0;
    a_focus_indicator->texture[0].data.lineart.y1 = 0;
    a_focus_indicator->texture[0].data.lineart.x2 = w - 1;
    a_focus_indicator->texture[0].data.lineart.y2 = 0;
    a_focus_indicator->texture[1].data.lineart.x1 = w - 1;
    a_focus_indicator->texture[1].data.lineart.y1 = 0;
    a_focus_indicator->texture[1].data.lineart.x2 = w - 1;
    a_focus_indicator->texture[1].data.lineart.y2 = h - 1;
    a_focus_indicator->texture[2].data.lineart.x1 = w - 1;
    a_focus_indicator->texture[2].data.lineart.y1 = h - 1;
    a_focus_indicator->texture[2].data.lineart.x2 = 0;
    a_focus_indicator->texture[2].data.lineart.y2 = h - 1;
    a_focus_indicator->texture[3].data.lineart.x1 = 0;
    a_focus_indicator->texture[3].data.lineart.y1 = wt - 1;
    a_focus_indicator->texture[3].data.lineart.x2 = 0;
    a_focus_indicator->texture[3].data.lineart.y2 = h - wb;
    RrPaint(a_focus_indicator, focus_indicator.right.window, w, h);

    x = c->frame->area.x;
    y = c->frame->area.y + c->frame->area.height - wb;
    w = c->frame->area.width;
    h = wb;

    val[0] = x;
    val[1] = y;
    val[2] = w;
    val[3] = h;
    xcb_configure_window(conn, focus_indicator.bottom.window, mask, val);
    a_focus_indicator->texture[0].data.lineart.x1 = 0;
    a_focus_indicator->texture[0].data.lineart.y1 = 0;
    a_focus_indicator->texture[0].data.lineart.x2 = 0;
    a_focus_indicator->texture[0].data.lineart.y2 = h - 1;
    a_focus_indicator->texture[1].data.lineart.x1 = 0;
    a_focus_indicator->texture[1].data.lineart.y1 = h - 1;
    a_focus_indicator->texture[1].data.lineart.x2 = w - 1;
    a_focus_indicator->texture[1].data.lineart.y2 = h - 1;
    a_focus_indicator->texture[2].data.lineart.x1 = w - 1;
    a_focus_indicator->texture[2].data.lineart.y1 = h - 1;
    a_focus_indicator->texture[2].data.lineart.x2 = w - 1;
    a_focus_indicator->texture[2].data.lineart.y2 = 0;
    a_focus_indicator->texture[3].data.lineart.x1 = wl - 1;
    a_focus_indicator->texture[3].data.lineart.y1 = 0;
    a_focus_indicator->texture[3].data.lineart.x2 = w - wr;
    a_focus_indicator->texture[3].data.lineart.y2 = 0;
    RrPaint(a_focus_indicator, focus_indicator.bottom.window, w, h);

    xcb_map_window(conn, focus_indicator.top.window);
    xcb_map_window(conn, focus_indicator.left.window);
    xcb_map_window(conn, focus_indicator.right.window);
    xcb_map_window(conn, focus_indicator.bottom.window);

    event_end_ignore_all_enters(ignore_start);

    visible = TRUE;
  }
}

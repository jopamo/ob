/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   framerender.c for the Openbox window manager
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
#include "openbox.h"
#include "screen.h"
#include "client.h"
#include "framerender.h"
#include "obrender/theme.h"

static void framerender_label(ObFrame* self, RrAppearance* a);
static void framerender_icon(ObFrame* self, RrAppearance* a);
static void framerender_max(ObFrame* self, RrAppearance* a);
static void framerender_iconify(ObFrame* self, RrAppearance* a);
static void framerender_desk(ObFrame* self, RrAppearance* a);
static void framerender_shade(ObFrame* self, RrAppearance* a);
static void framerender_close(ObFrame* self, RrAppearance* a);

void framerender_frame(ObFrame* self) {
  const RrInstance* inst = ob_rr_theme->inst;
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;

  if (frame_iconify_animating(self))
    return; /* delay redrawing until the animation is done */
  if (!self->need_render)
    return;
  if (!self->visible)
    return;
  self->need_render = FALSE;

  {
    const RrColor* inner = (self->focused ? ob_rr_theme->cb_focused_color : ob_rr_theme->cb_unfocused_color);
    const RrColor* border =
        (self->focused ? (self->client->undecorated ? ob_rr_theme->frame_undecorated_focused_border_color
                                                    : ob_rr_theme->frame_focused_border_color)
                       : (self->client->undecorated ? ob_rr_theme->frame_undecorated_unfocused_border_color
                                                    : ob_rr_theme->frame_unfocused_border_color));
    const RrColor* sep = border;

    RrClearWindowColor(inst, self->backback, inner);
    RrClearWindowColor(inst, self->innerleft, inner);
    RrClearWindowColor(inst, self->innertop, inner);
    RrClearWindowColor(inst, self->innerright, inner);
    RrClearWindowColor(inst, self->innerbottom, inner);
    RrClearWindowColor(inst, self->innerbll, inner);
    RrClearWindowColor(inst, self->innerbrr, inner);
    RrClearWindowColor(inst, self->innerblb, inner);
    RrClearWindowColor(inst, self->innerbrb, inner);

    RrClearWindowColor(inst, self->left, border);
    RrClearWindowColor(inst, self->right, border);

    RrClearWindowColor(inst, self->titleleft, border);
    RrClearWindowColor(inst, self->titletop, border);
    RrClearWindowColor(inst, self->titletopleft, border);
    RrClearWindowColor(inst, self->titletopright, border);
    RrClearWindowColor(inst, self->titleright, border);

    RrClearWindowColor(inst, self->handleleft, border);
    RrClearWindowColor(inst, self->handletop, border);
    RrClearWindowColor(inst, self->handleright, border);
    RrClearWindowColor(inst, self->handlebottom, border);

    RrClearWindowColor(inst, self->lgripleft, border);
    RrClearWindowColor(inst, self->lgriptop, border);
    RrClearWindowColor(inst, self->lgripbottom, border);

    RrClearWindowColor(inst, self->rgripright, border);
    RrClearWindowColor(inst, self->rgriptop, border);
    RrClearWindowColor(inst, self->rgripbottom, border);

    if (!self->client->shaded)
      sep = (self->focused ? ob_rr_theme->title_separator_focused_color : ob_rr_theme->title_separator_unfocused_color);

    RrClearWindowColor(inst, self->titlebottom, sep);
  }

  if (self->decorations & OB_FRAME_DECOR_TITLEBAR) {
    RrAppearance *t, *l, *m, *n, *i, *d, *s, *c, *clear;
    if (self->focused) {
      t = ob_rr_theme->a_focused_title;
      l = ob_rr_theme->a_focused_label;
      m = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE)
               ? ob_rr_theme->btn_max->a_focused_disabled
               : (self->client->max_vert || self->client->max_horz
                      ? (self->max_press ? ob_rr_theme->btn_max->a_focused_pressed_toggled
                                         : (self->max_hover ? ob_rr_theme->btn_max->a_focused_hover_toggled
                                                            : ob_rr_theme->btn_max->a_focused_unpressed_toggled))
                      : (self->max_press ? ob_rr_theme->btn_max->a_focused_pressed
                                         : (self->max_hover ? ob_rr_theme->btn_max->a_focused_hover
                                                            : ob_rr_theme->btn_max->a_focused_unpressed))));
      n = ob_rr_theme->a_icon;
      i = (!(self->decorations & OB_FRAME_DECOR_ICONIFY)
               ? ob_rr_theme->btn_iconify->a_focused_disabled
               : (self->iconify_press ? ob_rr_theme->btn_iconify->a_focused_pressed
                                      : (self->iconify_hover ? ob_rr_theme->btn_iconify->a_focused_hover
                                                             : ob_rr_theme->btn_iconify->a_focused_unpressed)));
      d = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS)
               ? ob_rr_theme->btn_desk->a_focused_disabled
               : (self->client->desktop == DESKTOP_ALL
                      ? (self->desk_press ? ob_rr_theme->btn_desk->a_focused_pressed_toggled
                                          : (self->desk_hover ? ob_rr_theme->btn_desk->a_focused_hover_toggled
                                                              : ob_rr_theme->btn_desk->a_focused_unpressed_toggled))
                      : (self->desk_press ? ob_rr_theme->btn_desk->a_focused_pressed
                                          : (self->desk_hover ? ob_rr_theme->btn_desk->a_focused_hover
                                                              : ob_rr_theme->btn_desk->a_focused_unpressed))));
      s = (!(self->decorations & OB_FRAME_DECOR_SHADE)
               ? ob_rr_theme->btn_shade->a_focused_disabled
               : (self->client->shaded
                      ? (self->shade_press ? ob_rr_theme->btn_shade->a_focused_pressed_toggled
                                           : (self->shade_hover ? ob_rr_theme->btn_shade->a_focused_hover_toggled
                                                                : ob_rr_theme->btn_shade->a_focused_unpressed_toggled))
                      : (self->shade_press ? ob_rr_theme->btn_shade->a_focused_pressed
                                           : (self->shade_hover ? ob_rr_theme->btn_shade->a_focused_hover
                                                                : ob_rr_theme->btn_shade->a_focused_unpressed))));
      c = (!(self->decorations & OB_FRAME_DECOR_CLOSE)
               ? ob_rr_theme->btn_close->a_focused_disabled
               : (self->close_press ? ob_rr_theme->btn_close->a_focused_pressed
                                    : (self->close_hover ? ob_rr_theme->btn_close->a_focused_hover
                                                         : ob_rr_theme->btn_close->a_focused_unpressed)));
    }
    else {
      t = ob_rr_theme->a_unfocused_title;
      l = ob_rr_theme->a_unfocused_label;
      m = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE)
               ? ob_rr_theme->btn_max->a_unfocused_disabled
               : (self->client->max_vert || self->client->max_horz
                      ? (self->max_press ? ob_rr_theme->btn_max->a_unfocused_pressed_toggled
                                         : (self->max_hover ? ob_rr_theme->btn_max->a_unfocused_hover_toggled
                                                            : ob_rr_theme->btn_max->a_unfocused_unpressed_toggled))
                      : (self->max_press ? ob_rr_theme->btn_max->a_unfocused_pressed
                                         : (self->max_hover ? ob_rr_theme->btn_max->a_unfocused_hover
                                                            : ob_rr_theme->btn_max->a_unfocused_unpressed))));
      n = ob_rr_theme->a_icon;
      i = (!(self->decorations & OB_FRAME_DECOR_ICONIFY)
               ? ob_rr_theme->btn_iconify->a_unfocused_disabled
               : (self->iconify_press ? ob_rr_theme->btn_iconify->a_unfocused_pressed
                                      : (self->iconify_hover ? ob_rr_theme->btn_iconify->a_unfocused_hover
                                                             : ob_rr_theme->btn_iconify->a_unfocused_unpressed)));
      d = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS)
               ? ob_rr_theme->btn_desk->a_unfocused_disabled
               : (self->client->desktop == DESKTOP_ALL
                      ? (self->desk_press ? ob_rr_theme->btn_desk->a_unfocused_pressed_toggled
                                          : (self->desk_hover ? ob_rr_theme->btn_desk->a_unfocused_hover_toggled
                                                              : ob_rr_theme->btn_desk->a_unfocused_unpressed_toggled))
                      : (self->desk_press ? ob_rr_theme->btn_desk->a_unfocused_pressed
                                          : (self->desk_hover ? ob_rr_theme->btn_desk->a_unfocused_hover
                                                              : ob_rr_theme->btn_desk->a_unfocused_unpressed))));
      s = (!(self->decorations & OB_FRAME_DECOR_SHADE)
               ? ob_rr_theme->btn_shade->a_unfocused_disabled
               : (self->client->shaded
                      ? (self->shade_press
                             ? ob_rr_theme->btn_shade->a_unfocused_pressed_toggled
                             : (self->shade_hover ? ob_rr_theme->btn_shade->a_unfocused_hover_toggled
                                                  : ob_rr_theme->btn_shade->a_unfocused_unpressed_toggled))
                      : (self->shade_press ? ob_rr_theme->btn_shade->a_unfocused_pressed
                                           : (self->shade_hover ? ob_rr_theme->btn_shade->a_unfocused_hover
                                                                : ob_rr_theme->btn_shade->a_unfocused_unpressed))));
      c = (!(self->decorations & OB_FRAME_DECOR_CLOSE)
               ? ob_rr_theme->btn_close->a_unfocused_disabled
               : (self->close_press ? ob_rr_theme->btn_close->a_unfocused_pressed
                                    : (self->close_hover ? ob_rr_theme->btn_close->a_unfocused_hover
                                                         : ob_rr_theme->btn_close->a_unfocused_unpressed)));
    }
    clear = ob_rr_theme->a_clear;

    RrPaint(t, self->title, self->width, geom->title_height);

    clear->surface.parent = t;
    clear->surface.parenty = 0;

    clear->surface.parentx = geom->grip_width;

    RrPaint(clear, self->topresize, self->width - geom->grip_width * 2, geom->paddingy + 1);

    clear->surface.parentx = 0;

    if (geom->grip_width > 0)
      RrPaint(clear, self->tltresize, geom->grip_width, geom->paddingy + 1);
    if (geom->title_height > 0)
      RrPaint(clear, self->tllresize, geom->paddingx + 1, geom->title_height);

    clear->surface.parentx = self->width - geom->grip_width;

    if (geom->grip_width > 0)
      RrPaint(clear, self->trtresize, geom->grip_width, geom->paddingy + 1);

    clear->surface.parentx = self->width - (geom->paddingx + 1);

    if (geom->title_height > 0)
      RrPaint(clear, self->trrresize, geom->paddingx + 1, geom->title_height);

    /* set parents for any parent relative guys */
    l->surface.parent = t;
    l->surface.parentx = self->label_x;
    l->surface.parenty = geom->paddingy;

    m->surface.parent = t;
    m->surface.parentx = self->max_x;
    m->surface.parenty = geom->paddingy + 1;

    n->surface.parent = t;
    n->surface.parentx = self->icon_x;
    n->surface.parenty = geom->paddingy;

    i->surface.parent = t;
    i->surface.parentx = self->iconify_x;
    i->surface.parenty = geom->paddingy + 1;

    d->surface.parent = t;
    d->surface.parentx = self->desk_x;
    d->surface.parenty = geom->paddingy + 1;

    s->surface.parent = t;
    s->surface.parentx = self->shade_x;
    s->surface.parenty = geom->paddingy + 1;

    c->surface.parent = t;
    c->surface.parentx = self->close_x;
    c->surface.parenty = geom->paddingy + 1;

    framerender_label(self, l);
    framerender_max(self, m);
    framerender_icon(self, n);
    framerender_iconify(self, i);
    framerender_desk(self, d);
    framerender_shade(self, s);
    framerender_close(self, c);
  }

  if (self->decorations & OB_FRAME_DECOR_HANDLE && geom->handle_height > 0) {
    RrAppearance *h, *g;

    h = (self->focused ? ob_rr_theme->a_focused_handle : ob_rr_theme->a_unfocused_handle);

    RrPaint(h, self->handle, self->width, geom->handle_height);

    if (self->decorations & OB_FRAME_DECOR_GRIPS) {
      g = (self->focused ? ob_rr_theme->a_focused_grip : ob_rr_theme->a_unfocused_grip);

      if (g->surface.grad == RR_SURFACE_PARENTREL)
        g->surface.parent = h;

      g->surface.parentx = 0;
      g->surface.parenty = 0;

      RrPaint(g, self->lgrip, geom->grip_width, geom->handle_height);

      g->surface.parentx = self->width - geom->grip_width;
      g->surface.parenty = 0;

      RrPaint(g, self->rgrip, geom->grip_width, geom->handle_height);
    }
  }

  RrFlush(inst);
}

static void framerender_label(ObFrame* self, RrAppearance* a) {
  if (!self->label_on)
    return;
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  /* set the texture's text! */
  a->texture[0].data.text.string = self->client->title;
  RrPaint(a, self->label, self->label_width, geom->label_height);
}

static void framerender_icon(ObFrame* self, RrAppearance* a) {
  RrImage* icon;

  if (!self->icon_on)
    return;
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;

  icon = client_icon(self->client);

  if (icon) {
    RrAppearanceClearTextures(a);
    a->texture[0].type = RR_TEXTURE_IMAGE;
    a->texture[0].data.image.alpha = 0xff;
    a->texture[0].data.image.image = icon;
  }
  else {
    RrAppearanceClearTextures(a);
    a->texture[0].type = RR_TEXTURE_NONE;
  }

  RrPaint(a, self->icon, geom->button_size + 2, geom->button_size + 2);
}

static void framerender_max(ObFrame* self, RrAppearance* a) {
  if (!self->max_on)
    return;
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  RrPaint(a, self->max, geom->button_size, geom->button_size);
}

static void framerender_iconify(ObFrame* self, RrAppearance* a) {
  if (!self->iconify_on)
    return;
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  RrPaint(a, self->iconify, geom->button_size, geom->button_size);
}

static void framerender_desk(ObFrame* self, RrAppearance* a) {
  if (!self->desk_on)
    return;
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  RrPaint(a, self->desk, geom->button_size, geom->button_size);
}

static void framerender_shade(ObFrame* self, RrAppearance* a) {
  if (!self->shade_on)
    return;
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  RrPaint(a, self->shade, geom->button_size, geom->button_size);
}

static void framerender_close(ObFrame* self, RrAppearance* a) {
  if (!self->close_on)
    return;
  const RrFrameGeometry* geom = &ob_rr_theme->frame_geom;
  RrPaint(a, self->close, geom->button_size, geom->button_size);
}

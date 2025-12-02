/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   window.c for the Openbox window manager
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

#include "window.h"
#include "menuframe.h"
#include "config.h"
#include "dock.h"
#include "client.h"
#include "frame.h"
#include "openbox.h"
#include "prompt.h"
#include "debug.h"
#include "grab.h"
#include "obt/prop.h"
#include "obt/xqueue.h"

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

static GHashTable* window_map;

static guint window_hash(Window* w) {
  return *w;
}
static gboolean window_comp(Window* w1, Window* w2) {
  return *w1 == *w2;
}

void window_startup(gboolean reconfig) {
  if (reconfig)
    return;

  window_map = g_hash_table_new((GHashFunc)window_hash, (GEqualFunc)window_comp);
}

void window_shutdown(gboolean reconfig) {
  if (reconfig)
    return;

  g_hash_table_destroy(window_map);
}

Window window_top(ObWindow* self) {
  switch (self->type) {
    case OB_WINDOW_CLASS_MENUFRAME:
      return WINDOW_AS_MENUFRAME(self)->window;
    case OB_WINDOW_CLASS_DOCK:
      return WINDOW_AS_DOCK(self)->frame;
    case OB_WINDOW_CLASS_CLIENT:
      return WINDOW_AS_CLIENT(self)->frame->window;
    case OB_WINDOW_CLASS_INTERNAL:
      return WINDOW_AS_INTERNAL(self)->window;
    case OB_WINDOW_CLASS_PROMPT:
      return WINDOW_AS_PROMPT(self)->super.window;
  }
  g_assert_not_reached();
  return None;
}

ObStackingLayer window_layer(ObWindow* self) {
  switch (self->type) {
    case OB_WINDOW_CLASS_DOCK:
      return config_dock_layer;
    case OB_WINDOW_CLASS_CLIENT:
      return ((ObClient*)self)->layer;
    case OB_WINDOW_CLASS_MENUFRAME:
    case OB_WINDOW_CLASS_INTERNAL:
      return OB_STACKING_LAYER_INTERNAL;
    case OB_WINDOW_CLASS_PROMPT:
      /* not used directly for stacking, prompts are managed as clients */
      g_assert_not_reached();
      break;
  }
  g_assert_not_reached();
  return None;
}

ObWindow* window_find(Window xwin) {
  return g_hash_table_lookup(window_map, &xwin);
}

void window_add(Window* xwin, ObWindow* win) {
  g_assert(xwin != NULL);
  g_assert(win != NULL);
  g_hash_table_insert(window_map, xwin, win);
}

void window_remove(Window xwin) {
  g_assert(xwin != None);
  g_hash_table_remove(window_map, &xwin);
}

void window_manage_all(void) {
  guint i, j, nchild;
  xcb_window_t* children;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  xcb_query_tree_reply_t* tree;
  xcb_get_window_attributes_reply_t* attrib;

  tree = xcb_query_tree_reply(conn, xcb_query_tree(conn, obt_root(ob_screen)), NULL);
  if (!tree) {
    ob_debug("xcb_query_tree failed in window_manage_all");
    return;
  }

  nchild = xcb_query_tree_children_length(tree);
  children = xcb_query_tree_children(tree);

  /* remove all icon windows from the list */
  for (i = 0; i < nchild; i++) {
    xcb_icccm_wm_hints_t wmhints;
    if (children[i] == XCB_WINDOW_NONE)
      continue;
    if (xcb_icccm_get_wm_hints_reply(conn, xcb_icccm_get_wm_hints(conn, children[i]), &wmhints, NULL)) {
      if ((wmhints.flags & XCB_ICCCM_WM_HINT_ICON_WINDOW) && (wmhints.icon_window != children[i]))
        for (j = 0; j < nchild; j++)
          if (children[j] == wmhints.icon_window) {
            /* XXX watch the window though */
            children[j] = XCB_WINDOW_NONE;
            break;
          }
    }
  }

  for (i = 0; i < nchild; ++i) {
    if (children[i] == XCB_WINDOW_NONE)
      continue;
    if (window_find(children[i]))
      continue; /* skip our own windows */

    attrib = xcb_get_window_attributes_reply(conn, xcb_get_window_attributes(conn, children[i]), NULL);
    if (attrib) {
      if (attrib->map_state == XCB_MAP_STATE_UNMAPPED)
        ;
      else
        window_manage(children[i]);
      free(attrib);
    }
  }

  free(tree);
}

static gboolean check_unmap(XEvent* e, gpointer data) {
  const Window win = *(Window*)data;
  return ((e->type == DestroyNotify && e->xdestroywindow.window == win) ||
          (e->type == UnmapNotify && e->xunmap.window == win));
}

void window_manage(Window win) {
  xcb_get_window_attributes_reply_t* attrib;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  gboolean no_manage = FALSE;
  gboolean is_dockapp = FALSE;
  Window icon_win = None;

  grab_server(TRUE);

  /* check if it has already been unmapped by the time we started
     mapping. the grab does a sync so we don't have to here */
  if (xqueue_exists_local(check_unmap, &win)) {
    ob_debug("Trying to manage unmapped window. Aborting that.");
    no_manage = TRUE;
  }
  else {
    attrib = xcb_get_window_attributes_reply(conn, xcb_get_window_attributes(conn, win), NULL);
    if (!attrib)
      no_manage = TRUE;
    else {
      xcb_icccm_wm_hints_t wmhints;

      /* is the window a docking app */
      is_dockapp = FALSE;
      if (xcb_icccm_get_wm_hints_reply(conn, xcb_icccm_get_wm_hints(conn, win), &wmhints, NULL)) {
        if ((wmhints.flags & XCB_ICCCM_WM_HINT_STATE) && wmhints.initial_state == XCB_ICCCM_WM_STATE_WITHDRAWN) {
          if (wmhints.flags & XCB_ICCCM_WM_HINT_ICON_WINDOW)
            icon_win = wmhints.icon_window;
          is_dockapp = TRUE;
        }
      }
      /* This is a new method to declare that a window is a dockapp, being
         implemented by Windowmaker, to alleviate pain in writing GTK+
         dock apps.
         http://thread.gmane.org/gmane.comp.window-managers.openbox/4881
      */
      if (!is_dockapp) {
        gchar** ss;
        if (OBT_PROP_GETSS_TYPE(win, WM_CLASS, STRING_NO_CC, &ss)) {
          if (ss[0] && ss[1] && strcmp(ss[1], "DockApp") == 0)
            is_dockapp = TRUE;
          g_strfreev(ss);
        }
      }
    }
  }

  if (!no_manage) {
    if (attrib->override_redirect) {
      ob_debug("not managing override redirect window 0x%x", (unsigned int)win);
      grab_server(FALSE);
    }
    else if (is_dockapp) {
      if (!icon_win)
        icon_win = win;
      dock_manage(icon_win, win);
    }
    else
      client_manage(win, NULL);
    free(attrib);
  }
  else {
    grab_server(FALSE);
    ob_debug("FAILED to manage window 0x%x", (unsigned int)win);
  }
}

void window_unmanage_all(void) {
  dock_unmanage_all();
  client_unmanage_all();
}

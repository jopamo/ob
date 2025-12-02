/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   grab.c for the Openbox window manager
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

#include "grab.h"
#include "openbox.h"
#include "event.h"
#include "screen.h"
#include "debug.h"
#include "obt/display.h"
#include "obt/keyboard.h"
#include "openbox/x11/x11.h"

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>

#define GRAB_PTR_MASK (XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION)
#define GRAB_KEY_MASK (XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE)

#define MASK_LIST_SIZE 8

/*! A list of all possible combinations of keyboard lock masks */
static guint mask_list[MASK_LIST_SIZE];
static guint kgrabs = 0;
static guint pgrabs = 0;
/*! The time at which the last grab was made */
static Time grab_time = CurrentTime;
static gint passive_count = 0;
static ObtIC* ic = NULL;

static Time ungrab_time(void) {
  Time t = event_time();
  if (grab_time == CurrentTime || !(t == CurrentTime || event_time_after(t, grab_time)))
    /* When the time moves backward on the server, then we can't use
       the grab time because that will be in the future. So instead we
       have to use CurrentTime.

       "XUngrabPointer does not release the pointer if the specified time
       is earlier than the last-pointer-grab time or is later than the
       current X server time."
    */
    t = CurrentTime; /*grab_time;*/
  return t;
}

static Window grab_window(void) {
  return screen_support_win;
}

gboolean grab_on_keyboard(void) {
  return kgrabs > 0;
}

gboolean grab_on_pointer(void) {
  return pgrabs > 0;
}

ObtIC* grab_input_context(void) {
  return ic;
}

static void xcb_sync(xcb_connection_t* conn) {
  free(xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), NULL));
}

gboolean grab_keyboard_full(gboolean grab) {
  gboolean ret = FALSE;

  if (grab) {
    if (kgrabs++ == 0) {
      xcb_connection_t* conn = XGetXCBConnection(obt_display);
      xcb_grab_keyboard_cookie_t cookie =
          xcb_grab_keyboard(conn, FALSE, grab_window(), event_time(), XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
      xcb_grab_keyboard_reply_t* reply = xcb_grab_keyboard_reply(conn, cookie, NULL);
      ret = reply && reply->status == XCB_GRAB_STATUS_SUCCESS;
      free(reply);

      if (!ret)
        --kgrabs;
      else {
        passive_count = 0;
        grab_time = event_time();
      }
    }
    else
      ret = TRUE;
  }
  else if (kgrabs > 0) {
    if (--kgrabs == 0) {
      xcb_ungrab_keyboard(XGetXCBConnection(obt_display), ungrab_time());
    }
    ret = TRUE;
  }

  return ret;
}

gboolean grab_pointer_full(gboolean grab, gboolean owner_events, gboolean confine, ObCursor cur) {
  gboolean ret = FALSE;

  if (grab) {
    if (pgrabs++ == 0) {
      xcb_connection_t* conn = XGetXCBConnection(obt_display);
      xcb_grab_pointer_cookie_t cookie =
          xcb_grab_pointer(conn, owner_events, grab_window(), GRAB_PTR_MASK, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                           (confine ? obt_root(ob_screen) : XCB_NONE), ob_cursor(cur), event_time());
      xcb_grab_pointer_reply_t* reply = xcb_grab_pointer_reply(conn, cookie, NULL);
      ret = reply && reply->status == XCB_GRAB_STATUS_SUCCESS;
      free(reply);

      if (!ret)
        --pgrabs;
      else
        grab_time = event_time();
    }
    else
      ret = TRUE;
  }
  else if (pgrabs > 0) {
    if (--pgrabs == 0) {
      xcb_ungrab_pointer(XGetXCBConnection(obt_display), ungrab_time());
    }
    ret = TRUE;
  }
  return ret;
}

gint grab_server(gboolean grab) {
  static guint sgrabs = 0;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  if (grab) {
    if (sgrabs++ == 0) {
      xcb_grab_server(conn);
      xcb_sync(conn);
    }
  }
  else if (sgrabs > 0) {
    if (--sgrabs == 0) {
      xcb_ungrab_server(conn);
      xcb_flush(conn);
    }
  }
  return sgrabs;
}

void grab_startup(gboolean reconfig) {
  guint i = 0;
  guint num, caps, scroll;

  num = obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_NUMLOCK);
  caps = obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_CAPSLOCK);
  scroll = obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_SCROLLLOCK);

  mask_list[i++] = 0;
  mask_list[i++] = num;
  mask_list[i++] = caps;
  mask_list[i++] = scroll;
  mask_list[i++] = num | caps;
  mask_list[i++] = num | scroll;
  mask_list[i++] = caps | scroll;
  mask_list[i++] = num | caps | scroll;
  g_assert(i == MASK_LIST_SIZE);

  ic = obt_keyboard_context_new(obt_root(ob_screen), grab_window());
}

void grab_shutdown(gboolean reconfig) {
  obt_keyboard_context_unref(ic);
  ic = NULL;

  if (reconfig)
    return;

  while (ungrab_keyboard())
    ;
  while (ungrab_pointer())
    ;
  while (grab_server(FALSE))
    ;
}

void grab_button_full(guint button, guint state, Window win, guint mask, gint pointer_mode, ObCursor cur) {
  guint i;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  xcb_generic_error_t* err = NULL;

  for (i = 0; i < MASK_LIST_SIZE; ++i) {
    err = xcb_request_check(conn, xcb_grab_button_checked(conn, FALSE, win, mask, pointer_mode, XCB_GRAB_MODE_ASYNC,
                                                          XCB_NONE, ob_cursor(cur), button, state | mask_list[i]));
    if (err) {
      ob_debug("Failed to grab button %d modifiers %d", button, state);
      free(err);
    }
  }
}

void ungrab_button(guint button, guint state, Window win) {
  guint i;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);

  for (i = 0; i < MASK_LIST_SIZE; ++i)
    xcb_ungrab_button(conn, button, win, state | mask_list[i]);
}

void grab_key(guint keycode, guint state, Window win, gint keyboard_mode) {
  guint i;
  xcb_connection_t* conn = XGetXCBConnection(obt_display);
  xcb_generic_error_t* err = NULL;

  for (i = 0; i < MASK_LIST_SIZE; ++i) {
    err = xcb_request_check(conn, xcb_grab_key_checked(conn, FALSE, win, state | mask_list[i], keycode, keyboard_mode,
                                                       XCB_GRAB_MODE_ASYNC));
    if (err) {
      ob_debug("Failed to grab keycode %d modifiers %d", keycode, state);
      free(err);
    }
  }
}

void ungrab_all_keys(Window win) {
  xcb_ungrab_key(XGetXCBConnection(obt_display), XCB_GRAB_ANY, win, XCB_MOD_MASK_ANY);
}

void grab_key_passive_count(int change) {
  if (grab_on_keyboard())
    return;
  passive_count += change;
  if (passive_count < 0)
    passive_count = 0;
}

void ungrab_passive_key(void) {
  /*ob_debug("ungrabbing %d passive grabs\n", passive_count);*/
  if (passive_count) {
    /* kill our passive grab */
    xcb_ungrab_keyboard(XGetXCBConnection(obt_display), event_time());
    passive_count = 0;
  }
}

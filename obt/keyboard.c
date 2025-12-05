/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/keyboard.c for the Openbox window manager
   Copyright (c) 2007        Dana Jansens

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

#include "obt/display.h"
#include "obt/keyboard.h"
#include "openbox/x11/x11.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

#include <locale.h>
#include <string.h>

struct _ObtIC {
  guint ref;
  XIC xic;
  Window client;
  Window focus;
};

/* These masks are constants and the modifier keys are bound to them as
   anyone sees fit:
        ShiftMask (1<<0), LockMask (1<<1), ControlMask (1<<2), Mod1Mask (1<<3),
        Mod2Mask (1<<4), Mod3Mask (1<<5), Mod4Mask (1<<6), Mod5Mask (1<<7)
*/
#define NUM_MASKS 8
#define ALL_MASKS 0xff /* an or'ing of all 8 keyboard masks */

static void xim_init(void);
void obt_keyboard_shutdown();
void obt_keyboard_context_renew(ObtIC* ic);

static struct xkb_context* keyboard_context;
static struct xkb_keymap* keyboard_keymap;
static struct xkb_state* keyboard_state;
static struct xkb_compose_table* keyboard_compose_table;
static struct xkb_compose_state* keyboard_compose_state;
static guint keyboard_last_serial = 0;
static guint keyboard_last_keycode = 0;
static guint keyboard_last_mask = 0;
static Time keyboard_last_time = CurrentTime;
static gboolean keyboard_selected_events = FALSE;
/*! This is a bitmask of the different masks for each modifier key */
static guint modkeys_keys[OBT_KEYBOARD_NUM_MODKEYS];

static gboolean started = FALSE;

static gboolean keyboard_build_keymap(void);
static gboolean keyboard_load_rule_names(struct xkb_rule_names* names);
static guint keyboard_mask_from_names(const char* const* names, guint fallback);
static void keyboard_update_modifiers(void);
static void keyboard_destroy_xkb(void);
static void keyboard_reset_compose_state(void);
static void keyboard_sync_state_from_x11(void);

static XIM xim = NULL;
static XIMStyle xim_style = 0;
static GSList* xic_all = NULL;
static gboolean keyboard_load_rule_names(struct xkb_rule_names* names) {
  ObX11PropertyValue prop_value = {0};
  ObX11StringList fields = {0};
  Atom prop;
  Window root;
  gboolean ret = FALSE;

  memset(names, 0, sizeof(*names));

  prop = ob_x11_atom_if_exists("_XKB_RULES_NAMES");
  if (!prop)
    return FALSE;

  root = obt_root(DefaultScreen(obt_display));
  if (!ob_x11_get_property(root, prop, XA_STRING, 8, 1, 0, &prop_value))
    goto done;

  if (!ob_x11_property_to_string_list(&prop_value, 1, 5, &fields))
    goto done;

  names->rules = (fields.n_items > 0 && fields.items[0] && *fields.items[0]) ? g_strdup(fields.items[0]) : NULL;
  names->model = (fields.n_items > 1 && fields.items[1] && *fields.items[1]) ? g_strdup(fields.items[1]) : NULL;
  names->layout = (fields.n_items > 2 && fields.items[2] && *fields.items[2]) ? g_strdup(fields.items[2]) : NULL;
  names->variant = (fields.n_items > 3 && fields.items[3] && *fields.items[3]) ? g_strdup(fields.items[3]) : NULL;
  names->options = (fields.n_items > 4 && fields.items[4] && *fields.items[4]) ? g_strdup(fields.items[4]) : NULL;

  ret = TRUE;

done:
  ob_x11_string_list_clear(&fields);
  ob_x11_property_value_clear(&prop_value);
  return ret;
}

static void keyboard_free_rule_names(struct xkb_rule_names* names) {
  g_free((gpointer)names->rules);
  g_free((gpointer)names->model);
  g_free((gpointer)names->layout);
  g_free((gpointer)names->variant);
  g_free((gpointer)names->options);
  memset(names, 0, sizeof(*names));
}

static guint keyboard_mask_from_names(const char* const* names, guint fallback) {
  if (!keyboard_keymap)
    return fallback;

  for (gint i = 0; names[i]; ++i) {
    xkb_mod_index_t idx = xkb_keymap_mod_get_index(keyboard_keymap, names[i]);
    if (idx != XKB_MOD_INVALID)
      return (guint)((1U << idx) & ALL_MASKS);
  }
  return fallback;
}

static void keyboard_update_modifiers(void) {
  static const char* caps_names[] = {XKB_MOD_NAME_CAPS, NULL};
  static const char* shift_names[] = {XKB_MOD_NAME_SHIFT, NULL};
  static const char* ctrl_names[] = {XKB_MOD_NAME_CTRL, NULL};
  static const char* alt_names[] = {XKB_MOD_NAME_ALT, NULL};
  static const char* meta_names[] = {"Meta", NULL};
  static const char* super_names[] = {XKB_MOD_NAME_LOGO, "Super", NULL};
  static const char* hyper_names[] = {"Hyper", NULL};
  static const char* num_names[] = {XKB_MOD_NAME_NUM, NULL};
  static const char* scroll_names[] = {"ScrollLock", NULL};

  modkeys_keys[OBT_KEYBOARD_MODKEY_CAPSLOCK] = keyboard_mask_from_names(caps_names, LockMask);
  modkeys_keys[OBT_KEYBOARD_MODKEY_SHIFT] = keyboard_mask_from_names(shift_names, ShiftMask);
  modkeys_keys[OBT_KEYBOARD_MODKEY_CONTROL] = keyboard_mask_from_names(ctrl_names, ControlMask);
  modkeys_keys[OBT_KEYBOARD_MODKEY_ALT] = keyboard_mask_from_names(alt_names, Mod1Mask);
  modkeys_keys[OBT_KEYBOARD_MODKEY_META] = keyboard_mask_from_names(meta_names, Mod1Mask);
  modkeys_keys[OBT_KEYBOARD_MODKEY_SUPER] = keyboard_mask_from_names(super_names, Mod4Mask);
  modkeys_keys[OBT_KEYBOARD_MODKEY_HYPER] = keyboard_mask_from_names(hyper_names, Mod4Mask);
  modkeys_keys[OBT_KEYBOARD_MODKEY_NUMLOCK] = keyboard_mask_from_names(num_names, 0);
  modkeys_keys[OBT_KEYBOARD_MODKEY_SCROLLLOCK] = keyboard_mask_from_names(scroll_names, 0);
}

static void keyboard_reset_compose_state(void) {
  if (!keyboard_context)
    return;

  if (keyboard_compose_state) {
    xkb_compose_state_unref(keyboard_compose_state);
    keyboard_compose_state = NULL;
  }
  if (keyboard_compose_table) {
    xkb_compose_table_unref(keyboard_compose_table);
    keyboard_compose_table = NULL;
  }

  const char* locale = setlocale(LC_CTYPE, NULL);
  if (!locale || !*locale)
    locale = "C";

  keyboard_compose_table = xkb_compose_table_new_from_locale(keyboard_context, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
  if (keyboard_compose_table)
    keyboard_compose_state = xkb_compose_state_new(keyboard_compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
}

static void keyboard_sync_state_from_x11(void) {
  if (!keyboard_state || !obt_display_extension_xkb)
    return;

  XkbStateRec x11_state;
  if (XkbGetState(obt_display, XkbUseCoreKbd, &x11_state) != Success)
    return;

  xkb_state_update_mask(keyboard_state, x11_state.base_mods, x11_state.latched_mods, x11_state.locked_mods,
                        x11_state.base_group, x11_state.latched_group, x11_state.locked_group);
}

static gboolean keyboard_build_keymap(void) {
  struct xkb_rule_names names;
  gboolean have_names = FALSE;

  if (!keyboard_context)
    keyboard_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!keyboard_context) {
    g_warning("Failed to initialize xkbcommon context");
    return FALSE;
  }

  have_names = keyboard_load_rule_names(&names);
  struct xkb_keymap* keymap =
      xkb_keymap_new_from_names(keyboard_context, have_names ? &names : NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!keymap) {
    g_warning("Failed to compile XKB keymap");
    if (have_names)
      keyboard_free_rule_names(&names);
    return FALSE;
  }

  struct xkb_state* state = xkb_state_new(keymap);
  if (!state) {
    g_warning("Failed to create XKB state");
    xkb_keymap_unref(keymap);
    if (have_names)
      keyboard_free_rule_names(&names);
    return FALSE;
  }

  if (keyboard_keymap)
    xkb_keymap_unref(keyboard_keymap);
  if (keyboard_state)
    xkb_state_unref(keyboard_state);

  keyboard_keymap = keymap;
  keyboard_state = state;

  keyboard_update_modifiers();
  keyboard_reset_compose_state();
  keyboard_sync_state_from_x11();

  if (!keyboard_selected_events && obt_display_extension_xkb) {
    XkbSelectEvents(obt_display, XkbUseCoreKbd, XkbNewKeyboardNotifyMask, XkbNewKeyboardNotifyMask);
    XkbSelectEventDetails(obt_display, XkbUseCoreKbd, XkbMapNotify, XkbAllMapComponentsMask, XkbAllMapComponentsMask);
    XkbSelectEventDetails(obt_display, XkbUseCoreKbd, XkbStateNotify, XkbAllStateComponentsMask,
                          XkbAllStateComponentsMask);
    keyboard_selected_events = TRUE;
  }

  if (have_names)
    keyboard_free_rule_names(&names);

  return TRUE;
}

static void keyboard_destroy_xkb(void) {
  if (keyboard_compose_state) {
    xkb_compose_state_unref(keyboard_compose_state);
    keyboard_compose_state = NULL;
  }
  if (keyboard_compose_table) {
    xkb_compose_table_unref(keyboard_compose_table);
    keyboard_compose_table = NULL;
  }
  if (keyboard_state) {
    xkb_state_unref(keyboard_state);
    keyboard_state = NULL;
  }
  if (keyboard_keymap) {
    xkb_keymap_unref(keyboard_keymap);
    keyboard_keymap = NULL;
  }
  if (keyboard_context) {
    xkb_context_unref(keyboard_context);
    keyboard_context = NULL;
  }
  keyboard_last_serial = 0;
  keyboard_last_keycode = 0;
  keyboard_last_mask = 0;
  keyboard_last_time = CurrentTime;
  keyboard_selected_events = FALSE;
}

void obt_keyboard_reload(void) {
  if (started)
    obt_keyboard_shutdown(); /* free stuff */
  started = TRUE;

  xim_init();

  for (gint i = 0; i < OBT_KEYBOARD_NUM_MODKEYS; ++i)
    modkeys_keys[i] = 0;

  if (!keyboard_build_keymap())
    g_error("Failed to initialize XKB keymap");
}

void obt_keyboard_shutdown(void) {
  GSList* it;

  keyboard_destroy_xkb();

  for (it = xic_all; it; it = g_slist_next(it)) {
    ObtIC* ic = it->data;
    if (ic->xic) {
      XDestroyIC(ic->xic);
      ic->xic = NULL;
    }
  }
  if (xim)
    XCloseIM(xim);
  xim = NULL;
  xim_style = 0;
  started = FALSE;
}

void xim_init(void) {
  GSList* it;
  gchar *aname, *aclass;

  aname = g_strdup(g_get_prgname());
  if (!aname)
    aname = g_strdup("obt");

  aclass = g_strdup(aname);
  if (g_ascii_islower(aclass[0]))
    aclass[0] = g_ascii_toupper(aclass[0]);

  xim = XOpenIM(obt_display, NULL, aname, aclass);

  if (!xim)
    g_message("Failed to open an Input Method");
  else {
    XIMStyles* xim_styles = NULL;
    char* r;

    /* get the input method styles */
    r = XGetIMValues(xim, XNQueryInputStyle, &xim_styles, NULL);
    if (r || !xim_styles)
      g_message("Input Method does not support any styles");
    if (xim_styles) {
      int i;

      /* find a style that doesnt need preedit or status */
      for (i = 0; i < xim_styles->count_styles; ++i) {
        if (xim_styles->supported_styles[i] == (XIMPreeditNothing | XIMStatusNothing)) {
          xim_style = xim_styles->supported_styles[i];
          break;
        }
      }
      XFree(xim_styles);
    }

    if (!xim_style) {
      g_message("Input Method does not support a usable style");

      XCloseIM(xim);
      xim = NULL;
    }
  }

  /* any existing contexts need to be recreated for the new input method */
  for (it = xic_all; it; it = g_slist_next(it))
    obt_keyboard_context_renew(it->data);

  g_free(aclass);
  g_free(aname);
}

guint obt_keyboard_keyevent_to_modmask(XEvent* e) {
  g_return_val_if_fail(e->type == KeyPress || e->type == KeyRelease, OBT_KEYBOARD_MODKEY_NONE);

  if (!keyboard_state)
    return 0;

  if (keyboard_last_keycode == e->xkey.keycode) {
    if ((keyboard_last_serial && keyboard_last_serial == e->xkey.serial) ||
        (!keyboard_last_serial && keyboard_last_time == e->xkey.time))
      return keyboard_last_mask;
  }
  return 0;
}

guint obt_keyboard_only_modmasks(guint mask) {
  mask &= ALL_MASKS;
  /* strip off these lock keys. they shouldn't affect key bindings */
  mask &= ~LockMask; /* use the LockMask, not what capslock is bound to,
                        because you could bind it to something else and it
                        should work as that modifier then. i think capslock
                        is weird in xkb. */
  mask &= ~obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_NUMLOCK);
  mask &= ~obt_keyboard_modkey_to_modmask(OBT_KEYBOARD_MODKEY_SCROLLLOCK);
  return mask;
}

guint obt_keyboard_modkey_to_modmask(ObtModkeysKey key) {
  return modkeys_keys[key];
}

KeyCode* obt_keyboard_keysym_to_keycode(KeySym sym) {
  KeyCode* ret = g_new(KeyCode, 1);
  guint n = 0;
  ret[0] = 0;

  if (!keyboard_keymap)
    return ret;

  xkb_keycode_t min = xkb_keymap_min_keycode(keyboard_keymap);
  xkb_keycode_t max = xkb_keymap_max_keycode(keyboard_keymap);

  for (xkb_keycode_t key = min; key <= max; ++key) {
    xkb_layout_index_t layouts = xkb_keymap_num_layouts_for_key(keyboard_keymap, key);
    gboolean matched = FALSE;
    for (xkb_layout_index_t layout = 0; layout < layouts && !matched; ++layout) {
      xkb_level_index_t levels = xkb_keymap_num_levels_for_key(keyboard_keymap, key, layout);
      for (xkb_level_index_t level = 0; level < levels; ++level) {
        const xkb_keysym_t* syms;
        gint count = xkb_keymap_key_get_syms_by_level(keyboard_keymap, key, layout, level, &syms);
        for (gint s = 0; s < count; ++s) {
          if (syms[s] == (xkb_keysym_t)sym) {
            ret = g_renew(KeyCode, ret, ++n + 1);
            ret[n - 1] = key;
            ret[n] = 0;
            matched = TRUE;
            break;
          }
        }
        if (matched)
          break;
      }
    }
  }
  return ret;
}

gunichar obt_keyboard_keypress_to_unichar(ObtIC* ic, XEvent* ev) {
  g_return_val_if_fail(ev->type == KeyPress, 0);

  if (!ic)
    g_warning(
        "Using obt_keyboard_keypress_to_unichar() without an "
        "Input Context.  No i18n support!");

  if (ic && ic->xic) {
    gunichar unikey = 0;
    KeySym sym = NoSymbol;
    Status status;
    gchar *buf, fixbuf[4];
    gint len, bufsz;
    gboolean got_string = FALSE;

    buf = fixbuf;
    bufsz = sizeof(fixbuf);

#ifdef X_HAVE_UTF8_STRING
    len = Xutf8LookupString(ic->xic, &ev->xkey, buf, bufsz, &sym, &status);
#else
    len = XmbLookupString(ic->xic, &ev->xkey, buf, bufsz, &sym, &status);
#endif

    if (status == XBufferOverflow) {
      buf = g_new(char, len);
      bufsz = len;

#ifdef X_HAVE_UTF8_STRING
      len = Xutf8LookupString(ic->xic, &ev->xkey, buf, bufsz, &sym, &status);
#else
      len = XmbLookupString(ic->xic, &ev->xkey, buf, bufsz, &sym, &status);
#endif
    }

    if ((status == XLookupChars || status == XLookupBoth)) {
      if ((guchar)buf[0] >= 32) {
#ifndef X_HAVE_UTF8_STRING
        gchar* buf2 = buf;
        buf = g_locale_to_utf8(buf2, len, NULL, NULL, NULL);
        g_free(buf2);
#endif
        got_string = TRUE;
      }
    }
    else if (status != XLookupKeySym)
      g_message("Bad keycode lookup. Keysym 0x%x Status: %s\n", (guint)sym,
                (status == XBufferOverflow ? "BufferOverflow"
                 : status == XLookupNone   ? "XLookupNone"
                 : status == XLookupKeySym ? "XLookupKeySym"
                                           : "Unknown status"));

    if (got_string) {
      gunichar u = g_utf8_get_char_validated(buf, len);
      if (u && u != (gunichar)-1 && u != (gunichar)-2)
        unikey = u;
    }

    if (buf != fixbuf)
      g_free(buf);

    return unikey;
  }

  if (!keyboard_state)
    return 0;

  KeySym sym = obt_keyboard_keypress_to_keysym(ev);
  if (sym == NoSymbol)
    return 0;

  if (keyboard_compose_state) {
    enum xkb_compose_feed_result feed = xkb_compose_state_feed(keyboard_compose_state, (xkb_keysym_t)sym);
    if (feed == XKB_COMPOSE_FEED_ACCEPTED) {
      switch (xkb_compose_state_get_status(keyboard_compose_state)) {
        case XKB_COMPOSE_COMPOSING:
          return 0;
        case XKB_COMPOSE_COMPOSED: {
          char composed[16];
          int size = xkb_compose_state_get_utf8(keyboard_compose_state, composed, sizeof(composed));
          xkb_compose_state_reset(keyboard_compose_state);
          if (size > 0) {
            gunichar u = g_utf8_get_char_validated(composed, size);
            if (u && u != (gunichar)-1 && u != (gunichar)-2)
              return u;
          }
          break;
        }
        case XKB_COMPOSE_CANCELLED:
          xkb_compose_state_reset(keyboard_compose_state);
          return 0;
        case XKB_COMPOSE_NOTHING:
          break;
      }
    }
  }

  gunichar u = (gunichar)xkb_keysym_to_utf32((xkb_keysym_t)sym);
  if (u && u != (gunichar)-1 && u != (gunichar)-2)
    return u;
  return 0;
}

KeySym obt_keyboard_keypress_to_keysym(XEvent* ev) {
  g_return_val_if_fail(ev->type == KeyPress, None);

  if (!keyboard_state)
    return None;
  return (KeySym)xkb_state_key_get_one_sym(keyboard_state, ev->xkey.keycode);
}

void obt_keyboard_handle_event(const XEvent* e) {
  if (!keyboard_state || (e->type != KeyPress && e->type != KeyRelease))
    return;

  enum xkb_key_direction dir = (e->type == KeyPress) ? XKB_KEY_DOWN : XKB_KEY_UP;
  xkb_mod_mask_t before = xkb_state_serialize_mods(keyboard_state, XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);
  xkb_state_update_key(keyboard_state, e->xkey.keycode, dir);
  xkb_mod_mask_t after = xkb_state_serialize_mods(keyboard_state, XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);
  xkb_mod_mask_t delta = (dir == XKB_KEY_DOWN) ? (after & ~before) : (before & ~after);

  keyboard_last_mask = (guint)(delta & ALL_MASKS);
  keyboard_last_serial = e->xkey.serial;
  keyboard_last_keycode = e->xkey.keycode;
  keyboard_last_time = e->xkey.time;
}

gboolean obt_keyboard_handle_xkb_event(const XkbAnyEvent* e) {
  if (!obt_display_extension_xkb)
    return FALSE;

  switch (e->xkb_type) {
    case XkbStateNotify: {
      const XkbStateNotifyEvent* sn = (const XkbStateNotifyEvent*)e;
      if (keyboard_state)
        xkb_state_update_mask(keyboard_state, sn->base_mods, sn->latched_mods, sn->locked_mods, sn->base_group,
                              sn->latched_group, sn->locked_group);
      return FALSE;
    }
    case XkbNewKeyboardNotify:
    case XkbMapNotify:
      keyboard_build_keymap();
      return TRUE;
    default:
      break;
  }

  return FALSE;
}

void obt_keyboard_context_renew(ObtIC* ic) {
  if (xim) {
    ic->xic = XCreateIC(xim, XNInputStyle, xim_style, XNClientWindow, ic->client, XNFocusWindow, ic->focus, NULL);
    if (!ic->xic)
      g_message("Error creating Input Context for window 0x%x 0x%x\n", (guint)ic->client, (guint)ic->focus);
  }
}

ObtIC* obt_keyboard_context_new(Window client, Window focus) {
  ObtIC* ic;

  g_return_val_if_fail(client != None && focus != None, NULL);

  ic = g_slice_new(ObtIC);
  ic->ref = 1;
  ic->client = client;
  ic->focus = focus;
  ic->xic = NULL;

  obt_keyboard_context_renew(ic);
  xic_all = g_slist_prepend(xic_all, ic);

  return ic;
}

void obt_keyboard_context_ref(ObtIC* ic) {
  ++ic->ref;
}

void obt_keyboard_context_unref(ObtIC* ic) {
  if (--ic->ref < 1) {
    xic_all = g_slist_remove(xic_all, ic);
    if (ic->xic)
      XDestroyIC(ic->xic);
    g_slice_free(ObtIC, ic);
  }
}

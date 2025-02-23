/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/prop.c for the Openbox window manager
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

#include "obt/prop.h"
#include "obt/display.h"

#include <X11/Xatom.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#endif

Atom prop_atoms[OBT_PROP_NUM_ATOMS];
gboolean prop_started = FALSE;

#define CREATE_NAME(var, name) (prop_atoms[OBT_PROP_##var] = \
                                XInternAtom((obt_display), (name), FALSE))
#define CREATE(var) CREATE_NAME(var, #var)
#define CREATE_(var) CREATE_NAME(var, "_" #var)

void obt_prop_startup(void)
{
    if (prop_started) return;
    prop_started = TRUE;

    g_assert(obt_display);

    CREATE(CARDINAL);
    CREATE(WINDOW);
    CREATE(PIXMAP);
    CREATE(ATOM);
    CREATE(STRING);
    CREATE(COMPOUND_TEXT);
    CREATE(UTF8_STRING);

    CREATE(MANAGER);

    CREATE(WM_COLORMAP_WINDOWS);
    CREATE(WM_PROTOCOLS);
    CREATE(WM_STATE);
    CREATE(WM_CHANGE_STATE);
    CREATE(WM_DELETE_WINDOW);
    CREATE(WM_TAKE_FOCUS);
    CREATE(WM_NAME);
    CREATE(WM_ICON_NAME);
    CREATE(WM_CLASS);
    CREATE(WM_WINDOW_ROLE);
    CREATE(WM_CLIENT_MACHINE);
    CREATE(WM_COMMAND);
    CREATE(WM_CLIENT_LEADER);
    CREATE(WM_TRANSIENT_FOR);
    CREATE_(MOTIF_WM_HINTS);
    CREATE_(MOTIF_WM_INFO);

    CREATE(SM_CLIENT_ID);

    CREATE_(NET_WM_FULL_PLACEMENT);

    CREATE_(NET_SUPPORTED);
    CREATE_(NET_CLIENT_LIST);
    CREATE_(NET_CLIENT_LIST_STACKING);
    CREATE_(NET_NUMBER_OF_DESKTOPS);
    CREATE_(NET_DESKTOP_GEOMETRY);
    CREATE_(NET_DESKTOP_VIEWPORT);
    CREATE_(NET_CURRENT_DESKTOP);
    CREATE_(NET_DESKTOP_NAMES);
    CREATE_(NET_ACTIVE_WINDOW);
/*    CREATE_(NET_RESTACK_WINDOW);*/
    CREATE_(NET_WORKAREA);
    CREATE_(NET_SUPPORTING_WM_CHECK);
    CREATE_(NET_DESKTOP_LAYOUT);
    CREATE_(NET_SHOWING_DESKTOP);

    CREATE_(NET_CLOSE_WINDOW);
    CREATE_(NET_WM_MOVERESIZE);
    CREATE_(NET_MOVERESIZE_WINDOW);
    CREATE_(NET_REQUEST_FRAME_EXTENTS);
    CREATE_(NET_RESTACK_WINDOW);

    CREATE_(NET_STARTUP_ID);

    CREATE_(NET_WM_NAME);
    CREATE_(NET_WM_VISIBLE_NAME);
    CREATE_(NET_WM_ICON_NAME);
    CREATE_(NET_WM_VISIBLE_ICON_NAME);
    CREATE_(NET_WM_DESKTOP);
    CREATE_(NET_WM_WINDOW_TYPE);
    CREATE_(NET_WM_STATE);
    CREATE_(NET_WM_STRUT);
    CREATE_(NET_WM_STRUT_PARTIAL);
    CREATE_(NET_WM_ICON);
    CREATE_(NET_WM_ICON_GEOMETRY);
    CREATE_(NET_WM_PID);
    CREATE_(NET_WM_ALLOWED_ACTIONS);
    CREATE_(NET_WM_WINDOW_OPACITY);
    CREATE_(NET_WM_USER_TIME);
/*  CREATE_(NET_WM_USER_TIME_WINDOW); */
    CREATE_(KDE_NET_WM_FRAME_STRUT);
    CREATE_(NET_FRAME_EXTENTS);

    CREATE_(NET_WM_PING);
#ifdef SYNC
    CREATE_(NET_WM_SYNC_REQUEST);
    CREATE_(NET_WM_SYNC_REQUEST_COUNTER);
#endif

    CREATE_(NET_WM_WINDOW_TYPE_DESKTOP);
    CREATE_(NET_WM_WINDOW_TYPE_DOCK);
    CREATE_(NET_WM_WINDOW_TYPE_TOOLBAR);
    CREATE_(NET_WM_WINDOW_TYPE_MENU);
    CREATE_(NET_WM_WINDOW_TYPE_UTILITY);
    CREATE_(NET_WM_WINDOW_TYPE_SPLASH);
    CREATE_(NET_WM_WINDOW_TYPE_DIALOG);
    CREATE_(NET_WM_WINDOW_TYPE_NORMAL);
    CREATE_(NET_WM_WINDOW_TYPE_POPUP_MENU);

    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_SIZE_TOPLEFT] = 0;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_SIZE_TOP] = 1;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_SIZE_TOPRIGHT] = 2;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_SIZE_RIGHT] = 3;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT] = 4;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_SIZE_BOTTOM] = 5;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT] = 6;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_SIZE_LEFT] = 7;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_MOVE] = 8;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_SIZE_KEYBOARD] = 9;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_MOVE_KEYBOARD] = 10;
    prop_atoms[OBT_PROP_NET_WM_MOVERESIZE_CANCEL] = 11;

    CREATE_(NET_WM_ACTION_MOVE);
    CREATE_(NET_WM_ACTION_RESIZE);
    CREATE_(NET_WM_ACTION_MINIMIZE);
    CREATE_(NET_WM_ACTION_SHADE);
    CREATE_(NET_WM_ACTION_MAXIMIZE_HORZ);
    CREATE_(NET_WM_ACTION_MAXIMIZE_VERT);
    CREATE_(NET_WM_ACTION_FULLSCREEN);
    CREATE_(NET_WM_ACTION_CHANGE_DESKTOP);
    CREATE_(NET_WM_ACTION_CLOSE);
    CREATE_(NET_WM_ACTION_ABOVE);
    CREATE_(NET_WM_ACTION_BELOW);

    CREATE_(NET_WM_STATE_MODAL);
/*    CREATE_(NET_WM_STATE_STICKY);*/
    CREATE_(NET_WM_STATE_MAXIMIZED_VERT);
    CREATE_(NET_WM_STATE_MAXIMIZED_HORZ);
    CREATE_(NET_WM_STATE_SHADED);
    CREATE_(NET_WM_STATE_SKIP_TASKBAR);
    CREATE_(NET_WM_STATE_SKIP_PAGER);
    CREATE_(NET_WM_STATE_HIDDEN);
    CREATE_(NET_WM_STATE_FULLSCREEN);
    CREATE_(NET_WM_STATE_ABOVE);
    CREATE_(NET_WM_STATE_BELOW);
    CREATE_(NET_WM_STATE_DEMANDS_ATTENTION);

    prop_atoms[OBT_PROP_NET_WM_STATE_ADD] = 1;
    prop_atoms[OBT_PROP_NET_WM_STATE_REMOVE] = 0;
    prop_atoms[OBT_PROP_NET_WM_STATE_TOGGLE] = 2;

    prop_atoms[OBT_PROP_NET_WM_ORIENTATION_HORZ] = 0;
    prop_atoms[OBT_PROP_NET_WM_ORIENTATION_VERT] = 1;
    prop_atoms[OBT_PROP_NET_WM_TOPLEFT] = 0;
    prop_atoms[OBT_PROP_NET_WM_TOPRIGHT] = 1;
    prop_atoms[OBT_PROP_NET_WM_BOTTOMRIGHT] = 2;
    prop_atoms[OBT_PROP_NET_WM_BOTTOMLEFT] = 3;

    CREATE_(KDE_WM_CHANGE_STATE);
    CREATE_(KDE_NET_WM_WINDOW_TYPE_OVERRIDE);

/*
    CREATE_NAME(ROOTPMAPId, "_XROOTPMAP_ID");
    CREATE_NAME(ESETROOTId, "ESETROOT_PMAP_ID");
*/

    CREATE_(OPENBOX_PID);
    CREATE_(OB_THEME);
    CREATE_(OB_CONFIG_FILE);
    CREATE_(OB_WM_ACTION_UNDECORATE);
    CREATE_(OB_WM_STATE_UNDECORATED);
    CREATE_(OB_CONTROL);
    CREATE_(OB_VERSION);
    CREATE_(OB_APP_ROLE);
    CREATE_(OB_APP_TITLE);
    CREATE_(OB_APP_NAME);
    CREATE_(OB_APP_CLASS);
    CREATE_(OB_APP_GROUP_NAME);
    CREATE_(OB_APP_GROUP_CLASS);
    CREATE_(OB_APP_TYPE);
}

Atom obt_prop_atom(ObtPropAtom a)
{
    g_assert(prop_started);
    g_assert(a < OBT_PROP_NUM_ATOMS);
    return prop_atoms[a];
}

static gboolean get_prealloc(Window win, Atom prop, Atom type, gint size,
                             guchar *data, gulong num)
{
    gboolean ret = FALSE;
    gint res;
    guchar *xdata = NULL;
    Atom ret_type;
    gint ret_size;
    gulong ret_items, bytes_left;
    glong num32 = 32 / size * num; /* num in 32-bit elements */

    res = XGetWindowProperty(obt_display, win, prop, 0l, num32,
                             FALSE, type, &ret_type, &ret_size,
                             &ret_items, &bytes_left, &xdata);
    if (res == Success && ret_items && xdata) {
        if (ret_size == size && ret_items >= num) {
            guint i;
            for (i = 0; i < num; ++i)
                switch (size) {
                case 8:
                    data[i] = xdata[i];
                    break;
                case 16:
                    ((guint16*)data)[i] = ((gushort*)xdata)[i];
                    break;
                case 32:
                    ((guint32*)data)[i] = ((gulong*)xdata)[i];
                    break;
                default:
                    g_assert_not_reached(); /* unhandled size */
                }
            ret = TRUE;
        }
        XFree(xdata);
    }
    return ret;
}

static gboolean get_all(Window win, Atom prop, Atom type, gint size,
                        guchar **data, guint *num)
{
    gboolean ret = FALSE;
    gint res;
    guchar *xdata = NULL;
    Atom ret_type;
    gint ret_size;
    gulong ret_items, bytes_left;

    res = XGetWindowProperty(obt_display, win, prop, 0l, G_MAXLONG,
                             FALSE, type, &ret_type, &ret_size,
                             &ret_items, &bytes_left, &xdata);
    if (res == Success) {
        if (ret_size == size && ret_items > 0) {
            guint i;

            *data = g_malloc(ret_items * (size / 8));
            for (i = 0; i < ret_items; ++i)
                switch (size) {
                case 8:
                    (*data)[i] = xdata[i];
                    break;
                case 16:
                    ((guint16*)*data)[i] = ((gushort*)xdata)[i];
                    break;
                case 32:
                    ((guint32*)*data)[i] = ((gulong*)xdata)[i];
                    break;
                default:
                    g_assert_not_reached(); /* unhandled size */
                }
            *num = ret_items;
            ret = TRUE;
        }
        XFree(xdata);
    }
    return ret;
}

/*! Get a text property from a window, and fill out the XTextProperty with it.
  @param win The window to read the property from.
  @param prop The atom of the property to read off the window.
  @param tprop The XTextProperty to fill out.
  @param type 0 to get text of any type, or a value from
    ObtPropTextType to restrict the value to a specific type.
  @return TRUE if the text was read and validated against the @type, and FALSE
    otherwise.
*/
static gboolean get_text_property(Window win, Atom prop,
                                  XTextProperty *tprop, ObtPropTextType type)
{
    if (!(XGetTextProperty(obt_display, win, tprop, prop) && tprop->nitems))
        return FALSE;
    if (!type)
        return TRUE; /* no type checking */
    switch (type) {
    case OBT_PROP_TEXT_STRING:
    case OBT_PROP_TEXT_STRING_XPCS:
    case OBT_PROP_TEXT_STRING_NO_CC:
        return tprop->encoding == OBT_PROP_ATOM(STRING);
    case OBT_PROP_TEXT_COMPOUND_TEXT:
        return tprop->encoding == OBT_PROP_ATOM(COMPOUND_TEXT);
    case OBT_PROP_TEXT_UTF8_STRING:
        return tprop->encoding == OBT_PROP_ATOM(UTF8_STRING);
    default:
        g_assert_not_reached();
        return FALSE;
    }
}

static gchar* convert_string_to_utf8(const gchar *input, gboolean *valid) {
    // Convert a string to UTF-8 and validate it
    const gchar *end;
    g_utf8_validate(input, -1, &end);
    *valid = (input == end) ? FALSE : TRUE;
    return g_strndup(input, end - input);
}

static gchar* convert_locale_to_utf8(const gchar *input) {
    // Convert a locale-encoded string to UTF-8
    gsize nvalid;
    gchar *utf = g_locale_to_utf8(input, -1, &nvalid, NULL, NULL);
    if (!utf) {
        utf = g_locale_to_utf8(input, nvalid, NULL, NULL, NULL);
    }
    g_assert(utf);
    return utf;
}

static gchar* convert_latin1_to_utf8(const gchar *input, ObtPropTextType type) {
    gsize nvalid = 0;
    gchar *utf;
    gchar *p = (gchar*)input;

    while (*p) {
        const guchar c = (guchar)*p;  // unsigned value at p
        // Look for invalid characters based on type
        if ((c < 32 && c != 9 && c != 10) || (c >= 127 && c <= 160)) {
            break; // Found an invalid control character
        }

        if (type == OBT_PROP_TEXT_STRING_NO_CC && c < 32) {
            break; // No control characters allowed
        }

        if (type == OBT_PROP_TEXT_STRING_XPCS) {
            if (!((c >= 32 && c < 128) || c == 9 || c == 10)) {
                break; // Strict whitelisting for XPCS
            }
        }
        ++p;
        ++nvalid;
    }

    // Convert from Latin1 to UTF-8
    utf = g_convert(input, nvalid, "utf-8", "iso-8859-1", &nvalid, NULL, NULL);
    if (!utf) {
        utf = g_convert(input, nvalid, "utf-8", "iso-8859-1", NULL, NULL, NULL);
    }
    g_assert(utf);
    return utf;
}

static void* convert_text_property(XTextProperty *tprop, ObtPropTextType type, gint max) {
    gboolean ok = FALSE;
    gchar **strlist = NULL;
    gchar **retlist = NULL;
    gint i, n_strs;

    // Direct integer value for OBT_PROP_ATOM(UTF8_STRING) and OBT_PROP_ATOM(STRING)
    const Atom UTF8_STRING = OBT_PROP_ATOM(UTF8_STRING);
    const Atom STRING = OBT_PROP_ATOM(STRING);

    // Check for encoding and process based on type
    if (tprop->encoding == OBT_PROP_ATOM(COMPOUND_TEXT)) {
        ok = (XmbTextPropertyToTextList(obt_display, tprop, &strlist, &n_strs) == Success);
        if (ok) {
            n_strs = (max >= 0) ? MIN(max, n_strs) : n_strs;
            retlist = g_new0(gchar*, n_strs + 1);
            for (i = 0; i < n_strs; ++i) {
                retlist[i] = convert_locale_to_utf8(strlist[i]);
            }
        }
    }
    else if (tprop->encoding == UTF8_STRING || tprop->encoding == STRING) {
        gchar *p = (gchar*)tprop->value;
        n_strs = 0;
        while (p < (gchar*)tprop->value + tprop->nitems) {
            p += strlen(p) + 1;  // Move to the next string
            ++n_strs;
        }

        n_strs = (max >= 0) ? MIN(max, n_strs) : n_strs;
        retlist = g_new0(gchar*, n_strs + 1);

        p = (gchar*)tprop->value;
        for (i = 0; i < n_strs; ++i) {
            if (tprop->encoding == UTF8_STRING) {
                retlist[i] = convert_string_to_utf8(p, &ok);
            } else if (tprop->encoding == STRING) {
                retlist[i] = convert_latin1_to_utf8(p, type);
            } else {
                ok = FALSE;
            }
            p += strlen(p) + 1;
        }
    }

    if (!ok || !retlist) {
        if (strlist) XFreeStringList(strlist);
        return NULL;
    }

    if (strlist) XFreeStringList(strlist);
    if (max == 1) {
        return retlist[0];
    } else {
        return retlist;
    }
}

gboolean obt_prop_get32(Window win, Atom prop, Atom type, guint32 *ret)
{
    return get_prealloc(win, prop, type, 32, (guchar*)ret, 1);
}

gboolean obt_prop_get_array32(Window win, Atom prop, Atom type, guint32 **ret,
                              guint *nret)
{
    return get_all(win, prop, type, 32, (guchar**)ret, nret);
}

gboolean obt_prop_get_text(Window win, Atom prop, ObtPropTextType type,
                           gchar **ret_string)
{
    XTextProperty tprop;
    gchar *str;
    gboolean ret = FALSE;

    if (get_text_property(win, prop, &tprop, type)) {
        str = (gchar*)convert_text_property(&tprop, type, 1);

        if (str) {
            *ret_string = str;
            ret = TRUE;
        }
    }
    XFree(tprop.value);
    return ret;
}

gboolean obt_prop_get_array_text(Window win, Atom prop,
                                 ObtPropTextType type,
                                 gchar ***ret_strings)
{
    XTextProperty tprop;
    gchar **strs;
    gboolean ret = FALSE;

    if (get_text_property(win, prop, &tprop, type)) {
        strs = (gchar**)convert_text_property(&tprop, type, -1);

        if (strs) {
            *ret_strings = strs;
            ret = TRUE;
        }
    }
    XFree(tprop.value);
    return ret;
}

void obt_prop_set32(Window win, Atom prop, Atom type, gulong val)
{
    XChangeProperty(obt_display, win, prop, type, 32, PropModeReplace,
                    (guchar*)&val, 1);
}

void obt_prop_set_array32(Window win, Atom prop, Atom type, gulong *val,
                      guint num)
{
    XChangeProperty(obt_display, win, prop, type, 32, PropModeReplace,
                    (guchar*)val, num);
}

void obt_prop_set_text(Window win, Atom prop, const gchar *val)
{
    XChangeProperty(obt_display, win, prop, OBT_PROP_ATOM(UTF8_STRING), 8,
                    PropModeReplace, (const guchar*)val, strlen(val));
}

void obt_prop_set_array_text(Window win, Atom prop, const gchar *const *strs)
{
    GString *str;
    gchar const *const *s;

    str = g_string_sized_new(0);
    for (s = strs; *s; ++s) {
        str = g_string_append(str, *s);
        str = g_string_append_c(str, '\0');
    }
    XChangeProperty(obt_display, win, prop, OBT_PROP_ATOM(UTF8_STRING), 8,
                    PropModeReplace, (guchar*)str->str, str->len);
    g_string_free(str, TRUE);
}

void obt_prop_erase(Window win, Atom prop)
{
    XDeleteProperty(obt_display, win, prop);
}

void obt_prop_message(gint screen, Window about, Atom messagetype,
                      glong data0, glong data1, glong data2, glong data3,
                      glong data4, glong mask)
{
    obt_prop_message_to(obt_root(screen), about, messagetype,
                        data0, data1, data2, data3, data4, mask);
}

void obt_prop_message_to(Window to, Window about,
                         Atom messagetype,
                         glong data0, glong data1, glong data2, glong data3,
                         glong data4, glong mask)
{
    XEvent ce;
    ce.xclient.type = ClientMessage;
    ce.xclient.message_type = messagetype;
    ce.xclient.display = obt_display;
    ce.xclient.window = about;
    ce.xclient.format = 32;
    ce.xclient.data.l[0] = data0;
    ce.xclient.data.l[1] = data1;
    ce.xclient.data.l[2] = data2;
    ce.xclient.data.l[3] = data3;
    ce.xclient.data.l[4] = data4;
    XSendEvent(obt_display, to, FALSE, mask, &ce);
}

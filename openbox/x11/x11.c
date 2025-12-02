/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   openbox/x11/x11.c for the Openbox window manager
   Copyright (c) 2024      Openbox

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

#include "openbox/x11/x11.h"

#include "obt/display.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdlib.h>
#include <stdint.h>

#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>

static GHashTable* atom_cache = NULL;
static GHashTable* atom_cache_optional = NULL;

static inline gpointer atom_to_pointer(Atom atom) {
  return (gpointer)(guintptr)atom;
}

static inline Atom pointer_to_atom(gconstpointer ptr) {
  return (Atom)(guintptr)ptr;
}

static GHashTable** cache_for_mode(gboolean only_if_exists) {
  return only_if_exists ? &atom_cache_optional : &atom_cache;
}

static inline xcb_connection_t* ob_x11_connection(void) {
  g_return_val_if_fail(obt_display != NULL, NULL);
  return XGetXCBConnection(obt_display);
}

static Atom ob_x11_atom_lookup(const char* name, gboolean only_if_exists) {
  GHashTable** cache = cache_for_mode(only_if_exists);
  gpointer cached;
  Atom atom;

  g_return_val_if_fail(name != NULL, None);
  g_return_val_if_fail(obt_display != NULL, None);

  if (!*cache)
    *cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  cached = g_hash_table_lookup(*cache, name);
  if (cached)
    return pointer_to_atom(cached);

  xcb_connection_t* conn = ob_x11_connection();
  if (!conn)
    return None;

  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_if_exists, strlen(name), name);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  atom = reply ? reply->atom : None;
  free(reply);

  if (atom != None)
    g_hash_table_insert(*cache, g_strdup(name), atom_to_pointer(atom));

  return atom;
}

Atom ob_x11_atom(const char* name) {
  return ob_x11_atom_lookup(name, FALSE);
}

Atom ob_x11_atom_if_exists(const char* name) {
  return ob_x11_atom_lookup(name, TRUE);
}

static gulong max_items_for_format(gint format) {
  const gulong element_bytes = format / 8;

  g_return_val_if_fail(element_bytes, 0);
  return OB_X11_MAX_PROP_BYTES / element_bytes;
}

static glong request_length(gulong items, gint format) {
  const gulong bits = items * format;
  const gulong request_32 = (bits + 31) / 32; /* number of 32-bit multiples */

  g_assert(request_32 <= G_MAXLONG);
  return (glong)request_32;
}

gboolean ob_x11_get_property(Window win,
                             Atom prop,
                             Atom type,
                             gint format,
                             gulong min_items,
                             gulong max_items,
                             ObX11PropertyValue* value) {
  gulong ret_items, bytes_left;
  gulong allowed_max;
  gboolean ret = FALSE;
  glong request;
  gsize element_bytes;
  gsize total_bytes;
  const guchar* xdata = NULL;
  xcb_connection_t* conn;
  xcb_get_property_cookie_t cookie;
  xcb_get_property_reply_t* reply = NULL;
  gsize value_len_bytes;
  uint32_t long_length;

  g_return_val_if_fail(value != NULL, FALSE);
  memset(value, 0, sizeof(*value));

  g_return_val_if_fail(obt_display != NULL, FALSE);
  g_return_val_if_fail(format == 8 || format == 16 || format == 32, FALSE);

  element_bytes = format / 8;
  allowed_max = max_items ? MIN(max_items, max_items_for_format(format)) : max_items_for_format(format);
  if (min_items > allowed_max)
    return FALSE;

  request = request_length(allowed_max, format);
  long_length = (uint32_t)request;
  conn = ob_x11_connection();
  if (!conn)
    return FALSE;

  cookie = xcb_get_property(conn, FALSE, win, prop, type ? type : XCB_GET_PROPERTY_TYPE_ANY, 0, long_length);
  reply = xcb_get_property_reply(conn, cookie, NULL);
  if (!reply)
    goto done;

  if (type != AnyPropertyType && reply->type != type)
    goto done;

  bytes_left = reply->bytes_after;
  value_len_bytes = xcb_get_property_value_length(reply);

  if (!value_len_bytes && min_items > 0)
    goto done;

  if (reply->format != format)
    goto done;
  if (bytes_left)
    goto done;
  if (element_bytes == 0 || (value_len_bytes % element_bytes))
    goto done;

  ret_items = value_len_bytes / element_bytes;
  if (ret_items < min_items || ret_items > allowed_max)
    goto done;

  total_bytes = ret_items * element_bytes;
  xdata = xcb_get_property_value(reply);
  if (total_bytes && xdata)
    value->data = g_memdup2(xdata, total_bytes);

  value->n_items = ret_items;
  value->format = format;
  value->type = reply->type;
  ret = TRUE;

done:
  free(reply);
  if (!ret)
    ob_x11_property_value_clear(value);
  return ret;
}

void ob_x11_property_value_clear(ObX11PropertyValue* value) {
  if (!value)
    return;
  g_free(value->data);
  value->data = NULL;
  value->n_items = 0;
  value->format = 0;
  value->type = None;
}

gboolean ob_x11_property_to_string_list(const ObX11PropertyValue* value,
                                        gsize min_strings,
                                        gsize max_strings,
                                        ObX11StringList* list) {
  GPtrArray* arr;
  const gchar* data;
  gsize offset = 0;

  g_return_val_if_fail(value != NULL, FALSE);
  g_return_val_if_fail(list != NULL, FALSE);
  g_return_val_if_fail(value->format == 8, FALSE);
  g_return_val_if_fail(max_strings == 0 || max_strings >= min_strings, FALSE);

  memset(list, 0, sizeof(*list));

  if (!value->n_items || !value->data) {
    if (min_strings == 0)
      return TRUE;
    return FALSE;
  }

  arr = g_ptr_array_new();
  data = (const gchar*)value->data;

  while (offset < value->n_items) {
    gsize remaining = value->n_items - offset;
    const gchar* start = data + offset;
    const gchar* end = memchr(start, '\0', remaining);
    gsize len;

    if (!end)
      goto fail;

    len = end - start;
    if (len == 0 && offset + 1 == value->n_items) {
      /* trailing NUL terminator, do not treat it as a string */
      offset = value->n_items;
      break;
    }

    if (max_strings && arr->len >= max_strings)
      goto fail;

    g_ptr_array_add(arr, g_strndup(start, len));
    offset += len + 1;
  }

  if (arr->len < min_strings)
    goto fail;

  list->n_items = arr->len;
  g_ptr_array_add(arr, NULL);
  list->items = (gchar**)g_ptr_array_free(arr, FALSE);
  return TRUE;

fail:
  for (guint i = 0; i < arr->len; ++i)
    g_free(arr->pdata[i]);
  g_ptr_array_free(arr, TRUE);
  list->items = NULL;
  list->n_items = 0;
  return FALSE;
}

void ob_x11_string_list_clear(ObX11StringList* list) {
  if (!list)
    return;
  if (list->items)
    g_strfreev(list->items);
  list->items = NULL;
  list->n_items = 0;
}

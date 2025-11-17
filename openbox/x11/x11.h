/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   openbox/x11/x11.h for the Openbox window manager
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

#ifndef __openbox_x11_h
#define __openbox_x11_h

#include <X11/Xlib.h>
#include <glib.h>

G_BEGIN_DECLS

#define OB_X11_MAX_PROP_BYTES (8 * 1024 * 1024)

typedef struct {
  guchar* data;
  gulong n_items;
  gint format;
} ObX11PropertyValue;

typedef struct {
  gchar** items;
  gsize n_items;
} ObX11StringList;

Atom ob_x11_atom(const char* name);
Atom ob_x11_atom_if_exists(const char* name);

gboolean ob_x11_get_property(Window win,
                             Atom prop,
                             Atom type,
                             gint format,
                             gulong min_items,
                             gulong max_items,
                             ObX11PropertyValue* value);
void ob_x11_property_value_clear(ObX11PropertyValue* value);

gboolean ob_x11_property_to_string_list(const ObX11PropertyValue* value,
                                        gsize min_strings,
                                        gsize max_strings,
                                        ObX11StringList* list);
void ob_x11_string_list_clear(ObX11StringList* list);

G_END_DECLS

#endif /* __openbox_x11_h */

/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/obtyaml.h for the Openbox window manager
   Copyright (c) 2024        Openbox Contributors

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

#ifndef __obt_obtyaml_h
#define __obt_obtyaml_h

#include <glib.h>

G_BEGIN_DECLS

#ifdef HAVE_YAML
gboolean obt_yaml_to_xml(const gchar* yaml_path, const gchar* root_node, gchar** xml_out, gsize* len_out);
#endif

G_END_DECLS

#endif

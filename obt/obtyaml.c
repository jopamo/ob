/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/yaml.c for the Openbox window manager
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

#include "obt/obtyaml.h"
#ifdef HAVE_YAML

#include <yaml.h>
#include <string.h>
#include <stdio.h>

/* Helper to escape XML entities */
static void xml_escape(GString* out, const gchar* str) {
  while (*str) {
    switch (*str) {
      case '<':
        g_string_append(out, "&lt;");
        break;
      case '>':
        g_string_append(out, "&gt;");
        break;
      case '&':
        g_string_append(out, "&amp;");
        break;
      case '"':
        g_string_append(out, "&quot;");
        break;
      case '\'':
        g_string_append(out, "&apos;");
        break;
      default:
        g_string_append_c(out, *str);
        break;
    }
    str++;
  }
}

/* Check if a key should be treated as an XML attribute for a given tag */
static gboolean is_attribute(const gchar* tag, const gchar* key) {
  if (!tag || !key)
    return FALSE;

  if (strcmp(tag, "font") == 0) {
    return strcmp(key, "place") == 0;
  }
  if (strcmp(tag, "action") == 0) {
    return strcmp(key, "name") == 0;
  }
  if (strcmp(tag, "keybind") == 0) {
    return strcmp(key, "key") == 0 || strcmp(key, "chroot") == 0;
  }
  if (strcmp(tag, "mousebind") == 0) {
    return strcmp(key, "action") == 0 || strcmp(key, "button") == 0;
  }
  if (strcmp(tag, "context") == 0) {
    return strcmp(key, "name") == 0;
  }
  if (strcmp(tag, "window_position") == 0) {
    return strcmp(key, "force") == 0;
  }
  if (strcmp(tag, "application") == 0) {
    return strcmp(key, "role") == 0 || strcmp(key, "title") == 0 || strcmp(key, "type") == 0 ||
           strcmp(key, "name") == 0 || strcmp(key, "class") == 0;
  }

  /* Menu specific */
  if (strcmp(tag, "menu") == 0) {
    return strcmp(key, "label") == 0 || strcmp(key, "execute") == 0 || strcmp(key, "id") == 0;
  }
  if (strcmp(tag, "item") == 0) {
    return strcmp(key, "label") == 0 || strcmp(key, "icon") == 0;
  }
  if (strcmp(tag, "separator") == 0) {
    return strcmp(key, "label") == 0;
  }

  return FALSE;
}

static void process_node(yaml_document_t* doc, yaml_node_t* node, const gchar* tag_name, GString* out);

static void process_mapping(yaml_document_t* doc, yaml_node_t* node, const gchar* tag_name, GString* out) {
  yaml_node_pair_t* pair;
  GList* attrs = NULL;
  GList* children = NULL;
  GList* l;

  /* First pass: separate attributes and children */
  for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key_node = yaml_document_get_node(doc, pair->key);
    yaml_node_t* value_node = yaml_document_get_node(doc, pair->value);

    if (!key_node || key_node->type != YAML_SCALAR_NODE)
      continue;

    gchar* key_str = (gchar*)key_node->data.scalar.value;

    if (is_attribute(tag_name, key_str)) {
      attrs = g_list_append(attrs, pair);
    }
    else {
      children = g_list_append(children, pair);
    }
  }

  /* Start tag */
  g_string_append_printf(out, "<%s", tag_name);

  /* Write attributes */
  for (l = attrs; l; l = l->next) {
    yaml_node_pair_t* p = l->data;
    yaml_node_t* k = yaml_document_get_node(doc, p->key);
    yaml_node_t* v = yaml_document_get_node(doc, p->value);

    gchar* key_s = (gchar*)k->data.scalar.value;

    g_string_append_printf(out, " %s=\"", key_s);
    if (v->type == YAML_SCALAR_NODE) {
      xml_escape(out, (gchar*)v->data.scalar.value);
    }
    g_string_append(out, "\"");
  }

  if (!children) {
    g_string_append(out, "/>");
  }
  else {
    g_string_append(out, ">");

    /* Write children */
    for (l = children; l; l = l->next) {
      yaml_node_pair_t* p = l->data;
      yaml_node_t* k = yaml_document_get_node(doc, p->key);
      yaml_node_t* v = yaml_document_get_node(doc, p->value);

      gchar* key_s = (gchar*)k->data.scalar.value;

      if (strcmp(key_s, "_content") == 0 || strcmp(key_s, "content") == 0) {
        if (v->type == YAML_SCALAR_NODE) {
          xml_escape(out, (gchar*)v->data.scalar.value);
        }
      }
      else {
        process_node(doc, v, key_s, out);
      }
    }

    g_string_append_printf(out, "</%s>", tag_name);
  }

  g_list_free(attrs);
  g_list_free(children);
}

static void process_node(yaml_document_t* doc, yaml_node_t* node, const gchar* tag_name, GString* out) {
  if (!node)
    return;

  switch (node->type) {
    case YAML_SCALAR_NODE:
      g_string_append_printf(out, "<%s>", tag_name);
      xml_escape(out, (gchar*)node->data.scalar.value);
      g_string_append_printf(out, "</%s>", tag_name);
      break;

    case YAML_SEQUENCE_NODE: {
      yaml_node_item_t* item;
      for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
        yaml_node_t* item_node = yaml_document_get_node(doc, *item);
        process_node(doc, item_node, tag_name, out);
      }
      break;
    }

    case YAML_MAPPING_NODE:
      process_mapping(doc, node, tag_name, out);
      break;

    default:
      break;
  }
}

gboolean obt_yaml_to_xml(const gchar* yaml_path, const gchar* root_node, gchar** xml_out, gsize* len_out) {
  FILE* fh = fopen(yaml_path, "r");
  yaml_parser_t parser;
  yaml_document_t doc;
  GString* out;
  gboolean success = FALSE;

  if (!fh)
    return FALSE;

  if (!yaml_parser_initialize(&parser)) {
    fclose(fh);
    return FALSE;
  }

  yaml_parser_set_input_file(&parser, fh);

  if (!yaml_parser_load(&parser, &doc)) {
    yaml_parser_delete(&parser);
    fclose(fh);
    return FALSE;
  }

  /* Check empty document */
  yaml_node_t* root = yaml_document_get_root_node(&doc);
  if (!root) {
    goto cleanup;
  }

  out = g_string_new("");
  g_string_append_printf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<%s>\n", root_node);

  if (root->type == YAML_MAPPING_NODE) {
    /* Iterate top level keys and treat them as children of root_node */
    yaml_node_pair_t* pair;
    for (pair = root->data.mapping.pairs.start; pair < root->data.mapping.pairs.top; pair++) {
      yaml_node_t* key_node = yaml_document_get_node(&doc, pair->key);
      yaml_node_t* value_node = yaml_document_get_node(&doc, pair->value);

      if (key_node && key_node->type == YAML_SCALAR_NODE) {
        process_node(&doc, value_node, (gchar*)key_node->data.scalar.value, out);
      }
    }
  }
  else {
    /* Unexpected root type, maybe just scalar or sequence? Treat as content? */
    /* For now, assume mapping is required at root */
  }

  g_string_append_printf(out, "</%s>\n", root_node);

  *xml_out = g_string_free(out, FALSE);
  *len_out = strlen(*xml_out);
  success = TRUE;

cleanup:
  yaml_document_delete(&doc);
  yaml_parser_delete(&parser);
  fclose(fh);

  return success;
}

#endif /* HAVE_YAML */

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
  g_message("process_mapping: tag='%s'", tag_name);
  yaml_node_pair_t* pair;
  GList* attrs = NULL;
  GList* children = NULL;
  GList* l;
  gchar* action_value = NULL;
  gchar* command_value = NULL;

  /* First pass: separate attributes and children, collect special values */
  for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key_node = yaml_document_get_node(doc, pair->key);
    yaml_node_t* value_node = yaml_document_get_node(doc, pair->value);

    if (!key_node || key_node->type != YAML_SCALAR_NODE)
      continue;

    gchar* key_str = (gchar*)key_node->data.scalar.value;

    /* For item tag, collect action and command values for special handling */
    if (strcmp(tag_name, "item") == 0) {
      if (strcmp(key_str, "action") == 0 && value_node->type == YAML_SCALAR_NODE) {
        action_value = g_strdup((gchar*)value_node->data.scalar.value);
        continue; /* Don't add to children list */
      }
      else if ((strcmp(key_str, "command") == 0 || strcmp(key_str, "execute") == 0) &&
               value_node->type == YAML_SCALAR_NODE) {
        command_value = g_strdup((gchar*)value_node->data.scalar.value);
        continue; /* Don't add to children list */
      }
    }

    if (is_attribute(tag_name, key_str) && value_node->type == YAML_SCALAR_NODE) {
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

  /* Special handling for separator - always produce <separator/> regardless of content */
  if (strcmp(tag_name, "separator") == 0) {
    g_string_append(out, "/>");
    g_list_free(attrs);
    g_list_free(children);
    g_free(action_value);
    g_free(command_value);
    return;
  }

  if (!children && !action_value && !command_value) {
    g_string_append(out, "/>");
  }
  else {
    g_string_append(out, ">");

    /* Special handling for item tag: combine action and command into Openbox XML structure */
    if (strcmp(tag_name, "item") == 0 && (action_value || command_value)) {
      g_string_append(out, "<action");
      if (action_value) {
        g_string_append(out, " name=\"");
        xml_escape(out, action_value);
        g_string_append(out, "\"");
      }
      g_string_append(out, ">");
      if (command_value) {
        g_string_append(out, "<execute>");
        xml_escape(out, command_value);
        g_string_append(out, "</execute>");
      }
      g_string_append(out, "</action>");
    }

    /* Write remaining children */
    for (l = children; l; l = l->next) {
      yaml_node_pair_t* p = l->data;
      yaml_node_t* k = yaml_document_get_node(doc, p->key);
      yaml_node_t* v = yaml_document_get_node(doc, p->value);

      gchar* key_s = (gchar*)k->data.scalar.value;

      /* Special handling for submenu */
      if (strcmp(key_s, "submenu") == 0 && (strcmp(tag_name, "menu") == 0 || strcmp(tag_name, "item") == 0)) {
        g_message("Processing submenu for %s tag, sequence length: %ld", tag_name,
                  (long)(v->data.sequence.items.top - v->data.sequence.items.start));
        /* submenu sequence items should be <item> elements */
        if (v->type == YAML_SEQUENCE_NODE) {
          yaml_node_item_t* sub_item;
          for (sub_item = v->data.sequence.items.start; sub_item < v->data.sequence.items.top; sub_item++) {
            yaml_node_t* sub_item_node = yaml_document_get_node(doc, *sub_item);
            process_node(doc, sub_item_node, "item", out);
          }
        }
        continue;
      }
      /* Rename command to execute if not already handled */
      else if (strcmp(key_s, "command") == 0) {
        key_s = "execute";
      }

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
  g_free(action_value);
  g_free(command_value);
}

static void process_node(yaml_document_t* doc, yaml_node_t* node, const gchar* tag_name, GString* out) {
  if (!node)
    return;

  g_message("process_node: tag='%s', node type=%d", tag_name, node->type);
  switch (node->type) {
    case YAML_SCALAR_NODE:
      /* Special handling for separator and action tags */
      if (strcmp(tag_name, "separator") == 0) {
        g_string_append(out, "<separator/>");
      }
      else if (strcmp(tag_name, "action") == 0) {
        /* action scalar becomes <action name="value"/> */
        g_string_append(out, "<action name=\"");
        xml_escape(out, (gchar*)node->data.scalar.value);
        g_string_append(out, "\"/>");
      }
      else {
        g_string_append_printf(out, "<%s>", tag_name);
        xml_escape(out, (gchar*)node->data.scalar.value);
        g_string_append_printf(out, "</%s>", tag_name);
      }
      break;

    case YAML_SEQUENCE_NODE: {
      yaml_node_item_t* item;
      const gchar* xml_tag = tag_name;
      /* Map YAML tag names to Openbox XML tag names */
      if (strcmp(tag_name, "submenu") == 0) {
        xml_tag = "menu";
      }
      for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
        yaml_node_t* item_node = yaml_document_get_node(doc, *item);
        process_node(doc, item_node, xml_tag, out);
      }
      break;
    }

    case YAML_MAPPING_NODE: {
      const gchar* xml_tag = tag_name;
      /* Map YAML tag names to Openbox XML tag names */
      if (strcmp(tag_name, "submenu") == 0) {
        xml_tag = "menu";
      }
      process_mapping(doc, node, xml_tag, out);
      break;
    }

    default:
      break;
  }
}

gboolean obt_yaml_to_xml(const gchar* yaml_path, const gchar* root_node, gchar** xml_out, gsize* len_out) {
  g_message("obt_yaml_to_xml called: %s, root_node=%s", yaml_path, root_node);
  FILE* fh = fopen(yaml_path, "r");
  yaml_parser_t parser;
  yaml_document_t doc;
  GString* out;
  gboolean success = FALSE;

  if (!fh) {
    g_message("Failed to open YAML file: %s", yaml_path);
    return FALSE;
  }

  if (!yaml_parser_initialize(&parser)) {
    fclose(fh);
    return FALSE;
  }

  yaml_parser_set_input_file(&parser, fh);

  if (!yaml_parser_load(&parser, &doc)) {
    g_message("yaml_parser_load failed");
    yaml_parser_delete(&parser);
    fclose(fh);
    return FALSE;
  }

  /* Check empty document */
  yaml_node_t* root = yaml_document_get_root_node(&doc);
  if (!root) {
    g_message("YAML document has no root node");
    goto cleanup;
  }

  out = g_string_new("");
  g_string_append_printf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<%s>\n", root_node);

  g_message("YAML root node type: %d (1=SCALAR,2=SEQUENCE,4=MAPPING)", root->type);
  if (root->type == YAML_MAPPING_NODE) {
    g_message("Root is mapping node");
    /* Iterate top level keys and treat them as children of root_node */
    yaml_node_pair_t* pair;
    g_message("YAML mapping pairs: %ld", (long)(root->data.mapping.pairs.top - root->data.mapping.pairs.start));
    for (pair = root->data.mapping.pairs.start; pair < root->data.mapping.pairs.top; pair++) {
      yaml_node_t* key_node = yaml_document_get_node(&doc, pair->key);
      yaml_node_t* value_node = yaml_document_get_node(&doc, pair->value);

      if (key_node && key_node->type == YAML_SCALAR_NODE) {
        g_message("Processing key: %s", key_node->data.scalar.value);
        process_node(&doc, value_node, (gchar*)key_node->data.scalar.value, out);
      }
      else {
        g_message("Key node not scalar: %p type=%d", (void*)key_node, key_node ? key_node->type : -1);
      }
    }
  }
  else {
    /* Only mapping roots are supported for complex format */
    g_message("Unsupported root node type: %d (expected mapping)", root->type);
    goto cleanup;
  }

  g_string_append_printf(out, "</%s>\n", root_node);

  *xml_out = g_string_free(out, FALSE);
  *len_out = strlen(*xml_out);
  success = TRUE;
  g_message("YAML to XML conversion succeeded (len=%zu): %.*s", *len_out, (int)(*len_out > 200 ? 200 : *len_out),
            *xml_out);

cleanup:
  yaml_document_delete(&doc);
  yaml_parser_delete(&parser);
  fclose(fh);

  return success;
}

#endif /* HAVE_YAML */

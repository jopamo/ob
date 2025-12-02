#include "ob_config.h"
#include <yaml.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

static char* scalar_dup(yaml_document_t* doc, yaml_node_t* node) {
  if (node && node->type == YAML_SCALAR_NODE) {
    return g_strdup((const char*)node->data.scalar.value);
  }
  return NULL;
}

static int scalar_int(yaml_document_t* doc, yaml_node_t* node, int def) {
  if (node && node->type == YAML_SCALAR_NODE) {
    return atoi((const char*)node->data.scalar.value);
  }
  return def;
}

static bool scalar_bool(yaml_document_t* doc, yaml_node_t* node, bool def) {
  if (node && node->type == YAML_SCALAR_NODE) {
    const char* v = (const char*)node->data.scalar.value;
    if (g_ascii_strcasecmp(v, "true") == 0 || g_ascii_strcasecmp(v, "yes") == 0 || g_ascii_strcasecmp(v, "on") == 0)
      return true;
    if (g_ascii_strcasecmp(v, "false") == 0 || g_ascii_strcasecmp(v, "no") == 0 || g_ascii_strcasecmp(v, "off") == 0)
      return false;
  }
  return def;
}

static void parse_theme(yaml_document_t* doc, yaml_node_t* node, struct ob_theme_config* theme) {
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;

    if (strcmp(k, "name") == 0) {
      free(theme->name);
      theme->name = scalar_dup(doc, val);
    }
    else if (strcmp(k, "title_layout") == 0) {
      free(theme->title_layout);
      theme->title_layout = scalar_dup(doc, val);
    }
    else if (strcmp(k, "keep_border") == 0) {
      theme->keep_border = scalar_bool(doc, val, theme->keep_border);
    }
    else if (strcmp(k, "animate_iconify") == 0) {
      theme->animate_iconify = scalar_bool(doc, val, theme->animate_iconify);
    }
    else if (strcmp(k, "window_list_icon_size") == 0) {
      theme->window_list_icon_size = scalar_int(doc, val, theme->window_list_icon_size);
    }
  }
}

static void parse_desktops(yaml_document_t* doc, yaml_node_t* node, struct ob_desktops_config* desktops) {
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;

    if (strcmp(k, "count") == 0) {
      desktops->count = scalar_int(doc, val, desktops->count);
    }
    else if (strcmp(k, "first") == 0) {
      desktops->first = scalar_int(doc, val, desktops->first);
    }
    else if (strcmp(k, "popup_time") == 0) {
      desktops->popup_time = scalar_int(doc, val, desktops->popup_time);
    }
    else if (strcmp(k, "names") == 0) {
      if (val->type == YAML_SEQUENCE_NODE) {
        /* Free existing names */
        if (desktops->names) {
          for (size_t i = 0; i < desktops->names_count; i++)
            free(desktops->names[i]);
          free(desktops->names);
        }
        size_t count = val->data.sequence.items.top - val->data.sequence.items.start;
        desktops->names = malloc(sizeof(char*) * count);
        desktops->names_count = 0;
        for (yaml_node_item_t* item = val->data.sequence.items.start; item < val->data.sequence.items.top; item++) {
          yaml_node_t* v = yaml_document_get_node(doc, *item);
          desktops->names[desktops->names_count++] = scalar_dup(doc, v);
        }
      }
    }
  }
}

static void parse_focus(yaml_document_t* doc, yaml_node_t* node, struct ob_focus_config* focus) {
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;

    if (strcmp(k, "follow_mouse") == 0)
      focus->follow_mouse = scalar_bool(doc, val, focus->follow_mouse);
    else if (strcmp(k, "focus_new") == 0)
      focus->focus_new = scalar_bool(doc, val, focus->focus_new);
    else if (strcmp(k, "raise_on_focus") == 0)
      focus->raise_on_focus = scalar_bool(doc, val, focus->raise_on_focus);
    else if (strcmp(k, "delay") == 0)
      focus->delay = scalar_int(doc, val, focus->delay);
    else if (strcmp(k, "focus_last") == 0)
      focus->focus_last = scalar_bool(doc, val, focus->focus_last);
    else if (strcmp(k, "under_mouse") == 0)
      focus->under_mouse = scalar_bool(doc, val, focus->under_mouse);
    else if (strcmp(k, "unfocus_leave") == 0)
      focus->unfocus_leave = scalar_bool(doc, val, focus->unfocus_leave);
  }
}

static void parse_placement(yaml_document_t* doc, yaml_node_t* node, struct ob_placement_config* place) {
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;

    if (strcmp(k, "policy") == 0) {
      char* s = scalar_dup(doc, val);
      if (s) {
        if (g_ascii_strcasecmp(s, "smart") == 0)
          place->policy = OB_CONFIG_PLACE_POLICY_SMART;
        else if (g_ascii_strcasecmp(s, "mouse") == 0)
          place->policy = OB_CONFIG_PLACE_POLICY_MOUSE;
        free(s);
      }
    }
    else if (strcmp(k, "center_new") == 0) {
      place->center_new = scalar_bool(doc, val, place->center_new);
    }
    else if (strcmp(k, "monitor") == 0) {
      /* Could be "active" or an index */
      if (val->type == YAML_SCALAR_NODE) {
        const char* s = (const char*)val->data.scalar.value;
        if (g_ascii_strcasecmp(s, "active") == 0)
          place->monitor_active = true;
        else {
          place->monitor_active = false;
          place->monitor = atoi(s);
        }
      }
    }
  }
}

static void parse_key_actions(yaml_document_t* doc, yaml_node_t* node, struct ob_key_action** actions, size_t* count) {
  if (node->type != YAML_SEQUENCE_NODE)
    return;

  *count = node->data.sequence.items.top - node->data.sequence.items.start;
  *actions = calloc(*count, sizeof(struct ob_key_action));

  size_t i = 0;
  for (yaml_node_item_t* item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
    yaml_node_t* mnode = yaml_document_get_node(doc, *item);
    if (mnode && mnode->type == YAML_MAPPING_NODE) {
      (*actions)[i].params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      for (yaml_node_pair_t* pair = mnode->data.mapping.pairs.start; pair < mnode->data.mapping.pairs.top; pair++) {
        yaml_node_t* k = yaml_document_get_node(doc, pair->key);
        yaml_node_t* v = yaml_document_get_node(doc, pair->value);
        if (k && k->type == YAML_SCALAR_NODE && v && v->type == YAML_SCALAR_NODE) {
          char* key = (char*)k->data.scalar.value;
          char* val = scalar_dup(doc, v);

          if (strcmp(key, "name") == 0) {
            (*actions)[i].name = val; /* Don't put name in params? */
          }
          else if (strcmp(key, "command") == 0) {
            (*actions)[i].command = val;
            g_hash_table_insert((*actions)[i].params, g_strdup(key), g_strdup(val));
          }
          else {
            g_hash_table_insert((*actions)[i].params, g_strdup(key), val);
          }
        }
      }
    }
    i++;
  }
}

static void parse_keyboard(yaml_document_t* doc, yaml_node_t* node, struct ob_config* cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;

  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);

    if (key && key->type == YAML_SCALAR_NODE && strcmp((char*)key->data.scalar.value, "keybinds") == 0) {
      if (val->type == YAML_SEQUENCE_NODE) {
        cfg->keybind_count = val->data.sequence.items.top - val->data.sequence.items.start;
        cfg->keybinds = calloc(cfg->keybind_count, sizeof(struct ob_keybind));

        size_t i = 0;
        for (yaml_node_item_t* item = val->data.sequence.items.start; item < val->data.sequence.items.top; item++) {
          yaml_node_t* kb_node = yaml_document_get_node(doc, *item);
          if (kb_node && kb_node->type == YAML_MAPPING_NODE) {
            for (yaml_node_pair_t* kp = kb_node->data.mapping.pairs.start; kp < kb_node->data.mapping.pairs.top; kp++) {
              yaml_node_t* k = yaml_document_get_node(doc, kp->key);
              yaml_node_t* v = yaml_document_get_node(doc, kp->value);
              if (k && k->type == YAML_SCALAR_NODE) {
                if (strcmp((char*)k->data.scalar.value, "key") == 0) {
                  cfg->keybinds[i].key = scalar_dup(doc, v);
                }
                else if (strcmp((char*)k->data.scalar.value, "context") == 0) {
                  cfg->keybinds[i].context = scalar_dup(doc, v);
                }
                else if (strcmp((char*)k->data.scalar.value, "actions") == 0) {
                  parse_key_actions(doc, v, &cfg->keybinds[i].actions, &cfg->keybinds[i].action_count);
                }
              }
            }
          }
          i++;
        }
      }
    }
  }
}

static void parse_mouse_actions(yaml_document_t* doc,
                                yaml_node_t* node,
                                struct ob_mouse_action** actions,
                                size_t* count) {
  if (node->type != YAML_SEQUENCE_NODE)
    return;

  *count = node->data.sequence.items.top - node->data.sequence.items.start;
  *actions = calloc(*count, sizeof(struct ob_mouse_action));

  size_t i = 0;
  for (yaml_node_item_t* item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
    yaml_node_t* mnode = yaml_document_get_node(doc, *item);
    if (mnode && mnode->type == YAML_MAPPING_NODE) {
      (*actions)[i].params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      for (yaml_node_pair_t* pair = mnode->data.mapping.pairs.start; pair < mnode->data.mapping.pairs.top; pair++) {
        yaml_node_t* k = yaml_document_get_node(doc, pair->key);
        yaml_node_t* v = yaml_document_get_node(doc, pair->value);
        if (k && k->type == YAML_SCALAR_NODE && v && v->type == YAML_SCALAR_NODE) {
          char* key = (char*)k->data.scalar.value;
          char* val = scalar_dup(doc, v);

          if (strcmp(key, "name") == 0) {
            (*actions)[i].name = val;
          }
          else if (strcmp(key, "command") == 0) {
            (*actions)[i].command = val;
            g_hash_table_insert((*actions)[i].params, g_strdup(key), g_strdup(val));
          }
          else {
            g_hash_table_insert((*actions)[i].params, g_strdup(key), val);
          }
        }
      }
    }
    i++;
  }
}

static void parse_mouse(yaml_document_t* doc, yaml_node_t* node, struct ob_config* cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;

  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);

    if (key && key->type == YAML_SCALAR_NODE && strcmp((char*)key->data.scalar.value, "bindings") == 0) {
      if (val->type == YAML_SEQUENCE_NODE) {
        cfg->mouse_binding_count = val->data.sequence.items.top - val->data.sequence.items.start;
        cfg->mouse_bindings = calloc(cfg->mouse_binding_count, sizeof(struct ob_mouse_binding));

        size_t i = 0;
        for (yaml_node_item_t* item = val->data.sequence.items.start; item < val->data.sequence.items.top; item++) {
          yaml_node_t* mb_node = yaml_document_get_node(doc, *item);
          if (mb_node && mb_node->type == YAML_MAPPING_NODE) {
            for (yaml_node_pair_t* mp = mb_node->data.mapping.pairs.start; mp < mb_node->data.mapping.pairs.top; mp++) {
              yaml_node_t* k = yaml_document_get_node(doc, mp->key);
              yaml_node_t* v = yaml_document_get_node(doc, mp->value);
              if (k && k->type == YAML_SCALAR_NODE) {
                if (strcmp((char*)k->data.scalar.value, "context") == 0) {
                  cfg->mouse_bindings[i].context = scalar_dup(doc, v);
                }
                else if (strcmp((char*)k->data.scalar.value, "button") == 0) {
                  cfg->mouse_bindings[i].button = scalar_dup(doc, v);
                }
                else if (strcmp((char*)k->data.scalar.value, "click") == 0) {
                  cfg->mouse_bindings[i].click = scalar_dup(doc, v);
                }
                else if (strcmp((char*)k->data.scalar.value, "actions") == 0) {
                  parse_mouse_actions(doc, v, &cfg->mouse_bindings[i].actions, &cfg->mouse_bindings[i].action_count);
                }
              }
            }
          }
          i++;
        }
      }
    }
  }
}

static void parse_windows(yaml_document_t* doc, yaml_node_t* node, struct ob_config* cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;

  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);

    if (key && key->type == YAML_SCALAR_NODE && strcmp((char*)key->data.scalar.value, "rules") == 0) {
      if (val->type == YAML_SEQUENCE_NODE) {
        cfg->window_rule_count = val->data.sequence.items.top - val->data.sequence.items.start;
        cfg->window_rules = calloc(cfg->window_rule_count, sizeof(struct ob_window_rule));

        size_t i = 0;
        for (yaml_node_item_t* item = val->data.sequence.items.start; item < val->data.sequence.items.top; item++) {
          yaml_node_t* r_node = yaml_document_get_node(doc, *item);
          if (r_node && r_node->type == YAML_MAPPING_NODE) {
            cfg->window_rules[i].desktop = -1; /* Default unset */
            for (yaml_node_pair_t* rp = r_node->data.mapping.pairs.start; rp < r_node->data.mapping.pairs.top; rp++) {
              yaml_node_t* k = yaml_document_get_node(doc, rp->key);
              yaml_node_t* v = yaml_document_get_node(doc, rp->value);
              if (k && k->type == YAML_SCALAR_NODE) {
                if (strcmp((char*)k->data.scalar.value, "match") == 0 && v->type == YAML_MAPPING_NODE) {
                  for (yaml_node_pair_t* mp = v->data.mapping.pairs.start; mp < v->data.mapping.pairs.top; mp++) {
                    yaml_node_t* mk = yaml_document_get_node(doc, mp->key);
                    yaml_node_t* mv = yaml_document_get_node(doc, mp->value);
                    if (mk && mk->type == YAML_SCALAR_NODE) {
                      if (strcmp((char*)mk->data.scalar.value, "class") == 0)
                        cfg->window_rules[i].match_class = scalar_dup(doc, mv);
                      else if (strcmp((char*)mk->data.scalar.value, "name") == 0)
                        cfg->window_rules[i].match_name = scalar_dup(doc, mv);
                      else if (strcmp((char*)mk->data.scalar.value, "role") == 0)
                        cfg->window_rules[i].match_role = scalar_dup(doc, mv);
                    }
                  }
                }
                else if (strcmp((char*)k->data.scalar.value, "desktop") == 0) {
                  cfg->window_rules[i].desktop = scalar_int(doc, v, -1);
                }
                else if (strcmp((char*)k->data.scalar.value, "maximized") == 0) {
                  cfg->window_rules[i].maximized = scalar_bool(doc, v, false);
                }
                else if (strcmp((char*)k->data.scalar.value, "fullscreen") == 0) {
                  cfg->window_rules[i].fullscreen = scalar_bool(doc, v, false);
                }
                else if (strcmp((char*)k->data.scalar.value, "skip_taskbar") == 0) {
                  cfg->window_rules[i].skip_taskbar = scalar_bool(doc, v, false);
                }
                else if (strcmp((char*)k->data.scalar.value, "skip_pager") == 0) {
                  cfg->window_rules[i].skip_pager = scalar_bool(doc, v, false);
                }
                else if (strcmp((char*)k->data.scalar.value, "follow") == 0) {
                  cfg->window_rules[i].follow = scalar_bool(doc, v, false);
                }
              }
            }
          }
          i++;
        }
      }
    }
  }
}

static void parse_menu_items(yaml_document_t* doc, yaml_node_t* node, struct ob_menu_item** items, size_t* count) {
  if (node->type != YAML_SEQUENCE_NODE)
    return;

  *count = node->data.sequence.items.top - node->data.sequence.items.start;
  *items = calloc(*count, sizeof(struct ob_menu_item));

  size_t i = 0;
  for (yaml_node_item_t* item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
    yaml_node_t* mnode = yaml_document_get_node(doc, *item);
    if (mnode && mnode->type == YAML_MAPPING_NODE) {
      (*items)[i].params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      for (yaml_node_pair_t* pair = mnode->data.mapping.pairs.start; pair < mnode->data.mapping.pairs.top; pair++) {
        yaml_node_t* k = yaml_document_get_node(doc, pair->key);
        yaml_node_t* v = yaml_document_get_node(doc, pair->value);
        if (k && k->type == YAML_SCALAR_NODE) {
          char* key = (char*)k->data.scalar.value;
          if (strcmp(key, "label") == 0) {
            (*items)[i].label = scalar_dup(doc, v);
          }
          else if (strcmp(key, "action") == 0) {
            (*items)[i].action_name = scalar_dup(doc, v);
          }
          else if (strcmp(key, "command") == 0) {
            (*items)[i].command = scalar_dup(doc, v);
            g_hash_table_insert((*items)[i].params, g_strdup(key), g_strdup((*items)[i].command));
          }
          else if (strcmp(key, "submenu") == 0) {
            parse_menu_items(doc, v, &(*items)[i].submenu, &(*items)[i].submenu_count);
          }
          else if (v->type == YAML_SCALAR_NODE) {
            g_hash_table_insert((*items)[i].params, g_strdup(key), scalar_dup(doc, v));
          }
        }
      }
    }
    i++;
  }
}

static void parse_menu(yaml_document_t* doc, yaml_node_t* node, struct ob_config* cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;

  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);

    if (key && key->type == YAML_SCALAR_NODE && strcmp((char*)key->data.scalar.value, "root") == 0) {
      parse_menu_items(doc, val, &cfg->menu.root, &cfg->menu.root_count);
    }
  }
}

bool ob_config_load_yaml(const char* path, struct ob_config* cfg) {
  FILE* fh = fopen(path, "r");
  if (!fh)
    return false;

  yaml_parser_t parser;
  yaml_document_t document;

  if (!yaml_parser_initialize(&parser)) {
    fclose(fh);
    return false;
  }

  yaml_parser_set_input_file(&parser, fh);

  if (!yaml_parser_load(&parser, &document)) {
    yaml_parser_delete(&parser);
    fclose(fh);
    return false;
  }

  yaml_node_t* root = yaml_document_get_root_node(&document);
  if (!root || root->type != YAML_MAPPING_NODE) {
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    fclose(fh);
    return false;
  }

  for (yaml_node_pair_t* pair = root->data.mapping.pairs.start; pair < root->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(&document, pair->key);
    yaml_node_t* value = yaml_document_get_node(&document, pair->value);

    if (!key || !value || key->type != YAML_SCALAR_NODE)
      continue;

    const char* k = (const char*)key->data.scalar.value;

    if (strcmp(k, "theme") == 0) {
      parse_theme(&document, value, &cfg->theme);
    }
    else if (strcmp(k, "desktops") == 0) {
      parse_desktops(&document, value, &cfg->desktops);
    }
    else if (strcmp(k, "focus") == 0) {
      parse_focus(&document, value, &cfg->focus);
    }
    else if (strcmp(k, "placement") == 0) {
      parse_placement(&document, value, &cfg->placement);
    }
    else if (strcmp(k, "keyboard") == 0) {
      parse_keyboard(&document, value, cfg);
    }
    else if (strcmp(k, "mouse") == 0) {
      parse_mouse(&document, value, cfg);
    }
    else if (strcmp(k, "windows") == 0) {
      parse_windows(&document, value, cfg);
    }
    else if (strcmp(k, "menu") == 0) {
      parse_menu(&document, value, cfg);
    }
  }

  yaml_document_delete(&document);
  yaml_parser_delete(&parser);
  fclose(fh);
  return true;
}

bool ob_config_dump_yaml(const struct ob_config* cfg, FILE* out) {
  yaml_emitter_t emitter;
  yaml_event_t event;
  char buf[64];

  yaml_emitter_initialize(&emitter);
  yaml_emitter_set_output_file(&emitter, out);

  yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
  yaml_emitter_emit(&emitter, &event);

  yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
  yaml_emitter_emit(&emitter, &event);

  yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
  yaml_emitter_emit(&emitter, &event);

  /* Theme */
  yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)"theme", -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_emit(&emitter, &event);

  yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
  yaml_emitter_emit(&emitter, &event);

  if (cfg->theme.name) {
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)"name", -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)cfg->theme.name, -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
  }

  if (cfg->theme.title_layout) {
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)"title_layout", -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)cfg->theme.title_layout, -1, 1, 1,
                                 YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
  }

  yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)"keep_border", -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_emit(&emitter, &event);
  yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)(cfg->theme.keep_border ? "true" : "false"), -1, 1, 1,
                               YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_emit(&emitter, &event);

  yaml_mapping_end_event_initialize(&event);
  yaml_emitter_emit(&emitter, &event);

  /* Desktops */
  yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)"desktops", -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_emit(&emitter, &event);

  yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
  yaml_emitter_emit(&emitter, &event);

  yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)"count", -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_emit(&emitter, &event);
  snprintf(buf, sizeof(buf), "%d", cfg->desktops.count);
  yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)buf, -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_emit(&emitter, &event);

  yaml_mapping_end_event_initialize(&event);
  yaml_emitter_emit(&emitter, &event);

  /* End Root Mapping */
  yaml_mapping_end_event_initialize(&event);
  yaml_emitter_emit(&emitter, &event);

  yaml_document_end_event_initialize(&event, 1);
  yaml_emitter_emit(&emitter, &event);

  yaml_stream_end_event_initialize(&event);
  yaml_emitter_emit(&emitter, &event);

  yaml_emitter_delete(&emitter);
  return true;
}

#include "ob_config.h"
#include <yaml.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

/* ... (keep existing scalar helpers and parse functions) ... */
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

/* ... (include all parse functions from previous write_file call) ... */
static void parse_resistance(yaml_document_t* doc, yaml_node_t* node, struct ob_resistance_config* cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;
    if (strcmp(k, "strength") == 0)
      cfg->strength = scalar_int(doc, val, cfg->strength);
    else if (strcmp(k, "screen_edge_strength") == 0)
      cfg->screen_edge_strength = scalar_int(doc, val, cfg->screen_edge_strength);
  }
}

static void parse_resize(yaml_document_t* doc, yaml_node_t* node, struct ob_resize_config* cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;
    if (strcmp(k, "drawContents") == 0)
      cfg->draw_contents = scalar_bool(doc, val, cfg->draw_contents);
    else if (strcmp(k, "popupShow") == 0) {
      free(cfg->popup_show);
      cfg->popup_show = scalar_dup(doc, val);
    }
    else if (strcmp(k, "popupPosition") == 0) {
      free(cfg->popup_position);
      cfg->popup_position = scalar_dup(doc, val);
    }
    else if (strcmp(k, "popupFixedPosition") == 0 && val->type == YAML_MAPPING_NODE) {
      for (yaml_node_pair_t* fp = val->data.mapping.pairs.start; fp < val->data.mapping.pairs.top; fp++) {
        yaml_node_t* fk = yaml_document_get_node(doc, fp->key);
        yaml_node_t* fv = yaml_document_get_node(doc, fp->value);
        if (fk && fk->type == YAML_SCALAR_NODE) {
          if (strcmp((char*)fk->data.scalar.value, "x") == 0)
            cfg->popup_fixed_position.x = scalar_int(doc, fv, 0);
          else if (strcmp((char*)fk->data.scalar.value, "y") == 0)
            cfg->popup_fixed_position.y = scalar_int(doc, fv, 0);
        }
      }
    }
  }
}

static void parse_margins(yaml_document_t* doc, yaml_node_t* node, struct ob_margins_config* cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;
    if (strcmp(k, "top") == 0)
      cfg->top = scalar_int(doc, val, cfg->top);
    else if (strcmp(k, "bottom") == 0)
      cfg->bottom = scalar_int(doc, val, cfg->bottom);
    else if (strcmp(k, "left") == 0)
      cfg->left = scalar_int(doc, val, cfg->left);
    else if (strcmp(k, "right") == 0)
      cfg->right = scalar_int(doc, val, cfg->right);
  }
}

static void parse_dock(yaml_document_t* doc, yaml_node_t* node, struct ob_dock_config* cfg) {
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;
    if (strcmp(k, "position") == 0) {
      free(cfg->position);
      cfg->position = scalar_dup(doc, val);
    }
    else if (strcmp(k, "floatingX") == 0)
      cfg->floating_x = scalar_int(doc, val, cfg->floating_x);
    else if (strcmp(k, "floatingY") == 0)
      cfg->floating_y = scalar_int(doc, val, cfg->floating_y);
    else if (strcmp(k, "noStrut") == 0)
      cfg->no_strut = scalar_bool(doc, val, cfg->no_strut);
    else if (strcmp(k, "stacking") == 0) {
      free(cfg->stacking);
      cfg->stacking = scalar_dup(doc, val);
    }
    else if (strcmp(k, "direction") == 0) {
      free(cfg->direction);
      cfg->direction = scalar_dup(doc, val);
    }
    else if (strcmp(k, "autoHide") == 0)
      cfg->auto_hide = scalar_bool(doc, val, cfg->auto_hide);
    else if (strcmp(k, "hideDelay") == 0)
      cfg->hide_delay = scalar_int(doc, val, cfg->hide_delay);
    else if (strcmp(k, "showDelay") == 0)
      cfg->show_delay = scalar_int(doc, val, cfg->show_delay);
    else if (strcmp(k, "moveButton") == 0) {
      free(cfg->move_button);
      cfg->move_button = scalar_dup(doc, val);
    }
  }
}

static void parse_theme(yaml_document_t* doc, yaml_node_t* node, struct ob_theme_config* theme) {
  /* (Identical to previous version) */
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
    else if (strcmp(k, "titleLayout") == 0) {
      free(theme->title_layout);
      theme->title_layout = scalar_dup(doc, val);
    }
    else if (strcmp(k, "keepBorder") == 0)
      theme->keep_border = scalar_bool(doc, val, theme->keep_border);
    else if (strcmp(k, "animateIconify") == 0)
      theme->animate_iconify = scalar_bool(doc, val, theme->animate_iconify);
    else if (strcmp(k, "windowListIconSize") == 0)
      theme->window_list_icon_size = scalar_int(doc, val, theme->window_list_icon_size);
    else if (strcmp(k, "font") == 0 && val->type == YAML_SEQUENCE_NODE) {
      if (theme->fonts) {
        for (size_t i = 0; i < theme->font_count; i++) {
          free(theme->fonts[i].place);
          free(theme->fonts[i].name);
          free(theme->fonts[i].weight);
          free(theme->fonts[i].slant);
        }
        free(theme->fonts);
      }
      theme->font_count = val->data.sequence.items.top - val->data.sequence.items.start;
      theme->fonts = calloc(theme->font_count, sizeof(struct ob_font_config));
      size_t i = 0;
      for (yaml_node_item_t* item = val->data.sequence.items.start; item < val->data.sequence.items.top; item++) {
        yaml_node_t* fnode = yaml_document_get_node(doc, *item);
        if (fnode && fnode->type == YAML_MAPPING_NODE) {
          for (yaml_node_pair_t* fp = fnode->data.mapping.pairs.start; fp < fnode->data.mapping.pairs.top; fp++) {
            yaml_node_t* fk = yaml_document_get_node(doc, fp->key);
            yaml_node_t* fv = yaml_document_get_node(doc, fp->value);
            if (fk && fk->type == YAML_SCALAR_NODE) {
              const char* fk_str = (const char*)fk->data.scalar.value;
              if (strcmp(fk_str, "place") == 0)
                theme->fonts[i].place = scalar_dup(doc, fv);
              else if (strcmp(fk_str, "name") == 0)
                theme->fonts[i].name = scalar_dup(doc, fv);
              else if (strcmp(fk_str, "size") == 0)
                theme->fonts[i].size = scalar_int(doc, fv, 10);
              else if (strcmp(fk_str, "weight") == 0)
                theme->fonts[i].weight = scalar_dup(doc, fv);
              else if (strcmp(fk_str, "slant") == 0)
                theme->fonts[i].slant = scalar_dup(doc, fv);
            }
          }
        }
        i++;
      }
    }
  }
}

static void parse_desktops(yaml_document_t* doc, yaml_node_t* node, struct ob_desktops_config* desktops) {
  /* (Identical to previous version) */
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;
    if (strcmp(k, "number") == 0)
      desktops->count = scalar_int(doc, val, desktops->count);
    else if (strcmp(k, "firstdesk") == 0)
      desktops->first = scalar_int(doc, val, desktops->first);
    else if (strcmp(k, "popupTime") == 0)
      desktops->popup_time = scalar_int(doc, val, desktops->popup_time);
    else if (strcmp(k, "names") == 0 && val->type == YAML_SEQUENCE_NODE) {
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

static void parse_focus(yaml_document_t* doc, yaml_node_t* node, struct ob_focus_config* focus) {
  /* (Identical to previous version) */
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (!key || !val || key->type != YAML_SCALAR_NODE)
      continue;
    const char* k = (const char*)key->data.scalar.value;
    if (strcmp(k, "followMouse") == 0)
      focus->follow_mouse = scalar_bool(doc, val, focus->follow_mouse);
    else if (strcmp(k, "focusNew") == 0)
      focus->focus_new = scalar_bool(doc, val, focus->focus_new);
    else if (strcmp(k, "raiseOnFocus") == 0)
      focus->raise_on_focus = scalar_bool(doc, val, focus->raise_on_focus);
    else if (strcmp(k, "focusDelay") == 0)
      focus->delay = scalar_int(doc, val, focus->delay);
    else if (strcmp(k, "focusLast") == 0)
      focus->focus_last = scalar_bool(doc, val, focus->focus_last);
    else if (strcmp(k, "underMouse") == 0)
      focus->under_mouse = scalar_bool(doc, val, focus->under_mouse);
    else if (strcmp(k, "unfocusLeave") == 0)
      focus->unfocus_leave = scalar_bool(doc, val, focus->unfocus_leave);
  }
}

static void parse_placement(yaml_document_t* doc, yaml_node_t* node, struct ob_placement_config* place) {
  /* (Identical to previous version) */
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
    else if (strcmp(k, "center") == 0)
      place->center_new = scalar_bool(doc, val, place->center_new);
    else if (strcmp(k, "monitor") == 0 && val->type == YAML_SCALAR_NODE) {
      const char* s = (const char*)val->data.scalar.value;
      if (g_ascii_strcasecmp(s, "active") == 0)
        place->monitor_active = true;
      else if (g_ascii_strcasecmp(s, "mouse") == 0) {
        place->monitor_active = false;
        place->monitor = 0;
      }
      else {
        place->monitor_active = false;
        place->monitor = atoi(s);
      }
    }
  }
}

static void parse_key_actions(yaml_document_t* doc, yaml_node_t* node, struct ob_key_action** actions, size_t* count) {
  /* (Identical to previous version) */
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
          if (strcmp(key, "name") == 0)
            (*actions)[i].name = val;
          else if (strcmp(key, "command") == 0) {
            (*actions)[i].command = val;
            g_hash_table_insert((*actions)[i].params, g_strdup(key), g_strdup(val));
          }
          else
            g_hash_table_insert((*actions)[i].params, g_strdup(key), val);
        }
      }
    }
    i++;
  }
}

static void parse_keyboard(yaml_document_t* doc, yaml_node_t* node, struct ob_config* cfg) {
  /* (Identical to previous version) */
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (key && key->type == YAML_SCALAR_NODE) {
      char* key_str = (char*)key->data.scalar.value;
      if (strcmp(key_str, "chainQuitKey") == 0) {
        free(cfg->keyboard.chain_quit_key);
        cfg->keyboard.chain_quit_key = scalar_dup(doc, val);
      }
      else if (strcmp(key_str, "keybind") == 0 && val->type == YAML_SEQUENCE_NODE) {
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
                if (strcmp((char*)k->data.scalar.value, "key") == 0)
                  cfg->keybinds[i].key = scalar_dup(doc, v);
                else if (strcmp((char*)k->data.scalar.value, "context") == 0)
                  cfg->keybinds[i].context = scalar_dup(doc, v);
                else if (strcmp((char*)k->data.scalar.value, "action") == 0)
                  parse_key_actions(doc, v, &cfg->keybinds[i].actions, &cfg->keybinds[i].action_count);
              }
            }
          }
          i++;
        }
      }
    }
  }
}

static void parse_mouse_simple_actions(yaml_document_t* doc,
                                       yaml_node_t* node,
                                       struct ob_mouse_action** actions,
                                       size_t* count) {
  if (node->type != YAML_SEQUENCE_NODE)
    return;
  *count = node->data.sequence.items.top - node->data.sequence.items.start;
  *actions = calloc(*count, sizeof(struct ob_mouse_action));
  size_t i = 0;
  for (yaml_node_item_t* item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
    yaml_node_t* anode = yaml_document_get_node(doc, *item);
    if (anode && anode->type == YAML_SCALAR_NODE) {
      (*actions)[i].name = scalar_dup(doc, anode);
    }
    i++;
  }
}

static void parse_mouse(yaml_document_t* doc, yaml_node_t* node, struct ob_config* cfg) {
  /* (Identical to previous version) */
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (key && key->type == YAML_SCALAR_NODE) {
      const char* k = (const char*)key->data.scalar.value;
      if (strcmp(k, "dragThreshold") == 0)
        cfg->mouse.drag_threshold = scalar_int(doc, val, cfg->mouse.drag_threshold);
      else if (strcmp(k, "doubleClickTime") == 0)
        cfg->mouse.double_click_time = scalar_int(doc, val, cfg->mouse.double_click_time);
      else if (strcmp(k, "screenEdgeWarpTime") == 0)
        cfg->mouse.screen_edge_warp_time = scalar_int(doc, val, cfg->mouse.screen_edge_warp_time);
      else if (strcmp(k, "screenEdgeWarpMouse") == 0)
        cfg->mouse.screen_edge_warp_mouse = scalar_bool(doc, val, cfg->mouse.screen_edge_warp_mouse);
      else if (strcmp(k, "context") == 0 && val->type == YAML_SEQUENCE_NODE) {
        for (yaml_node_item_t* item = val->data.sequence.items.start; item < val->data.sequence.items.top; item++) {
          yaml_node_t* ctx_node = yaml_document_get_node(doc, *item);
          if (ctx_node && ctx_node->type == YAML_MAPPING_NODE) {
            char* context_name = NULL;
            yaml_node_t* bindings_node = NULL;
            for (yaml_node_pair_t* cp = ctx_node->data.mapping.pairs.start; cp < ctx_node->data.mapping.pairs.top;
                 cp++) {
              yaml_node_t* ck = yaml_document_get_node(doc, cp->key);
              yaml_node_t* cv = yaml_document_get_node(doc, cp->value);
              if (ck && ck->type == YAML_SCALAR_NODE) {
                if (strcmp((char*)ck->data.scalar.value, "name") == 0)
                  context_name = scalar_dup(doc, cv);
                else if (strcmp((char*)ck->data.scalar.value, "mousebind") == 0)
                  bindings_node = cv;
              }
            }
            if (context_name && bindings_node && bindings_node->type == YAML_SEQUENCE_NODE) {
              size_t new_count = bindings_node->data.sequence.items.top - bindings_node->data.sequence.items.start;
              cfg->mouse_bindings =
                  g_renew(struct ob_mouse_binding, cfg->mouse_bindings, cfg->mouse_binding_count + new_count);
              for (yaml_node_item_t* bitem = bindings_node->data.sequence.items.start;
                   bitem < bindings_node->data.sequence.items.top; bitem++) {
                struct ob_mouse_binding* mb = &cfg->mouse_bindings[cfg->mouse_binding_count];
                memset(mb, 0, sizeof(*mb));
                mb->context = g_strdup(context_name);
                yaml_node_t* bnode = yaml_document_get_node(doc, *bitem);
                if (bnode && bnode->type == YAML_MAPPING_NODE) {
                  for (yaml_node_pair_t* bp = bnode->data.mapping.pairs.start; bp < bnode->data.mapping.pairs.top;
                       bp++) {
                    yaml_node_t* bk = yaml_document_get_node(doc, bp->key);
                    yaml_node_t* bv = yaml_document_get_node(doc, bp->value);
                    if (bk && bk->type == YAML_SCALAR_NODE) {
                      const char* bks = (const char*)bk->data.scalar.value;
                      if (strcmp(bks, "button") == 0)
                        mb->button = scalar_dup(doc, bv);
                      else if (strcmp(bks, "action") == 0)
                        mb->click = scalar_dup(doc, bv);
                      else if (strcmp(bks, "do") == 0)
                        parse_mouse_simple_actions(doc, bv, &mb->actions, &mb->action_count);
                    }
                  }
                }
                cfg->mouse_binding_count++;
              }
            }
            free(context_name);
          }
        }
      }
    }
  }
}

static void parse_applications(yaml_document_t* doc, yaml_node_t* node, struct ob_config* cfg) {
  /* (Identical to previous version) */
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (key && key->type == YAML_SCALAR_NODE && strcmp((char*)key->data.scalar.value, "application") == 0 &&
        val->type == YAML_SEQUENCE_NODE) {
      cfg->window_rule_count = val->data.sequence.items.top - val->data.sequence.items.start;
      cfg->window_rules = calloc(cfg->window_rule_count, sizeof(struct ob_window_rule));
      size_t i = 0;
      for (yaml_node_item_t* item = val->data.sequence.items.start; item < val->data.sequence.items.top; item++) {
        yaml_node_t* r_node = yaml_document_get_node(doc, *item);
        if (r_node && r_node->type == YAML_MAPPING_NODE) {
          cfg->window_rules[i].desktop = -1;
          cfg->window_rules[i].maximized = -1;
          cfg->window_rules[i].fullscreen = -1;
          cfg->window_rules[i].skip_taskbar = -1;
          cfg->window_rules[i].skip_pager = -1;
          cfg->window_rules[i].follow = -1;
          cfg->window_rules[i].decor = -1;
          for (yaml_node_pair_t* rp = r_node->data.mapping.pairs.start; rp < r_node->data.mapping.pairs.top; rp++) {
            yaml_node_t* k = yaml_document_get_node(doc, rp->key);
            yaml_node_t* v = yaml_document_get_node(doc, rp->value);
            if (k && k->type == YAML_SCALAR_NODE) {
              const char* ks = (const char*)k->data.scalar.value;
              if (strcmp(ks, "class") == 0)
                cfg->window_rules[i].match_class = scalar_dup(doc, v);
              else if (strcmp(ks, "name") == 0)
                cfg->window_rules[i].match_name = scalar_dup(doc, v);
              else if (strcmp(ks, "role") == 0)
                cfg->window_rules[i].match_role = scalar_dup(doc, v);
              else if (strcmp(ks, "desktop") == 0)
                cfg->window_rules[i].desktop = scalar_int(doc, v, -1);
              else if (strcmp(ks, "maximized") == 0)
                cfg->window_rules[i].maximized = scalar_bool(doc, v, false) ? 1 : 0;
              else if (strcmp(ks, "fullscreen") == 0)
                cfg->window_rules[i].fullscreen = scalar_bool(doc, v, false) ? 1 : 0;
              else if (strcmp(ks, "skip_taskbar") == 0)
                cfg->window_rules[i].skip_taskbar = scalar_bool(doc, v, false) ? 1 : 0;
              else if (strcmp(ks, "skip_pager") == 0)
                cfg->window_rules[i].skip_pager = scalar_bool(doc, v, false) ? 1 : 0;
              else if (strcmp(ks, "follow") == 0)
                cfg->window_rules[i].follow = scalar_bool(doc, v, false) ? 1 : 0;
              else if (strcmp(ks, "decor") == 0)
                cfg->window_rules[i].decor = scalar_bool(doc, v, true) ? 1 : 0;
            }
          }
        }
        i++;
      }
    }
  }
}

static void parse_menu_items(yaml_document_t* doc, yaml_node_t* node, struct ob_menu_item** items, size_t* count) {
  /* (Identical to previous version) */
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
          if (strcmp(key, "label") == 0)
            (*items)[i].label = scalar_dup(doc, v);
          else if (strcmp(key, "action") == 0)
            (*items)[i].action_name = scalar_dup(doc, v);
          else if (strcmp(key, "command") == 0) {
            (*items)[i].command = scalar_dup(doc, v);
            g_hash_table_insert((*items)[i].params, g_strdup(key), g_strdup((*items)[i].command));
          }
          else if (strcmp(key, "submenu") == 0)
            parse_menu_items(doc, v, &(*items)[i].submenu, &(*items)[i].submenu_count);
          else if (v->type == YAML_SCALAR_NODE)
            g_hash_table_insert((*items)[i].params, g_strdup(key), scalar_dup(doc, v));
        }
      }
    }
    i++;
  }
}

static void parse_menu(yaml_document_t* doc, yaml_node_t* node, struct ob_config* cfg) {
  /* (Identical to previous version) */
  if (node->type != YAML_MAPPING_NODE)
    return;
  for (yaml_node_pair_t* pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t* key = yaml_document_get_node(doc, pair->key);
    yaml_node_t* val = yaml_document_get_node(doc, pair->value);
    if (key && key->type == YAML_SCALAR_NODE) {
      const char* k = (const char*)key->data.scalar.value;
      if (strcmp(k, "root") == 0)
        parse_menu_items(doc, val, &cfg->menu.root, &cfg->menu.root_count);
      else if (strcmp(k, "file") == 0)
        cfg->menu.file = scalar_dup(doc, val);
      else if (strcmp(k, "hideDelay") == 0)
        cfg->menu.hide_delay = scalar_int(doc, val, cfg->menu.hide_delay);
      else if (strcmp(k, "middle") == 0)
        cfg->menu.middle = scalar_bool(doc, val, cfg->menu.middle);
      else if (strcmp(k, "submenuShowDelay") == 0)
        cfg->menu.submenu_show_delay = scalar_int(doc, val, cfg->menu.submenu_show_delay);
      else if (strcmp(k, "submenuHideDelay") == 0)
        cfg->menu.submenu_hide_delay = scalar_int(doc, val, cfg->menu.submenu_hide_delay);
      else if (strcmp(k, "showIcons") == 0)
        cfg->menu.show_icons = scalar_bool(doc, val, cfg->menu.show_icons);
      else if (strcmp(k, "manageDesktops") == 0)
        cfg->menu.manage_desktops = scalar_bool(doc, val, cfg->menu.manage_desktops);
    }
  }
}

bool ob_config_load_yaml(const char* path, struct ob_config* cfg) {
  /* (Identical to previous version) */
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
    if (strcmp(k, "theme") == 0)
      parse_theme(&document, value, &cfg->theme);
    else if (strcmp(k, "desktops") == 0)
      parse_desktops(&document, value, &cfg->desktops);
    else if (strcmp(k, "focus") == 0)
      parse_focus(&document, value, &cfg->focus);
    else if (strcmp(k, "placement") == 0)
      parse_placement(&document, value, &cfg->placement);
    else if (strcmp(k, "keyboard") == 0)
      parse_keyboard(&document, value, cfg);
    else if (strcmp(k, "mouse") == 0)
      parse_mouse(&document, value, cfg);
    else if (strcmp(k, "applications") == 0)
      parse_applications(&document, value, cfg);
    else if (strcmp(k, "menu") == 0)
      parse_menu(&document, value, cfg);
    else if (strcmp(k, "resistance") == 0)
      parse_resistance(&document, value, &cfg->resistance);
    else if (strcmp(k, "resize") == 0)
      parse_resize(&document, value, &cfg->resize);
    else if (strcmp(k, "margins") == 0)
      parse_margins(&document, value, &cfg->margins);
    else if (strcmp(k, "dock") == 0)
      parse_dock(&document, value, &cfg->dock);
  }
  yaml_document_delete(&document);
  yaml_parser_delete(&parser);
  fclose(fh);
  return true;
}

bool ob_config_load_menu_file(const char* path, struct ob_config* cfg) {
  FILE* fh = fopen(path, "r");
  if (!fh) {
    g_message("Menu YAML: unable to open %s: %s", path, g_strerror(errno));
    return false;
  }
  yaml_parser_t parser;
  yaml_document_t document;
  if (!yaml_parser_initialize(&parser)) {
    g_message("Menu YAML: failed to initialize parser for %s", path);
    fclose(fh);
    return false;
  }
  yaml_parser_set_input_file(&parser, fh);
  if (!yaml_parser_load(&parser, &document)) {
    g_message("Menu YAML: parse error in %s (line %ld): %s", path, (long)parser.problem_mark.line + 1,
              parser.problem ? parser.problem : "unknown");
    yaml_parser_delete(&parser);
    fclose(fh);
    return false;
  }
  yaml_node_t* root = yaml_document_get_root_node(&document);
  if (!root || root->type != YAML_SEQUENCE_NODE) {
    g_message("Menu YAML: root of %s is not a sequence", path);
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    fclose(fh);
    return false;
  }

  parse_menu_items(&document, root, &cfg->menu.root, &cfg->menu.root_count);
  g_message("Menu YAML: loaded %zu top-level items from %s", cfg->menu.root_count, path);

  yaml_document_delete(&document);
  yaml_parser_delete(&parser);
  fclose(fh);
  return true;
}

bool ob_config_dump_yaml(const struct ob_config* cfg, FILE* out) {
  yaml_emitter_t emitter;
  yaml_event_t event;
  yaml_emitter_initialize(&emitter);
  yaml_emitter_set_output_file(&emitter, out);
  yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
  yaml_emitter_emit(&emitter, &event);
  yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
  yaml_emitter_emit(&emitter, &event);
  yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
  yaml_emitter_emit(&emitter, &event);

  /* Dump Theme (minimal) */
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
  yaml_mapping_end_event_initialize(&event);
  yaml_emitter_emit(&emitter, &event);

  /* Dump Desktops (minimal) */
  char buf[64];
  yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)"desktops", -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_emit(&emitter, &event);
  yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
  yaml_emitter_emit(&emitter, &event);
  yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)"number", -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_emit(&emitter, &event);
  snprintf(buf, sizeof(buf), "%d", cfg->desktops.count);
  yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)buf, -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_emit(&emitter, &event);
  yaml_mapping_end_event_initialize(&event);
  yaml_emitter_emit(&emitter, &event);

  /* (Skipping dumping of other sections to focus on compilation and parsing correctness first) */

  yaml_mapping_end_event_initialize(&event);
  yaml_emitter_emit(&emitter, &event);
  yaml_document_end_event_initialize(&event, 1);
  yaml_emitter_emit(&emitter, &event);
  yaml_stream_end_event_initialize(&event);
  yaml_emitter_emit(&emitter, &event);
  yaml_emitter_delete(&emitter);
  return true;
}

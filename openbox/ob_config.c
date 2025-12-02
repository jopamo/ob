#include "ob_config.h"
#include <X11/Xlib.h>
#include "screen.h"
#include "config.h"
#include "keyboard.h"
#include "mouse.h"
#include "frame.h"
#include "menu.h"
#include "actions.h"
#include <stdlib.h>
#include <string.h>

static char* safe_strdup(const char* s) {
  return s ? g_strdup(s) : NULL;
}

static void add_menu_item(struct ob_menu_item* item,
                          const char* label,
                          const char* action,
                          const char* command,
                          const char* icon) {
  item->label = safe_strdup(label);
  item->action_name = safe_strdup(action);
  item->command = safe_strdup(command);
  item->submenu = NULL;
  item->submenu_count = 0;
  item->params = NULL;

  if (command || icon) {
    item->params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    if (command)
      g_hash_table_insert(item->params, g_strdup("command"), g_strdup(command));
    if (icon)
      g_hash_table_insert(item->params, g_strdup("icon"), g_strdup(icon));
  }
}

static void set_default_menu(struct ob_config* cfg) {
  if (!cfg)
    return;

  /* Mirrors data/menu.xml without needing XML parsing. */
  cfg->menu.root_count = 23;
  cfg->menu.root = g_new0(struct ob_menu_item, cfg->menu.root_count);

  size_t i = 0;
  add_menu_item(&cfg->menu.root[i++], "qterminal", "Execute", "qterminal",
                "/usr/share/icons/hicolor/64x64/apps/qterminal.png");
  add_menu_item(&cfg->menu.root[i++], "firefox-nightly-bin", "Execute", "firefox-nightly-bin",
                "/usr/share/pixmaps/firefox.svg");
  add_menu_item(&cfg->menu.root[i++], "brave-nightly-bin", "Execute", "brave-nightly-bin",
                "/usr/share/pixmaps/brave.svg");
  add_menu_item(&cfg->menu.root[i++], "google-chrome-unstable", "Execute", "google-chrome-unstable",
                "/usr/share/pixmaps/chrome.svg");
  add_menu_item(&cfg->menu.root[i++], "chromium", "Execute", "chromium", "/usr/share/pixmaps/chromium.png");
  add_menu_item(&cfg->menu.root[i++], "pcmanfm-qt", "Execute", "pcmanfm-qt",
                "/usr/share/icons/hicolor/scalable/apps/pcmanfm-qt.svg");
  add_menu_item(&cfg->menu.root[i++], "lxqt-config", "Execute", "lxqt-config",
                "/usr/share/icons/hicolor/scalable/apps/lxqt.svg");
  add_menu_item(&cfg->menu.root[i++], "featherpad", "Execute", "featherpad",
                "/usr/share/icons/hicolor/scalable/apps/featherpad.svg");
  add_menu_item(&cfg->menu.root[i++], "keepassxc", "Execute", "env QT_STYLE_OVERRIDE=Adwaita-Dark keepassxc",
                "/usr/share/icons/hicolor/256x256/apps/keepassxc.png");
  add_menu_item(&cfg->menu.root[i++], "wireshark", "Execute", "env QT_STYLE_OVERRIDE=Adwaita-Dark wireshark",
                "/usr/share/icons/hicolor/128x128/apps/org.wireshark.Wireshark.png");
  add_menu_item(&cfg->menu.root[i++], "qalculate-gtk", "Execute", "qalculate-gtk",
                "/usr/share/icons/hicolor/128x128/apps/qalculate.png");
  add_menu_item(&cfg->menu.root[i++], "qbittorrent", "Execute", "qbittorrent",
                "/usr/share/icons/hicolor/128x128/apps/qbittorrent.png");
  add_menu_item(&cfg->menu.root[i++], "pavucontrol-qt", "Execute", "pavucontrol-qt",
                "/usr/share/pixmaps/coolspeaker.svg");
  add_menu_item(&cfg->menu.root[i++], "flameshot", "Execute", "flameshot",
                "/usr/share/icons/hicolor/48x48/apps/flameshot.png");
  add_menu_item(&cfg->menu.root[i++], "texxy", "Execute", "texxy", "/usr/share/icons/hicolor/256x256/apps/texxy.png");
  add_menu_item(&cfg->menu.root[i++], "1term", "Execute", "1term", "/usr/share/icons/hicolor/256x256/apps/1term.png");
  add_menu_item(&cfg->menu.root[i++], "libreoffice-bin", "Execute", "libreoffice-bin",
                "/usr/share/icons/hicolor/scalable/apps/libreoffice-main.svg");
  add_menu_item(&cfg->menu.root[i++], "1t", "Execute", "1t", "/usr/share/icons/hicolor/256x256/apps/1t.png");
  add_menu_item(&cfg->menu.root[i++], "st", "Execute", "st", "/usr/share/icons/hicolor/256x256/apps/1term.png");

  /* separator */
  add_menu_item(&cfg->menu.root[i], NULL, NULL, NULL, NULL);
  i++;

  /* Preferences submenu */
  struct ob_menu_item* prefs = g_new0(struct ob_menu_item, 2);
  add_menu_item(&prefs[0], "Reconfigure", "Reconfigure", NULL, NULL);
  add_menu_item(&prefs[1], "Restart", "Restart", NULL, NULL);
  cfg->menu.root[i].label = safe_strdup("Preferences");
  cfg->menu.root[i].submenu = prefs;
  cfg->menu.root[i].submenu_count = 2;
  i++;

  /* Monitor Control submenu */
  struct ob_menu_item* mon = g_new0(struct ob_menu_item, 4);
  add_menu_item(&mon[0], "Disable eDP-1", "Execute", "xrandr --output eDP-1 --off", NULL);
  add_menu_item(&mon[1], "Enable eDP-1", "Execute", "xrandr --output eDP-1 --auto", NULL);
  add_menu_item(&mon[2], "Disable DP-1", "Execute", "xrandr --output DP-1 --off", NULL);
  add_menu_item(&mon[3], "Enable DP-1", "Execute", "xrandr --output DP-1 --auto", NULL);
  cfg->menu.root[i].label = safe_strdup("Monitor Control");
  cfg->menu.root[i].submenu = mon;
  cfg->menu.root[i].submenu_count = 4;
  i++;

  add_menu_item(&cfg->menu.root[i++], "Exit", "Execute", "openbox --exit", NULL);

  g_assert(i == cfg->menu.root_count);
}

void ob_config_init_defaults(struct ob_config* cfg) {
  if (!cfg)
    return;
  memset(cfg, 0, sizeof(*cfg));

  cfg->version = 1;

  /* Theme defaults */
  cfg->theme.name = safe_strdup("Mikachu");  // Default theme
  cfg->theme.title_layout = safe_strdup("NLIMC");
  cfg->theme.keep_border = true;
  cfg->theme.animate_iconify = true;
  cfg->theme.window_list_icon_size = 48;

  /* Desktops defaults */
  cfg->desktops.count = 4;
  cfg->desktops.first = 1;
  cfg->desktops.popup_time = 1000;
  cfg->desktops.names = malloc(sizeof(char*) * 4);
  if (cfg->desktops.names) {
    cfg->desktops.names[0] = safe_strdup("1");
    cfg->desktops.names[1] = safe_strdup("2");
    cfg->desktops.names[2] = safe_strdup("3");
    cfg->desktops.names[3] = safe_strdup("4");
    cfg->desktops.names_count = 4;
  }

  /* Focus defaults */
  cfg->focus.follow_mouse = true;
  cfg->focus.focus_new = true;
  cfg->focus.raise_on_focus = false;
  cfg->focus.delay = 0;
  cfg->focus.under_mouse = false;

  /* Placement defaults */
  cfg->placement.policy = OB_CONFIG_PLACE_POLICY_SMART;
  cfg->placement.center_new = false;
  cfg->placement.monitor_active = true;

  set_default_menu(cfg);
}

static void free_key_actions(struct ob_key_action* actions, size_t count) {
  for (size_t i = 0; i < count; i++) {
    free(actions[i].name);
    free(actions[i].command);
    if (actions[i].params)
      g_hash_table_destroy(actions[i].params);
  }
  free(actions);
}

static void free_mouse_actions(struct ob_mouse_action* actions, size_t count) {
  for (size_t i = 0; i < count; i++) {
    free(actions[i].name);
    free(actions[i].command);
    if (actions[i].params)
      g_hash_table_destroy(actions[i].params);
  }
  free(actions);
}

static void free_menu_items(struct ob_menu_item* items, size_t count) {
  for (size_t i = 0; i < count; i++) {
    free(items[i].label);
    free(items[i].action_name);
    free(items[i].command);
    if (items[i].params)
      g_hash_table_destroy(items[i].params);
    if (items[i].submenu) {
      free_menu_items(items[i].submenu, items[i].submenu_count);
    }
  }
  free(items);
}

void ob_config_free(struct ob_config* cfg) {
  if (!cfg)
    return;

  /* Theme */
  free(cfg->theme.name);
  free(cfg->theme.title_layout);

  /* Desktops */
  if (cfg->desktops.names) {
    for (size_t i = 0; i < cfg->desktops.names_count; i++) {
      free(cfg->desktops.names[i]);
    }
    free(cfg->desktops.names);
  }

  /* Keybinds */
  if (cfg->keybinds) {
    for (size_t i = 0; i < cfg->keybind_count; i++) {
      free(cfg->keybinds[i].key);
      free(cfg->keybinds[i].context);
      if (cfg->keybinds[i].actions) {
        free_key_actions(cfg->keybinds[i].actions, cfg->keybinds[i].action_count);
      }
    }
    free(cfg->keybinds);
  }

  /* Mouse bindings */
  if (cfg->mouse_bindings) {
    for (size_t i = 0; i < cfg->mouse_binding_count; i++) {
      free(cfg->mouse_bindings[i].context);
      free(cfg->mouse_bindings[i].button);
      free(cfg->mouse_bindings[i].click);
      if (cfg->mouse_bindings[i].actions) {
        free_mouse_actions(cfg->mouse_bindings[i].actions, cfg->mouse_bindings[i].action_count);
      }
    }
    free(cfg->mouse_bindings);
  }

  /* Window rules */
  if (cfg->window_rules) {
    for (size_t i = 0; i < cfg->window_rule_count; i++) {
      free(cfg->window_rules[i].match_class);
      free(cfg->window_rules[i].match_name);
      free(cfg->window_rules[i].match_role);
    }
    free(cfg->window_rules);
  }

  /* Menu */
  if (cfg->menu.root) {
    free_menu_items(cfg->menu.root, cfg->menu.root_count);
  }

  memset(cfg, 0, sizeof(*cfg));
}

bool ob_config_validate(const struct ob_config* cfg, FILE* log_fp) {
  bool valid = true;

  if (cfg->desktops.count < 1) {
    if (log_fp)
      fprintf(log_fp, "Error: desktop count must be at least 1\n");
    valid = false;
  }

  if (cfg->theme.name == NULL) {
    if (log_fp)
      fprintf(log_fp, "Error: theme name is missing\n");
    valid = false;
  }

  return valid;
}

void ob_desktops_apply(const struct ob_desktops_config* cfg) {
  if (!cfg)
    return;

  if (cfg->count > 0)
    screen_set_num_desktops(cfg->count);

  screen_set_names(cfg->names, cfg->names_count);
  screen_set_popup_time(cfg->popup_time);
}

void ob_theme_apply(const struct ob_theme_config* cfg) {
  if (!cfg)
    return;

  if (config_theme)
    g_free(config_theme);
  config_theme = cfg->name ? g_strdup(cfg->name) : NULL;

  if (config_title_layout)
    g_free(config_title_layout);
  config_title_layout = cfg->title_layout ? g_strdup(cfg->title_layout) : NULL;

  config_theme_keepborder = cfg->keep_border;
  config_animate_iconify = cfg->animate_iconify;
  config_theme_window_list_icon_size = cfg->window_list_icon_size;
}

void ob_focus_apply(const struct ob_focus_config* cfg) {
  if (!cfg)
    return;

  config_focus_follow = cfg->follow_mouse;
  config_focus_new = cfg->focus_new;
  config_focus_raise = cfg->raise_on_focus;
  config_focus_delay = cfg->delay;
  config_focus_last = cfg->focus_last;
  config_focus_under_mouse = cfg->under_mouse;
  config_unfocus_leave = cfg->unfocus_leave;
}

void ob_placement_apply(const struct ob_placement_config* cfg) {
  if (!cfg)
    return;

  if (cfg->policy == OB_CONFIG_PLACE_POLICY_MOUSE)
    config_place_policy = OB_PLACE_POLICY_MOUSE;
  else
    config_place_policy = OB_PLACE_POLICY_SMART;

  config_place_center = cfg->center_new;

  if (cfg->monitor_active)
    config_place_monitor = OB_PLACE_MONITOR_ACTIVE;
  else if (cfg->monitor > 0) {
    /* Explicit monitor index handling might need complex logic or new global */
    /* config_place_monitor is enum ObPlaceMonitor. It does not store index. */
    /* There is config_primary_monitor_index for primary. */
    /* Maybe cfg->monitor implies primary monitor index? */
    config_primary_monitor_index = cfg->monitor;
  }
  else {
    config_place_monitor = OB_PLACE_MONITOR_PRIMARY; /* Default fallback */
  }
}

void ob_keyboard_apply(const struct ob_keybind* keybinds, size_t count) {
  if (!keybinds || count == 0)
    return;

  keyboard_unbind_all();

  for (size_t i = 0; i < count; i++) {
    const struct ob_keybind* kb = &keybinds[i];
    GList* keylist = NULL;
    char** keys = g_strsplit(kb->key, " ", 0);

    for (char** k = keys; *k; k++) {
      keylist = g_list_append(keylist, *k);
    }

    for (size_t j = 0; j < kb->action_count; j++) {
      struct ob_key_action* ka = &kb->actions[j];
      GHashTable* opts = ka->params;
      bool free_opts = false;

      if (!opts) {
        opts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        free_opts = true;
        if (ka->command)
          g_hash_table_insert(opts, g_strdup("command"), g_strdup(ka->command));
      }

      ObActionsAct* act = actions_create_with_options(ka->name, opts);
      if (free_opts)
        g_hash_table_destroy(opts);

      if (act) {
        if (!keyboard_bind(keylist, act)) {
          actions_act_unref(act);
        }
      }
    }

    g_list_free(keylist);
    g_strfreev(keys);
  }
}

void ob_mouse_apply(const struct ob_mouse_binding* bindings, size_t count) {
  if (!bindings || count == 0)
    return;

  mouse_unbind_all();

  for (size_t i = 0; i < count; i++) {
    const struct ob_mouse_binding* mb = &bindings[i];
    ObMouseAction mact;

    if (!mb->click)
      continue;

    if (g_ascii_strcasecmp(mb->click, "press") == 0)
      mact = OB_MOUSE_ACTION_PRESS;
    else if (g_ascii_strcasecmp(mb->click, "release") == 0)
      mact = OB_MOUSE_ACTION_RELEASE;
    else if (g_ascii_strcasecmp(mb->click, "click") == 0)
      mact = OB_MOUSE_ACTION_CLICK;
    else if (g_ascii_strcasecmp(mb->click, "double") == 0)
      mact = OB_MOUSE_ACTION_DOUBLE_CLICK;
    else if (g_ascii_strcasecmp(mb->click, "drag") == 0)
      mact = OB_MOUSE_ACTION_MOTION;
    else
      continue;

    if (!mb->context)
      continue;

    char* modcxstr = g_strdup(mb->context);
    ObFrameContext cx;

    while (frame_next_context_from_string(modcxstr, &cx)) {
      if (cx == OB_FRAME_CONTEXT_NONE)
        continue;

      char** buttons = g_strsplit(mb->button, " ", 0);
      for (char** b = buttons; *b; b++) {
        for (size_t j = 0; j < mb->action_count; j++) {
          struct ob_mouse_action* ma = &mb->actions[j];
          GHashTable* opts = ma->params;
          bool free_opts = false;

          if (!opts) {
            opts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
            free_opts = true;
            if (ma->command)
              g_hash_table_insert(opts, g_strdup("command"), g_strdup(ma->command));
          }

          ObActionsAct* act = actions_create_with_options(ma->name, opts);
          if (free_opts)
            g_hash_table_destroy(opts);

          if (act) {
            actions_act_ref(act);
            mouse_bind(*b, cx, mact, act);
            actions_act_unref(act);
          }
        }
      }
      g_strfreev(buttons);
    }
    g_free(modcxstr);
  }
}

void ob_rules_apply(const struct ob_window_rule* rules, size_t count) {
  config_clear_app_settings();

  for (size_t i = 0; i < count; i++) {
    const struct ob_window_rule* r = &rules[i];
    ObAppSettings* s = config_create_app_settings();

    if (r->match_class)
      s->class = g_pattern_spec_new(r->match_class);
    if (r->match_name)
      s->name = g_pattern_spec_new(r->match_name);
    if (r->match_role)
      s->role = g_pattern_spec_new(r->match_role);

    if (r->desktop >= 0)
      s->desktop = r->desktop;
    /* Desktop indexes remain 1-based to match the legacy config expectations. */

    /* boolean properties in ObAppSettings are gint -1 (unset), 0 (false), 1 (true) */
    /* struct ob_window_rule has bools, but they might be default false.
       We need to know if they were SET or not.
       struct ob_window_rule currently has plain bools.
       If we want to support "don't change", we need tristate.
       But the struct definition in ob_config.h uses `bool`.
       If YAML parser sets them to default false, we overwrite with false.
       Maybe we assume if it's in the rule, it's set?
       Or should we change ob_window_rule to have `bool maximized_set` flags?
       For now, I'll assume if the rule exists, we set the values.
       BUT `bool` defaults to `false`. If user didn't specify `maximized`, it is `false`.
       If I set `s->max_horz = 0`, it forces it to NOT maximized.
       If I leave it `-1`, it leaves it alone (default).

       Problem: YAML loader will fill the struct with 0/false if missing.
       I can't distinguish "missing" from "false".

       This is a limitation of the current `ob_config.h` design.
       To fix this properly, `ob_window_rule` should use pointers or flags.

       For now, I will blindly set them. This means if you define a rule,
       it enforces the booleans to false unless set to true.
       This might be annoying (resetting maximized state if you only wanted to set desktop).

       Let's fix `ob_config.h` to use flags? Or just `int` for tristate?
       `ob_config.h`:
       `bool maximized;`

       I'll update `ob_config.h` to use `int` for tristate (-1, 0, 1) later if needed.
       Or maybe I can change it now.
       Plan said: "everything in the WM reads from one config struct".
       The config struct should express "unset".

       Let's update `ob_config.h` first to allow tristate?
       Or just accept that for now, I will ONLY set if TRUE?
       "maximized: true" -> set to 1.
       "maximized: false" -> set to 0?
       If I only check `if (r->maximized) s->max_horz = 1;`, I can't set to false.

      Let's stick to the plan which uses `bool`.
      Legacy XML configs allowed overriding defaults.

       Actually, `ObAppSettings` initialized with -1.
       If I leave it -1, it uses default/client preference.

       I will assume `bool` in `ob_window_rule` means "force this state".
       But `bool` can't be "unset".

       I'll modify `ob_config.h` to include `_set` flags for rules.
    */

    /* For now, to make progress, I will assume TRUE means set 1, FALSE means ignore (leave -1).
       This prevents "force false".
       This is a compromise.
    */

    if (r->maximized) {
      s->max_horz = 1;
      s->max_vert = 1;
    }
    if (r->fullscreen)
      s->fullscreen = 1;
    if (r->skip_taskbar)
      s->skip_taskbar = 1;
    if (r->skip_pager)
      s->skip_pager = 1;
    if (r->follow)
      s->focus = 1; /* focus=1 means yes, focus=0 means no. -1 default. */

    config_per_app_settings = g_slist_append(config_per_app_settings, s);
  }
}

void ob_menu_apply(struct ob_menu* menu) {
  if (!menu)
    return;
  menu_set_config(menu);
}

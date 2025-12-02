#ifndef OB_CONFIG_H
#define OB_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <glib.h>

struct ob_theme_config {
  char* name;
  char* title_layout;
  bool keep_border;
  bool animate_iconify;
  int window_list_icon_size;
  /* Fonts could go here or separate */
};

struct ob_desktops_config {
  int count;
  int first;
  char** names; /* Array of strings */
  size_t names_count;
  int popup_time;
};

struct ob_focus_config {
  bool follow_mouse;
  bool focus_new;
  bool raise_on_focus;
  int delay;
  bool focus_last;
  bool under_mouse;
  bool unfocus_leave;
};

enum ob_place_policy { OB_CONFIG_PLACE_POLICY_SMART, OB_CONFIG_PLACE_POLICY_MOUSE };

struct ob_placement_config {
  enum ob_place_policy policy;
  bool center_new;
  int monitor;         /* 0 for primary, or explicit index */
  bool monitor_active; /* Place on active monitor */
};

struct ob_key_action {
  char* name;
  char* command;
  GHashTable* params;
};

struct ob_keybind {
  char* key;
  char* context;
  struct ob_key_action* actions;
  size_t action_count;
};

struct ob_mouse_action {
  char* name;
  char* command;
  GHashTable* params;
};

struct ob_mouse_binding {
  char* context;
  char* button; /* "left", "right", "middle", "button8", etc */
  char* click;  /* "press", "release", "double", "drag" */
  struct ob_mouse_action* actions;
  size_t action_count;
};

struct ob_window_rule {
  char* match_class;
  char* match_name;
  char* match_role;

  int desktop; /* -1 for unchanged */
  bool maximized;
  bool fullscreen;
  bool skip_taskbar;
  bool skip_pager;
  bool follow; /* Jump to desktop */
               /* Add other rule properties as needed */
};

struct ob_menu_item {
  char* label;
  char* action_name;
  char* command;
  struct ob_menu_item* submenu;
  size_t submenu_count;
  GHashTable* params;
};

struct ob_menu {
  struct ob_menu_item* root;
  size_t root_count;
};

struct ob_config {
  int version;
  struct ob_theme_config theme;
  struct ob_desktops_config desktops;
  struct ob_focus_config focus;
  struct ob_placement_config placement;

  struct ob_keybind* keybinds;
  size_t keybind_count;

  struct ob_mouse_binding* mouse_bindings;
  size_t mouse_binding_count;

  struct ob_window_rule* window_rules;
  size_t window_rule_count;

  struct ob_menu menu;
};

void ob_config_init_defaults(struct ob_config* cfg);
void ob_config_free(struct ob_config* cfg);
bool ob_config_validate(const struct ob_config* cfg, FILE* log_fp);
bool ob_config_load_yaml(const char* path, struct ob_config* cfg);
bool ob_config_dump_yaml(const struct ob_config* cfg, FILE* out);

void ob_desktops_apply(const struct ob_desktops_config* cfg);
void ob_theme_apply(const struct ob_theme_config* cfg);
void ob_focus_apply(const struct ob_focus_config* cfg);
void ob_placement_apply(const struct ob_placement_config* cfg);
void ob_keyboard_apply(const struct ob_keybind* keybinds, size_t count);
void ob_mouse_apply(const struct ob_mouse_binding* bindings, size_t count);
void ob_rules_apply(const struct ob_window_rule* rules, size_t count);
void ob_menu_apply(struct ob_menu* menu);

#endif

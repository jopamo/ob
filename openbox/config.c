/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   config.c for the Openbox window manager
*/

#include "config.h"
#include "keyboard.h"
#include "mouse.h"
#include "actions.h"
#include "translate.h"
#include "client.h"
#include "screen.h"
#include "openbox.h"
#include "gettext.h"
#include <stdlib.h>

gboolean config_focus_new;
gboolean config_focus_follow;
guint config_focus_delay;
gboolean config_focus_raise;
gboolean config_focus_last;
gboolean config_focus_under_mouse;
gboolean config_unfocus_leave;

ObPlacePolicy config_place_policy;
gboolean config_place_center;
ObPlaceMonitor config_place_monitor;

guint config_primary_monitor_index;
ObPlaceMonitor config_primary_monitor;

StrutPartial config_margins;

gchar* config_theme;
gboolean config_theme_keepborder;
guint config_theme_window_list_icon_size;

gchar* config_title_layout;

gboolean config_animate_iconify;

RrFont* config_font_activewindow;
RrFont* config_font_inactivewindow;
RrFont* config_font_menuitem;
RrFont* config_font_menutitle;
RrFont* config_font_activeosd;
RrFont* config_font_inactiveosd;

guint config_desktops_num;
GSList* config_desktops_names;
guint config_screen_firstdesk;
guint config_desktop_popup_time;

gboolean config_resize_redraw;
gint config_resize_popup_show;
ObResizePopupPos config_resize_popup_pos;
GravityPoint config_resize_popup_fixed;

ObStackingLayer config_dock_layer;
gboolean config_dock_floating;
gboolean config_dock_nostrut;
ObDirection config_dock_pos;
gint config_dock_x;
gint config_dock_y;
ObOrientation config_dock_orient;
gboolean config_dock_hide;
guint config_dock_hide_delay;
guint config_dock_show_delay;
guint config_dock_app_move_button;
guint config_dock_app_move_modifiers;

guint config_keyboard_reset_keycode;
guint config_keyboard_reset_state;
gboolean config_keyboard_rebind_on_mapping_notify;

gint config_mouse_threshold;
gint config_mouse_dclicktime;
gint config_mouse_screenedgetime;
gboolean config_mouse_screenedgewarp;

guint config_menu_hide_delay;
gboolean config_menu_middle;
guint config_submenu_show_delay;
guint config_submenu_hide_delay;
gboolean config_menu_manage_desktops;
gboolean config_menu_show_icons;

gint config_resist_win;
gint config_resist_edge;

GSList* config_per_app_settings;

ObAppSettings* config_create_app_settings(void) {
  ObAppSettings* settings = g_slice_new0(ObAppSettings);
  settings->type = -1;
  settings->decor = -1;
  settings->shade = -1;
  settings->monitor_type = OB_PLACE_MONITOR_ANY;
  settings->monitor = -1;
  settings->focus = -1;
  settings->desktop = 0;
  settings->layer = -2;
  settings->iconic = -1;
  settings->skip_pager = -1;
  settings->skip_taskbar = -1;
  settings->fullscreen = -1;
  settings->max_horz = -1;
  settings->max_vert = -1;
  return settings;
}

#define copy_if(setting, default) \
  if (src->setting != default)    \
  dst->setting = src->setting
void config_app_settings_copy_non_defaults(const ObAppSettings* src, ObAppSettings* dst) {
  g_assert(src != NULL);
  g_assert(dst != NULL);

  copy_if(type, (ObClientType)-1);
  copy_if(decor, -1);
  copy_if(shade, -1);
  copy_if(monitor_type, OB_PLACE_MONITOR_ANY);
  copy_if(monitor, -1);
  copy_if(focus, -1);
  copy_if(desktop, 0);
  copy_if(layer, -2);
  copy_if(iconic, -1);
  copy_if(skip_pager, -1);
  copy_if(skip_taskbar, -1);
  copy_if(fullscreen, -1);
  copy_if(max_horz, -1);
  copy_if(max_vert, -1);

  if (src->pos_given) {
    dst->pos_given = TRUE;
    dst->pos_force = src->pos_force;
    dst->position = src->position;
  }

  dst->width_num = src->width_num;
  dst->width_denom = src->width_denom;
  dst->height_num = src->height_num;
  dst->height_denom = src->height_denom;
}

void config_parse_relative_number(gchar* s, gint* num, gint* denom) {
  *num = strtol(s, &s, 10);

  if (*s == '%') {
    *denom = 100;
  }
  else if (*s == '/') {
    *denom = atoi(s + 1);
  }
}

/* Default keybindings mirroring the legacy XML defaults */
static void bind_default_keyboard(void) {
  struct {
    const gchar* keys;
    const gchar* act;
  } binds[] = {{"C-A-Left", "DesktopPrevious"},
               {"C-A-Right", "DesktopNext"},
               {"A-F4", "Close"},
               {"A-Tab", "NextWindow"},
               {"A-S-Tab", "PreviousWindow"},
               {"W-F1", "DesktopFirst"},
               {"W-F2", "DesktopSecond"},
               {"W-F3", "DesktopThird"},
               {"W-F4", "DesktopFourth"},
               {NULL, NULL}};
  for (guint i = 0; binds[i].keys; ++i) {
    GList* l = NULL;
    gchar** keys = g_strsplit(binds[i].keys, " ", 0);
    for (gchar** k = keys; *k; ++k)
      l = g_list_append(l, *k);
    keyboard_bind(l, actions_parse_string(binds[i].act));
    g_list_free(l);
    g_strfreev(keys);
  }
}

/* Default mouse bindings mirroring the legacy XML defaults */
typedef struct {
  const gchar* button;
  const gchar* context;
  const ObMouseAction mact;
  const gchar* actname;
  const gchar* menu_name; /* optional ShowMenu target */
} ObDefMouseBind;

static void bind_default_mouse(void) {
  ObDefMouseBind* it;
  ObDefMouseBind binds[] = {{"Left", "Client", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Middle", "Client", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Right", "Client", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "Desktop", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Middle", "Desktop", OB_MOUSE_ACTION_PRESS, "ShowMenu", "client-list-combined-menu"},
                            {"Right", "Desktop", OB_MOUSE_ACTION_PRESS, "ShowMenu", "root-menu"},
                            {"Left", "Titlebar", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "Bottom", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "BLCorner", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "BRCorner", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "TLCorner", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "TRCorner", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "Close", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "Maximize", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "Iconify", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "Icon", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "AllDesktops", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "Shade", OB_MOUSE_ACTION_PRESS, "Focus", NULL},
                            {"Left", "Client", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "Titlebar", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Middle", "Titlebar", OB_MOUSE_ACTION_CLICK, "Lower", NULL},
                            {"Left", "BLCorner", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "BRCorner", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "TLCorner", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "TRCorner", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "Close", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "Maximize", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "Iconify", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "Icon", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "AllDesktops", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "Shade", OB_MOUSE_ACTION_CLICK, "Raise", NULL},
                            {"Left", "Close", OB_MOUSE_ACTION_CLICK, "Close", NULL},
                            {"Left", "Maximize", OB_MOUSE_ACTION_CLICK, "ToggleMaximize", NULL},
                            {"Left", "Iconify", OB_MOUSE_ACTION_CLICK, "Iconify", NULL},
                            {"Left", "AllDesktops", OB_MOUSE_ACTION_CLICK, "ToggleOmnipresent", NULL},
                            {"Left", "Shade", OB_MOUSE_ACTION_CLICK, "ToggleShade", NULL},
                            {"Left", "TLCorner", OB_MOUSE_ACTION_MOTION, "Resize", NULL},
                            {"Left", "TRCorner", OB_MOUSE_ACTION_MOTION, "Resize", NULL},
                            {"Left", "BLCorner", OB_MOUSE_ACTION_MOTION, "Resize", NULL},
                            {"Left", "BRCorner", OB_MOUSE_ACTION_MOTION, "Resize", NULL},
                            {"Left", "Top", OB_MOUSE_ACTION_MOTION, "Resize", NULL},
                            {"Left", "Bottom", OB_MOUSE_ACTION_MOTION, "Resize", NULL},
                            {"Left", "Left", OB_MOUSE_ACTION_MOTION, "Resize", NULL},
                            {"Left", "Right", OB_MOUSE_ACTION_MOTION, "Resize", NULL},
                            {"Left", "Titlebar", OB_MOUSE_ACTION_MOTION, "Move", NULL},
                            {"A-Left", "Frame", OB_MOUSE_ACTION_MOTION, "Move", NULL},
                            {"A-Middle", "Frame", OB_MOUSE_ACTION_MOTION, "Resize", NULL},
                            {NULL, NULL, 0, NULL, NULL}};

  for (it = binds; it->button; ++it) {
    ObActionsAct* act = NULL;
    if (it->menu_name) {
      GHashTable* opts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      g_hash_table_insert(opts, g_strdup("menu"), g_strdup(it->menu_name));
      act = actions_create_with_options(it->actname, opts);
      g_hash_table_destroy(opts);
    }
    else {
      act = actions_parse_string(it->actname);
    }
    if (act)
      mouse_bind(it->button, frame_context_from_string(it->context), it->mact, act);
  }
}

void config_load_defaults(void) {
  config_focus_new = TRUE;
  config_focus_follow = FALSE;
  config_focus_delay = 0;
  config_focus_raise = FALSE;
  config_focus_last = TRUE;
  config_focus_under_mouse = FALSE;
  config_unfocus_leave = FALSE;

  config_place_policy = OB_PLACE_POLICY_SMART;
  config_place_center = TRUE;
  config_place_monitor = OB_PLACE_MONITOR_PRIMARY;

  config_primary_monitor_index = 1;
  config_primary_monitor = OB_PLACE_MONITOR_ACTIVE;

  STRUT_PARTIAL_SET(config_margins, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  config_theme = NULL;
  config_animate_iconify = TRUE;
  config_title_layout = g_strdup("NLIMC");
  config_theme_keepborder = TRUE;
  config_theme_window_list_icon_size = 36;

  config_font_activewindow = NULL;
  config_font_inactivewindow = NULL;
  config_font_menuitem = NULL;
  config_font_menutitle = NULL;
  config_font_activeosd = NULL;
  config_font_inactiveosd = NULL;

  config_desktops_num = 4;
  config_screen_firstdesk = 1;
  config_desktops_names = NULL;
  config_desktop_popup_time = 875;

  config_resize_redraw = TRUE;
  config_resize_popup_show = 1; /* nonpixel increments */
  config_resize_popup_pos = OB_RESIZE_POS_CENTER;
  GRAVITY_COORD_SET(config_resize_popup_fixed.x, 0, FALSE, FALSE);
  GRAVITY_COORD_SET(config_resize_popup_fixed.y, 0, FALSE, FALSE);

  config_dock_layer = OB_STACKING_LAYER_ABOVE;
  config_dock_pos = OB_DIRECTION_NORTHEAST;
  config_dock_floating = FALSE;
  config_dock_nostrut = FALSE;
  config_dock_x = 0;
  config_dock_y = 0;
  config_dock_orient = OB_ORIENTATION_VERT;
  config_dock_hide = FALSE;
  config_dock_hide_delay = 300;
  config_dock_show_delay = 300;
  config_dock_app_move_button = 2; /* middle */
  config_dock_app_move_modifiers = 0;

  translate_key("C-g", &config_keyboard_reset_state, &config_keyboard_reset_keycode);
  config_keyboard_rebind_on_mapping_notify = TRUE;

  keyboard_unbind_all();
  bind_default_keyboard();

  config_mouse_threshold = 8;
  config_mouse_dclicktime = 500;
  config_mouse_screenedgetime = 400;
  config_mouse_screenedgewarp = FALSE;

  mouse_unbind_all();
  bind_default_mouse();

  config_resist_win = 10;
  config_resist_edge = 20;

  config_menu_hide_delay = 250;
  config_menu_middle = FALSE;
  config_submenu_show_delay = 100;
  config_submenu_hide_delay = 400;
  config_menu_manage_desktops = TRUE;
  config_menu_show_icons = TRUE;

  config_per_app_settings = NULL;
}

void config_clear_app_settings(void) {
  GSList* it;
  for (it = config_per_app_settings; it; it = g_slist_next(it)) {
    ObAppSettings* itd = (ObAppSettings*)it->data;
    if (itd->name)
      g_pattern_spec_free(itd->name);
    if (itd->role)
      g_pattern_spec_free(itd->role);
    if (itd->title)
      g_pattern_spec_free(itd->title);
    if (itd->class)
      g_pattern_spec_free(itd->class);
    if (itd->group_name)
      g_pattern_spec_free(itd->group_name);
    if (itd->group_class)
      g_pattern_spec_free(itd->group_class);
    g_slice_free(ObAppSettings, it->data);
  }
  g_slist_free(config_per_app_settings);
  config_per_app_settings = NULL;
}

void config_shutdown(void) {
  GSList* it;

  g_free(config_theme);
  config_theme = NULL;

  g_free(config_title_layout);
  config_title_layout = NULL;

  RrFontClose(config_font_activewindow);
  RrFontClose(config_font_inactivewindow);
  RrFontClose(config_font_menuitem);
  RrFontClose(config_font_menutitle);
  RrFontClose(config_font_activeosd);
  RrFontClose(config_font_inactiveosd);

  for (it = config_desktops_names; it; it = g_slist_next(it))
    g_free(it->data);
  g_slist_free(config_desktops_names);
  config_desktops_names = NULL;

  config_clear_app_settings();
}

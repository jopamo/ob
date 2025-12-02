#include "openbox/actions.h"
#include "openbox/actions/helpers.h"
#include "openbox/screen.h"
#include "openbox/client.h"
#include "openbox/openbox.h"
#include "obt/keyboard.h"
#include <stdlib.h>
#include <string.h>

typedef enum { LAST, CURRENT, RELATIVE, ABSOLUTE } SwitchType;

typedef struct {
  SwitchType type;
  union {
    struct {
      guint desktop;
    } abs;

    struct {
      gboolean linear;
      gboolean wrap;
      ObDirection dir;
    } rel;
  } u;
  gboolean send;
  gboolean follow;
} Options;

static gpointer setup_func(GHashTable* options);
static gpointer setup_send_func(GHashTable* options);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData* data, gpointer options);

/* 3.4-compatibility */
static gpointer setup_go_last_func(GHashTable* options);
static gpointer setup_send_last_func(GHashTable* options);
static gpointer setup_go_abs_func(GHashTable* options);
static gpointer setup_go_next_func(GHashTable* options);
static gpointer setup_send_next_func(GHashTable* options);
static gpointer setup_go_prev_func(GHashTable* options);
static gpointer setup_send_prev_func(GHashTable* options);
static gpointer setup_go_left_func(GHashTable* options);
static gpointer setup_send_left_func(GHashTable* options);
static gpointer setup_go_right_func(GHashTable* options);
static gpointer setup_send_right_func(GHashTable* options);
static gpointer setup_go_up_func(GHashTable* options);
static gpointer setup_send_up_func(GHashTable* options);
static gpointer setup_go_down_func(GHashTable* options);
static gpointer setup_send_down_func(GHashTable* options);
static gpointer setup_go_first_func(GHashTable* options);
static gpointer setup_go_second_func(GHashTable* options);
static gpointer setup_go_third_func(GHashTable* options);
static gpointer setup_go_fourth_func(GHashTable* options);
static gpointer setup_send_first_func(GHashTable* options);
static gpointer setup_send_second_func(GHashTable* options);
static gpointer setup_send_third_func(GHashTable* options);
static gpointer setup_send_fourth_func(GHashTable* options);

void action_desktop_startup(void) {
  actions_register_opt("GoToDesktop", setup_func, free_func, run_func);
  actions_register_opt("SendToDesktop", setup_send_func, free_func, run_func);
  /* 3.4-compatibility */
  actions_register_opt("DesktopLast", setup_go_last_func, free_func, run_func);
  actions_register_opt("SendToDesktopLast", setup_send_last_func, free_func, run_func);
  actions_register_opt("Desktop", setup_go_abs_func, free_func, run_func);
  actions_register_opt("DesktopNext", setup_go_next_func, free_func, run_func);
  actions_register_opt("SendToDesktopNext", setup_send_next_func, free_func, run_func);
  actions_register_opt("DesktopPrevious", setup_go_prev_func, free_func, run_func);
  actions_register_opt("SendToDesktopPrevious", setup_send_prev_func, free_func, run_func);
  actions_register_opt("DesktopLeft", setup_go_left_func, free_func, run_func);
  actions_register_opt("SendToDesktopLeft", setup_send_left_func, free_func, run_func);
  actions_register_opt("DesktopRight", setup_go_right_func, free_func, run_func);
  actions_register_opt("SendToDesktopRight", setup_send_right_func, free_func, run_func);
  actions_register_opt("DesktopUp", setup_go_up_func, free_func, run_func);
  actions_register_opt("SendToDesktopUp", setup_send_up_func, free_func, run_func);
  actions_register_opt("DesktopDown", setup_go_down_func, free_func, run_func);
  actions_register_opt("SendToDesktopDown", setup_send_down_func, free_func, run_func);
  /* default keybindings compatibility */
  actions_register_opt("DesktopFirst", setup_go_first_func, free_func, run_func);
  actions_register_opt("DesktopSecond", setup_go_second_func, free_func, run_func);
  actions_register_opt("DesktopThird", setup_go_third_func, free_func, run_func);
  actions_register_opt("DesktopFourth", setup_go_fourth_func, free_func, run_func);
  actions_register_opt("SendToDesktopFirst", setup_send_first_func, free_func, run_func);
  actions_register_opt("SendToDesktopSecond", setup_send_second_func, free_func, run_func);
  actions_register_opt("SendToDesktopThird", setup_send_third_func, free_func, run_func);
  actions_register_opt("SendToDesktopFourth", setup_send_fourth_func, free_func, run_func);
}

static gpointer setup_func(GHashTable* options) {
  Options* o;
  const char* s;
  const char* wrap_s;

  o = g_slice_new0(Options);
  /* don't go anywhere if there are no options given */
  o->type = ABSOLUTE;
  o->u.abs.desktop = screen_desktop;
  /* wrap by default - it's handy! */
  o->u.rel.wrap = TRUE;

  if ((s = g_hash_table_lookup(options, "to"))) {
    if (!g_ascii_strcasecmp(s, "last"))
      o->type = LAST;
    else if (!g_ascii_strcasecmp(s, "current"))
      o->type = CURRENT;
    else if (!g_ascii_strcasecmp(s, "next")) {
      o->type = RELATIVE;
      o->u.rel.linear = TRUE;
      o->u.rel.dir = OB_DIRECTION_EAST;
    }
    else if (!g_ascii_strcasecmp(s, "previous")) {
      o->type = RELATIVE;
      o->u.rel.linear = TRUE;
      o->u.rel.dir = OB_DIRECTION_WEST;
    }
    else if (!g_ascii_strcasecmp(s, "north") || !g_ascii_strcasecmp(s, "up")) {
      o->type = RELATIVE;
      o->u.rel.dir = OB_DIRECTION_NORTH;
    }
    else if (!g_ascii_strcasecmp(s, "south") || !g_ascii_strcasecmp(s, "down")) {
      o->type = RELATIVE;
      o->u.rel.dir = OB_DIRECTION_SOUTH;
    }
    else if (!g_ascii_strcasecmp(s, "west") || !g_ascii_strcasecmp(s, "left")) {
      o->type = RELATIVE;
      o->u.rel.dir = OB_DIRECTION_WEST;
    }
    else if (!g_ascii_strcasecmp(s, "east") || !g_ascii_strcasecmp(s, "right")) {
      o->type = RELATIVE;
      o->u.rel.dir = OB_DIRECTION_EAST;
    }
    else {
      o->type = ABSOLUTE;
      o->u.abs.desktop = atoi(s) - 1;
    }
  }

  if ((wrap_s = g_hash_table_lookup(options, "wrap")))
    o->u.rel.wrap = actions_parse_bool(wrap_s);

  return o;
}

static gpointer setup_send_func(GHashTable* options) {
  Options* o;
  const char* s;
  const char* follow_s;

  o = setup_func(options);
  if ((s = g_hash_table_lookup(options, "desktop"))) {
    /* 3.4 compatibility */
    o->u.abs.desktop = atoi(s) - 1;
    o->type = ABSOLUTE;
  }
  o->send = TRUE;
  o->follow = TRUE;

  if ((follow_s = g_hash_table_lookup(options, "follow")))
    o->follow = actions_parse_bool(follow_s);

  return o;
}

static void free_func(gpointer o) {
  g_slice_free(Options, o);
}

static gboolean run_func(ObActionsData* data, gpointer options) {
  Options* o = options;
  guint d;

  switch (o->type) {
    case LAST:
      d = screen_last_desktop;
      break;
    case CURRENT:
      d = screen_desktop;
      break;
    case ABSOLUTE:
      d = o->u.abs.desktop;
      break;
    case RELATIVE:
      d = screen_find_desktop(screen_desktop, o->u.rel.dir, o->u.rel.wrap, o->u.rel.linear);
      break;
    default:
      g_assert_not_reached();
  }

  if (d < screen_num_desktops && (d != screen_desktop || (data->client && data->client->desktop != screen_desktop))) {
    gboolean go = TRUE;

    actions_client_move(data, TRUE);
    if (o->send && data->client && client_normal(data->client)) {
      client_set_desktop(data->client, d, o->follow, FALSE);
      go = o->follow;
    }

    if (go) {
      screen_set_desktop(d, TRUE);
      if (data->client)
        client_bring_helper_windows(data->client);
    }

    actions_client_move(data, FALSE);
  }

  return FALSE;
}

/* 3.4-compatilibity */
static gpointer setup_follow(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  const char* follow_s;
  o->send = TRUE;
  o->follow = TRUE;
  if ((follow_s = g_hash_table_lookup(options, "follow")))
    o->follow = actions_parse_bool(follow_s);
  return o;
}

static gpointer setup_go_last_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  o->type = LAST;
  return o;
}

static gpointer setup_send_last_func(GHashTable* options) {
  Options* o = setup_follow(options);
  o->type = LAST;
  return o;
}

static gpointer setup_go_abs_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  const char* s;
  o->type = ABSOLUTE;
  if ((s = g_hash_table_lookup(options, "desktop")))
    o->u.abs.desktop = atoi(s) - 1;
  else
    o->u.abs.desktop = screen_desktop;
  return o;
}

static void setup_rel(Options* o, GHashTable* options, gboolean lin, ObDirection dir) {
  const char* wrap_s;

  o->type = RELATIVE;
  o->u.rel.linear = lin;
  o->u.rel.dir = dir;
  o->u.rel.wrap = TRUE;

  if ((wrap_s = g_hash_table_lookup(options, "wrap")))
    o->u.rel.wrap = actions_parse_bool(wrap_s);
}

static gpointer setup_go_next_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  setup_rel(o, options, TRUE, OB_DIRECTION_EAST);
  return o;
}

static gpointer setup_send_next_func(GHashTable* options) {
  Options* o = setup_follow(options);
  setup_rel(o, options, TRUE, OB_DIRECTION_EAST);
  return o;
}

static gpointer setup_go_prev_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  setup_rel(o, options, TRUE, OB_DIRECTION_WEST);
  return o;
}

static gpointer setup_send_prev_func(GHashTable* options) {
  Options* o = setup_follow(options);
  setup_rel(o, options, TRUE, OB_DIRECTION_WEST);
  return o;
}

static gpointer setup_go_left_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  setup_rel(o, options, FALSE, OB_DIRECTION_WEST);
  return o;
}

static gpointer setup_send_left_func(GHashTable* options) {
  Options* o = setup_follow(options);
  setup_rel(o, options, FALSE, OB_DIRECTION_WEST);
  return o;
}

static gpointer setup_go_right_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  setup_rel(o, options, FALSE, OB_DIRECTION_EAST);
  return o;
}

static gpointer setup_send_right_func(GHashTable* options) {
  Options* o = setup_follow(options);
  setup_rel(o, options, FALSE, OB_DIRECTION_EAST);
  return o;
}

static gpointer setup_go_up_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  setup_rel(o, options, FALSE, OB_DIRECTION_NORTH);
  return o;
}

static gpointer setup_send_up_func(GHashTable* options) {
  Options* o = setup_follow(options);
  setup_rel(o, options, FALSE, OB_DIRECTION_NORTH);
  return o;
}

static gpointer setup_go_down_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  setup_rel(o, options, FALSE, OB_DIRECTION_SOUTH);
  return o;
}

static gpointer setup_send_down_func(GHashTable* options) {
  Options* o = setup_follow(options);
  setup_rel(o, options, FALSE, OB_DIRECTION_SOUTH);
  return o;
}

static Options* setup_absolute(guint desktop, gboolean send) {
  Options* o = g_slice_new0(Options);
  o->type = ABSOLUTE;
  o->u.abs.desktop = desktop;
  o->send = send;
  o->follow = send;
  return o;
}

static gpointer setup_go_first_func(GHashTable* options) {
  return setup_absolute(0, FALSE);
}

static gpointer setup_go_second_func(GHashTable* options) {
  return setup_absolute(1, FALSE);
}

static gpointer setup_go_third_func(GHashTable* options) {
  return setup_absolute(2, FALSE);
}

static gpointer setup_go_fourth_func(GHashTable* options) {
  return setup_absolute(3, FALSE);
}

static gpointer setup_send_first_func(GHashTable* options) {
  return setup_absolute(0, TRUE);
}

static gpointer setup_send_second_func(GHashTable* options) {
  return setup_absolute(1, TRUE);
}

static gpointer setup_send_third_func(GHashTable* options) {
  return setup_absolute(2, TRUE);
}

static gpointer setup_send_fourth_func(GHashTable* options) {
  return setup_absolute(3, TRUE);
}

#include "openbox/actions.h"
#include "openbox/event.h"
#include "openbox/stacking.h"
#include "openbox/window.h"
#include "openbox/focus_cycle.h"
#include "openbox/openbox.h"
#include "openbox/misc.h"
#include "gettext.h"
#include "obt/keyboard.h"
#include "openbox/actions/helpers.h"

typedef struct {
  gboolean interactive;
  gboolean dialog;
  gboolean dock_windows;
  gboolean desktop_windows;
  ObDirection direction;
  gboolean bar;
  gboolean raise;
  GSList* actions;
} Options;

static gboolean cycling = FALSE;

static gpointer setup_func(GHashTable* options);
static gpointer setup_cycle_func(GHashTable* options,
                                 ObActionsIPreFunc* pre,
                                 ObActionsIInputFunc* input,
                                 ObActionsICancelFunc* cancel,
                                 ObActionsIPostFunc* post);
static gpointer setup_target_func(GHashTable* options);
static void free_func(gpointer options);
static gboolean run_func(ObActionsData* data, gpointer options);
static gboolean i_input_func(guint initial_state, XEvent* e, ObtIC* ic, gpointer options, gboolean* used);
static void i_cancel_func(gpointer options);

static void end_cycle(gboolean cancel, guint state, Options* o);

/* 3.4-compatibility */
static gpointer setup_north_cycle_func(GHashTable* options,
                                       ObActionsIPreFunc* pre,
                                       ObActionsIInputFunc* in,
                                       ObActionsICancelFunc* c,
                                       ObActionsIPostFunc* post);
static gpointer setup_south_cycle_func(GHashTable* options,
                                       ObActionsIPreFunc* pre,
                                       ObActionsIInputFunc* in,
                                       ObActionsICancelFunc* c,
                                       ObActionsIPostFunc* post);
static gpointer setup_east_cycle_func(GHashTable* options,
                                      ObActionsIPreFunc* pre,
                                      ObActionsIInputFunc* in,
                                      ObActionsICancelFunc* c,
                                      ObActionsIPostFunc* post);
static gpointer setup_west_cycle_func(GHashTable* options,
                                      ObActionsIPreFunc* pre,
                                      ObActionsIInputFunc* in,
                                      ObActionsICancelFunc* c,
                                      ObActionsIPostFunc* post);
static gpointer setup_northwest_cycle_func(GHashTable* options,
                                           ObActionsIPreFunc* pre,
                                           ObActionsIInputFunc* in,
                                           ObActionsICancelFunc* c,
                                           ObActionsIPostFunc* post);
static gpointer setup_northeast_cycle_func(GHashTable* options,
                                           ObActionsIPreFunc* pre,
                                           ObActionsIInputFunc* in,
                                           ObActionsICancelFunc* c,
                                           ObActionsIPostFunc* post);
static gpointer setup_southwest_cycle_func(GHashTable* options,
                                           ObActionsIPreFunc* pre,
                                           ObActionsIInputFunc* in,
                                           ObActionsICancelFunc* c,
                                           ObActionsIPostFunc* post);
static gpointer setup_southeast_cycle_func(GHashTable* options,
                                           ObActionsIPreFunc* pre,
                                           ObActionsIInputFunc* in,
                                           ObActionsICancelFunc* c,
                                           ObActionsIPostFunc* post);
static gpointer setup_north_target_func(GHashTable* options);
static gpointer setup_south_target_func(GHashTable* options);
static gpointer setup_east_target_func(GHashTable* options);
static gpointer setup_west_target_func(GHashTable* options);
static gpointer setup_northwest_target_func(GHashTable* options);
static gpointer setup_northeast_target_func(GHashTable* options);
static gpointer setup_southwest_target_func(GHashTable* options);
static gpointer setup_southeast_target_func(GHashTable* options);

void action_directionalwindows_startup(void) {
  actions_register_i_opt("DirectionalCycleWindows", setup_cycle_func, free_func, run_func);
  actions_register_opt("DirectionalTargetWindow", setup_target_func, free_func, run_func);
  /* 3.4-compatibility */
  actions_register_i_opt("DirectionalFocusNorth", setup_north_cycle_func, free_func, run_func);
  actions_register_i_opt("DirectionalFocusSouth", setup_south_cycle_func, free_func, run_func);
  actions_register_i_opt("DirectionalFocusWest", setup_west_cycle_func, free_func, run_func);
  actions_register_i_opt("DirectionalFocusEast", setup_east_cycle_func, free_func, run_func);
  actions_register_i_opt("DirectionalFocusNorthWest", setup_northwest_cycle_func, free_func, run_func);
  actions_register_i_opt("DirectionalFocusNorthEast", setup_northeast_cycle_func, free_func, run_func);
  actions_register_i_opt("DirectionalFocusSouthWest", setup_southwest_cycle_func, free_func, run_func);
  actions_register_i_opt("DirectionalFocusSouthEast", setup_southeast_cycle_func, free_func, run_func);
  actions_register_opt("DirectionalTargetNorth", setup_north_target_func, free_func, run_func);
  actions_register_opt("DirectionalTargetSouth", setup_south_target_func, free_func, run_func);
  actions_register_opt("DirectionalTargetWest", setup_west_target_func, free_func, run_func);
  actions_register_opt("DirectionalTargetEast", setup_east_target_func, free_func, run_func);
  actions_register_opt("DirectionalTargetNorthWest", setup_northwest_target_func, free_func, run_func);
  actions_register_opt("DirectionalTargetNorthEast", setup_northeast_target_func, free_func, run_func);
  actions_register_opt("DirectionalTargetSouthWest", setup_southwest_target_func, free_func, run_func);
  actions_register_opt("DirectionalTargetSouthEast", setup_southeast_target_func, free_func, run_func);
}

static void parse_final_actions(const gchar* val, Options* o) {
  if (!val)
    return;
  gchar** names = g_strsplit_set(val, ", ", 0);
  for (gchar** it = names; it && *it; ++it) {
    if (**it == '\0')
      continue;
    ObActionsAct* action = actions_parse_string(*it);
    if (action)
      o->actions = g_slist_append(o->actions, action);
  }
  g_strfreev(names);
}

static gpointer setup_func(GHashTable* options) {
  Options* o;
  const gchar* val;

  o = g_slice_new0(Options);
  o->dialog = TRUE;
  o->bar = TRUE;

  if (options) {
    val = g_hash_table_lookup(options, "dialog");
    if (val)
      o->dialog = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "bar");
    if (val)
      o->bar = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "raise");
    if (val)
      o->raise = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "panels");
    if (val)
      o->dock_windows = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "desktop");
    if (val)
      o->desktop_windows = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "direction");
    if (val)
      o->direction = actions_parse_direction(val);
    val = g_hash_table_lookup(options, "finalactions");
    if (val)
      parse_final_actions(val, o);
  }

  if (!o->actions) {
    o->actions = g_slist_prepend(o->actions, actions_parse_string("Focus"));
    o->actions = g_slist_prepend(o->actions, actions_parse_string("Raise"));
    o->actions = g_slist_prepend(o->actions, actions_parse_string("Unshade"));
  }

  return o;
}

static gpointer setup_cycle_func(GHashTable* options,
                                 ObActionsIPreFunc* pre,
                                 ObActionsIInputFunc* input,
                                 ObActionsICancelFunc* cancel,
                                 ObActionsIPostFunc* post) {
  Options* o = setup_func(options);
  o->interactive = TRUE;
  *input = i_input_func;
  *cancel = i_cancel_func;
  return o;
}

static gpointer setup_target_func(GHashTable* options) {
  Options* o = setup_func(options);
  o->interactive = FALSE;
  return o;
}

static void free_func(gpointer options) {
  Options* o = options;

  while (o->actions) {
    actions_act_unref(o->actions->data);
    o->actions = g_slist_delete_link(o->actions, o->actions);
  }

  g_slice_free(Options, o);
}

static gboolean run_func(ObActionsData* data, gpointer options) {
  Options* o = options;

  if (!o->interactive)
    end_cycle(FALSE, data->state, o);
  else {
    struct _ObClient* ft;

    ft = focus_directional_cycle(o->direction, o->dock_windows, o->desktop_windows, TRUE, o->bar, o->dialog, FALSE,
                                 FALSE);
    cycling = TRUE;

    stacking_restore();
    if (o->raise && ft)
      stacking_temp_raise(CLIENT_AS_WINDOW(ft));
  }

  return o->interactive;
}

static gboolean i_input_func(guint initial_state, XEvent* e, ObtIC* ic, gpointer options, gboolean* used) {
  guint mods, initial_mods;

  initial_mods = obt_keyboard_only_modmasks(initial_state);
  mods = obt_keyboard_only_modmasks(e->xkey.state);
  if (e->type == KeyRelease) {
    /* remove from the state the mask of the modifier key being
       released, if it is a modifier key being released that is */
    mods &= ~obt_keyboard_keyevent_to_modmask(e);
  }

  if (e->type == KeyPress) {
    KeySym sym = obt_keyboard_keypress_to_keysym(e);

    /* Escape cancels no matter what */
    if (sym == XK_Escape) {
      end_cycle(TRUE, e->xkey.state, options);
      return FALSE;
    }

    /* There were no modifiers and they pressed enter */
    else if ((sym == XK_Return || sym == XK_KP_Enter) && !initial_mods) {
      end_cycle(FALSE, e->xkey.state, options);
      return FALSE;
    }
  }
  /* They released the modifiers */
  else if (e->type == KeyRelease && initial_mods && !(mods & initial_mods)) {
    end_cycle(FALSE, e->xkey.state, options);
    return FALSE;
  }

  return TRUE;
}

static void i_cancel_func(gpointer options) {
  /* we get cancelled when we move focus, but we're not cycling anymore, so
     just ignore that */
  if (cycling)
    end_cycle(TRUE, 0, options);
}

static void end_cycle(gboolean cancel, guint state, Options* o) {
  struct _ObClient* ft;

  ft = focus_directional_cycle(o->direction, o->dock_windows, o->desktop_windows, o->interactive, o->bar, o->dialog,
                               TRUE, cancel);
  cycling = FALSE;

  if (ft)
    actions_run_acts(o->actions, OB_USER_ACTION_KEYBOARD_KEY, state, -1, -1, 0, OB_FRAME_CONTEXT_NONE, ft);

  stacking_restore();
}

/* 3.4-compatibility */
static gpointer setup_north_cycle_func(GHashTable* options,
                                       ObActionsIPreFunc* pre,
                                       ObActionsIInputFunc* input,
                                       ObActionsICancelFunc* cancel,
                                       ObActionsIPostFunc* post) {
  Options* o = setup_cycle_func(options, pre, input, cancel, post);
  o->direction = OB_DIRECTION_NORTH;
  return o;
}

static gpointer setup_south_cycle_func(GHashTable* options,
                                       ObActionsIPreFunc* pre,
                                       ObActionsIInputFunc* input,
                                       ObActionsICancelFunc* cancel,
                                       ObActionsIPostFunc* post) {
  Options* o = setup_cycle_func(options, pre, input, cancel, post);
  o->direction = OB_DIRECTION_SOUTH;
  return o;
}

static gpointer setup_east_cycle_func(GHashTable* options,
                                      ObActionsIPreFunc* pre,
                                      ObActionsIInputFunc* input,
                                      ObActionsICancelFunc* cancel,
                                      ObActionsIPostFunc* post) {
  Options* o = setup_cycle_func(options, pre, input, cancel, post);
  o->direction = OB_DIRECTION_EAST;
  return o;
}

static gpointer setup_west_cycle_func(GHashTable* options,
                                      ObActionsIPreFunc* pre,
                                      ObActionsIInputFunc* input,
                                      ObActionsICancelFunc* cancel,
                                      ObActionsIPostFunc* post) {
  Options* o = setup_cycle_func(options, pre, input, cancel, post);
  o->direction = OB_DIRECTION_WEST;
  return o;
}

static gpointer setup_northwest_cycle_func(GHashTable* options,
                                           ObActionsIPreFunc* pre,
                                           ObActionsIInputFunc* input,
                                           ObActionsICancelFunc* cancel,
                                           ObActionsIPostFunc* post) {
  Options* o = setup_cycle_func(options, pre, input, cancel, post);
  o->direction = OB_DIRECTION_NORTHWEST;
  return o;
}

static gpointer setup_northeast_cycle_func(GHashTable* options,
                                           ObActionsIPreFunc* pre,
                                           ObActionsIInputFunc* input,
                                           ObActionsICancelFunc* cancel,
                                           ObActionsIPostFunc* post) {
  Options* o = setup_cycle_func(options, pre, input, cancel, post);
  o->direction = OB_DIRECTION_NORTHEAST;
  return o;
}

static gpointer setup_southwest_cycle_func(GHashTable* options,
                                           ObActionsIPreFunc* pre,
                                           ObActionsIInputFunc* input,
                                           ObActionsICancelFunc* cancel,
                                           ObActionsIPostFunc* post) {
  Options* o = setup_cycle_func(options, pre, input, cancel, post);
  o->direction = OB_DIRECTION_SOUTHWEST;
  return o;
}

static gpointer setup_southeast_cycle_func(GHashTable* options,
                                           ObActionsIPreFunc* pre,
                                           ObActionsIInputFunc* input,
                                           ObActionsICancelFunc* cancel,
                                           ObActionsIPostFunc* post) {
  Options* o = setup_cycle_func(options, pre, input, cancel, post);
  o->direction = OB_DIRECTION_SOUTHEAST;
  return o;
}

static gpointer setup_north_target_func(GHashTable* options) {
  Options* o = setup_target_func(options);
  o->direction = OB_DIRECTION_NORTH;
  return o;
}

static gpointer setup_south_target_func(GHashTable* options) {
  Options* o = setup_target_func(options);
  o->direction = OB_DIRECTION_SOUTH;
  return o;
}

static gpointer setup_east_target_func(GHashTable* options) {
  Options* o = setup_target_func(options);
  o->direction = OB_DIRECTION_EAST;
  return o;
}

static gpointer setup_west_target_func(GHashTable* options) {
  Options* o = setup_target_func(options);
  o->direction = OB_DIRECTION_WEST;
  return o;
}

static gpointer setup_northwest_target_func(GHashTable* options) {
  Options* o = setup_target_func(options);
  o->direction = OB_DIRECTION_NORTHWEST;
  return o;
}

static gpointer setup_northeast_target_func(GHashTable* options) {
  Options* o = setup_target_func(options);
  o->direction = OB_DIRECTION_NORTHEAST;
  return o;
}

static gpointer setup_southwest_target_func(GHashTable* options) {
  Options* o = setup_target_func(options);
  o->direction = OB_DIRECTION_SOUTHWEST;
  return o;
}

static gpointer setup_southeast_target_func(GHashTable* options) {
  Options* o = setup_target_func(options);
  o->direction = OB_DIRECTION_SOUTHEAST;
  return o;
}

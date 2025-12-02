#include "openbox/actions.h"
#include "openbox/actions/helpers.h"
#include "openbox/stacking.h"
#include "openbox/window.h"
#include "openbox/event.h"
#include "openbox/focus_cycle.h"
#include "openbox/openbox.h"
#include "gettext.h"
#include "obt/keyboard.h"

typedef struct {
  gboolean linear;
  gboolean dock_windows;
  gboolean desktop_windows;
  gboolean only_hilite_windows;
  gboolean all_desktops;
  gboolean forward;
  gboolean bar;
  gboolean raise;
  gboolean interactive;
  ObFocusCyclePopupMode dialog_mode;
  GSList* actions;

  /* options for after we're done */
  gboolean cancel; /* did the user cancel or not */
  guint state;     /* keyboard state when finished */
} Options;

static gpointer setup_func(GHashTable* options,
                           ObActionsIPreFunc* pre,
                           ObActionsIInputFunc* in,
                           ObActionsICancelFunc* c,
                           ObActionsIPostFunc* post);
static gpointer setup_forward_func(GHashTable* options,
                                   ObActionsIPreFunc* pre,
                                   ObActionsIInputFunc* in,
                                   ObActionsICancelFunc* c,
                                   ObActionsIPostFunc* post);
static gpointer setup_backward_func(GHashTable* options,
                                    ObActionsIPreFunc* pre,
                                    ObActionsIInputFunc* in,
                                    ObActionsICancelFunc* c,
                                    ObActionsIPostFunc* post);
static void free_func(gpointer options);
static gboolean run_func(ObActionsData* data, gpointer options);
static gboolean i_input_func(guint initial_state, XEvent* e, ObtIC* ic, gpointer options, gboolean* used);
static void i_cancel_func(gpointer options);
static void i_post_func(gpointer options);

void action_cyclewindows_startup(void) {
  actions_register_i_opt("NextWindow", setup_forward_func, free_func, run_func);
  actions_register_i_opt("PreviousWindow", setup_backward_func, free_func, run_func);
}

static ObFocusCyclePopupMode parse_dialog_mode(const gchar* val) {
  if (!val)
    return OB_FOCUS_CYCLE_POPUP_MODE_LIST;
  if (!g_ascii_strcasecmp(val, "none") || !g_ascii_strcasecmp(val, "no"))
    return OB_FOCUS_CYCLE_POPUP_MODE_NONE;
  if (!g_ascii_strcasecmp(val, "icons"))
    return OB_FOCUS_CYCLE_POPUP_MODE_ICONS;
  return OB_FOCUS_CYCLE_POPUP_MODE_LIST;
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

static gpointer setup_func(GHashTable* options,
                           ObActionsIPreFunc* pre,
                           ObActionsIInputFunc* input,
                           ObActionsICancelFunc* cancel,
                           ObActionsIPostFunc* post) {
  Options* o;
  const gchar* val;

  o = g_slice_new0(Options);
  o->bar = TRUE;
  o->dialog_mode = OB_FOCUS_CYCLE_POPUP_MODE_LIST;
  o->interactive = TRUE;

  if (options) {
    val = g_hash_table_lookup(options, "linear");
    if (val)
      o->linear = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "dialog");
    if (val)
      o->dialog_mode = parse_dialog_mode(val);
    val = g_hash_table_lookup(options, "interactive");
    if (val)
      o->interactive = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "bar");
    if (val)
      o->bar = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "raise");
    if (val)
      o->raise = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "panels");
    if (val)
      o->dock_windows = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "hilite");
    if (val)
      o->only_hilite_windows = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "desktop");
    if (val)
      o->desktop_windows = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "allDesktops");
    if (val)
      o->all_desktops = actions_parse_bool(val);
    val = g_hash_table_lookup(options, "finalactions");
    if (val)
      parse_final_actions(val, o);
  }

  if (!o->actions) {
    o->actions = g_slist_prepend(o->actions, actions_parse_string("Focus"));
    o->actions = g_slist_prepend(o->actions, actions_parse_string("Raise"));
    o->actions = g_slist_prepend(o->actions, actions_parse_string("Unshade"));
  }

  *input = i_input_func;
  *cancel = i_cancel_func;
  *post = i_post_func;
  return o;
}

static gpointer setup_forward_func(GHashTable* options,
                                   ObActionsIPreFunc* pre,
                                   ObActionsIInputFunc* input,
                                   ObActionsICancelFunc* cancel,
                                   ObActionsIPostFunc* post) {
  Options* o = setup_func(options, pre, input, cancel, post);
  o->forward = TRUE;
  return o;
}

static gpointer setup_backward_func(GHashTable* options,
                                    ObActionsIPreFunc* pre,
                                    ObActionsIInputFunc* input,
                                    ObActionsICancelFunc* cancel,
                                    ObActionsIPostFunc* post) {
  Options* o = setup_func(options, pre, input, cancel, post);
  o->forward = FALSE;
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
  struct _ObClient* ft;

  gboolean done = FALSE;
  gboolean cancel = FALSE;

  ft = focus_cycle(o->forward, o->all_desktops, !o->only_hilite_windows, o->dock_windows, o->desktop_windows, o->linear,
                   (o->interactive ? o->bar : FALSE),
                   (o->interactive ? o->dialog_mode : OB_FOCUS_CYCLE_POPUP_MODE_NONE), done, cancel);

  stacking_restore();
  if (o->raise && ft)
    stacking_temp_raise(CLIENT_AS_WINDOW(ft));

  return o->interactive;
}

static gboolean i_input_func(guint initial_state, XEvent* e, ObtIC* ic, gpointer options, gboolean* used) {
  Options* o = options;
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
      o->cancel = TRUE;
      o->state = e->xkey.state;
      return FALSE;
    }

    /* There were no modifiers and they pressed enter */
    else if ((sym == XK_Return || sym == XK_KP_Enter) && !initial_mods) {
      o->cancel = FALSE;
      o->state = e->xkey.state;
      return FALSE;
    }
  }
  /* They released the modifiers */
  else if (e->type == KeyRelease && initial_mods && !(mods & initial_mods)) {
    o->cancel = FALSE;
    o->state = e->xkey.state;
    return FALSE;
  }

  return TRUE;
}

static void i_cancel_func(gpointer options) {
  Options* o = options;
  o->cancel = TRUE;
  o->state = 0;
}

static void i_post_func(gpointer options) {
  Options* o = options;
  struct _ObClient* ft;

  gboolean done = TRUE;

  ft = focus_cycle(o->forward, o->all_desktops, !o->only_hilite_windows, o->dock_windows, o->desktop_windows, o->linear,
                   o->bar, o->dialog_mode, done, o->cancel);

  if (ft)
    actions_run_acts(o->actions, OB_USER_ACTION_KEYBOARD_KEY, o->state, -1, -1, 0, OB_FRAME_CONTEXT_NONE, ft);

  stacking_restore();
}

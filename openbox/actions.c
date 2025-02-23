#include "actions.h"
#include "gettext.h"
#include "grab.h"
#include "screen.h"
#include "event.h"
#include "config.h"
#include "client.h"
#include "focus.h"
#include "openbox.h"
#include "debug.h"

#include "actions/all.h"

static void     actions_definition_ref(ObActionsDefinition *def);
static void     actions_definition_unref(ObActionsDefinition *def);
static gboolean actions_interactive_begin_act(ObActionsAct *act, guint state);
static void     actions_interactive_end_act(void);
static ObActionsAct* actions_build_act_from_string(const gchar *name);

static void     actions_shutdown_def(gpointer key, gpointer value, gpointer user_data);

/* --------------------------------------------------------------------------
 * Global / static variables
 * -------------------------------------------------------------------------- */
static ObActionsAct *interactive_act = NULL;
static guint         interactive_initial_state = 0;

/* Replace GSList with a global hash table */
static GHashTable   *action_table = NULL;

/* --------------------------------------------------------------------------
 * Structs
 * -------------------------------------------------------------------------- */
struct _ObActionsDefinition {
    guint ref;

    gchar *name;

    gboolean canbeinteractive;
    union {
        ObActionsIDataSetupFunc i;
        ObActionsDataSetupFunc  n;
    } setup;
    ObActionsDataFreeFunc  free;
    ObActionsRunFunc       run;
    ObActionsShutdownFunc  shutdown;
    gboolean modifies_focused_window;
    gboolean can_stop;
};

struct _ObActionsAct {
    guint ref;

    ObActionsDefinition *def;
    ObActionsIPreFunc    i_pre;
    ObActionsIInputFunc  i_input;
    ObActionsICancelFunc i_cancel;
    ObActionsIPostFunc   i_post;
    gpointer             options;
};

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

void
actions_startup(gboolean reconfig)
{
    if (reconfig) return;

    /* Initialize the actions system */
    action_all_startup();

    /* Create our hash table if it doesn't exist already */
    if (!action_table) {
        /* key   = def->name (string),
         * value = pointer to ObActionsDefinition
         */
        action_table = g_hash_table_new(g_str_hash, g_str_equal);
    }
}

static void
actions_shutdown_def(gpointer key, gpointer value, gpointer user_data)
{
    (void)key;       /* unused */
    (void)user_data; /* unused */

    ObActionsDefinition *d = (ObActionsDefinition *)value;
    if (d->shutdown)
        d->shutdown();
    actions_definition_unref(d);
}

void
actions_shutdown(gboolean reconfig)
{
    actions_interactive_cancel_act();

    if (reconfig) return;

    /* Emulate the old "free all actions" loop using a hash table. */
    if (action_table) {
        g_hash_table_foreach(action_table, actions_shutdown_def, NULL);
        g_hash_table_destroy(action_table);
        action_table = NULL;
    }
}

/* --------------------------------------------------------------------------
 * Registration
 * -------------------------------------------------------------------------- */

static ObActionsDefinition*
do_register(const gchar               *name,
            ObActionsDataFreeFunc     free_func,
            ObActionsRunFunc          run)
{
    g_assert(run != NULL);

    /* If already registered, return NULL */
    if (g_hash_table_contains(action_table, name))
        return NULL;

    ObActionsDefinition *def = g_slice_new0(ObActionsDefinition);
    def->ref = 1;
    def->name = g_strdup(name);
    def->free = free_func;
    def->run = run;
    def->shutdown = NULL;
    def->modifies_focused_window = TRUE;
    def->can_stop = FALSE;

    /* Insert into the hash table (key: def->name, value: def) */
    g_hash_table_insert(action_table, def->name, def);

    return def;
}

gboolean
actions_register_i(const gchar           *name,
                   ObActionsIDataSetupFunc setup,
                   ObActionsDataFreeFunc   free,
                   ObActionsRunFunc        run)
{
    ObActionsDefinition *def = do_register(name, free, run);
    if (def) {
        def->canbeinteractive = TRUE;
        def->setup.i = setup;
    }
    return def != NULL;
}

gboolean
actions_register(const gchar         *name,
                 ObActionsDataSetupFunc setup,
                 ObActionsDataFreeFunc  free,
                 ObActionsRunFunc       run)
{
    ObActionsDefinition *def = do_register(name, free, run);
    if (def) {
        def->canbeinteractive = FALSE;
        def->setup.n = setup;
    }
    return def != NULL;
}

/* --------------------------------------------------------------------------
 * Setters
 * -------------------------------------------------------------------------- */

gboolean
actions_set_shutdown(const gchar *name,
                     ObActionsShutdownFunc shutdown)
{
    ObActionsDefinition *def = g_hash_table_lookup(action_table, name);
    if (!def)
        return FALSE;

    def->shutdown = shutdown;
    return TRUE;
}

gboolean
actions_set_modifies_focused_window(const gchar *name,
                                    gboolean modifies)
{
    ObActionsDefinition *def = g_hash_table_lookup(action_table, name);
    if (!def)
        return FALSE;

    def->modifies_focused_window = modifies;
    return TRUE;
}

gboolean
actions_set_can_stop(const gchar *name,
                     gboolean can_stop)
{
    ObActionsDefinition *def = g_hash_table_lookup(action_table, name);
    if (!def)
        return FALSE;

    def->can_stop = can_stop;
    return TRUE;
}

/* --------------------------------------------------------------------------
 * Reference Counting
 * -------------------------------------------------------------------------- */

static void
actions_definition_ref(ObActionsDefinition *def)
{
    ++def->ref;
}

static void
actions_definition_unref(ObActionsDefinition *def)
{
    if (def && --def->ref == 0) {
        g_free(def->name);
        g_slice_free(ObActionsDefinition, def);
    }
}

/* --------------------------------------------------------------------------
 * Building and Parsing Actions
 * -------------------------------------------------------------------------- */

static ObActionsAct*
actions_build_act_from_string(const gchar *name)
{
    ObActionsDefinition *def = g_hash_table_lookup(action_table, name);
    if (!def) {
        g_message(_("Invalid action \"%s\" requested. No such action exists."), name);
        return NULL;
    }

    ObActionsAct *act = g_slice_new(ObActionsAct);
    act->ref = 1;
    act->def = def;
    actions_definition_ref(def);

    act->i_pre = NULL;
    act->i_input = NULL;
    act->i_cancel = NULL;
    act->i_post = NULL;
    act->options = NULL;

    return act;
}

ObActionsAct*
actions_parse_string(const gchar *name)
{
    ObActionsAct *act = actions_build_act_from_string(name);
    if (!act)
        return NULL;

    if (act->def->canbeinteractive) {
        if (act->def->setup.i) {
            act->options = act->def->setup.i(NULL,
                                             &act->i_pre,
                                             &act->i_input,
                                             &act->i_cancel,
                                             &act->i_post);
        }
    } else {
        if (act->def->setup.n)
            act->options = act->def->setup.n(NULL);
    }

    return act;
}

ObActionsAct*
actions_parse(xmlNodePtr node)
{
    gchar *name;
    ObActionsAct *act = NULL;

    if (obt_xml_attr_string(node, "name", &name)) {
        act = actions_build_act_from_string(name);
        if (act) {
            if (act->def->canbeinteractive) {
                if (act->def->setup.i) {
                    act->options = act->def->setup.i(node->children,
                                                     &act->i_pre,
                                                     &act->i_input,
                                                     &act->i_cancel,
                                                     &act->i_post);
                }
            } else {
                if (act->def->setup.n) {
                    act->options = act->def->setup.n(node->children);
                }
            }
        }
        g_free(name);
    }

    return act;
}

/* --------------------------------------------------------------------------
 * Action Reference Handling
 * -------------------------------------------------------------------------- */

gboolean
actions_act_is_interactive(ObActionsAct *act)
{
    return (act->i_input != NULL);
}

void
actions_act_ref(ObActionsAct *act)
{
    ++act->ref;
}

void
actions_act_unref(ObActionsAct *act)
{
    if (act && --act->ref == 0) {
        /* free the action-specific options */
        if (act->def->free)
            act->def->free(act->options);

        /* unref the definition */
        actions_definition_unref(act->def);

        g_slice_free(ObActionsAct, act);
    }
}

/* --------------------------------------------------------------------------
 * Running Actions
 * -------------------------------------------------------------------------- */

static void
actions_setup_data(ObActionsData     *data,
                   ObUserAction       uact,
                   guint              state,
                   gint               x,
                   gint               y,
                   gint               button,
                   ObFrameContext     con,
                   struct _ObClient  *client)
{
    data->uact = uact;
    data->state = state;
    data->x = x;
    data->y = y;
    data->button = button;
    data->context = con;
    data->client = client;
}

void
actions_run_acts(GSList        *acts,
                 ObUserAction   uact,
                 guint          state,
                 gint           x,
                 gint           y,
                 gint           button,
                 ObFrameContext con,
                 struct _ObClient *client)
{
    GSList *it;
    gboolean update_user_time;

    /* Don't allow saving the initial state when running things from the menu */
    if (uact == OB_USER_ACTION_MENU_SELECTION)
        state = 0;

    /* If x and y are < 0 then use the current pointer position */
    if (x < 0 && y < 0)
        screen_pointer_pos(&x, &y);

    update_user_time = FALSE;
    for (it = acts; it; it = g_slist_next(it)) {
        ObActionsData data;
        ObActionsAct *act = (ObActionsAct *)it->data;
        gboolean ok = TRUE;

        actions_setup_data(&data, uact, state, x, y, button, con, client);

        /*
         * If they have the same run function, then we'll assume they are
         * cooperating and not cancel each other out
         */
        if (!interactive_act || interactive_act->def->run != act->def->run) {
            if (actions_act_is_interactive(act)) {
                /* cancel the old one */
                if (interactive_act)
                    actions_interactive_cancel_act();

                if (act->i_pre) {
                    if (!act->i_pre(state, act->options)) {
                        /* remove the interactivity if i_pre fails */
                        act->i_input = NULL;
                    }
                }
            }

            /* check again because it might have been cancelled */
            if (actions_act_is_interactive(act))
                ok = actions_interactive_begin_act(act, state);
        }

        /* fire the action's run function with this data */
        if (ok) {
            if (!act->def->run(&data, act->options)) {
                /* run returned FALSE => completed action */
                if (actions_act_is_interactive(act)) {
                    actions_interactive_end_act();
                }
                if (client && client == focus_client && act->def->modifies_focused_window) {
                    update_user_time = TRUE;
                }
            } else {
                /*
                 * run returned TRUE => "in progress"
                 * Make sure it's interactive OR can_stop
                 */
                g_assert(act->i_input || act->def->can_stop);

                /* no further actions run after an interactive one returns TRUE */
                break;
            }
        }
    }
    if (update_user_time)
        event_update_user_time();
}

/* --------------------------------------------------------------------------
 * Interactive Actions
 * -------------------------------------------------------------------------- */

gboolean
actions_interactive_act_running(void)
{
    return (interactive_act != NULL);
}

void
actions_interactive_cancel_act(void)
{
    if (interactive_act) {
        if (interactive_act->i_cancel)
            interactive_act->i_cancel(interactive_act->options);
        actions_interactive_end_act();
    }
}

static gboolean
actions_interactive_begin_act(ObActionsAct *act, guint state)
{
    if (grab_keyboard()) {
        interactive_act = act;
        actions_act_ref(interactive_act);

        interactive_initial_state = state;

        /*
         * if using focus_delay, stop the timer now so that focus doesn't
         * move on us, which would kill the action
         */
        event_halt_focus_delay();

        return TRUE;
    }
    else
        return FALSE;
}

static void
actions_interactive_end_act(void)
{
    if (interactive_act) {
        ObActionsAct *ia = interactive_act;

        /*
         * set this to NULL first so that i_post() cannot cause
         * a re-entrant call that tries to end the action again.
         */
        interactive_act = NULL;

        ungrab_keyboard();

        if (ia->i_post)
            ia->i_post(ia->options);

        actions_act_unref(ia);
    }
}

gboolean
actions_interactive_input_event(XEvent *e)
{
    gboolean used = FALSE;

    if (interactive_act) {
        if (!interactive_act->i_input(interactive_initial_state,
                                      e,
                                      grab_input_context(),
                                      interactive_act->options,
                                      &used))
        {
            used = TRUE;  /* If it canceled the action, we consider it 'used' */
            actions_interactive_end_act();
        }
    }
    return used;
}

/* --------------------------------------------------------------------------
 * Extra: Window Move Example
 * -------------------------------------------------------------------------- */

void
actions_client_move(ObActionsData *data, gboolean start)
{
    static gulong ignore_start = 0;

    if (start) {
        ignore_start = event_start_ignore_all_enters();
    } else if (config_focus_follow && data->context != OB_FRAME_CONTEXT_CLIENT) {
        if (data->uact == OB_USER_ACTION_MOUSE_PRESS) {
            struct _ObClient *c;

            /*
             * Usually this is redundant, but with a press action that moves
             * windows from under the cursor, the enter event comes as a
             * GrabNotify (ignored), so we fake an enter event.
             *
             * Don't do this if there's a pointer grab. Enter events are
             * ignored during a grab, so skip it.
             */
            if (!grab_on_pointer()) {
                if ((c = client_under_pointer()) && c != data->client) {
                    ob_debug_type(OB_DEBUG_FOCUS,
                                  "Generating fake enter (mouse-event action)");
                    event_enter_client(c);
                }
                else if (!c && c != data->client) {
                    ob_debug_type(OB_DEBUG_FOCUS,
                                  "Generating fake leave (mouse-event action)");
                    event_leave_client(data->client);
                }
            }
        }
        else if (!data->button && !config_focus_under_mouse) {
            event_end_ignore_all_enters(ignore_start);
        }
    }
}

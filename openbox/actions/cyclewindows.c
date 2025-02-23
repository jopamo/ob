#include "openbox/actions.h"
#include "openbox/stacking.h"
#include "openbox/window.h"
#include "openbox/event.h"
#include "openbox/focus_cycle.h"
#include "openbox/openbox.h"
#include "gettext.h"
#include "obt/keyboard.h"

/*!
 * \brief Options for cycling windows interactively or non-interactively.
 *
 * \c forward           => TRUE to move forward in list, FALSE to move backward
 * \c linear            => TRUE if linear cycling mode is on
 * \c dock_windows      => TRUE to include dock/panel windows
 * \c desktop_windows   => TRUE to include desktop windows
 * \c only_hilite_windows => TRUE to only cycle through windows that are "hilited"
 * \c all_desktops      => TRUE to cycle across all desktops
 * \c bar               => TRUE to show a bar or panel while cycling
 * \c raise             => TRUE to raise the newly focused window
 * \c interactive       => TRUE if we should keep cycling until modifiers are released
 * \c dialog_mode       => Controls how the cycle popup (dialog) is shown
 * \c actions           => A list (GSList) of final actions to apply after cycling
 * \c cancel            => Set to TRUE if user canceled the cycle (e.g. pressed Escape)
 * \c state             => Keyboard state (modifiers) captured at cycle end
 */
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
    GSList *actions;

    /* Options for after we're done */
    gboolean cancel;
    guint state;
} Options;

/* Forward declarations */
static gpointer setup_func(xmlNodePtr node,
                           ObActionsIPreFunc *pre,
                           ObActionsIInputFunc *in,
                           ObActionsICancelFunc *c,
                           ObActionsIPostFunc *post);
static gpointer setup_forward_func(xmlNodePtr node,
                                   ObActionsIPreFunc *pre,
                                   ObActionsIInputFunc *in,
                                   ObActionsICancelFunc *c,
                                   ObActionsIPostFunc *post);
static gpointer setup_backward_func(xmlNodePtr node,
                                    ObActionsIPreFunc *pre,
                                    ObActionsIInputFunc *in,
                                    ObActionsICancelFunc *c,
                                    ObActionsIPostFunc *post);
static void     free_func(gpointer options);
static gboolean run_func(ObActionsData *data, gpointer options);

/* Interactive function pointers */
static gboolean i_input_func(guint initial_state,
                             XEvent *e,
                             ObtIC *ic,
                             gpointer options,
                             gboolean *used);
static void     i_cancel_func(gpointer options);
static void     i_post_func(gpointer options);

/*!
 * \brief Registers the "NextWindow" and "PreviousWindow" actions as interactive,
 * which allows user to cycle between windows with keys.
 */
void
action_cyclewindows_startup(void)
{
    actions_register_i("NextWindow",
                       setup_forward_func,
                       free_func,
                       run_func);

    actions_register_i("PreviousWindow",
                       setup_backward_func,
                       free_func,
                       run_func);
}

/*!
 * \brief Common setup function for both forward/backward cycling.
 * Reads XML attributes to fill in an Options struct, sets final actions, etc.
 *
 * \param node  XML node describing configuration (<dialog>, <bar>, <panels>, etc.)
 * \param pre   [unused] We do not define a pre-function in this action
 * \param input Assigned to i_input_func (handle key events)
 * \param cancel Assigned to i_cancel_func
 * \param post  Assigned to i_post_func (applies final actions after cycle done)
 * \return A newly allocated Options pointer
 */
static gpointer
setup_func(xmlNodePtr node,
           ObActionsIPreFunc *pre,
           ObActionsIInputFunc *input,
           ObActionsICancelFunc *cancel,
           ObActionsIPostFunc *post)
{
    UNUSED(pre);

    xmlNodePtr n;
    Options *o = g_new0(Options, 1);

    /* Defaults */
    o->bar = TRUE;
    o->dialog_mode = OB_FOCUS_CYCLE_POPUP_MODE_LIST;
    o->interactive = TRUE;

    /* Parse various boolean or string options from XML. */
    if ((n = obt_xml_find_node(node, "linear")))
        o->linear = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "dialog"))) {
        if (obt_xml_node_contains(n, "none") ||
            obt_xml_node_contains(n, "no"))
        {
            o->dialog_mode = OB_FOCUS_CYCLE_POPUP_MODE_NONE;
        } else if (obt_xml_node_contains(n, "icons")) {
            o->dialog_mode = OB_FOCUS_CYCLE_POPUP_MODE_ICONS;
        }
    }
    if ((n = obt_xml_find_node(node, "interactive")))
        o->interactive = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "bar")))
        o->bar = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "raise")))
        o->raise = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "panels")))
        o->dock_windows = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "hilite")))
        o->only_hilite_windows = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "desktop")))
        o->desktop_windows = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "allDesktops")))
        o->all_desktops = obt_xml_node_bool(n);

    /*
     * finalactions: a list of <action> child nodes that define
     * what actions are run after the user finishes cycling.
     */
    if ((n = obt_xml_find_node(node, "finalactions"))) {
        xmlNodePtr m = obt_xml_find_node(n->children, "action");
        while (m) {
            ObActionsAct *action = actions_parse(m);
            if (action) {
                o->actions = g_slist_append(o->actions, action);
            }
            m = obt_xml_find_node(m->next, "action");
        }
    }
    else {
        /*
         * If no finalactions are specified, default to:
         * Unshade, Raise, Focus
         */
        o->actions = g_slist_prepend(o->actions,
                                     actions_parse_string("Focus"));
        o->actions = g_slist_prepend(o->actions,
                                     actions_parse_string("Raise"));
        o->actions = g_slist_prepend(o->actions,
                                     actions_parse_string("Unshade"));
    }

    /* We define no i_pre function, but we define i_input, i_cancel, i_post. */
    *input = i_input_func;
    *cancel = i_cancel_func;
    *post = i_post_func;
    return o;
}

/*!
 * \brief Setup function specifically for "NextWindow" (forward cycling).
 */
static gpointer
setup_forward_func(xmlNodePtr node,
                   ObActionsIPreFunc *pre,
                   ObActionsIInputFunc *input,
                   ObActionsICancelFunc *cancel,
                   ObActionsIPostFunc *post)
{
    Options *o = setup_func(node, pre, input, cancel, post);
    o->forward = TRUE;
    return o;
}

/*!
 * \brief Setup function specifically for "PreviousWindow" (backward cycling).
 */
static gpointer
setup_backward_func(xmlNodePtr node,
                    ObActionsIPreFunc *pre,
                    ObActionsIInputFunc *input,
                    ObActionsICancelFunc *cancel,
                    ObActionsIPostFunc *post)
{
    Options *o = setup_func(node, pre, input, cancel, post);
    o->forward = FALSE;
    return o;
}

/*!
 * \brief Frees the Options structure, including all final actions.
 */
static void
free_func(gpointer options)
{
    Options *o = (Options *)options;

    /* Unref any registered final actions */
    while (o->actions) {
        actions_act_unref(o->actions->data);
        o->actions = g_slist_delete_link(o->actions, o->actions);
    }

    g_free(o);
}

/*!
 * \brief The core run function: performs one cycle step, possibly returns TRUE
 *        if the action is interactive and hasn't finished.
 * \return TRUE if it's an ongoing interactive cycle, FALSE if completed.
 */
static gboolean
run_func(ObActionsData *data, gpointer options)
{
    (void)data; /* data is not used here, but part of the signature */

    Options *o = (Options *)options;
    struct _ObClient *ft;
    gboolean done = FALSE;
    gboolean cancel = FALSE;

    /*
     * focus_cycle() moves the focus forward or backward
     * among the available windows based on the Options.
     */
    ft = focus_cycle(o->forward,
                     o->all_desktops,
                     !o->only_hilite_windows,
                     o->dock_windows,
                     o->desktop_windows,
                     o->linear,
                     (o->interactive ? o->bar : FALSE),
                     (o->interactive ? o->dialog_mode : OB_FOCUS_CYCLE_POPUP_MODE_NONE),
                     done,
                     cancel);

    /* Restore stacking after the cycle step */
    stacking_restore();

    /* Optionally raise the newly focused window */
    if (o->raise && ft) {
        stacking_temp_raise(CLIENT_AS_WINDOW(ft));
    }

    /*
     * If interactive is TRUE, returning TRUE will keep the action alive,
     * waiting for user to release modifiers or press ESC/Enter.
     * If interactive is FALSE, we always complete now.
     */
    return o->interactive;
}

/*!
 * \brief Handles XEvents for an in-progress interactive cycle.
 * \param initial_state The keyboard modifiers at the start of the action
 * \param e The incoming XEvent
 * \param ic The input context (unused)
 * \param options Pointer to our Options
 * \param used Set to TRUE if this event is consumed
 * \return FALSE to end the interactive action (e.g., user canceled), TRUE to continue
 */
static gboolean
i_input_func(guint initial_state,
             XEvent *e,
             ObtIC *ic,
             gpointer options,
             gboolean *used)
{
    (void)ic;   /* unused */
    (void)used; /* typically set if event is consumed, you can modify if needed */

    Options *o = (Options *)options;

    /* Extract just the modmask bits from initial and current states */
    guint initial_mods = obt_keyboard_only_modmasks(initial_state);
    guint mods         = obt_keyboard_only_modmasks(e->xkey.state);

    if (e->type == KeyRelease) {
        /* remove from 'mods' the mask for the key being released */
        mods &= ~obt_keyboard_keyevent_to_modmask(e);
    }

    if (e->type == KeyPress) {
        KeySym sym = obt_keyboard_keypress_to_keysym(e);

        /* ESC always cancels the cycle */
        if (sym == XK_Escape) {
            o->cancel = TRUE;
            o->state  = e->xkey.state;
            return FALSE;
        }
        /* If no modifiers are pressed and user hits Enter => confirm cycle */
        else if ((sym == XK_Return || sym == XK_KP_Enter) && !initial_mods) {
            o->cancel = FALSE;
            o->state  = e->xkey.state;
            return FALSE;
        }
    }
    /* If user released the original modifiers => cycle ends */
    else if (e->type == KeyRelease && initial_mods && !(mods & initial_mods)) {
        o->cancel = FALSE;
        o->state  = e->xkey.state;
        return FALSE;
    }

    /* Return TRUE => keep going (still in the cycle) */
    return TRUE;
}

/*!
 * \brief Called if the user or system cancels the interactive action mid-way.
 */
static void
i_cancel_func(gpointer options)
{
    Options *o = (Options *)options;
    o->cancel = TRUE;
    o->state  = 0;
}

/*!
 * \brief Called after the user finishes (or cancels) the interactive cycle.
 *        Actually performs the "finalactions" on the newly focused window.
 */
static void
i_post_func(gpointer options)
{
    Options *o = (Options *)options;
    struct _ObClient *ft;
    gboolean done = TRUE;

    /*
     * 'focus_cycle' is called one last time with 'done' = TRUE to finalize
     * focus, raise, etc.  The 'cancel' flag tells focus_cycle if the user
     * canceled or not.
     */
    ft = focus_cycle(o->forward,
                     o->all_desktops,
                     !o->only_hilite_windows,
                     o->dock_windows,
                     o->desktop_windows,
                     o->linear,
                     o->bar,
                     o->dialog_mode,
                     done,
                     o->cancel);

    /* If we ended on a client, run the final actions (e.g., Raise, Focus) on it. */
    if (ft) {
        actions_run_acts(o->actions,
                         OB_USER_ACTION_KEYBOARD_KEY,
                         o->state,
                         -1, -1, 0,
                         OB_FRAME_CONTEXT_NONE,
                         ft);
    }

    /* Restore stacking again post-cycle. */
    stacking_restore();
}

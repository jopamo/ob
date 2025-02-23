#include "openbox/actions.h"
#include "openbox/screen.h"

/*
 * The 'Options' struct holds parameters for this action:
 *   - current: whether to add/remove the "current" or the "last" desktop
 *   - add:     TRUE to add a desktop, FALSE to remove one
 */
typedef struct {
    gboolean current;
    gboolean add;
} Options;

static gpointer setup_func(xmlNodePtr node);
static gpointer setup_add_func(xmlNodePtr node);
static gpointer setup_remove_func(xmlNodePtr node);
static void     free_func(gpointer o);
static gboolean run_func(ObActionsData *data, gpointer options);

/* 3.4-compatibility */
static gpointer setup_addcurrent_func(xmlNodePtr node);
static gpointer setup_addlast_func(xmlNodePtr node);
static gpointer setup_removecurrent_func(xmlNodePtr node);
static gpointer setup_removelast_func(xmlNodePtr node);

/*!
 * Register the "AddDesktop" and "RemoveDesktop" actions,
 * plus older 3.4-style compat actions.
 */
void
action_addremovedesktop_startup(void)
{
    /* Modern actions */
    actions_register("AddDesktop",    setup_add_func,    free_func, run_func);
    actions_register("RemoveDesktop", setup_remove_func, free_func, run_func);

    /* 3.4-compatibility actions */
    actions_register("AddDesktopLast",    setup_addlast_func,
                     free_func, run_func);
    actions_register("RemoveDesktopLast", setup_removelast_func,
                     free_func, run_func);
    actions_register("AddDesktopCurrent", setup_addcurrent_func,
                     free_func, run_func);
    actions_register("RemoveDesktopCurrent", setup_removecurrent_func,
                     free_func, run_func);
}

/*!
 * Base helper that reads an optional <where> node from XML.
 * "last" => current = FALSE
 * "current" => current = TRUE
 */
static gpointer
setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o = g_new0(Options, 1);

    if ((n = obt_xml_find_node(node, "where"))) {
        gchar *s = obt_xml_node_string(n);
        if (s) {
            if (!g_ascii_strcasecmp(s, "last")) {
                o->current = FALSE;
            } else if (!g_ascii_strcasecmp(s, "current")) {
                o->current = TRUE;
            }
            g_free(s);
        }
    }
    return o;
}

/*!
 * Setup function for "AddDesktop".
 * Uses setup_func internally, then marks 'add' as TRUE.
 */
static gpointer
setup_add_func(xmlNodePtr node)
{
    Options *o = setup_func(node);
    o->add = TRUE;
    return o;
}

/*!
 * Setup function for "RemoveDesktop".
 * Uses setup_func internally, then marks 'add' as FALSE.
 */
static gpointer
setup_remove_func(xmlNodePtr node)
{
    Options *o = setup_func(node);
    o->add = FALSE;
    return o;
}

/*!
 * Free function for our Options struct.
 */
static void
free_func(gpointer o)
{
    g_free(o);
}

/*!
 * The run function called when the action executes.
 * \return FALSE (non-interactive, always completes immediately).
 */
static gboolean
run_func(ObActionsData *data, gpointer options)
{
    Options *o = (Options *)options;

    /* Some actions require an indication that a move is in progress */
    actions_client_move(data, TRUE);

    if (o->add) {
        screen_add_desktop(o->current);
    } else {
        screen_remove_desktop(o->current);
    }

    /* End move */
    actions_client_move(data, FALSE);

    /* Return FALSE => action is complete (non-interactive) */
    return FALSE;
}

/* --------------------------------------------------------------------------
 * 3.4-compatibility setup functions
 * -------------------------------------------------------------------------- */

static gpointer
setup_addcurrent_func(xmlNodePtr node)
{
    Options *o = setup_add_func(node);
    o->current = TRUE;
    return o;
}

static gpointer
setup_addlast_func(xmlNodePtr node)
{
    Options *o = setup_add_func(node);
    o->current = FALSE;
    return o;
}

static gpointer
setup_removecurrent_func(xmlNodePtr node)
{
    Options *o = setup_remove_func(node);
    o->current = TRUE;
    return o;
}

static gpointer
setup_removelast_func(xmlNodePtr node)
{
    Options *o = setup_remove_func(node);
    o->current = FALSE;
    return o;
}

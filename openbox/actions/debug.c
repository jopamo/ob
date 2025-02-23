#include "openbox/actions.h"
#include "obt/xml.h"

/*!
 * \brief Holds the debug string to be printed by the "Debug" action.
 */
typedef struct {
    gchar *str;
} Options;

/* Forward declarations */
static gpointer setup_func(xmlNodePtr node);
static void     free_func(gpointer options);
static gboolean run_func(ObActionsData *data, gpointer options);

/*!
 * \brief Registers the "Debug" action, which prints a user-defined string.
 */
void
action_debug_startup(void)
{
    /*
     * We register the "Debug" action with:
     * - setup_func: parse <string> from XML
     * - free_func: free the Options struct
     * - run_func:  print the debug string
     */
    actions_register("Debug", setup_func, free_func, run_func);
}

/*!
 * \brief Setup function that reads the <string> node from XML and stores it.
 * \param node  XML node referencing configuration for this action
 * \return A newly allocated Options struct
 */
static gpointer
setup_func(xmlNodePtr node)
{
    Options *o = g_new0(Options, 1);
    xmlNodePtr n = obt_xml_find_node(node, "string");

    if (n) {
        /* obt_xml_node_string() duplicates the string */
        o->str = obt_xml_node_string(n);
    }
    return o;
}

/*!
 * \brief Frees the allocated Options struct (and its stored string).
 */
static void
free_func(gpointer options)
{
    Options *o = (Options *)options;
    g_free(o->str);
    g_free(o);
}

/*!
 * \brief Prints the stored debug string.
 * \param data    Pointer to ObActionsData (unused here)
 * \param options Pointer to our Options struct with the debug string
 * \return FALSE, indicating a non-interactive action (completes immediately)
 */
static gboolean
run_func(ObActionsData *data, gpointer options)
{
    (void)data; /* Unused parameter */

    Options *o = (Options *)options;
    if (o->str) {
        g_print("%s\n", o->str);
    }
    /* Return FALSE => this action finishes immediately (non-interactive) */
    return FALSE;
}

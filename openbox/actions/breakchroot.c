#include "openbox/actions.h"
#include "openbox/keyboard.h"

static gboolean run_func(ObActionsData *data, gpointer options);

/*!
 * \brief Registers the "BreakChroot" action, which calls \c keyboard_reset_chains(1).
 */
void
action_breakchroot_startup(void)
{
    /*
     * We register the "BreakChroot" action with no setup function (NULL),
     * no free function (NULL), and run_func as our action executor.
     *
     * This action is non-interactive, so we always return FALSE.
     */
    actions_register("BreakChroot",
                     NULL,   /* No setup function */
                     NULL,   /* No free function */
                     run_func);
}

/*!
 * \brief Runs the "BreakChroot" action, resetting keyboard chains to break out of one chroot.
 * \param data    Pointer to the ObActionsData (unused here).
 * \param options Additional options (unused).
 * \return FALSE, indicating the action completes immediately (non-interactive).
 */
static gboolean
run_func(ObActionsData *data, gpointer options)
{
    (void)data;
    (void)options;

    /* break out of one chroot */
    keyboard_reset_chains(1);

    return FALSE;  /* Non-interactive: completed */
}

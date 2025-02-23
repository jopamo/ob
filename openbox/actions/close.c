#include "openbox/actions.h"
#include "openbox/client.h"

static gboolean run_func(ObActionsData *data, gpointer options);

/*!
 * \brief Registers the "Close" action, which closes the given client window.
 */
void
action_close_startup(void)
{
    /*
     * "Close" has no setup or free function,
     * and run_func is the action executor.
     */
    actions_register("Close",
                     NULL,   /* setup_func not needed */
                     NULL,   /* free_func not needed */
                     run_func);
}

/*!
 * \brief Runs the "Close" action, closing the client window.
 * \param data    Pointer to ObActionsData containing the client.
 * \param options Unused.
 * \return FALSE, indicating the action completes immediately (non-interactive).
 */
static gboolean
run_func(ObActionsData *data, gpointer options)
{
    (void)options;  /* Unused parameter */

    if (data->client) {
        client_close(data->client);
    }

    return FALSE;  /* Non-interactive: completed */
}

#include "openbox/actions.h"
#include "openbox/client.h"

static gboolean run_func_on(ObActionsData *data, gpointer options);
static gboolean run_func_off(ObActionsData *data, gpointer options);
static gboolean run_func_toggle(ObActionsData *data, gpointer options);

/*!
 * \brief Registers three actions that control window decorations:
 *        - "Decorate"
 *        - "Undecorate"
 *        - "ToggleDecorations"
 */
void
action_decorations_startup(void)
{
    /* No setup/free functions are needed; run_func_* covers all. */
    actions_register("Decorate",
                     NULL,  /* setup_func */
                     NULL,  /* free_func */
                     run_func_on);

    actions_register("Undecorate",
                     NULL,
                     NULL,
                     run_func_off);

    actions_register("ToggleDecorations",
                     NULL,
                     NULL,
                     run_func_toggle);
}

/*!
 * \brief Adds decorations to the client window (if any).
 * \return FALSE, indicating a non-interactive action (completes immediately).
 */
static gboolean
run_func_on(ObActionsData *data, gpointer options)
{
    (void)options;  /* Unused */

    if (data->client) {
        /* Indicate we're about to move/resize a client */
        actions_client_move(data, TRUE);

        /* Ensure window is decorated */
        client_set_undecorated(data->client, FALSE);

        /* Indicate we're finished moving/resizing the client */
        actions_client_move(data, FALSE);
    }
    return FALSE;
}

/*!
 * \brief Removes decorations from the client window (if any).
 * \return FALSE, indicating a non-interactive action (completes immediately).
 */
static gboolean
run_func_off(ObActionsData *data, gpointer options)
{
    (void)options;  /* Unused */

    if (data->client) {
        actions_client_move(data, TRUE);
        client_set_undecorated(data->client, TRUE);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}

/*!
 * \brief Toggles window decorations on/off based on the current state.
 * \return FALSE, indicating a non-interactive action (completes immediately).
 */
static gboolean
run_func_toggle(ObActionsData *data, gpointer options)
{
    (void)options;  /* Unused */

    if (data->client) {
        actions_client_move(data, TRUE);
        client_set_undecorated(data->client, !data->client->undecorated);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}

#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <X11/Xlib.h>
#include <glib.h>

#include "frame.h"
#include "misc.h"
#include "obt/keyboard.h"
#include "obt/xml.h"

#ifndef UNUSED
#define UNUSED( x ) (void)( x )
#endif

typedef struct _ObActionsDefinition ObActionsDefinition;
typedef struct _ObActionsAct ObActionsAct;
typedef struct _ObActionsData ObActionsData;
typedef struct _ObActionsAnyData ObActionsAnyData;
typedef struct _ObActionsGlobalData ObActionsGlobalData;
typedef struct _ObActionsClientData ObActionsClientData;
typedef struct _ObActionsSelectorData ObActionsSelectorData;

/*! Free function for custom action options */
typedef void ( *ObActionsDataFreeFunc )( gpointer options );

/*! Run function: returns TRUE if the action is still "in progress". */
typedef gboolean ( *ObActionsRunFunc )( ObActionsData *data, gpointer options );

/*! Non-interactive setup function (e.g., parse XML) */
typedef gpointer ( *ObActionsDataSetupFunc )( xmlNodePtr node );

/*! Cleanup function when shutting down actions */
typedef void ( *ObActionsShutdownFunc )( void );

/*
 * Interactive action functions
 */

/*!
 * Called prior to an interactive action beginning.
 * Return TRUE if you want the action to proceed interactively,
 * or FALSE if not.
 */
typedef gboolean ( *ObActionsIPreFunc )( guint initial_state, gpointer options );

/*! Called after an interactive action finishes. */
typedef void ( *ObActionsIPostFunc )( gpointer options );

/*!
 * Called when handling XEvents for an in-progress interactive action.
 * Return FALSE if the action should be canceled, or TRUE to keep going.
 * If *used is set to TRUE, it means the event was consumed by this action.
 */
typedef gboolean ( *ObActionsIInputFunc )( guint initial_state, XEvent *e, ObtIC *ic, gpointer options,
                                           gboolean *used );

/*! Called if an interactive action is canceled mid-way. */
typedef void ( *ObActionsICancelFunc )( gpointer options );

/*!
 * Interactive setup: parse XML plus assign i_pre, i_input, i_cancel, i_post
 */
typedef gpointer ( *ObActionsIDataSetupFunc )( xmlNodePtr node, ObActionsIPreFunc *pre, ObActionsIInputFunc *input,
                                               ObActionsICancelFunc *cancel, ObActionsIPostFunc *post );

/* --------------------------------------------------------------------------
 * Data Structures
 * -------------------------------------------------------------------------- */

struct _ObActionsData {
  ObUserAction uact;
  guint state;
  gint x;
  gint y;
  gint button;

  struct _ObClient *client;
  ObFrameContext context;
};

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/*!
 * Initialize the actions subsystem (creates internal registry, etc.).
 * \param reconfig Whether this is a "reconfigure" call (true) or full startup (false).
 */
void actions_startup( gboolean reconfig );

/*!
 * Shut down the actions subsystem, freeing all registered actions.
 * \param reconfig Whether this is a "reconfigure" call (true) or full shutdown (false).
 */
void actions_shutdown( gboolean reconfig );

/*!
 * Register a new action that can be interactive.
 * \return TRUE if registration was successful, FALSE if action name already in use.
 */
gboolean actions_register_i( const gchar *name, ObActionsIDataSetupFunc setup, ObActionsDataFreeFunc free,
                             ObActionsRunFunc run );

/*!
 * Register a new action that is non-interactive.
 * \return TRUE if registration was successful, FALSE if action name already in use.
 */
gboolean actions_register( const gchar *name, ObActionsDataSetupFunc setup, ObActionsDataFreeFunc free,
                           ObActionsRunFunc run );

/*!
 * Set an optional shutdown function for a registered action.
 * Called when actions_shutdown() is invoked.
 * \return TRUE if the action was found and updated, FALSE otherwise.
 */
gboolean actions_set_shutdown( const gchar *name, ObActionsShutdownFunc shutdown );

/*!
 * Set whether an action modifies the currently focused window.
 * Some code uses this to track whether user time should be updated, etc.
 * \return TRUE if updated successfully, FALSE otherwise.
 */
gboolean actions_set_modifies_focused_window( const gchar *name, gboolean modifies );

/*!
 * Set whether an action can "stop" while running (i.e., returns TRUE from run()).
 * \return TRUE if updated, FALSE otherwise.
 */
gboolean actions_set_can_stop( const gchar *name, gboolean modifies );

/*!
 * Parse an XML node to build an ObActionsAct instance.
 * Typically used when reading rc.xml or similar config files.
 */
ObActionsAct *actions_parse( xmlNodePtr node );

/*!
 * Build an ObActionsAct from just an action name (no XML).
 */
ObActionsAct *actions_parse_string( const gchar *name );

/*!
 * Check if a given action is interactive.
 */
gboolean actions_act_is_interactive( ObActionsAct *act );

/*!
 * Increase the reference count of an action instance.
 */
void actions_act_ref( ObActionsAct *act );

/*!
 * Decrease the reference count of an action instance (frees if it hits zero).
 */
void actions_act_unref( ObActionsAct *act );

/*!
 * Internal pointer replay management. Some actions (like window move) may
 * need an XAllowEvents(ReplayPointer).
 */
void actions_set_need_pointer_replay_before_move( gboolean replay );
gboolean actions_get_need_pointer_replay_before_move( void );

/*!
 * Run a list of actions (GSList of ObActionsAct).
 * \param acts   A GSList of ObActionsAct*
 * \param uact   The user action (e.g. mouse press, menu selection)
 * \param state  Current modifier state (e.g. shift, ctrl)
 * \param x, y   Mouse coordinates (or -1 for unknown)
 * \param button Mouse button pressed (or 0)
 * \param con    Frame context (titlebar, handle, etc.)
 * \param client The client window being acted on (if any)
 */
void actions_run_acts( GSList *acts, ObUserAction uact, guint state, gint x, gint y, gint button, ObFrameContext con,
                       struct _ObClient *client );

/*!
 * Check whether there is currently an interactive action in progress.
 */
gboolean actions_interactive_act_running( void );

/*!
 * Cancel the currently running interactive action, if any.
 */
void actions_interactive_cancel_act( void );

/*!
 * Dispatch an XEvent to the currently running interactive action, if any.
 * \return TRUE if the event was "used," FALSE otherwise.
 */
gboolean actions_interactive_input_event( XEvent *e );

/*!
 * Called by certain actions that move a client window around.
 * \param data  The action data
 * \param start TRUE if movement just started, FALSE if it just ended
 */
void actions_client_move( ObActionsData *data, gboolean start );

#endif /* __ACTIONS_H__ */

/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   keyboard.h for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#ifndef ob__keyboard_h
#define ob__keyboard_h

#include <X11/Xlib.h>
#include <glib.h>

#include "frame.h"
#include "keytree.h"

struct _ObClient;
struct _ObActionsAct;

/*!
 * \brief The root of the key binding tree, which holds all bound keys.
 *
 * This tree structure is used for mapping key combinations (such as Ctrl+Alt+T) to actions.
 */
extern KeyBindingTree *keyboard_firstnode;

/*!
 * \brief Initializes the keyboard system.
 *
 * Sets up key bindings, grabs, and any necessary resources.
 *
 * \param reconfig TRUE if this is a reconfiguration, FALSE for a fresh initialization.
 */
void keyboard_startup( gboolean reconfig );

/*!
 * \brief Shuts down the keyboard system.
 *
 * Cleans up key bindings, releases resources, and unbinds all keys.
 *
 * \param reconfig TRUE if this is a reconfiguration, FALSE for a full shutdown.
 */
void keyboard_shutdown( gboolean reconfig );

/*!
 * \brief Rebinds all keys based on the current configuration.
 *
 * This function rebuilds the key binding tree and re-grabs all key bindings.
 */
void keyboard_rebind( void );

/*!
 * \brief Creates a chroot (nested key binding scope) from a list of keys.
 *
 * This allows for temporary key binding scopes, useful for multi-step key bindings or chained actions.
 *
 * \param keylist The list of keys that define the chroot.
 */
void keyboard_chroot( GList *keylist );

/*!
 * \brief Binds a list of keys to an action.
 *
 * This function binds a specific combination of keys (e.g., "Ctrl+Alt+T") to a user-defined action.
 *
 * \param keylist The list of keys to bind.
 * \param action The action to associate with the key combination.
 * \return TRUE if the binding was successful, FALSE if there was an error (e.g., key conflict).
 */
gboolean keyboard_bind( GList *keylist, struct _ObActionsAct *action );

/*!
 * \brief Unbinds all currently configured key bindings.
 *
 * This function removes all existing key bindings from the key binding tree.
 */
void keyboard_unbind_all( void );

/*!
 * \brief Processes a keyboard event and triggers the appropriate action.
 *
 * This function is responsible for checking if the key pressed or released matches any
 * of the configured key bindings and runs the corresponding action.
 *
 * \param client The client associated with the key event (may be NULL).
 * \param e The X event representing the key press or release.
 * \return TRUE if the event triggered an action, FALSE otherwise.
 */
gboolean keyboard_event( struct _ObClient *client, const XEvent *e );

/*!
 * \brief Resets the current key binding chain.
 *
 * This function resets or partially unwinds the current chain of key bindings, depending on the
 * value of \p break_chroots. If \p break_chroots is -1, all chroots will be broken.
 *
 * \param break_chroots The number of chroots to break. -1 means to break them all.
 */
void keyboard_reset_chains( gint break_chroots );

#endif /* ob__keyboard_h */

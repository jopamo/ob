/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   keyboard.c for the Openbox window manager
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

#include "focus.h"
#include "screen.h"
#include "frame.h"
#include "openbox.h"
#include "event.h"
#include "grab.h"
#include "client.h"
#include "actions.h"
#include "menuframe.h"
#include "config.h"
#include "keytree.h"
#include "keyboard.h"
#include "translate.h"
#include "moveresize.h"
#include "popup.h"
#include "gettext.h"
#include "obt/keyboard.h"

#include <glib.h>

/*!
 * \brief Pointer to the top-level key binding tree, which holds all bound keys.
 */
KeyBindingTree *keyboard_firstnode = NULL;

/*!
 * \brief A popup for displaying chain or chroot info (e.g. "Ctrl-X - Ctrl-Y").
 */
static ObPopup *popup = NULL;

/*!
 * \brief The current position in the key binding tree (for chained bindings).
 */
static KeyBindingTree *curpos = NULL;

/*!
 * \brief An active timeout ID for automatically resetting key chains.
 *
 * If no further keys are pressed, this timer resets the current chain after a short delay.
 */
static guint chain_timer = 0;

/*!
 * \brief Helper function to (un)grab all keys from the current chain node or root node.
 *
 * \param grab TRUE => grab the relevant keys, FALSE => ungrab them.
 *
 * If \c curpos is set, we also grab/ungrab the reset key
 * (\c config_keyboard_reset_keycode + \c config_keyboard_reset_state).
 */
static void
grab_keys(gboolean grab)
{
    KeyBindingTree *p;

    /* Always ungrab all keys from the root window before re-grabbing. */
    ungrab_all_keys(obt_root(ob_screen));

    if (grab) {
        /* If we're in the middle of a chain, we grab from curpos->first_child,
         * else we grab from the entire keyboard_firstnode list. */
        p = (curpos ? curpos->first_child : keyboard_firstnode);
        while (p) {
            if (p->key) {
                grab_key(p->key, p->state, obt_root(ob_screen), GrabModeAsync);
            }
            p = p->next_sibling;
        }

        if (curpos) {
            /* Also grab the reset key so we can break out of the chain. */
            grab_key(config_keyboard_reset_keycode,
                     config_keyboard_reset_state,
                     obt_root(ob_screen), GrabModeAsync);
        }
    }
}

/*!
 * \brief Timeout callback to reset chain if no further keys are pressed in time.
 *
 * \param data Unused
 * \return FALSE to destroy the timer once it fires.
 */
static gboolean
chain_timeout(gpointer data)
{
    (void)data;
    keyboard_reset_chains(0);
    return FALSE; /* don't repeat this callback */
}

/*!
 * \brief Called once the chain timer is removed; zeroes out \c chain_timer.
 */
static void
chain_done(gpointer data)
{
    (void)data;
    chain_timer = 0;
}

/*!
 * \brief Updates \c curpos to a new tree node and updates key grabs accordingly.
 *
 * \param newpos The new node in the key binding tree to consider the "current" chain position.
 *
 * If \c newpos is non-NULL, we also show a popup with the combined textual representation
 * of the chain so far. If \c newpos is NULL, we hide the popup.
 */
static void
set_curpos(KeyBindingTree *newpos)
{
    if (curpos == newpos) {
        return;
    }

    grab_keys(FALSE);
    curpos = newpos;
    grab_keys(TRUE);

    if (curpos != NULL) {
        gchar *text = NULL;
        GList *it;
        const Rect *a;

        /* Build a string like "Ctrl+X - Ctrl+Y" from curpos->keylist. */
        for (it = curpos->keylist; it; it = g_list_next(it)) {
            gchar *oldtext = text;
            if (text == NULL) {
                text = g_strdup(it->data);
            } else {
                text = g_strconcat(text, " - ", (gchar *)it->data, NULL);
            }
            g_free(oldtext);
        }

        /* Position the popup near the top-left of the primary monitor. */
        a = screen_physical_area_primary(FALSE);
        popup_position(popup, NorthWestGravity, a->x + 10, a->y + 10);
        /* Delay 1s for the popup to show. */
        popup_delay_show(popup, 1000, text);
        g_free(text);
    } else {
        popup_hide(popup);
    }
}

/*!
 * \brief Resets or partially unwinds the current chain, optionally "breaking" any chroots.
 *
 * \param break_chroots -1 => total reset (kill chain). \n
 *                      0 => reset until the first chroot. \n
 *                      positive => how many chroots to break out of.
 *
 * We walk upward in the chain from \c curpos until we find a chroot or we've broken out
 * of as many as requested, then call \c set_curpos() at that node.
 */
void
keyboard_reset_chains(gint break_chroots)
{
    KeyBindingTree *p;

    /* Step upward from curpos. */
    for (p = curpos; p; p = p->parent) {
        if (p->chroot) {
            if (break_chroots == 0) {
                break; /* Stop at first chroot. */
            }
            if (break_chroots > 0) {
                --break_chroots;
            }
        }
    }
    set_curpos(p);
}

/*!
 * \brief Unbinds (destroys) all keyboard bindings, clearing \c keyboard_firstnode.
 */
void
keyboard_unbind_all(void)
{
    tree_destroy(keyboard_firstnode);
    keyboard_firstnode = NULL;
}

/*!
 * \brief Creates or updates a chroot in the existing key binding tree from a GList of keys.
 *
 * \param keylist A GList of keys describing the path to the chroot.
 */
void
keyboard_chroot(GList *keylist)
{
    /* Attempt to add it in the existing tree. If that fails, build a new tree. */
    if (!tree_chroot(keyboard_firstnode, keylist)) {
        KeyBindingTree *tree = tree_build(keylist);
        if (!tree) {
            return;
        }
        tree_chroot(tree, keylist);
        tree_assimilate(tree);
    }
}

/*!
 * \brief Binds an \c ObActionsAct to a GList of keys, creating or merging a path in the tree.
 *
 * \param keylist A GList describing the key chord (like "Ctrl", "X").
 * \param action  The action to run when this chord is pressed.
 * \return TRUE if bound successfully, FALSE if there's a conflict or other error.
 */
gboolean
keyboard_bind(GList *keylist, ObActionsAct *action)
{
    KeyBindingTree *tree, *t;
    gboolean conflict;

    g_assert(keylist != NULL);
    g_assert(action != NULL);

    tree = tree_build(keylist);
    if (!tree) {
        return FALSE;
    }

    /* see if there's an existing place in the tree for these keys. */
    t = tree_find(tree, &conflict);
    if (t) {
        /* Already bound => use existing node in main tree. */
        tree_destroy(tree);
        tree = NULL;
    } else if (conflict) {
        /* There's a partial conflict. */
        g_message(_("Conflict with key binding in config file"));
        tree_destroy(tree);
        return FALSE;
    } else {
        /* The newly built tree is unique. */
        t = tree;
    }

    /* Descend to the leaf node. */
    for (; t->first_child; t = t->first_child) {
        /* nothing inside loop, just go to final leaf */
    }

    /* Append the action to that leaf. */
    t->actions = g_slist_append(t->actions, action);

    /* Assimilate tree to main. That merges/destroys \p tree as needed. */
    if (tree) {
        tree_assimilate(tree);
    }

    return TRUE;
}

#if 0
/*!
 * \brief (Unused) Example function for processing an interactive keyboard grab.
 *
 * \param e       The \c XEvent to process.
 * \param client  Set to the client if relevant.
 * \return TRUE if the event was handled, FALSE otherwise.
 */
/*
gboolean
keyboard_process_interactive_grab(const XEvent *e, ObClient **client)
{
    ...
}
*/
#endif

/*!
 * \brief Handles a KeyPress or KeyRelease for the current key binding system.
 *
 * \param client Possibly relevant \c ObClient window under pointer/focus, if any.
 * \param e      The \c XEvent (key press/release).
 * \return TRUE if the key event was used/consumed, FALSE otherwise.
 */
gboolean
keyboard_event(ObClient *client, const XEvent *e)
{
    KeyBindingTree *p;
    gboolean used;
    guint mods;

    if (e->type == KeyRelease) {
        /* Decrement the passive grab count. */
        grab_key_passive_count(-1);
        return FALSE;
    }

    /* KeyPress scenario. */
    g_assert(e->type == KeyPress);
    grab_key_passive_count(1);

    mods = obt_keyboard_only_modmasks(e->xkey.state);

    /* If it's the reset key, do a total reset out of all chroots. */
    if ((e->xkey.keycode == config_keyboard_reset_keycode) &&
        (mods == config_keyboard_reset_state))
    {
        if (chain_timer) {
            g_source_remove(chain_timer);
        }
        keyboard_reset_chains(-1);
        return TRUE;
    }

    used = FALSE;

    /* If no current chain, start from the root. Otherwise, start from the children of curpos. */
    p = (curpos ? curpos->first_child : keyboard_firstnode);
    while (p) {
        if (p->key == e->xkey.keycode && p->state == mods) {
            /* Found a matching binding => close menus, handle chain or final action. */
            if (menu_frame_visible) {
                menu_frame_hide_all();
            }

            if (p->first_child != NULL) {
                /* This is a chain node => set up chain timer for 3s. */
                if (chain_timer) {
                    g_source_remove(chain_timer);
                }
                chain_timer = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                                 3000, chain_timeout, NULL,
                                                 chain_done);
                set_curpos(p);
            } else if (p->chroot) {
                /* It's an empty chroot node => just set curpos. */
                set_curpos(p);
            } else {
                /* Leaf node => run its actions. */
                GSList *it;
                for (it = p->actions; it; it = g_slist_next(it)) {
                    if (actions_act_is_interactive(it->data)) {
                        /* If any interactive action, don't reset chain. */
                        break;
                    }
                }
                if (it == NULL) {
                    /* All non-interactive => reset chain after finishing. */
                    keyboard_reset_chains(0);
                }

                actions_run_acts(p->actions,
                                 OB_USER_ACTION_KEYBOARD_KEY,
                                 e->xkey.state,
                                 e->xkey.x_root,
                                 e->xkey.y_root,
                                 0,
                                 OB_FRAME_CONTEXT_NONE,
                                 client);
            }
            used = TRUE;
            break;
        }
        p = p->next_sibling;
    }
    return used;
}

/*!
 * \brief Recursively rebinds each node in a temporary key tree to the main tree.
 *
 * \param node The root node of a partial or old key tree.
 *
 * We recursively find all leaf nodes and re-bind their actions into the main
 * tree using \c keyboard_bind. Chroot nodes are re-applied with
 * \c keyboard_chroot. Siblings are processed after children.
 */
static void
node_rebind(KeyBindingTree *node)
{
    if (node->first_child) {
        /* Rebind any children first. */
        node_rebind(node->first_child);

        /* If this node is a chroot, re-chroot it after children. */
        if (node->chroot) {
            keyboard_chroot(node->keylist);
        }
    } else {
        /* It's a leaf => for each action, do a new bind in the main tree. */
        while (node->actions) {
            /* Move each action to the main tree. */
            keyboard_bind(node->keylist, node->actions->data);
            /* Remove from the old node to avoid freeing it. */
            node->actions = g_slist_delete_link(node->actions, node->actions);
        }

        if (node->chroot) {
            /* If it's a leaf with chroot, do that. */
            keyboard_chroot(node->keylist);
        }
    }

    if (node->next_sibling) {
        node_rebind(node->next_sibling);
    }
}

/*!
 * \brief Re-binds all key actions in \c keyboard_firstnode from a preexisting or partial tree.
 *
 * We move \c keyboard_firstnode to \c old, create a fresh tree, recursively
 * rebind each node from \c old using \c node_rebind, then destroy \c old. Finally,
 * \c set_curpos(NULL) and re-grab keys.
 */
void
keyboard_rebind(void)
{
    KeyBindingTree *old;

    old = keyboard_firstnode;
    keyboard_firstnode = NULL;
    if (old) {
        node_rebind(old);
    }

    tree_destroy(old);
    set_curpos(NULL);
    grab_keys(TRUE);
}

/*!
 * \brief Initializes the keyboard system, sets up popups, and grabs current keys.
 *
 * \param reconfig TRUE if re-initializing during a reconfiguration, FALSE if cold startup.
 */
void
keyboard_startup(gboolean reconfig)
{
    (void)reconfig;

    /* Start out by grabbing current keys. */
    grab_keys(TRUE);

    /* Create a popup for chain display. */
    popup = popup_new();
    popup_set_text_align(popup, RR_JUSTIFY_CENTER);
}

/*!
 * \brief Shuts down the keyboard system, unbinding everything and freeing popups.
 * \param reconfig TRUE if reinitializing, FALSE if fully shutting down.
 */
void
keyboard_shutdown(gboolean reconfig)
{
    (void)reconfig;

    /* If a chain timer is active, remove it. */
    if (chain_timer) {
        g_source_remove(chain_timer);
    }

    /* Unbind all keys. */
    keyboard_unbind_all();

    /* Clear the chain position and free the popup. */
    set_curpos(NULL);
    popup_free(popup);
    popup = NULL;
}

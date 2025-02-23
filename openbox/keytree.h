/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   keytree.h for the Openbox window manager
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

#ifndef __plugin_keyboard_tree_h
#define __plugin_keyboard_tree_h

#include <glib.h>

/**
 * @brief A structure representing a keybinding tree.
 *
 * This structure is used to build and manage a tree of keybindings. Each node
 * represents a keybinding and can have child nodes to represent key sequences.
 */
typedef struct KeyBindingTree {
    guint state;                /**< The modifier state of the keybinding */
    guint key;                  /**< The key associated with the binding */
    GList *keylist;             /**< List of key events that make up this binding */
    GSList *actions;            /**< List of actions associated with this keybinding */
    gboolean chroot;            /**< Whether this node is part of a chroot (overriding keybinding context) */

    struct KeyBindingTree *parent;        /**< The parent node in the tree */
    struct KeyBindingTree *next_sibling;  /**< The next sibling node at the same level */
    struct KeyBindingTree *first_child;   /**< The first child node, representing a chained sequence */
} KeyBindingTree;

/**
 * @brief Destroy a KeyBindingTree.
 *
 * This function recursively destroys the entire tree, freeing all resources
 * associated with each node including keybindings, actions, and the tree structure itself.
 *
 * @param tree The root of the KeyBindingTree to be destroyed.
 */
void tree_destroy(KeyBindingTree *tree);

/**
 * @brief Build a KeyBindingTree from a list of key events.
 *
 * This function creates a tree where each node represents a keybinding. The tree is
 * built from the key events in the list, with child nodes representing key sequences.
 *
 * @param keylist A list of key events representing the keybinding sequence.
 *
 * @return A pointer to the root node of the created tree, or NULL if the list is empty.
 */
KeyBindingTree *tree_build(GList *keylist);

/**
 * @brief Assimilate a new keybinding tree into the existing tree.
 *
 * This function checks whether a keybinding already exists in the tree, and if
 * not, it adds the new tree. It ensures that existing nodes are preserved and only
 * necessary additions are made.
 *
 * @param node The KeyBindingTree node to be assimilated into the existing tree.
 */
void tree_assimilate(KeyBindingTree *node);

/**
 * @brief Find a matching node in the keybinding tree.
 *
 * This function searches the tree for a node that matches the given search node.
 * It also checks for conflicts where the keybinding's state and key are already bound elsewhere.
 *
 * @param search The KeyBindingTree node to search for.
 * @param conflict A pointer to a boolean that will be set to TRUE if a conflict is found.
 *
 * @return A pointer to the found node, or NULL if no matching node is found.
 */
KeyBindingTree *tree_find(KeyBindingTree *search, gboolean *conflict);

/**
 * @brief Mark a keybinding node as part of a chroot (temporary override).
 *
 * This function searches the tree for a keybinding node and marks it as part of a chroot,
 * which temporarily overrides the parent keybindings.
 *
 * @param tree The root node of the tree to search through.
 * @param keylist The list of key events representing the keybinding to be marked.
 *
 * @return TRUE if the chroot was successfully applied, FALSE otherwise.
 */
gboolean tree_chroot(KeyBindingTree *tree, GList *keylist);

#endif

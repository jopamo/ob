/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   keytree.c for the Openbox window manager
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

#include "keyboard.h"
#include "translate.h"
#include "actions.h"
#include <glib.h>
#include <stdio.h>

/**
 * @brief Free a single node in the KeyBindingTree.
 *
 * This function frees the resources (keylist, actions) associated with the
 * node and then frees the node itself.
 *
 * @param node The KeyBindingTree node to be freed.
 */
static void tree_free_node(KeyBindingTree *node) {
    if (node) {
        GList *it;
        GSList *sit;

        // Free the key list
        for (it = node->keylist; it != NULL; it = it->next)
            g_free(it->data);
        g_list_free(node->keylist);

        // Free the actions
        for (sit = node->actions; sit != NULL; sit = sit->next)
            actions_act_unref(sit->data);
        g_slist_free(node->actions);

        g_slice_free(KeyBindingTree, node);
    }
}

/**
 * @brief Destroy the entire KeyBindingTree.
 *
 * This function recursively destroys the tree by calling `tree_free_node`
 * to release the memory associated with each node.
 *
 * @param tree The root of the KeyBindingTree to be destroyed.
 */
void tree_destroy(KeyBindingTree *tree) {
    KeyBindingTree *current, *next;

    for (current = tree; current != NULL; current = next) {
        next = current->next_sibling;
        tree_free_node(current);
    }
}

/**
 * @brief Build a KeyBindingTree from a list of key events.
 *
 * This function builds a tree where each node represents a keybinding. The
 * keybinding list is processed from the last key event to the first, creating
 * child nodes for each key press and chaining them together.
 *
 * @param keylist A list of key events representing the keybinding sequence.
 *
 * @return A pointer to the root node of the created tree, or NULL if the list is empty.
 */
KeyBindingTree *tree_build(GList *keylist) {
    if (g_list_length(keylist) <= 0) return NULL;

    GList *it;
    KeyBindingTree *root = NULL, *node;

    for (it = g_list_last(keylist); it; it = g_list_previous(it)) {
        node = g_slice_new0(KeyBindingTree);
        node->keylist = g_list_prepend(node->keylist, g_strdup(it->data));
        node->first_child = root;
        if (root) root->parent = node;

        translate_key(it->data, &node->state, &node->key);

        root = node;
    }

    return root;
}

/**
 * @brief Assimilate a new tree into the existing keybinding tree.
 *
 * This function checks whether a keybinding already exists in the tree, and
 * if not, it adds the new tree. It ensures that existing nodes are preserved,
 * and only the necessary additions are made.
 *
 * @param new_tree The new tree to be assimilated into the existing keybinding tree.
 */
void tree_assimilate(KeyBindingTree *new_tree) {
    if (!keyboard_firstnode) {
        keyboard_firstnode = new_tree;
        return;
    }

    KeyBindingTree *existing = keyboard_firstnode;
    while (existing) {
        // Check if the new tree matches the existing one
        if (existing->state == new_tree->state && existing->key == new_tree->key) {
            if (existing->first_child == NULL) {
                existing->first_child = new_tree->first_child;
                if (new_tree->first_child) new_tree->first_child->parent = existing;
            } else {
                g_slice_free(KeyBindingTree, new_tree);
            }
            return;
        }
        existing = existing->next_sibling;
    }

    // If no match found, add the new tree as a sibling
    new_tree->next_sibling = keyboard_firstnode;
    keyboard_firstnode = new_tree;
}

/**
 * @brief Find a node in the KeyBindingTree.
 *
 * This function searches for a node that matches the given search node. It also
 * checks for conflicts, such as if the node's state and key are already bound elsewhere.
 *
 * @param search The node to search for in the tree.
 * @param conflict A pointer to a boolean that will be set to TRUE if a conflict is found.
 *
 * @return A pointer to the found node, or NULL if no matching node is found.
 */
KeyBindingTree *tree_find(KeyBindingTree *search, gboolean *conflict) {
    KeyBindingTree *current = keyboard_firstnode;
    while (current) {
        if (current->state == search->state && current->key == search->key) {
            if ((current->first_child == NULL) == (search->first_child == NULL)) {
                if (current->first_child == NULL) {
                    return current;
                } else {
                    *conflict = TRUE;
                    return NULL;
                }
            }
        }
        current = current->next_sibling;
    }
    return NULL;
}

/**
 * @brief Mark a keybinding node as part of a chroot.
 *
 * This function searches the tree for a keybinding and marks it as a chroot.
 * A chroot is a keybinding that temporarily overrides the parent keybindings.
 *
 * @param tree The root node of the tree to search through.
 * @param keylist The list of key events that represent the keybinding to be marked.
 *
 * @return TRUE if the chroot was successfully applied, FALSE otherwise.
 */
gboolean tree_chroot(KeyBindingTree *tree, GList *keylist) {
    guint key, state;
    translate_key(keylist->data, &state, &key);
    while (tree != NULL && !(tree->state == state && tree->key == key))
        tree = tree->next_sibling;

    if (tree != NULL) {
        if (keylist->next == NULL) {
            tree->chroot = TRUE;
            return TRUE;
        } else {
            return tree_chroot(tree->first_child, keylist->next);
        }
    }
    return FALSE;
}

/**
 * @brief Debug function to print the keybinding tree.
 *
 * This function recursively prints the structure of the keybinding tree to
 * help with debugging.
 *
 * @param tree The root node of the keybinding tree to print.
 * @param prefix A string to prepend to each line of output for indentation.
 */
void tree_debug(KeyBindingTree *tree, const char *prefix) {
    if (tree == NULL) return;
    printf("%sState: %u, Key: %u\n", prefix, tree->state, tree->key);

    // Recursively print child nodes
    tree_debug(tree->first_child, "  ");
    tree_debug(tree->next_sibling, prefix);
}

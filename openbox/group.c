/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   group.c for the Openbox window manager
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

#include "group.h"
#include "client.h"

/*!
 * \brief A global hash table that maps from a window leader (type Window) to an ObGroup*.
 *
 * Each entry is keyed by the leader window ID (the "group leader"), pointing to an ObGroup
 * structure containing the leader and a list of client members.
 */
static GHashTable *group_map = NULL;

/*!
 * \brief A simple hash function for a Window pointer.
 * \param w Pointer to a Window variable.
 * \return A guint representing the hashed window ID.
 *
 * Since \c Window is typically an \c unsigned long, we can just return the
 * value directly as a \c guint to form a hash.
 */
static guint
window_hash(Window *w)
{
    return *w;
}

/*!
 * \brief A comparison function to check if two Windows are equal.
 * \param w1 Pointer to the first Window.
 * \param w2 Pointer to the second Window.
 * \return TRUE if both \c Window IDs match, FALSE otherwise.
 */
static gboolean
window_comp(Window *w1, Window *w2)
{
    return (*w1 == *w2);
}

/*!
 * \brief Initializes the global group hash map unless this is a reconfiguration.
 * \param reconfig TRUE if called during a reconfiguration, FALSE otherwise.
 *
 * If \c reconfig is FALSE, creates a new GHashTable from \c window_hash and
 * \c window_comp. Otherwise, does nothing.
 */
void
group_startup(gboolean reconfig)
{
    if (reconfig)
        return;

    group_map = g_hash_table_new((GHashFunc)window_hash,
                                 (GEqualFunc)window_comp);
}

/*!
 * \brief Destroys the global group hash map unless this is a reconfiguration.
 * \param reconfig TRUE if called during a reconfiguration, FALSE otherwise.
 *
 * If \c reconfig is FALSE, frees the hash table completely. Otherwise, does nothing.
 */
void
group_shutdown(gboolean reconfig)
{
    if (reconfig)
        return;

    g_hash_table_destroy(group_map);
    group_map = NULL;
}

/*!
 * \brief Adds (or retrieves) a group for a given leader window, then appends a client to it.
 * \param leader The leader window ID (group leader).
 * \param client The ObClient to add as a member of this group.
 * \return A pointer to the \c ObGroup for \p leader.
 *
 * If \p leader isn't found in \c group_map, we create a new group, insert it
 * into the map, and then append \p client to \c self->members. If the group
 * already exists, we simply append \p client to the existing group.
 */
ObGroup*
group_add(Window leader, ObClient *client)
{
    ObGroup *self;

    self = g_hash_table_lookup(group_map, &leader);
    if (self == NULL) {
        /* Create a new group if none exists for this leader. */
        self = g_slice_new(ObGroup);
        self->leader = leader;
        self->members = NULL;
        /* Note: we store &self->leader as the key because it's stable in memory
           for as long as 'self' is alive. */
        g_hash_table_insert(group_map, &self->leader, self);
    }

    self->members = g_slist_append(self->members, client);
    return self;
}

/*!
 * \brief Removes a client from an ObGroup and frees the group if it becomes empty.
 * \param self   Pointer to the ObGroup from which \p client is removed.
 * \param client The ObClient to be removed from this group.
 *
 * If the list of members becomes empty, we remove this group from \c group_map
 * and free its memory.
 */
void
group_remove(ObGroup *self, ObClient *client)
{
    self->members = g_slist_remove(self->members, client);

    if (self->members == NULL) {
        /* If no more members, remove from map and free. */
        g_hash_table_remove(group_map, &self->leader);
        g_slice_free(ObGroup, self);
    }
}

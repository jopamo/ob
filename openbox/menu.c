/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   menu.c for the Openbox window manager
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

#include "debug.h"
#include "menu.h"
#include "openbox.h"
#include "stacking.h"
#include "grab.h"
#include "client.h"
#include "config.h"
#include "actions.h"
#include "screen.h"
#include "menuframe.h"
#include "keyboard.h"
#include "geom.h"
#include "misc.h"
#include "client_menu.h"
#include "client_list_menu.h"
#include "client_list_combined_menu.h"
#include "gettext.h"
#include "obt/xml.h"
#include "obt/paths.h"

typedef struct _ObMenuParseState ObMenuParseState;

struct _ObMenuParseState
{
    ObMenu *parent;
    ObMenu *pipe_creator;
};

static GHashTable *menu_hash = NULL;
static ObtXmlInst *menu_parse_inst;
static ObMenuParseState menu_parse_state;
static gboolean menu_can_hide = FALSE;
static guint menu_timeout_id = 0;

static void menu_destroy_hash_value(ObMenu *self);
static void parse_menu_item(xmlNodePtr node, gpointer data);
static void parse_menu_separator(xmlNodePtr node, gpointer data);
static void parse_menu(xmlNodePtr node, gpointer data);
static gunichar parse_shortcut(const gchar *label, gboolean allow_shortcut,
                               gchar **strippedlabel, guint *position,
                               gboolean *always_show);

/**
 * @brief Initializes the menu system.
 *
 * This function sets up the menu hash table, registers XML parsing functions,
 * and loads the menu configuration files.
 *
 * @param reconfig Flag indicating whether the configuration is being reloaded.
 */
void menu_startup(gboolean reconfig) {
    gboolean loaded = FALSE;
    GSList *it;

    menu_hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
                                      (GDestroyNotify)menu_destroy_hash_value);

    client_list_menu_startup(reconfig);
    client_list_combined_menu_startup(reconfig);
    client_menu_startup();

    menu_parse_inst = obt_xml_instance_new();
    menu_parse_state.parent = NULL;
    menu_parse_state.pipe_creator = NULL;

    obt_xml_register(menu_parse_inst, "menu", parse_menu, &menu_parse_state);
    obt_xml_register(menu_parse_inst, "item", parse_menu_item, &menu_parse_state);
    obt_xml_register(menu_parse_inst, "separator", parse_menu_separator, &menu_parse_state);

    // Attempt to load menu files
    for (it = config_menu_files; it; it = g_slist_next(it)) {
        if (obt_xml_load_config_file(menu_parse_inst, "openbox", it->data, "openbox_menu") ||
            obt_xml_load_file(menu_parse_inst, it->data, "openbox_menu")) {
            loaded = TRUE;
            obt_xml_tree_from_root(menu_parse_inst);
            obt_xml_close(menu_parse_inst);
            break;  // File successfully loaded, exit the loop
        } else {
            g_message(_("Unable to find a valid menu file \"%s\""), (const gchar*)it->data);
        }
    }

    // Default fallback to "menu.xml" if no valid menu file was loaded
    if (!loaded) {
        if (!obt_xml_load_config_file(menu_parse_inst, "openbox", "menu.xml", "openbox_menu")) {
            g_message(_("Unable to find a valid menu file \"%s\""), "menu.xml");
        } else {
            obt_xml_tree_from_root(menu_parse_inst);
            obt_xml_close(menu_parse_inst);
        }
    }

    g_assert(menu_parse_state.parent == NULL);
}

/**
 * @brief Shuts down the menu system and frees allocated resources.
 *
 * This function cleans up the menu system, unregisters XML parsing functions,
 * and releases memory.
 *
 * @param reconfig Flag indicating whether the configuration is being reloaded.
 */
void menu_shutdown(gboolean reconfig) {
    obt_xml_instance_unref(menu_parse_inst);
    menu_parse_inst = NULL;

    menu_frame_hide_all();

    client_list_combined_menu_shutdown(reconfig);
    client_list_menu_shutdown(reconfig);

    g_hash_table_destroy(menu_hash);
    menu_hash = NULL;
}

/**
 * @brief Checks if a menu has a pipe submenu.
 *
 * This function is used as a callback to check if a given menu item has a
 * submenu that is created from a pipe (a dynamically generated submenu from
 * a shell command).
 *
 * @param key The key in the hash table (not used here).
 * @param val The value (menu) stored in the hash table.
 * @param data User data (not used here).
 * @return TRUE if the menu has a pipe submenu, otherwise FALSE.
 */
static gboolean menu_pipe_submenu(gpointer key, gpointer val, gpointer data)
{
    ObMenu *menu = val;  // Cast the value to ObMenu
    return menu->pipe_creator != NULL;  // Return TRUE if the menu has a pipe submenu
}

/**
 * @brief Clears the cache for a menu.
 *
 * This function is used as a callback to clear cached entries for a menu
 * that has been executed. It is called when clearing cached entries for
 * pipe menus or other dynamic entries.
 *
 * @param key The key in the hash table (not used here).
 * @param val The value (menu) stored in the hash table.
 * @param data User data (not used here).
 */
static void clear_cache(gpointer key, gpointer val, gpointer data)
{
    ObMenu *menu = val;  // Cast the value to ObMenu
    if (menu->execute) {
        // If the menu has an execute command, clear its cached entries
        menu_clear_entries(menu);
    }
}

/**
 * @brief Removes submenu caches for pipe menus from the hash table.
 *
 * This function clears all cached submenu entries for pipe menus.
 */
void menu_clear_pipe_caches(void) {
    g_hash_table_foreach_remove(menu_hash, menu_pipe_submenu, NULL);
    g_hash_table_foreach(menu_hash, clear_cache, NULL);
}

/**
 * @brief Executes the command for a pipe menu and parses its output.
 *
 * Executes a shell command for a pipe menu and parses the output to create
 * new menu entries dynamically.
 *
 * @param self The menu that triggered the pipe execution.
 */
void menu_pipe_execute(ObMenu *self)
{
    gchar *output;
    GError *err = NULL;

    if (!self->execute) return; // No execute command defined for this menu

    // If the menu entries are already cached, no need to execute the command
    if (self->entries) return;

    if (!g_spawn_command_line_sync(self->execute, &output, NULL, NULL, &err)) {
        g_message(_("Failed to execute command for pipe-menu \"%s\": %s"),
                  self->execute, err->message);
        g_error_free(err);
        return;
    }

    // Parse the output of the executed command to generate menu items
    if (obt_xml_load_mem(menu_parse_inst, output, strlen(output),
                         "openbox_pipe_menu"))
    {
        menu_parse_state.pipe_creator = self;
        menu_parse_state.parent = self;
        obt_xml_tree_from_root(menu_parse_inst);
        obt_xml_close(menu_parse_inst);
    } else {
        g_message(_("Invalid output from pipe-menu \"%s\""), self->execute);
    }

    g_free(output); // Free the output buffer
}

/**
 * @brief Finds and returns a menu by its name.
 *
 * Retrieves a menu from the hash table by its name. If the menu is not found,
 * it logs a warning.
 *
 * @param name The name of the menu to search for.
 * @return The menu object if found, NULL otherwise.
 */
static ObMenu* menu_from_name(gchar *name)
{
    g_assert(name != NULL);

    ObMenu *self = g_hash_table_lookup(menu_hash, name);

    if (!self) {
        g_message(_("Attempted to access menu \"%s\" but it does not exist"), name);
    }

    return self;
}

#define VALID_SHORTCUT(c) (((c) >= '0' && (c) <= '9') || \
                           ((c) >= 'A' && (c) <= 'Z') || \
                           ((c) >= 'a' && (c) <= 'z'))

/**
 * @brief Parses a label and extracts a shortcut key if allowed.
 *
 * This function processes the label of a menu item to extract any shortcut key
 * indicated by an underscore ('_'). It also removes the underscore and modifies
 * the label if a shortcut key is present.
 *
 * @param label The label string of the menu item.
 * @param allow_shortcut A flag to determine if shortcuts are allowed.
 * @param strippedlabel The label with the underscore removed (output).
 * @param position The position of the shortcut key within the label.
 * @param always_show If TRUE, the shortcut is always visible.
 * @return The shortcut key (if any).
 */
static gunichar parse_shortcut(const gchar *label, gboolean allow_shortcut,
                               gchar **strippedlabel, guint *position,
                               gboolean *always_show)
{
    gunichar shortcut = 0;

    *position = 0;
    *always_show = FALSE;

    g_assert(strippedlabel != NULL);

    if (label == NULL) {
        *strippedlabel = NULL;
    } else {
        gchar *i;
        gboolean escape;

        *strippedlabel = g_strdup(label); // Deep copy of the label

        // Allow "__" to escape an underscore and avoid shortcut conflicts
        i = *strippedlabel;
        do {
            escape = FALSE;
            i = strchr(i, '_');
            if (i && *(i + 1) == '_') {
                gchar *j;

                // Remove the escape '_' from the string
                for (j = i; *j != '\0'; ++j)
                    *j = *(j + 1);

                ++i;
                escape = TRUE;
            }
        } while (escape);

        if (allow_shortcut && i != NULL) {
            // If there's an underscore, check if it can be used as a shortcut
            if (VALID_SHORTCUT(*(i + 1))) {
                shortcut = g_unichar_tolower(g_utf8_get_char(i + 1));
                *position = i - *strippedlabel;
                *always_show = TRUE;

                // Remove the underscore from the label
                for (; *i != '\0'; ++i)
                    *i = *(i + 1);
            } else if (*(i + 1) == '\0') {
                // Skip the shortcut if the underscore is the last character
                *i = '\0';
            }
        } else {
            // If no underscore, pick the first valid character
            for (i = *strippedlabel; *i != '\0'; ++i)
                if (VALID_SHORTCUT(*i)) {
                    *position = i - *strippedlabel;
                    shortcut = g_unichar_tolower(g_utf8_get_char(i));
                    break;
                }
        }
    }

    return shortcut;
}

/**
 * @brief Parses a menu item XML node and adds it to the parent menu.
 *
 * This function is called when parsing the `<item>` XML tag for a menu. It
 * extracts the label, icon, and actions, then adds them to the parent menu.
 *
 * @param node The XML node representing the item.
 * @param data The user data (ObMenuParseState) passed to the parser.
 */
static void parse_menu_item(xmlNodePtr node, gpointer data)
{
    ObMenuParseState *state = data;
    gchar *label;
    gchar *icon;
    ObMenuEntry *e;

    if (state->parent) {
        // Extract the label attribute for the menu item
        if (obt_xml_attr_string_unstripped(node, "label", &label)) {
            xmlNodePtr c;
            GSList *acts = NULL;

            // Extract actions associated with the menu item
            c = obt_xml_find_node(node->children, "action");
            while (c) {
                ObActionsAct *action = actions_parse(c);
                if (action)
                    acts = g_slist_append(acts, action);
                c = obt_xml_find_node(c->next, "action");
            }

            // Add the menu item to the parent menu
            e = menu_add_normal(state->parent, -1, label, acts, TRUE);

            // If icons are enabled, add the icon to the menu entry
            if (config_menu_show_icons && obt_xml_attr_string(node, "icon", &icon)) {
                e->data.normal.icon = RrImageNewFromName(ob_rr_icons, icon);
                if (e->data.normal.icon)
                    e->data.normal.icon_alpha = 0xff;

                g_free(icon);
            }

            g_free(label);
        }
    }
}

/**
 * @brief Parses a menu separator XML node.
 *
 * This function is called when parsing the `<separator>` XML tag for a menu.
 * It adds a separator entry to the parent menu.
 *
 * @param node The XML node representing the separator.
 * @param data The user data (ObMenuParseState) passed to the parser.
 */
static void parse_menu_separator(xmlNodePtr node, gpointer data)
{
    ObMenuParseState *state = data;

    if (state->parent) {
        gchar *label;

        if (!obt_xml_attr_string_unstripped(node, "label", &label))
            label = NULL;

        // Add a separator to the menu
        menu_add_separator(state->parent, -1, label);
        g_free(label);
    }
}

/**
 * @brief Parses a menu XML node and adds it to the menu system.
 *
 * This function handles parsing the `<menu>` XML tag, creating a new menu,
 * and adding it to the menu system.
 *
 * @param node The XML node representing the menu.
 * @param data The user data (ObMenuParseState) passed to the parser.
 */
static void parse_menu(xmlNodePtr node, gpointer data)
{
    ObMenuParseState *state = data;
    gchar *name = NULL, *title = NULL, *script = NULL;
    ObMenu *menu;
    ObMenuEntry *e;
    gchar *icon;

    if (!obt_xml_attr_string(node, "id", &name))
        goto parse_menu_fail;

    if (!g_hash_table_lookup(menu_hash, name)) {
        if (!obt_xml_attr_string_unstripped(node, "label", &title))
            goto parse_menu_fail;

        if ((menu = menu_new(name, title, TRUE, NULL))) {
            menu->pipe_creator = state->pipe_creator;
            if (obt_xml_attr_string(node, "execute", &script)) {
                menu->execute = obt_paths_expand_tilde(script);
            } else {
                ObMenu *old;

                old = state->parent;
                state->parent = menu;
                obt_xml_tree(menu_parse_inst, node->children);
                state->parent = old;
            }
        }
    }

    if (state->parent) {
        e = menu_add_submenu(state->parent, -1, name);

        if (config_menu_show_icons && obt_xml_attr_string(node, "icon", &icon)) {
            e->data.submenu.icon = RrImageNewFromName(ob_rr_icons, icon);

            if (e->data.submenu.icon)
                e->data.submenu.icon_alpha = 0xff;

            g_free(icon);
        }
    }

parse_menu_fail:
    g_free(name);
    g_free(title);
    g_free(script);
}

/**
 * @brief Creates a new menu.
 *
 * This function initializes a new menu, sets its properties (like name,
 * title, shortcut, etc.), and adds it to the global menu hash table.
 *
 * @param name The name of the menu.
 * @param title The title of the menu.
 * @param allow_shortcut_selection A flag indicating if shortcut selection is allowed.
 * @param data User data to associate with the menu.
 * @return The newly created menu object.
 */
ObMenu* menu_new(const gchar *name, const gchar *title,
                 gboolean allow_shortcut_selection, gpointer data)
{
    ObMenu *self;

    self = g_slice_new0(ObMenu);
    self->name = g_strdup(name);
    self->data = data;

    // Parse the shortcut key if applicable
    self->shortcut = parse_shortcut(title, allow_shortcut_selection,
                                    &self->title, &self->shortcut_position,
                                    &self->shortcut_always_show);
    self->collate_key = g_utf8_collate_key(self->title, -1);

    // Add menu to the global hash table
    g_hash_table_replace(menu_hash, self->name, self);

    // Create a "More..." submenu for the menu
    self->more_menu = g_slice_new0(ObMenu);
    self->more_menu->name = _("More...");
    self->more_menu->title = _("More...");
    self->more_menu->collate_key = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"; // Non-collating key
    self->more_menu->data = data;
    self->more_menu->shortcut = g_unichar_tolower(g_utf8_get_char("M"));

    return self;
}

/**
 * @brief Cleans up and frees the resources associated with the menu.
 *
 * This function ensures that the menu is properly destroyed, cleaning up
 * any associated memory and freeing the resources.
 *
 * @param self The menu to destroy.
 */
static void menu_destroy_hash_value(ObMenu *self)
{
    // Ensure menu isn't visible before destroying it
    {
        GList *it;
        ObMenuFrame *f;

        for (it = menu_frame_visible; it; it = g_list_next(it)) {
            f = it->data;
            if (f->menu == self)
                menu_frame_hide_all();
        }
    }

    // Call the destroy function if set
    if (self->destroy_func)
        self->destroy_func(self, self->data);

    // Clear the menu entries and free associated memory
    menu_clear_entries(self);
    g_free(self->name);
    g_free(self->title);
    g_free(self->collate_key);
    g_free(self->execute);
    g_slice_free(ObMenu, self->more_menu);

    g_slice_free(ObMenu, self); // Free the menu object
}

/**
 * @brief Frees the menu from the global menu hash table.
 *
 * This function removes the menu from the global hash table, ensuring it
 * is no longer accessible.
 *
 * @param menu The menu to free.
 */
void menu_free(ObMenu *menu)
{
    if (menu)
        g_hash_table_remove(menu_hash, menu->name);
}

/**
 * @brief Function triggered when the hide delay expires.
 *
 * This function is used to trigger the menu hide functionality after
 * the configured delay has passed.
 *
 * @param data User data passed to the callback.
 * @return FALSE to ensure the function is not called again.
 */
static gboolean menu_hide_delay_func(gpointer data)
{
    menu_can_hide = TRUE;
    menu_timeout_id = 0;

    return FALSE; // No repeat
}

/**
 * @brief Displays the menu at a specified position.
 *
 * This function shows the menu at the specified position, either in
 * response to a mouse click or a keyboard input.
 *
 * @param name The name of the menu to show.
 * @param pos The position on the screen to show the menu.
 * @param monitor The monitor index on which the menu will be displayed.
 * @param mouse Whether the menu was triggered by mouse input.
 * @param user_positioned Whether the menu was positioned manually by the user.
 * @param client The client associated with the menu.
 */
void menu_show(gchar *name, const GravityPoint *pos, gint monitor,
               gboolean mouse, gboolean user_positioned, ObClient *client)
{
    ObMenu *self;
    ObMenuFrame *frame;

    if (!(self = menu_from_name(name)) || grab_on_keyboard() || grab_on_pointer())
        return;

    // Prevent showing the menu if it's already visible
    if (menu_frame_visible) {
        frame = menu_frame_visible->data;
        if (frame->menu == self)
            return;
    }

    // Hide any currently visible menus
    menu_frame_hide_all();

    // Clear pipe menu caches when displaying a new menu
    menu_clear_pipe_caches();

    frame = menu_frame_new(self, 0, client);
    if (!menu_frame_show_topmenu(frame, pos, monitor, mouse, user_positioned))
        menu_frame_free(frame);
    else {
        if (!mouse) {
            // Select the first normal entry if opened via keyboard
            GList *it = frame->entries;
            while (it) {
                ObMenuEntryFrame *e = it->data;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL) {
                    menu_frame_select(frame, e, FALSE);
                    break;
                } else if (e->entry->type == OB_MENU_ENTRY_TYPE_SEPARATOR)
                    it = g_list_next(it);
                else
                    break;
            }
        }

        // Reset the hide timer if necessary
        if (!mouse)
            menu_can_hide = TRUE;
        else {
            menu_can_hide = FALSE;
            if (menu_timeout_id) g_source_remove(menu_timeout_id);
            menu_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                                 config_menu_hide_delay,
                                                 menu_hide_delay_func,
                                                 NULL, NULL);
        }
    }
}

/**
 * @brief Checks whether the menu hide delay has been reached.
 *
 * This function checks if the configured delay for hiding the menu has expired.
 *
 * @return TRUE if the menu can be hidden, otherwise FALSE.
 */
gboolean menu_hide_delay_reached(void)
{
    return menu_can_hide;
}

/**
 * @brief Resets the menu hide delay timer.
 *
 * This function resets the timer for hiding the menu.
 */
void menu_hide_delay_reset(void)
{
    if (menu_timeout_id) g_source_remove(menu_timeout_id);
    menu_hide_delay_func(NULL);
}

/**
 * @brief Creates a new menu entry.
 *
 * This function creates a new menu entry of the specified type and associates
 * it with the provided menu.
 *
 * @param menu The menu to associate the entry with.
 * @param type The type of the menu entry (normal, submenu, separator).
 * @param id The ID of the entry.
 * @return The newly created menu entry.
 */
static ObMenuEntry* menu_entry_new(ObMenu *menu, ObMenuEntryType type, gint id)
{
    ObMenuEntry *self;

    g_assert(menu);

    self = g_slice_new0(ObMenuEntry);
    self->ref = 1;
    self->type = type;
    self->menu = menu;
    self->id = id;

    if (type == OB_MENU_ENTRY_TYPE_NORMAL) {
        self->data.normal.enabled = TRUE;
    }

    return self;
}

/**
 * @brief Increments the reference count of a menu entry.
 *
 * This function is used to increment the reference count of a menu entry.
 *
 * @param self The menu entry to reference.
 */
void menu_entry_ref(ObMenuEntry *self)
{
    ++self->ref;
}

/**
 * @brief Decrements the reference count of a menu entry.
 *
 * This function is used to decrement the reference count of a menu entry,
 * and frees the entry if the reference count reaches zero.
 *
 * @param self The menu entry to unreference.
 */
void menu_entry_unref(ObMenuEntry *self)
{
    if (self && --self->ref == 0) {
        // Free resources based on the entry type
        switch (self->type) {
        case OB_MENU_ENTRY_TYPE_NORMAL:
            RrImageUnref(self->data.normal.icon);
            g_free(self->data.normal.label);
            g_free(self->data.normal.collate_key);
            while (self->data.normal.actions) {
                actions_act_unref(self->data.normal.actions->data);
                self->data.normal.actions =
                    g_slist_delete_link(self->data.normal.actions,
                                        self->data.normal.actions);
            }
            break;
        case OB_MENU_ENTRY_TYPE_SUBMENU:
            RrImageUnref(self->data.submenu.icon);
            g_free(self->data.submenu.name);
            break;
        case OB_MENU_ENTRY_TYPE_SEPARATOR:
            g_free(self->data.separator.label);
            break;
        }

        g_slice_free(ObMenuEntry, self);
    }
}

/**
 * @brief Clears all entries from the menu.
 *
 * This function removes all menu entries and clears the entries list,
 * ensuring that the menu is empty.
 *
 * @param self The menu whose entries need to be cleared.
 */
void menu_clear_entries(ObMenu *self)
{
#ifdef DEBUG
    /* Assert that the menu isn't currently visible */
    {
        GList *it;
        ObMenuFrame *f;

        for (it = menu_frame_visible; it; it = g_list_next(it)) {
            f = it->data;
            g_assert(f->menu != self);
        }
    }
#endif

    /* Unreference and remove each entry from the menu */
    while (self->entries) {
        menu_entry_unref(self->entries->data);
        self->entries = g_list_delete_link(self->entries, self->entries);
    }

    /* Keep 'more_menu' entries in sync */
    self->more_menu->entries = self->entries;
}

/**
 * @brief Removes a menu entry.
 *
 * This function removes a specific entry from the menu and unreferences it.
 *
 * @param self The menu entry to remove.
 */
void menu_entry_remove(ObMenuEntry *self)
{
    self->menu->entries = g_list_remove(self->menu->entries, self);
    menu_entry_unref(self);
}

/**
 * @brief Adds a normal menu entry.
 *
 * This function adds a normal entry to the menu, associates it with actions,
 * and sets the label and shortcut.
 *
 * @param self The menu to which the entry is added.
 * @param id The ID of the entry.
 * @param label The label for the entry.
 * @param actions The actions associated with the entry.
 * @param allow_shortcut Whether to allow shortcut parsing.
 * @return The newly added menu entry.
 */
ObMenuEntry* menu_add_normal(ObMenu *self, gint id, const gchar *label,
                             GSList *actions, gboolean allow_shortcut)
{
    ObMenuEntry *e = menu_entry_new(self, OB_MENU_ENTRY_TYPE_NORMAL, id);
    e->data.normal.actions = actions;
    menu_entry_set_label(e, label, allow_shortcut);

    self->entries = g_list_append(self->entries, e);
    self->more_menu->entries = self->entries; /* keep it in sync */

    return e;
}

/**
 * @brief Adds a "more" submenu entry.
 *
 * This function creates a submenu entry that points to itself and can show more
 * items if the menu exceeds the available screen space.
 *
 * @param self The menu to which the "more" submenu is added.
 * @param show_from The index from which to start showing more items.
 * @return The newly added "more" menu entry.
 */
ObMenuEntry* menu_get_more(ObMenu *self, guint show_from)
{
    ObMenuEntry *e = menu_entry_new(self, OB_MENU_ENTRY_TYPE_SUBMENU, -1);
    e->data.submenu.name = g_strdup(self->name);
    e->data.submenu.submenu = self;
    e->data.submenu.show_from = show_from;

    return e;
}

/**
 * @brief Adds a submenu entry to the menu.
 *
 * This function adds a submenu entry to the menu, associating it with a specific submenu.
 *
 * @param self The menu to which the submenu entry is added.
 * @param id The ID of the submenu entry.
 * @param submenu The name of the submenu.
 * @return The newly added submenu entry.
 */
ObMenuEntry* menu_add_submenu(ObMenu *self, gint id, const gchar *submenu)
{
    ObMenuEntry *e = menu_entry_new(self, OB_MENU_ENTRY_TYPE_SUBMENU, id);
    e->data.submenu.name = g_strdup(submenu);

    self->entries = g_list_append(self->entries, e);
    self->more_menu->entries = self->entries; /* keep it in sync */

    return e;
}

/**
 * @brief Adds a separator entry to the menu.
 *
 * This function adds a separator entry to the menu, optionally setting a label.
 *
 * @param self The menu to which the separator is added.
 * @param id The ID of the separator entry.
 * @param label The label for the separator, or NULL.
 * @return The newly added separator entry.
 */
ObMenuEntry* menu_add_separator(ObMenu *self, gint id, const gchar *label)
{
    ObMenuEntry *e = menu_entry_new(self, OB_MENU_ENTRY_TYPE_SEPARATOR, id);
    menu_entry_set_label(e, label, FALSE);

    self->entries = g_list_append(self->entries, e);
    self->more_menu->entries = self->entries; /* keep it in sync */

    return e;
}

/**
 * @brief Sets the function to show the menu.
 *
 * This function sets the callback function to show the menu.
 *
 * @param self The menu to set the show function for.
 * @param func The callback function to show the menu.
 */
void menu_set_show_func(ObMenu *self, ObMenuShowFunc func)
{
    self->show_func = func;
}

/**
 * @brief Sets the function to hide the menu.
 *
 * This function sets the callback function to hide the menu.
 *
 * @param self The menu to set the hide function for.
 * @param func The callback function to hide the menu.
 */
void menu_set_hide_func(ObMenu *self, ObMenuHideFunc func)
{
    self->hide_func = func;
}

/**
 * @brief Sets the function to update the menu.
 *
 * This function sets the callback function to update the menu.
 *
 * @param self The menu to set the update function for.
 * @param func The callback function to update the menu.
 */
void menu_set_update_func(ObMenu *self, ObMenuUpdateFunc func)
{
    self->update_func = func;
}

/**
 * @brief Sets the function to execute when an item is selected.
 *
 * This function sets the callback function to execute an action when an item
 * in the menu is selected.
 *
 * @param self The menu to set the execute function for.
 * @param func The callback function to execute an action.
 */
void menu_set_execute_func(ObMenu *self, ObMenuExecuteFunc func)
{
    self->execute_func = func;
    self->more_menu->execute_func = func; /* keep it in sync */
}

/**
 * @brief Sets the function to clean up the menu.
 *
 * This function sets the callback function to clean up the menu when it is destroyed.
 *
 * @param self The menu to set the cleanup function for.
 * @param func The callback function to clean up the menu.
 */
void menu_set_cleanup_func(ObMenu *self, ObMenuCleanupFunc func)
{
    self->cleanup_func = func;
}

/**
 * @brief Sets the function to destroy the menu.
 *
 * This function sets the callback function to destroy the menu when it is no longer needed.
 *
 * @param self The menu to set the destroy function for.
 * @param func The callback function to destroy the menu.
 */
void menu_set_destroy_func(ObMenu *self, ObMenuDestroyFunc func)
{
    self->destroy_func = func;
}

/**
 * @brief Sets the function to determine the placement of the menu.
 *
 * This function sets the callback function to control the placement of the menu.
 *
 * @param self The menu to set the place function for.
 * @param func The callback function to place the menu.
 */
void menu_set_place_func(ObMenu *self, ObMenuPlaceFunc func)
{
    self->place_func = func;
}

/**
 * @brief Finds a menu entry by its ID.
 *
 * This function searches the menu for an entry with the specified ID.
 *
 * @param self The menu to search in.
 * @param id The ID of the entry to find.
 * @return The found menu entry, or NULL if not found.
 */
ObMenuEntry* menu_find_entry_id(ObMenu *self, gint id)
{
    GList *it;

    for (it = self->entries; it; it = g_list_next(it)) {
        ObMenuEntry *e = it->data;
        if (e->id == id)
            return e;
    }

    return NULL;
}

/**
 * @brief Finds and associates submenus with their respective menu entries.
 *
 * This function iterates over the entries and assigns the submenu associated
 * with each entry, if applicable.
 *
 * @param self The menu to check for submenus.
 */
void menu_find_submenus(ObMenu *self)
{
    GList *it;

    for (it = self->entries; it; it = g_list_next(it)) {
        ObMenuEntry *e = it->data;
        if (e->type == OB_MENU_ENTRY_TYPE_SUBMENU) {
            e->data.submenu.submenu = menu_from_name(e->data.submenu.name);
        }
    }
}

/**
 * @brief Sets the label of a menu entry.
 *
 * This function sets the label for a menu entry, and processes the shortcut
 * if applicable.
 *
 * @param self The menu entry whose label is being set.
 * @param label The new label for the entry.
 * @param allow_shortcut Whether to allow shortcut parsing.
 */
void menu_entry_set_label(ObMenuEntry *self, const gchar *label,
                          gboolean allow_shortcut)
{
    switch (self->type) {
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        g_free(self->data.separator.label);
        self->data.separator.label = g_strdup(label);
        break;
    case OB_MENU_ENTRY_TYPE_NORMAL:
        g_free(self->data.normal.label);
        g_free(self->data.normal.collate_key);
        self->data.normal.shortcut =
            parse_shortcut(label, allow_shortcut, &self->data.normal.label,
                           &self->data.normal.shortcut_position,
                           &self->data.normal.shortcut_always_show);
        self->data.normal.collate_key =
            g_utf8_collate_key(self->data.normal.label, -1);
        break;
    default:
        g_assert_not_reached();
    }
}

/**
 * @brief Displays or hides all shortcuts in the menu.
 *
 * This function controls whether all shortcuts are visible in the menu.
 *
 * @param self The menu to modify.
 * @param show Whether to show all shortcuts.
 */
void menu_show_all_shortcuts(ObMenu *self, gboolean show)
{
    self->show_all_shortcuts = show;
}

/**
 * @brief Comparison function for sorting menu entries.
 *
 * This function compares two menu entries, prioritizing normal entries
 * and then submenus. The sorting is based on the collate key of normal
 * entries or submenus.
 *
 * @param a Pointer to the first menu entry.
 * @param b Pointer to the second menu entry.
 * @return A negative value if a < b, zero if a == b, positive if a > b.
 */
static int sort_func(const void *a, const void *b) {
    const ObMenuEntry *entry_a = *(const ObMenuEntry**)a;
    const ObMenuEntry *entry_b = *(const ObMenuEntry**)b;

    const gchar *key_a, *key_b;

    // Determine the collate key for the first entry
    if (entry_a->type == OB_MENU_ENTRY_TYPE_NORMAL) {
        key_a = entry_a->data.normal.collate_key;
    } else {
        g_assert(entry_a->type == OB_MENU_ENTRY_TYPE_SUBMENU);
        key_a = entry_a->data.submenu.submenu ? entry_a->data.submenu.submenu->collate_key : "";
    }

    // Determine the collate key for the second entry
    if (entry_b->type == OB_MENU_ENTRY_TYPE_NORMAL) {
        key_b = entry_b->data.normal.collate_key;
    } else {
        g_assert(entry_b->type == OB_MENU_ENTRY_TYPE_SUBMENU);
        key_b = entry_b->data.submenu.submenu ? entry_b->data.submenu.submenu->collate_key : "";
    }

    return strcmp(key_a, key_b);
}

/**
 * @brief Sorts a range of entries in the menu.
 *
 * This function sorts the specified range of menu entries using a quicksort
 * algorithm. It handles the sorting of the menu entries in place.
 *
 * @param self The menu whose entries need to be sorted.
 * @param start The first entry in the range to sort.
 * @param end The last entry in the range to sort.
 * @param len The length of the range to sort.
 */
static void sort_range(ObMenu *self, GList *start, GList *end, guint len)
{
    if (len < 2) {
        return; // No sorting needed if the range has fewer than two elements
    }

    ObMenuEntry **entries = g_slice_alloc(len * sizeof(ObMenuEntry*));

    // Copy the entries into the array
    GList *it = start;
    for (guint i = 0; it != g_list_next(end); ++i, it = g_list_next(it)) {
        entries[i] = it->data;
    }

    // Perform the sorting
    qsort(entries, len, sizeof(ObMenuEntry*), sort_func);

    // Copy the sorted entries back into the list
    it = start;
    for (guint i = 0; it != g_list_next(end); ++i, it = g_list_next(it)) {
        it->data = entries[i];
    }

    g_slice_free1(len * sizeof(ObMenuEntry*), entries);
}

/**
 * @brief Sorts the entries of the menu.
 *
 * This function sorts the menu entries, taking into account separators and
 * submenus. It groups and sorts the entries in their respective ranges.
 *
 * @param self The menu whose entries need to be sorted.
 */
void menu_sort_entries(ObMenu *self)
{
    // Ensure submenus are populated with their collate keys
    menu_find_submenus(self);

    GList *it = self->entries;
    GList *start = NULL;
    guint len = 0;

    // Iterate over the entries to find groups of entries to sort
    while (it) {
        ObMenuEntry *entry = it->data;

        if (entry->type == OB_MENU_ENTRY_TYPE_SEPARATOR) {
            // Sort the range up to the separator
            if (start) {
                sort_range(self, start, g_list_previous(it), len);
                start = NULL;
                len = 0;
            }
        } else {
            // Collect non-separator entries to sort
            if (start == NULL) {
                start = it;
            }
            len++;
        }

        it = g_list_next(it);
    }

    // Sort the last group of entries if any
    if (start) {
        sort_range(self, start, g_list_last(self->entries), len);
    }
}

/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   if.c for the Openbox window manager
   Copyright (c) 2007        Mikael Magnusson
   Copyright (c) 2007        Dana Jansens

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

#include "openbox/actions.h"
#include "openbox/misc.h"
#include "openbox/client.h"
#include "openbox/frame.h"
#include "openbox/screen.h"
#include "openbox/focus.h"
#include <glib.h>

typedef enum {
    QUERY_TARGET_IS_ACTION_TARGET,
    QUERY_TARGET_IS_FOCUS_TARGET,
} QueryTarget;

typedef enum {
    MATCH_TYPE_NONE = 0,
    MATCH_TYPE_PATTERN,
    MATCH_TYPE_REGEX,
    MATCH_TYPE_EXACT,
} MatchType;

typedef struct {
    MatchType type;
    union {
        GPatternSpec *pattern;
        GRegex *regex;
        gchar *exact;
    } m;
} TypedMatch;

typedef struct {
    QueryTarget target;
    gboolean shaded_on, shaded_off;
    gboolean maxvert_on, maxvert_off;
    gboolean maxhorz_on, maxhorz_off;
    gboolean maxfull_on, maxfull_off;
    gboolean iconic_on, iconic_off;
    gboolean focused, unfocused;
    gboolean urgent_on, urgent_off;
    gboolean decor_off, decor_on;
    gboolean omnipresent_on, omnipresent_off;
    gboolean desktop_current, desktop_other;
    guint desktop_number, screendesktop_number, client_monitor;
    TypedMatch title, class, name, role, type;
} Query;

typedef struct {
    GArray *queries;
    GSList *thenacts, *elseacts;
} Options;

static gpointer setup_func(xmlNodePtr node);
static void free_func(gpointer options);
static gboolean run_func_if(ObActionsData *data, gpointer options);
static gboolean run_func_stop(ObActionsData *data, gpointer options);
static gboolean run_func_foreach(ObActionsData *data, gpointer options);

static gboolean foreach_stop;

void action_if_startup(void)
{
    actions_register("If", setup_func, free_func, run_func_if);
    actions_register("Stop", NULL, NULL, run_func_stop);
    actions_register("ForEach", setup_func, free_func, run_func_foreach);
    actions_set_can_stop("Stop", TRUE);
}

static inline void set_bool(xmlNodePtr node, const char *name, gboolean *on, gboolean *off)
{
    xmlNodePtr n = obt_xml_find_node(node, name);
    if (n) {
        *on = obt_xml_node_bool(n);
        *off = !*on;
    }
}

static void setup_typed_match(TypedMatch *tm, xmlNodePtr n)
{
    gchar *s = obt_xml_node_string(n);
    if (!s) return;

    gchar *type = NULL;
    if (!obt_xml_attr_string(n, "type", &type) || !g_ascii_strcasecmp(type, "pattern")) {
        tm->type = MATCH_TYPE_PATTERN;
        tm->m.pattern = g_pattern_spec_new(s);
    } else if (type && !g_ascii_strcasecmp(type, "regex")) {
        tm->type = MATCH_TYPE_REGEX;
        tm->m.regex = g_regex_new(s, 0, 0, NULL);
    } else if (type && !g_ascii_strcasecmp(type, "exact")) {
        tm->type = MATCH_TYPE_EXACT;
        tm->m.exact = g_strdup(s);
    }

    g_free(s);
    g_free(type);
}

static void free_typed_match(TypedMatch *tm)
{
    switch (tm->type) {
        case MATCH_TYPE_PATTERN: g_pattern_spec_free(tm->m.pattern); break;
        case MATCH_TYPE_REGEX: g_regex_unref(tm->m.regex); break;
        case MATCH_TYPE_EXACT: g_free(tm->m.exact); break;
        default: break;
    }
}

static gboolean check_typed_match(TypedMatch *tm, const gchar *s)
{
    switch (tm->type) {
        case MATCH_TYPE_PATTERN: return g_pattern_spec_match_string(tm->m.pattern, s);
        case MATCH_TYPE_REGEX: return g_regex_match(tm->m.regex, s, 0, NULL);
        case MATCH_TYPE_EXACT: return g_strcmp0(tm->m.exact, s) == 0;
        default: return TRUE;
    }
}

static void setup_query(Options *o, xmlNodePtr node, QueryTarget target)
{
    Query *q = g_slice_new0(Query);
    g_array_append_val(o->queries, q);

    q->target = target;
    set_bool(node, "shaded", &q->shaded_on, &q->shaded_off);
    set_bool(node, "maximized", &q->maxfull_on, &q->maxfull_off);
    set_bool(node, "maximizedhorizontal", &q->maxhorz_on, &q->maxhorz_off);
    set_bool(node, "maximizedvertical", &q->maxvert_on, &q->maxvert_off);
    set_bool(node, "iconified", &q->iconic_on, &q->iconic_off);
    set_bool(node, "focused", &q->focused, &q->unfocused);
    set_bool(node, "urgent", &q->urgent_on, &q->urgent_off);
    set_bool(node, "undecorated", &q->decor_off, &q->decor_on);
    set_bool(node, "omnipresent", &q->omnipresent_on, &q->omnipresent_off);

    // Parse desktop info
    xmlNodePtr n;
    if ((n = obt_xml_find_node(node, "desktop"))) {
        gchar *s = obt_xml_node_string(n);
        if (s) {
            if (!g_ascii_strcasecmp(s, "current"))
                q->desktop_current = TRUE;
            else if (!g_ascii_strcasecmp(s, "other"))
                q->desktop_other = TRUE;
            else
                q->desktop_number = atoi(s);
            g_free(s);
        }
    }

    if ((n = obt_xml_find_node(node, "activedesktop"))) {
        q->screendesktop_number = obt_xml_node_int(n);
    }

    // Setup title, class, name, role, and type matches
    if ((n = obt_xml_find_node(node, "title"))) setup_typed_match(&q->title, n);
    if ((n = obt_xml_find_node(node, "class"))) setup_typed_match(&q->class, n);
    if ((n = obt_xml_find_node(node, "name"))) setup_typed_match(&q->name, n);
    if ((n = obt_xml_find_node(node, "role"))) setup_typed_match(&q->role, n);
    if ((n = obt_xml_find_node(node, "type"))) setup_typed_match(&q->type, n);

    if ((n = obt_xml_find_node(node, "monitor"))) {
        q->client_monitor = obt_xml_node_int(n);
    }
}

static gpointer setup_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->queries = g_array_new(FALSE, FALSE, sizeof(Query));

    xmlNodePtr n;
    if ((n = obt_xml_find_node(node, "then"))) {
        for (xmlNodePtr m = obt_xml_find_node(n->children, "action"); m; m = obt_xml_find_node(m->next, "action")) {
            ObActionsAct *action = actions_parse(m);
            if (action) o->thenacts = g_slist_append(o->thenacts, action);
        }
    }

    if ((n = obt_xml_find_node(node, "else"))) {
        for (xmlNodePtr m = obt_xml_find_node(n->children, "action"); m; m = obt_xml_find_node(m->next, "action")) {
            ObActionsAct *action = actions_parse(m);
            if (action) o->elseacts = g_slist_append(o->elseacts, action);
        }
    }

    xmlNodePtr query_node = obt_xml_find_node(node, "query");
    if (!query_node) {
        // Default query
        setup_query(o, node, QUERY_TARGET_IS_ACTION_TARGET);
    } else {
        while (query_node) {
            QueryTarget query_target = QUERY_TARGET_IS_ACTION_TARGET;
            if (obt_xml_attr_contains(query_node, "target", "focus"))
                query_target = QUERY_TARGET_IS_FOCUS_TARGET;

            setup_query(o, query_node->children, query_target);
            query_node = obt_xml_find_node(query_node->next, "query");
        }
    }

    return o;
}

static void free_func(gpointer options)
{
    Options *o = options;

    for (guint i = 0; i < o->queries->len; ++i) {
        Query *q = g_array_index(o->queries, Query*, i);

        free_typed_match(&q->title);
        free_typed_match(&q->class);
        free_typed_match(&q->name);
        free_typed_match(&q->role);
        free_typed_match(&q->type);

        g_slice_free(Query, q);
    }

    while (o->thenacts) {
        actions_act_unref(o->thenacts->data);
        o->thenacts = g_slist_delete_link(o->thenacts, o->thenacts);
    }

    while (o->elseacts) {
        actions_act_unref(o->elseacts->data);
        o->elseacts = g_slist_delete_link(o->elseacts, o->elseacts);
    }

    g_array_unref(o->queries);
    g_slice_free(Options, o);
}

/* Always return FALSE because it's not interactive */
static gboolean run_func_if(ObActionsData *data, gpointer options)
{
    Options *o = options;
    ObClient *action_target = data->client;
    gboolean is_true = TRUE;

    for (guint i = 0; is_true && i < o->queries->len; ++i) {
        Query *q = g_array_index(o->queries, Query*, i);
        ObClient *query_target = (q->target == QUERY_TARGET_IS_ACTION_TARGET) ? data->client : focus_client;

        /* If there's no client to query, set false */
        if (!query_target) {
            is_true = FALSE;
            break;
        }

        // Check shaded state
        is_true &= (q->shaded_on && query_target->shaded) || (q->shaded_off && !query_target->shaded);

        // Check iconic state
        is_true &= (q->iconic_on && query_target->iconic) || (q->iconic_off && !query_target->iconic);

        // Check max horizontal state
        is_true &= (q->maxhorz_on && query_target->max_horz) || (q->maxhorz_off && !query_target->max_horz);

        // Check max vertical state
        is_true &= (q->maxvert_on && query_target->max_vert) || (q->maxvert_off && !query_target->max_vert);

        // Check max full (both max horizontal and vertical)
        gboolean is_max_full = query_target->max_vert && query_target->max_horz;
        is_true &= (q->maxfull_on && is_max_full) || (q->maxfull_off && !is_max_full);

        // Check focus state
        is_true &= (q->focused && query_target == focus_client) || (q->unfocused && query_target != focus_client);

        // Check urgency state
        gboolean is_urgent = query_target->urgent || query_target->demands_attention;
        is_true &= (q->urgent_on && is_urgent) || (q->urgent_off && !is_urgent);

        // Check decoration state
        gboolean has_visible_title_bar = !query_target->undecorated && (query_target->decorations & OB_FRAME_DECOR_TITLEBAR);
        is_true &= (q->decor_on && has_visible_title_bar) || (q->decor_off && !has_visible_title_bar);

        // Check omnipresent state
        is_true &= (q->omnipresent_on && query_target->desktop == DESKTOP_ALL) || (q->omnipresent_off && query_target->desktop != DESKTOP_ALL);

        // Check desktop conditions
        gboolean is_on_current_desktop = query_target->desktop == screen_desktop || query_target->desktop == DESKTOP_ALL;
        is_true &= (q->desktop_current && is_on_current_desktop) || (q->desktop_other && !is_on_current_desktop);

        // Check specific desktop number
        if (q->desktop_number) {
            gboolean is_on_desktop = query_target->desktop == q->desktop_number - 1 || query_target->desktop == DESKTOP_ALL;
            is_true &= is_on_desktop;
        }

        // Check screendesktop number
        if (q->screendesktop_number) {
            is_true &= screen_desktop == q->screendesktop_number - 1;
        }

        // Check title, class, name, role, and type matches
        is_true &= check_typed_match(&q->title, query_target->original_title);
        is_true &= check_typed_match(&q->class, query_target->class);
        is_true &= check_typed_match(&q->name, query_target->name);
        is_true &= check_typed_match(&q->role, query_target->role);
        is_true &= check_typed_match(&q->type, client_type_to_string(query_target));

        // Check client monitor
        if (q->client_monitor) {
            is_true &= client_monitor(query_target) == q->client_monitor - 1;
        }
    }

    // Run actions based on the outcome
    GSList *acts = is_true ? o->thenacts : o->elseacts;
    actions_run_acts(acts, data->uact, data->state, data->x, data->y, data->button, data->context, action_target);

    return FALSE;
}

static gboolean run_func_foreach(ObActionsData *data, gpointer options)
{
    GList *it;
    foreach_stop = FALSE;

    // Iterate over the client list and execute actions for each client
    for (it = client_list; it; it = g_list_next(it)) {
        data->client = it->data;
        run_func_if(data, options);

        // Stop the loop if the foreach_stop flag is set
        if (foreach_stop) {
            foreach_stop = FALSE;
            break;
        }
    }

    return FALSE;
}

static gboolean run_func_stop(ObActionsData *data, gpointer options)
{
	UNUSED(data);
	UNUSED(options);

    /* Stop further actions for the current client */
    foreach_stop = TRUE;
    return TRUE;
}

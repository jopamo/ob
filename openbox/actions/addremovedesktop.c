#include "openbox/actions.h"
#include "openbox/screen.h"
#include <glib.h>

typedef struct {
  gboolean current;
  gboolean add;
} Options;

static gpointer setup_func(GHashTable* options);
static gpointer setup_add_func(GHashTable* options);
static gpointer setup_remove_func(GHashTable* options);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData* data, gpointer options);
/* 3.4-compatibility */
static gpointer setup_addcurrent_func(GHashTable* options);
static gpointer setup_addlast_func(GHashTable* options);
static gpointer setup_removecurrent_func(GHashTable* options);
static gpointer setup_removelast_func(GHashTable* options);

void action_addremovedesktop_startup(void) {
  actions_register_opt("AddDesktop", setup_add_func, free_func, run_func);
  actions_register_opt("RemoveDesktop", setup_remove_func, free_func, run_func);

  /* 3.4-compatibility */
  actions_register_opt("AddDesktopLast", setup_addlast_func, free_func, run_func);
  actions_register_opt("RemoveDesktopLast", setup_removelast_func, free_func, run_func);
  actions_register_opt("AddDesktopCurrent", setup_addcurrent_func, free_func, run_func);
  actions_register_opt("RemoveDesktopCurrent", setup_removecurrent_func, free_func, run_func);
}

static gpointer setup_func(GHashTable* options) {
  Options* o;
  const char* where;

  o = g_slice_new0(Options);

  where = options ? g_hash_table_lookup(options, "where") : NULL;
  if (where) {
    if (!g_ascii_strcasecmp(where, "last"))
      o->current = FALSE;
    else if (!g_ascii_strcasecmp(where, "current"))
      o->current = TRUE;
  }

  return o;
}

static gpointer setup_add_func(GHashTable* options) {
  Options* o = setup_func(options);
  o->add = TRUE;
  return o;
}

static gpointer setup_remove_func(GHashTable* options) {
  Options* o = setup_func(options);
  o->add = FALSE;
  return o;
}

static void free_func(gpointer o) {
  g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData* data, gpointer options) {
  Options* o = options;

  actions_client_move(data, TRUE);

  if (o->add)
    screen_add_desktop(o->current);
  else
    screen_remove_desktop(o->current);

  actions_client_move(data, FALSE);

  return FALSE;
}

/* 3.4-compatibility */
static gpointer setup_addcurrent_func(GHashTable* options) {
  Options* o = setup_add_func(options);
  o->current = TRUE;
  return o;
}

static gpointer setup_addlast_func(GHashTable* options) {
  Options* o = setup_add_func(options);
  o->current = FALSE;
  return o;
}

static gpointer setup_removecurrent_func(GHashTable* options) {
  Options* o = setup_remove_func(options);
  o->current = TRUE;
  return o;
}

static gpointer setup_removelast_func(GHashTable* options) {
  Options* o = setup_remove_func(options);
  o->current = FALSE;
  return o;
}

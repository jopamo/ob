#include "openbox/actions.h"
#include <glib.h>

typedef struct {
  gchar* str;
} Options;

static gpointer setup_func(GHashTable* options);
static void free_func(gpointer options);
static gboolean run_func(ObActionsData* data, gpointer options);

void action_debug_startup(void) {
  actions_register_opt("Debug", setup_func, free_func, run_func);
}

static gpointer setup_func(GHashTable* options) {
  Options* o;
  const char* val;

  o = g_slice_new0(Options);

  val = options ? g_hash_table_lookup(options, "string") : NULL;
  if (val)
    o->str = g_strdup(val);
  return o;
}

static void free_func(gpointer options) {
  Options* o = options;
  g_free(o->str);
  g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData* data, gpointer options) {
  Options* o = options;

  if (o->str)
    g_print("%s\n", o->str);

  return FALSE;
}

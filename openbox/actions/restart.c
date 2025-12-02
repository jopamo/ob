#include "openbox/actions.h"
#include "openbox/openbox.h"
#include "obt/paths.h"

typedef struct {
  gchar* cmd;
} Options;

static gpointer setup_func(GHashTable* options);
static void free_func(gpointer options);
static gboolean run_func(ObActionsData* data, gpointer options);

void action_restart_startup(void) {
  actions_register_opt("Restart", setup_func, free_func, run_func);
}

static gpointer setup_func(GHashTable* options) {
  Options* o;
  const char* cmd;

  o = g_slice_new0(Options);

  cmd = options ? g_hash_table_lookup(options, "command") : NULL;
  if (!cmd && options)
    cmd = g_hash_table_lookup(options, "execute");

  if (cmd)
    o->cmd = obt_paths_expand_tilde(cmd);

  return o;
}

static void free_func(gpointer options) {
  Options* o = options;
  g_free(o->cmd);
  g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData* data, gpointer options) {
  Options* o = options;

  ob_restart_other(o->cmd);

  return FALSE;
}

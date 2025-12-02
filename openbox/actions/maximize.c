#include "openbox/actions.h"
#include "openbox/client.h"

/* These match the values for client_maximize */
typedef enum { BOTH = 0, HORZ = 1, VERT = 2 } MaxDirection;

typedef struct {
  MaxDirection dir;
} Options;

static gpointer setup_func(GHashTable* options);
static void free_func(gpointer o);
static gboolean run_func_on(ObActionsData* data, gpointer options);
static gboolean run_func_off(ObActionsData* data, gpointer options);
static gboolean run_func_toggle(ObActionsData* data, gpointer options);
/* 3.4-compatibility */
static gpointer setup_both_func(GHashTable* options);
static gpointer setup_horz_func(GHashTable* options);
static gpointer setup_vert_func(GHashTable* options);

void action_maximize_startup(void) {
  actions_register_opt("Maximize", setup_func, free_func, run_func_on);
  actions_register_opt("Unmaximize", setup_func, free_func, run_func_off);
  actions_register_opt("ToggleMaximize", setup_func, free_func, run_func_toggle);
  /* 3.4-compatibility */
  actions_register_opt("MaximizeFull", setup_both_func, free_func, run_func_on);
  actions_register_opt("UnmaximizeFull", setup_both_func, free_func, run_func_off);
  actions_register_opt("ToggleMaximizeFull", setup_both_func, free_func, run_func_toggle);
  actions_register_opt("MaximizeHorz", setup_horz_func, free_func, run_func_on);
  actions_register_opt("UnmaximizeHorz", setup_horz_func, free_func, run_func_off);
  actions_register_opt("ToggleMaximizeHorz", setup_horz_func, free_func, run_func_toggle);
  actions_register_opt("MaximizeVert", setup_vert_func, free_func, run_func_on);
  actions_register_opt("UnmaximizeVert", setup_vert_func, free_func, run_func_off);
  actions_register_opt("ToggleMaximizeVert", setup_vert_func, free_func, run_func_toggle);
}

static gpointer setup_func(GHashTable* options) {
  Options* o;
  const char* dir;

  o = g_slice_new0(Options);
  o->dir = BOTH;

  dir = options ? g_hash_table_lookup(options, "direction") : NULL;
  if (dir) {
    if (!g_ascii_strcasecmp(dir, "vertical") || !g_ascii_strcasecmp(dir, "vert"))
      o->dir = VERT;
    else if (!g_ascii_strcasecmp(dir, "horizontal") || !g_ascii_strcasecmp(dir, "horz"))
      o->dir = HORZ;
  }

  return o;
}

static void free_func(gpointer o) {
  g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func_on(ObActionsData* data, gpointer options) {
  Options* o = options;
  if (data->client) {
    actions_client_move(data, TRUE);
    client_maximize(data->client, TRUE, o->dir);
    actions_client_move(data, FALSE);
  }
  return FALSE;
}

/* Always return FALSE because its not interactive */
static gboolean run_func_off(ObActionsData* data, gpointer options) {
  Options* o = options;
  if (data->client) {
    actions_client_move(data, TRUE);
    client_maximize(data->client, FALSE, o->dir);
    actions_client_move(data, FALSE);
  }
  return FALSE;
}

/* Always return FALSE because its not interactive */
static gboolean run_func_toggle(ObActionsData* data, gpointer options) {
  Options* o = options;
  if (data->client) {
    gboolean toggle;
    actions_client_move(data, TRUE);
    toggle = ((o->dir == HORZ && !data->client->max_horz) || (o->dir == VERT && !data->client->max_vert) ||
              (o->dir == BOTH && !(data->client->max_horz && data->client->max_vert)));
    client_maximize(data->client, toggle, o->dir);
    actions_client_move(data, FALSE);
  }
  return FALSE;
}

/* 3.4-compatibility */
static gpointer setup_both_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->dir = BOTH;
  return o;
}

static gpointer setup_horz_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->dir = HORZ;
  return o;
}

static gpointer setup_vert_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->dir = VERT;
  return o;
}

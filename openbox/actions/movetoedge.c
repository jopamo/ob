#include "openbox/actions.h"
#include "openbox/misc.h"
#include "openbox/client.h"
#include "openbox/frame.h"
#include "openbox/geom.h"
#include "openbox/actions/helpers.h"
#include <glib.h>

typedef struct {
  ObDirection dir;
} Options;

static gpointer setup_func(GHashTable* options);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData* data, gpointer options);
/* 3.4-compatibility */
static gpointer setup_north_func(GHashTable* options);
static gpointer setup_south_func(GHashTable* options);
static gpointer setup_east_func(GHashTable* options);
static gpointer setup_west_func(GHashTable* options);

void action_movetoedge_startup(void) {
  actions_register_opt("MoveToEdge", setup_func, free_func, run_func);
  /* 3.4-compatibility */
  actions_register_opt("MoveToEdgeNorth", setup_north_func, free_func, run_func);
  actions_register_opt("MoveToEdgeSouth", setup_south_func, free_func, run_func);
  actions_register_opt("MoveToEdgeEast", setup_east_func, free_func, run_func);
  actions_register_opt("MoveToEdgeWest", setup_west_func, free_func, run_func);
}

static gpointer setup_func(GHashTable* options) {
  Options* o;
  const char* dir;

  o = g_slice_new0(Options);
  o->dir = OB_DIRECTION_NORTH;

  dir = options ? g_hash_table_lookup(options, "direction") : NULL;
  if (dir)
    o->dir = actions_parse_direction(dir);

  return o;
}

static void free_func(gpointer o) {
  g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData* data, gpointer options) {
  Options* o = options;

  if (data->client) {
    gint x, y;

    client_find_move_directional(data->client, o->dir, &x, &y);
    if (x != data->client->area.x || y != data->client->area.y) {
      actions_client_move(data, TRUE);
      client_move(data->client, x, y);
      actions_client_move(data, FALSE);
    }
  }

  return FALSE;
}

/* 3.4-compatibility */
static gpointer setup_north_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->dir = OB_DIRECTION_NORTH;
  return o;
}

static gpointer setup_south_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->dir = OB_DIRECTION_SOUTH;
  return o;
}

static gpointer setup_east_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->dir = OB_DIRECTION_EAST;
  return o;
}

static gpointer setup_west_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->dir = OB_DIRECTION_WEST;
  return o;
}

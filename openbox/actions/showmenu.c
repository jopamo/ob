#include "openbox/actions.h"
#include "openbox/menu.h"
#include "openbox/place.h"
#include "openbox/geom.h"
#include "openbox/screen.h"
#include "openbox/config.h"
#include <glib.h>
#include <stdlib.h>

typedef struct {
  gchar* name;
  GravityPoint position;
  ObPlaceMonitor monitor_type;
  gint monitor;
  gboolean use_position;
} Options;

static gpointer setup_func(GHashTable* options);
static void free_func(gpointer options);
static gboolean run_func(ObActionsData* data, gpointer options);

void action_showmenu_startup(void) {
  actions_register_opt("ShowMenu", setup_func, free_func, run_func);
}

static void parse_gravity_string(const char* val, GravityCoord* coord) {
  if (!val)
    return;

  if (!g_ascii_strcasecmp(val, "center")) {
    coord->center = TRUE;
    return;
  }

  const char* pos = val;
  if (val[0] == '-')
    coord->opposite = TRUE;
  if (val[0] == '-' || val[0] == '+')
    pos++;

  gchar* dup = g_strdup(pos);
  config_parse_relative_number(dup, &coord->pos, &coord->denom);
  g_free(dup);
}

static gpointer setup_func(GHashTable* options) {
  Options* o;
  gboolean have_x = FALSE;
  const char* val;

  o = g_slice_new0(Options);
  o->monitor = -1;

  val = options ? g_hash_table_lookup(options, "menu") : NULL;
  if (val)
    o->name = g_strdup(val);

  val = options ? g_hash_table_lookup(options, "x") : NULL;
  if (val) {
    parse_gravity_string(val, &o->position.x);
    have_x = TRUE;
  }

  val = options ? g_hash_table_lookup(options, "y") : NULL;
  if (val && have_x) {
    parse_gravity_string(val, &o->position.y);
    o->use_position = TRUE;
  }

  /* unlike client placement, x/y is needed to specify a monitor,
   * either it's under the mouse or it's in an exact actual position */
  if (o->use_position) {
    val = options ? g_hash_table_lookup(options, "monitor") : NULL;
    if (val) {
      if (!g_ascii_strcasecmp(val, "mouse"))
        o->monitor_type = OB_PLACE_MONITOR_MOUSE;
      else if (!g_ascii_strcasecmp(val, "active"))
        o->monitor_type = OB_PLACE_MONITOR_ACTIVE;
      else if (!g_ascii_strcasecmp(val, "primary"))
        o->monitor_type = OB_PLACE_MONITOR_PRIMARY;
      else if (!g_ascii_strcasecmp(val, "all"))
        o->monitor_type = OB_PLACE_MONITOR_ALL;
      else
        o->monitor = atoi(val) - 1;
    }
  }
  return o;
}

static void free_func(gpointer options) {
  Options* o = options;
  g_free(o->name);
  g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData* data, gpointer options) {
  Options* o = options;
  GravityPoint position = {
      {
          0,
      },
      {
          0,
      },
  };
  gint monitor = -1;

  if (o->use_position) {
    if (o->monitor >= 0)
      monitor = o->monitor;
    else
      switch (o->monitor_type) {
        case OB_PLACE_MONITOR_ANY:
        case OB_PLACE_MONITOR_PRIMARY:
          monitor = screen_monitor_primary(FALSE);
          break;
        case OB_PLACE_MONITOR_MOUSE:
          monitor = screen_monitor_pointer();
          break;
        case OB_PLACE_MONITOR_ACTIVE:
          monitor = screen_monitor_active();
          break;
        case OB_PLACE_MONITOR_ALL:
          monitor = screen_num_monitors;
          break;
        default:
          g_assert_not_reached();
      }

    position = o->position;
  }
  else {
    const Rect* allmon;
    monitor = screen_num_monitors;
    allmon = screen_physical_area_monitor(monitor);
    position.x.pos = data->x - allmon->x;
    position.y.pos = data->y - allmon->y;
  }

  /* you cannot call ShowMenu from inside a menu */
  if (data->uact != OB_USER_ACTION_MENU_SELECTION && o->name)
    menu_show(o->name, &position, monitor, data->button != 0, o->use_position, data->client);

  return FALSE;
}

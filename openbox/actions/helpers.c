#include "openbox/actions/helpers.h"
#include <stdlib.h>
#include <string.h>

gboolean actions_parse_bool(const gchar* val) {
  return (val && (g_ascii_strcasecmp(val, "yes") == 0 || g_ascii_strcasecmp(val, "true") == 0 ||
                  g_ascii_strcasecmp(val, "on") == 0));
}

gint actions_parse_int(const gchar* val) {
  if (!val)
    return 0;
  return atoi(val);
}

ObDirection actions_parse_direction(const gchar* val) {
  if (!val)
    return OB_DIRECTION_NORTH;
  if (!g_ascii_strcasecmp(val, "north") || !g_ascii_strcasecmp(val, "up"))
    return OB_DIRECTION_NORTH;
  if (!g_ascii_strcasecmp(val, "south") || !g_ascii_strcasecmp(val, "down"))
    return OB_DIRECTION_SOUTH;
  if (!g_ascii_strcasecmp(val, "west") || !g_ascii_strcasecmp(val, "left"))
    return OB_DIRECTION_WEST;
  if (!g_ascii_strcasecmp(val, "east") || !g_ascii_strcasecmp(val, "right"))
    return OB_DIRECTION_EAST;
  if (!g_ascii_strcasecmp(val, "northeast"))
    return OB_DIRECTION_NORTHEAST;
  if (!g_ascii_strcasecmp(val, "southeast"))
    return OB_DIRECTION_SOUTHEAST;
  if (!g_ascii_strcasecmp(val, "southwest"))
    return OB_DIRECTION_SOUTHWEST;
  if (!g_ascii_strcasecmp(val, "northwest"))
    return OB_DIRECTION_NORTHWEST;
  return OB_DIRECTION_NORTH;
}

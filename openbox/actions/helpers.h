#ifndef __actions_helpers_h
#define __actions_helpers_h

#include <glib.h>
#include "openbox/openbox.h"

gboolean actions_parse_bool(const gchar* val);
gint actions_parse_int(const gchar* val);
ObDirection actions_parse_direction(const gchar* val);

#endif

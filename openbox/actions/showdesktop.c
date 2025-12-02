#include "openbox/actions.h"
#include "openbox/actions/helpers.h"
#include "openbox/screen.h"

typedef struct {
  /* If true, windows are unable to be shown while in the showing-desktop
     state. */
  gboolean strict;
} Options;

static gpointer setup_func(GHashTable* options);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData* data, gpointer options);

void action_showdesktop_startup(void) {
  actions_register_opt("ToggleShowDesktop", setup_func, free_func, run_func);
}

static gpointer setup_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  const char* val;

  o->strict = FALSE;

  val = options ? g_hash_table_lookup(options, "strict") : NULL;
  if (val)
    o->strict = actions_parse_bool(val);

  return o;
}

static void free_func(gpointer o) {
  g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData* data, gpointer options) {
  Options* o = options;

  ObScreenShowDestopMode show_mode;
  if (screen_showing_desktop())
    show_mode = SCREEN_SHOW_DESKTOP_NO;
  else if (!o->strict)
    show_mode = SCREEN_SHOW_DESKTOP_UNTIL_WINDOW;
  else
    show_mode = SCREEN_SHOW_DESKTOP_UNTIL_TOGGLE;

  screen_show_desktop(show_mode, NULL);

  return FALSE;
}

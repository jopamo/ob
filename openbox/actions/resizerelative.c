#include "openbox/actions.h"
#include "openbox/client.h"
#include "openbox/screen.h"
#include "openbox/frame.h"
#include "openbox/config.h"

typedef struct {
  gint left;
  gint left_denom;
  gint right;
  gint right_denom;
  gint top;
  gint top_denom;
  gint bottom;
  gint bottom_denom;
} Options;

static gpointer setup_func(GHashTable* options);
static void free_func(gpointer options);
static gboolean run_func(ObActionsData* data, gpointer options);

void action_resizerelative_startup(void) {
  actions_register_opt("ResizeRelative", setup_func, free_func, run_func);
}

static void parse_relative_option(GHashTable* options, const char* key, gint* num, gint* denom) {
  const char* val;
  if (!options)
    return;

  val = g_hash_table_lookup(options, key);
  if (val) {
    gchar* dup = g_strdup(val);
    config_parse_relative_number(dup, num, denom);
    g_free(dup);
  }
}

static gpointer setup_func(GHashTable* options) {
  Options* o;

  o = g_slice_new0(Options);

  parse_relative_option(options, "left", &o->left, &o->left_denom);
  parse_relative_option(options, "right", &o->right, &o->right_denom);
  parse_relative_option(options, "top", &o->top, &o->top_denom);
  parse_relative_option(options, "up", &o->top, &o->top_denom);
  parse_relative_option(options, "bottom", &o->bottom, &o->bottom_denom);
  parse_relative_option(options, "down", &o->bottom, &o->bottom_denom);

  return o;
}

static void free_func(gpointer o) {
  g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData* data, gpointer options) {
  Options* o = options;

  if (data->client) {
    ObClient* c = data->client;
    gint x, y, ow, xoff, nw, oh, yoff, nh, lw, lh;
    gint left = o->left, right = o->right, top = o->top, bottom = o->bottom;

    if (o->left_denom)
      left = left * c->area.width / o->left_denom;
    if (o->right_denom)
      right = right * c->area.width / o->right_denom;
    if (o->top_denom)
      top = top * c->area.height / o->top_denom;
    if (o->bottom_denom)
      bottom = bottom * c->area.height / o->bottom_denom;

    if (left && ABS(left) < c->size_inc.width)
      left = left < 0 ? -c->size_inc.width : c->size_inc.width;
    if (right && ABS(right) < c->size_inc.width)
      right = right < 0 ? -c->size_inc.width : c->size_inc.width;
    if (top && ABS(top) < c->size_inc.height)
      top = top < 0 ? -c->size_inc.height : c->size_inc.height;
    if (bottom && ABS(bottom) < c->size_inc.height)
      bottom = bottom < 0 ? -c->size_inc.height : c->size_inc.height;

    /* When resizing, if the resize has a non-zero value then make sure it
       is at least as big as the size increment so the window does actually
       resize. */
    x = c->area.x;
    y = c->area.y;
    ow = c->area.width;
    xoff = -left;
    nw = ow + right + left;
    oh = c->area.height;
    yoff = -top;
    nh = oh + bottom + top;

    client_try_configure(c, &x, &y, &nw, &nh, &lw, &lh, TRUE);
    xoff = xoff == 0 ? 0 : (xoff < 0 ? MAX(xoff, ow - nw) : MIN(xoff, ow - nw));
    yoff = yoff == 0 ? 0 : (yoff < 0 ? MAX(yoff, oh - nh) : MIN(yoff, oh - nh));

    actions_client_move(data, TRUE);
    client_move_resize(c, x + xoff, y + yoff, nw, nh);
    actions_client_move(data, FALSE);
  }

  return FALSE;
}

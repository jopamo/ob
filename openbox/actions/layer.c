#include "openbox/actions.h"
#include "openbox/actions/helpers.h"
#include "openbox/client.h"

typedef struct {
  gint layer; /*!< -1 for below, 0 for normal, and 1 for above */
  gboolean toggle;
} Options;

static gpointer setup_func_top(GHashTable* options);
static gpointer setup_func_bottom(GHashTable* options);
static gpointer setup_func_send(GHashTable* options);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData* data, gpointer options);
/* 3.4-compatibility */
static gpointer setup_sendtop_func(GHashTable* options);
static gpointer setup_sendbottom_func(GHashTable* options);
static gpointer setup_sendnormal_func(GHashTable* options);

void action_layer_startup(void) {
  actions_register_opt("ToggleAlwaysOnTop", setup_func_top, free_func, run_func);
  actions_register_opt("ToggleAlwaysOnBottom", setup_func_bottom, free_func, run_func);
  actions_register_opt("SendToLayer", setup_func_send, free_func, run_func);
  /* 3.4-compatibility */
  actions_register_opt("SendToTopLayer", setup_sendtop_func, free_func, run_func);
  actions_register_opt("SendToBottomLayer", setup_sendbottom_func, free_func, run_func);
  actions_register_opt("SendToNormalLayer", setup_sendnormal_func, free_func, run_func);
}

static gpointer setup_func_top(GHashTable* options) {
  (void)options;
  Options* o = g_slice_new0(Options);
  o->layer = 1;
  o->toggle = TRUE;
  return o;
}

static gpointer setup_func_bottom(GHashTable* options) {
  (void)options;
  Options* o = g_slice_new0(Options);
  o->layer = -1;
  o->toggle = TRUE;
  return o;
}

static gpointer setup_func_send(GHashTable* options) {
  Options* o;
  const char* layer;

  o = g_slice_new0(Options);

  layer = options ? g_hash_table_lookup(options, "layer") : NULL;
  if (layer) {
    if (!g_ascii_strcasecmp(layer, "above") || !g_ascii_strcasecmp(layer, "top"))
      o->layer = 1;
    else if (!g_ascii_strcasecmp(layer, "below") || !g_ascii_strcasecmp(layer, "bottom"))
      o->layer = -1;
    else if (!g_ascii_strcasecmp(layer, "normal") || !g_ascii_strcasecmp(layer, "middle"))
      o->layer = 0;
  }

  if (options) {
    const char* toggle = g_hash_table_lookup(options, "toggle");
    if (toggle)
      o->toggle = actions_parse_bool(toggle);
  }

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

    actions_client_move(data, TRUE);

    if (o->layer < 0) {
      if (o->toggle || !c->below)
        client_set_layer(c, c->below ? 0 : -1);
    }
    else if (o->layer > 0) {
      if (o->toggle || !c->above)
        client_set_layer(c, c->above ? 0 : 1);
    }
    else if (c->above || c->below)
      client_set_layer(c, 0);

    actions_client_move(data, FALSE);
  }

  return FALSE;
}

/* 3.4-compatibility */
static gpointer setup_sendtop_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->layer = 1;
  o->toggle = FALSE;
  return o;
}

static gpointer setup_sendbottom_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->layer = -1;
  o->toggle = FALSE;
  return o;
}

static gpointer setup_sendnormal_func(GHashTable* options) {
  Options* o = g_slice_new0(Options);
  (void)options;
  o->layer = 0;
  o->toggle = FALSE;
  return o;
}

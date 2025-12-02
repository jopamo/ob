#define _POSIX_C_SOURCE 200809L

#include "obt/prop.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <glib-unix.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

/* obt/display.h drags in a pile of extension headers that are irrelevant for
 * the panel. We only need the obt_display symbol, so forward-declare it. */
extern Display* obt_display;
void obt_prop_startup(void);

typedef enum {
  PANEL_EDGE_TOP,
  PANEL_EDGE_BOTTOM,
} PanelEdge;

typedef struct {
  gchar* display_name;
  PanelEdge edge;
  gint height;
  gchar* font_spec;
  gchar* clock_format;
  gchar* status_command;
  guint status_interval;
  gchar* mixer_device;
  gchar* mixer_element;
  gboolean show_all_tasks;
} PanelOptions;

typedef struct {
  Atom net_client_list;
  Atom net_active_window;
  Atom net_current_desktop;
  Atom net_number_of_desktops;
  Atom net_wm_desktop;
  Atom net_wm_state;
  Atom net_wm_state_skip_taskbar;
  Atom net_wm_state_skip_pager;
  Atom net_wm_state_hidden;
  Atom net_wm_state_demands_attention;
  Atom net_wm_state_sticky;
  Atom net_wm_state_above;
  Atom net_wm_window_type;
  Atom net_wm_window_type_dock;
  Atom net_wm_window_type_desktop;
  Atom net_wm_window_type_splash;
  Atom net_wm_window_type_dropdown;
  Atom net_wm_window_type_popup;
  Atom net_wm_window_type_tooltip;
  Atom net_wm_window_type_notification;
  Atom net_wm_visible_name;
  Atom net_wm_name;
  Atom wm_name;
  Atom wm_hints;
} PanelAtoms;

typedef struct {
  Window window;
  gchar* title;
  guint desktop;
  gboolean has_desktop;
  gboolean sticky_desktop;
  gboolean sticky_state;
  gboolean skip_taskbar;
  gboolean urgent;
  gboolean hidden;
  guint64 serial;
} PanelTask;

typedef struct {
  Window win;
  gdouble x;
  gdouble width;
} PanelTaskRegion;

#ifdef HAVE_ALSA
typedef struct {
  snd_mixer_t* handle;
  snd_mixer_elem_t* elem;
  long min;
  long max;
} PanelMixer;
#endif

typedef struct {
  PanelOptions opts;

  Display* dpy;
  gint screen;
  Window root;
  Window win;

  gint width;
  gint height;
  gint y;

  cairo_surface_t* surface;
  cairo_t* cr;

  gchar* font_family;
  gdouble font_size;

  PanelAtoms atoms;

  guint current_desktop;
  guint num_desktops;
  Window active;

  GHashTable* task_lookup;
  GPtrArray* visible_tasks;
  GArray* task_regions;
  guint64 task_serial;

  gchar clock_text[64];
  guint clock_source;
  gchar* clock_format;

  gchar* status_text;
  guint status_source;
  gchar* status_command;

  gchar* volume_text;
  guint volume_source;
  gboolean show_volume;
  gboolean show_all_tasks;

#ifdef HAVE_ALSA
  PanelMixer mixer;
  gboolean mixer_ready;
#endif

  PanelTaskRegion clock_region;
  PanelTaskRegion volume_region;
  PanelTaskRegion status_region;

  GMainLoop* loop;
} Panel;

static const gdouble PANEL_BG[3] = {0.07, 0.07, 0.07};
static const gdouble PANEL_FG[3] = {0.85, 0.85, 0.85};
static const gdouble PANEL_ACTIVE[3] = {0.25, 0.46, 0.83};
static const gdouble PANEL_URGENT[3] = {0.84, 0.33, 0.33};
static const gdouble PANEL_DIM[3] = {0.55, 0.55, 0.55};

static void panel_destroy(Panel* panel);
static gboolean panel_queue_redraw(Panel* panel);
static void panel_refresh_tasks(Panel* panel);
static void panel_update_root_state(Panel* panel);
static void panel_apply_struts(Panel* panel);
static void panel_update_geometry(Panel* panel, gboolean force);

static gchar* dup_string(const gchar* s) {
  return s ? g_strdup(s) : NULL;
}

static gboolean parse_font_spec(const gchar* spec, gchar** family_out, gdouble* size_out) {
  if (!spec || !*spec)
    return FALSE;
  gchar* trimmed = g_strstrip(g_strdup(spec));
  gchar* last = strrchr(trimmed, ' ');
  gdouble size = 11.0;
  if (last) {
    gchar* endptr = NULL;
    gdouble parsed = g_ascii_strtod(last + 1, &endptr);
    if (endptr && *endptr == '\0')
      size = parsed;
    else
      last = NULL;
  }
  gchar* family = NULL;
  if (last) {
    *last = '\0';
    family = g_strdup(trimmed);
  }
  else
    family = g_strdup(trimmed);
  g_free(trimmed);
  if (!family || !*family) {
    g_free(family);
    return FALSE;
  }
  *family_out = family;
  *size_out = size > 0 ? size : 11.0;
  return TRUE;
}

static gboolean panel_open_display(Panel* panel, const PanelOptions* opts) {
  obt_display = XOpenDisplay(opts->display_name);
  if (!obt_display)
    return FALSE;
  panel->dpy = obt_display;
  obt_prop_startup();
  panel->screen = DefaultScreen(panel->dpy);
  panel->root = RootWindow(panel->dpy, panel->screen);
  return TRUE;
}

static void panel_init_atoms(Panel* panel) {
  PanelAtoms* a = &panel->atoms;
  a->net_client_list = OBT_PROP_ATOM(NET_CLIENT_LIST);
  a->net_active_window = OBT_PROP_ATOM(NET_ACTIVE_WINDOW);
  a->net_current_desktop = OBT_PROP_ATOM(NET_CURRENT_DESKTOP);
  a->net_number_of_desktops = OBT_PROP_ATOM(NET_NUMBER_OF_DESKTOPS);
  a->net_wm_desktop = OBT_PROP_ATOM(NET_WM_DESKTOP);
  a->net_wm_state = OBT_PROP_ATOM(NET_WM_STATE);
  a->net_wm_state_skip_taskbar = OBT_PROP_ATOM(NET_WM_STATE_SKIP_TASKBAR);
  a->net_wm_state_skip_pager = OBT_PROP_ATOM(NET_WM_STATE_SKIP_PAGER);
  a->net_wm_state_hidden = OBT_PROP_ATOM(NET_WM_STATE_HIDDEN);
  a->net_wm_state_demands_attention = OBT_PROP_ATOM(NET_WM_STATE_DEMANDS_ATTENTION);
  a->net_wm_state_above = OBT_PROP_ATOM(NET_WM_STATE_ABOVE);
  a->net_wm_window_type = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE);
  a->net_wm_window_type_dock = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_DOCK);
  a->net_wm_window_type_desktop = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_DESKTOP);
  a->net_wm_window_type_splash = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_SPLASH);
  a->net_wm_window_type_dropdown = XInternAtom(panel->dpy, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
  a->net_wm_window_type_popup = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_POPUP_MENU);
  a->net_wm_window_type_tooltip = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_TOOLTIP);
  a->net_wm_window_type_notification = OBT_PROP_ATOM(NET_WM_WINDOW_TYPE_NOTIFICATION);
  a->net_wm_state_sticky = XInternAtom(panel->dpy, "_NET_WM_STATE_STICKY", False);
  a->net_wm_visible_name = OBT_PROP_ATOM(NET_WM_VISIBLE_NAME);
  a->net_wm_name = OBT_PROP_ATOM(NET_WM_NAME);
  a->wm_name = XA_WM_NAME;
  a->wm_hints = XA_WM_HINTS;
}

static void panel_create_surface(Panel* panel) {
  if (panel->surface)
    cairo_surface_destroy(panel->surface);
  if (panel->cr)
    cairo_destroy(panel->cr);
  panel->surface = cairo_xlib_surface_create(panel->dpy, panel->win, DefaultVisual(panel->dpy, panel->screen),
                                             panel->width, panel->height);
  panel->cr = cairo_create(panel->surface);
  cairo_set_antialias(panel->cr, CAIRO_ANTIALIAS_BEST);
}

static gint panel_target_y(const Panel* panel) {
  const gint screen_height = DisplayHeight(panel->dpy, panel->screen);
  return (panel->opts.edge == PANEL_EDGE_BOTTOM) ? (screen_height - panel->height) : 0;
}

static void panel_update_geometry(Panel* panel, gboolean force) {
  const gint new_width = DisplayWidth(panel->dpy, panel->screen);
  const gint new_y = panel_target_y(panel);
  const gboolean size_changed = (panel->width != new_width);
  const gboolean pos_changed = (panel->y != new_y);

  if (size_changed || pos_changed || force) {
    panel->width = new_width;
    panel->y = new_y;
    XMoveResizeWindow(panel->dpy, panel->win, 0, panel->y, panel->width, panel->height);
    panel_apply_struts(panel);
    panel_create_surface(panel);
  }
}

static void panel_watch_root(Panel* panel) {
  XSelectInput(panel->dpy, panel->root, StructureNotifyMask | PropertyChangeMask);
}

static void panel_watch_window(Panel* panel, Window win) {
  XSelectInput(panel->dpy, win, PropertyChangeMask | StructureNotifyMask);
}

static gboolean panel_init_window(Panel* panel, const PanelOptions* opts) {
  panel->height = MAX(20, opts->height);
  const gint screen_height = DisplayHeight(panel->dpy, panel->screen);
  panel->width = DisplayWidth(panel->dpy, panel->screen);
  panel->y = (opts->edge == PANEL_EDGE_BOTTOM) ? (screen_height - panel->height) : 0;

  XSetWindowAttributes attrs = {0};
  attrs.background_pixel = BlackPixel(panel->dpy, panel->screen);
  attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask | PropertyChangeMask;

  panel->win = XCreateWindow(panel->dpy, panel->root, 0, panel->y, panel->width, panel->height, 0, CopyFromParent,
                             InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &attrs);
  if (!panel->win)
    return FALSE;

  XStoreName(panel->dpy, panel->win, "obpanel");

  XClassHint class_hint = {.res_name = "obpanel", .res_class = "Obpanel"};
  XSetClassHint(panel->dpy, panel->win, &class_hint);

  Atom window_type = panel->atoms.net_wm_window_type_dock;
  XChangeProperty(panel->dpy, panel->win, panel->atoms.net_wm_window_type, XA_ATOM, 32, PropModeReplace,
                  (unsigned char*)&window_type, 1);

  const gulong desktop_all = 0xFFFFFFFF;
  OBT_PROP_SET32(panel->win, NET_WM_DESKTOP, CARDINAL, desktop_all);

  Atom states[2] = {panel->atoms.net_wm_state_sticky, panel->atoms.net_wm_state_above};
  XChangeProperty(panel->dpy, panel->win, panel->atoms.net_wm_state, XA_ATOM, 32, PropModeReplace,
                  (unsigned char*)states, 2);

  panel_update_geometry(panel, TRUE);
  panel_watch_root(panel);
  XMapRaised(panel->dpy, panel->win);
  XFlush(panel->dpy);
  return TRUE;
}

static void panel_apply_struts(Panel* panel) {
  gulong partial[12] = {0};
  if (panel->opts.edge == PANEL_EDGE_BOTTOM) {
    partial[3] = panel->height;
    partial[10] = 0;
    partial[11] = panel->width;
  }
  else {
    partial[2] = panel->height;
    partial[8] = 0;
    partial[9] = panel->width;
  }
  OBT_PROP_SETA32(panel->win, NET_WM_STRUT_PARTIAL, CARDINAL, partial, 12);
  gulong simple[4] = {partial[0], partial[1], partial[2], partial[3]};
  OBT_PROP_SETA32(panel->win, NET_WM_STRUT, CARDINAL, simple, 4);
  XFlush(panel->dpy);
}

static PanelTask* panel_task_new(Panel* panel, Window win);
static void panel_task_free(gpointer data) {
  PanelTask* task = data;
  g_free(task->title);
  g_free(task);
}

static Panel* panel_create(const PanelOptions* opts) {
  Panel* panel = g_new0(Panel, 1);
  panel->opts = *opts;
  panel->opts.display_name = dup_string(opts->display_name);
  panel->opts.font_spec = dup_string(opts->font_spec);
  panel->opts.clock_format = dup_string(opts->clock_format);
  panel->opts.status_command = dup_string(opts->status_command);
  panel->opts.mixer_device = dup_string(opts->mixer_device);
  panel->opts.mixer_element = dup_string(opts->mixer_element);
  panel->clock_format = dup_string(opts->clock_format);
  panel->status_command = dup_string(opts->status_command);
  panel->show_volume = TRUE;
  panel->show_all_tasks = opts->show_all_tasks;

  if (!panel_open_display(panel, opts)) {
    g_free(panel);
    return NULL;
  }
  panel_init_atoms(panel);
  if (!parse_font_spec(opts->font_spec, &panel->font_family, &panel->font_size))
    panel->font_family = g_strdup("Sans");
  if (!panel_init_window(panel, opts)) {
    panel_destroy(panel);
    return NULL;
  }
  panel->task_lookup = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, panel_task_free);
  panel->visible_tasks = g_ptr_array_new();
  panel->task_regions = g_array_new(FALSE, FALSE, sizeof(PanelTaskRegion));
  panel->status_text = g_strdup("");
  panel->volume_text = g_strdup("vol --");
  return panel;
}

static void panel_options_clear(PanelOptions* opts) {
  if (!opts)
    return;
  g_free(opts->display_name);
  g_free(opts->font_spec);
  g_free(opts->clock_format);
  g_free(opts->status_command);
  g_free(opts->mixer_device);
  g_free(opts->mixer_element);
  opts->display_name = NULL;
  opts->font_spec = NULL;
  opts->clock_format = NULL;
  opts->status_command = NULL;
  opts->mixer_device = NULL;
  opts->mixer_element = NULL;
}

static gboolean panel_task_visible(const Panel* panel, const PanelTask* task) {
  if (task->skip_taskbar)
    return FALSE;
  if (panel->show_all_tasks)
    return TRUE;
  if (task->sticky_desktop || task->sticky_state)
    return TRUE;
  if (!task->has_desktop)
    return TRUE;
  if (panel->num_desktops == 0)
    return TRUE;
  return task->desktop == panel->current_desktop;
}

static gboolean panel_window_is_skip_type(const Panel* panel, Window win) {
  guint32* types = NULL;
  guint num = 0;
  gboolean skip = FALSE;
  if (OBT_PROP_GETA32(win, NET_WM_WINDOW_TYPE, ATOM, &types, &num)) {
    for (guint i = 0; i < num; ++i) {
      Atom type = (Atom)types[i];
      if (type == panel->atoms.net_wm_window_type_dock || type == panel->atoms.net_wm_window_type_desktop ||
          type == panel->atoms.net_wm_window_type_splash || type == panel->atoms.net_wm_window_type_dropdown ||
          type == panel->atoms.net_wm_window_type_popup || type == panel->atoms.net_wm_window_type_tooltip ||
          type == panel->atoms.net_wm_window_type_notification) {
        skip = TRUE;
        break;
      }
    }
  }
  g_free(types);
  return skip;
}

static void panel_task_update_title(Panel* panel, PanelTask* task) {
  gchar* title = NULL;
  if (!OBT_PROP_GETS_UTF8(task->window, NET_WM_VISIBLE_NAME, &title) &&
      !OBT_PROP_GETS_UTF8(task->window, NET_WM_NAME, &title) && !OBT_PROP_GETS(task->window, WM_NAME, &title)) {
    g_free(title);
    title = g_strdup_printf("0x%lx", (unsigned long)task->window);
  }
  if (!title)
    return;
  if (!task->title || g_strcmp0(task->title, title) != 0) {
    g_free(task->title);
    task->title = title;
  }
  else
    g_free(title);
}

static void panel_task_update_desktop(PanelTask* task) {
  task->has_desktop = FALSE;
  task->sticky_desktop = FALSE;
  guint32 desktop = 0;
  if (OBT_PROP_GET32(task->window, NET_WM_DESKTOP, CARDINAL, &desktop)) {
    task->has_desktop = TRUE;
    if (desktop == G_MAXUINT32)
      task->sticky_desktop = TRUE;
    else
      task->desktop = desktop;
  }
}

static void panel_task_update_state(Panel* panel, PanelTask* task) {
  guint32* states = NULL;
  guint num = 0;
  task->skip_taskbar = FALSE;
  task->urgent = FALSE;
  task->hidden = FALSE;
  task->sticky_state = FALSE;
  if (OBT_PROP_GETA32(task->window, NET_WM_STATE, ATOM, &states, &num)) {
    for (guint i = 0; i < num; ++i) {
      Atom state = (Atom)states[i];
      if (state == panel->atoms.net_wm_state_skip_taskbar || state == panel->atoms.net_wm_state_skip_pager)
        task->skip_taskbar = TRUE;
      else if (state == panel->atoms.net_wm_state_hidden)
        task->hidden = TRUE;
      else if (state == panel->atoms.net_wm_state_demands_attention)
        task->urgent = TRUE;
      else if (state == panel->atoms.net_wm_state_sticky)
        task->sticky_state = TRUE;
    }
  }
  g_free(states);

  XWMHints* hints = XGetWMHints(panel->dpy, task->window);
  if (hints) {
    if (hints->flags & XUrgencyHint)
      task->urgent = TRUE;
    XFree(hints);
  }
}

static gboolean panel_task_sync(Panel* panel, PanelTask* task) {
  if (panel_window_is_skip_type(panel, task->window))
    return FALSE;
  panel_task_update_title(panel, task);
  panel_task_update_desktop(task);
  panel_task_update_state(panel, task);
  return !task->skip_taskbar;
}

static PanelTask* panel_task_new(Panel* panel, Window win) {
  if (panel_window_is_skip_type(panel, win))
    return NULL;
  PanelTask* task = g_new0(PanelTask, 1);
  task->window = win;
  panel_watch_window(panel, win);
  if (!panel_task_sync(panel, task)) {
    panel_task_free(task);
    return NULL;
  }
  return task;
}

static void panel_remove_task(Panel* panel, Window win) {
  g_hash_table_remove(panel->task_lookup, GUINT_TO_POINTER(win));
}

static PanelTask* panel_find_task(Panel* panel, Window win) {
  return g_hash_table_lookup(panel->task_lookup, GUINT_TO_POINTER(win));
}

static void panel_refresh_tasks(Panel* panel) {
  guint32* wins = NULL;
  guint n = 0;
  if (!OBT_PROP_GETA32(panel->root, NET_CLIENT_LIST, WINDOW, &wins, &n)) {
    if (panel->visible_tasks)
      g_ptr_array_set_size(panel->visible_tasks, 0);
    return;
  }

  panel->task_serial++;
  g_ptr_array_set_size(panel->visible_tasks, 0);

  for (guint i = 0; i < n; ++i) {
    Window w = (Window)wins[i];
    PanelTask* task = panel_find_task(panel, w);
    if (!task) {
      task = panel_task_new(panel, w);
      if (!task)
        continue;
      g_hash_table_insert(panel->task_lookup, GUINT_TO_POINTER(w), task);
    }
    else if (!panel_task_sync(panel, task)) {
      panel_remove_task(panel, w);
      continue;
    }

    task->serial = panel->task_serial;
    if (panel_task_visible(panel, task))
      g_ptr_array_add(panel->visible_tasks, task);
  }

  GHashTableIter iter;
  gpointer key = NULL;
  PanelTask* task = NULL;
  g_hash_table_iter_init(&iter, panel->task_lookup);
  while (g_hash_table_iter_next(&iter, &key, (gpointer*)&task)) {
    if (task->serial != panel->task_serial)
      g_hash_table_iter_remove(&iter);
  }
  g_free(wins);
}

static void panel_update_root_state(Panel* panel) {
  guint32 desktop = 0;
  if (OBT_PROP_GET32(panel->root, NET_CURRENT_DESKTOP, CARDINAL, &desktop))
    panel->current_desktop = desktop;
  guint32 count = 0;
  if (OBT_PROP_GET32(panel->root, NET_NUMBER_OF_DESKTOPS, CARDINAL, &count))
    panel->num_desktops = count;
  guint32 active = 0;
  if (OBT_PROP_GET32(panel->root, NET_ACTIVE_WINDOW, WINDOW, &active))
    panel->active = (Window)active;
  else
    panel->active = None;
}

static gchar* clock_now_text(const gchar* format) {
  time_t now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);
  gchar buf[sizeof(((Panel*)NULL)->clock_text)];
  if (!strftime(buf, sizeof(buf), format, &tm_now))
    return g_strdup("");
  return g_strdup(buf);
}

static gboolean panel_clock_tick(gpointer data) {
  Panel* panel = data;
  gchar* current = clock_now_text(panel->clock_format ? panel->clock_format : "%a %d %b %H:%M");
  if (g_strcmp0(current, panel->clock_text) != 0) {
    g_strlcpy(panel->clock_text, current, sizeof(panel->clock_text));
    panel_queue_redraw(panel);
  }
  g_free(current);
  return G_SOURCE_CONTINUE;
}

static gchar* read_status_command(const gchar* command) {
  gchar* stdout_str = NULL;
  gchar* stderr_str = NULL;
  gint status = 0;
  if (!command)
    return g_strdup("");
  if (!g_spawn_command_line_sync(command, &stdout_str, &stderr_str, &status, NULL)) {
    g_clear_pointer(&stdout_str, g_free);
    g_clear_pointer(&stderr_str, g_free);
    return g_strdup("");
  }
  g_free(stderr_str);
  if (!stdout_str)
    return g_strdup("");
  gchar* trimmed = g_strstrip(stdout_str);
  gchar* ret = g_strndup(trimmed, 256);
  g_free(stdout_str);
  return ret;
}

static gboolean panel_status_tick(gpointer data) {
  Panel* panel = data;
  if (!panel->status_command)
    return G_SOURCE_CONTINUE;
  gchar* text = read_status_command(panel->status_command);
  if (g_strcmp0(text, panel->status_text) != 0) {
    g_free(panel->status_text);
    panel->status_text = text;
    panel_queue_redraw(panel);
  }
  else
    g_free(text);
  return G_SOURCE_CONTINUE;
}

#ifdef HAVE_ALSA
static gboolean mixer_open(Panel* panel) {
  if (panel->mixer_ready)
    return TRUE;
  if (snd_mixer_open(&panel->mixer.handle, 0) < 0)
    return FALSE;
  if (snd_mixer_attach(panel->mixer.handle, panel->opts.mixer_device ? panel->opts.mixer_device : "default") < 0)
    goto fail;
  snd_mixer_selem_register(panel->mixer.handle, NULL, NULL);
  if (snd_mixer_load(panel->mixer.handle) < 0)
    goto fail;

  snd_mixer_selem_id_t* sid = NULL;
  snd_mixer_selem_id_malloc(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, panel->opts.mixer_element ? panel->opts.mixer_element : "Master");
  panel->mixer.elem = snd_mixer_find_selem(panel->mixer.handle, sid);
  snd_mixer_selem_id_free(sid);
  if (!panel->mixer.elem)
    goto fail;
  snd_mixer_selem_get_playback_volume_range(panel->mixer.elem, &panel->mixer.min, &panel->mixer.max);
  panel->mixer_ready = TRUE;
  return TRUE;

fail:
  if (panel->mixer.handle) {
    snd_mixer_close(panel->mixer.handle);
    panel->mixer.handle = NULL;
  }
  return FALSE;
}

static gboolean mixer_get_volume(Panel* panel, gint* level, gboolean* muted) {
  if (!mixer_open(panel))
    return FALSE;
  long left = 0, right = 0;
  if (snd_mixer_selem_get_playback_volume(panel->mixer.elem, SND_MIXER_SCHN_FRONT_LEFT, &left) < 0 ||
      snd_mixer_selem_get_playback_volume(panel->mixer.elem, SND_MIXER_SCHN_FRONT_RIGHT, &right) < 0)
    return FALSE;
  gint percent = 0;
  if (panel->mixer.max > panel->mixer.min)
    percent = (gint)(((left + right) / 2 - panel->mixer.min) * 100 / (panel->mixer.max - panel->mixer.min));
  *level = CLAMP(percent, 0, 100);
  int sw = 1;
  snd_mixer_selem_get_playback_switch(panel->mixer.elem, SND_MIXER_SCHN_FRONT_LEFT, &sw);
  *muted = (sw == 0);
  return TRUE;
}

static gboolean mixer_set_volume(Panel* panel, gint delta) {
  if (!mixer_open(panel))
    return FALSE;
  gint level = 0;
  gboolean muted = FALSE;
  if (!mixer_get_volume(panel, &level, &muted))
    return FALSE;
  level = CLAMP(level + delta, 0, 100);
  const long value = panel->mixer.min + (panel->mixer.max - panel->mixer.min) * level / 100;
  snd_mixer_selem_set_playback_volume(panel->mixer.elem, SND_MIXER_SCHN_FRONT_LEFT, value);
  snd_mixer_selem_set_playback_volume(panel->mixer.elem, SND_MIXER_SCHN_FRONT_RIGHT, value);
  return TRUE;
}

static gboolean mixer_toggle_mute(Panel* panel) {
  if (!mixer_open(panel))
    return FALSE;
  int sw = 1;
  snd_mixer_selem_get_playback_switch(panel->mixer.elem, SND_MIXER_SCHN_FRONT_LEFT, &sw);
  sw = sw ? 0 : 1;
  snd_mixer_selem_set_playback_switch(panel->mixer.elem, SND_MIXER_SCHN_FRONT_LEFT, sw);
  snd_mixer_selem_set_playback_switch(panel->mixer.elem, SND_MIXER_SCHN_FRONT_RIGHT, sw);
  return TRUE;
}
#endif

static gboolean panel_volume_tick(gpointer data) {
  Panel* panel = data;
  if (!panel->show_volume)
    return G_SOURCE_CONTINUE;
#ifdef HAVE_ALSA
  gint level = 0;
  gboolean muted = FALSE;
  if (mixer_get_volume(panel, &level, &muted)) {
    gchar* text = g_strdup_printf("%s %d%%", muted ? "MUTE" : "VOL", level);
    g_free(panel->volume_text);
    panel->volume_text = text;
  }
  else {
    g_free(panel->volume_text);
    panel->volume_text = g_strdup("vol --");
  }
#else
  g_free(panel->volume_text);
  panel->volume_text = g_strdup("");
#endif
  panel_queue_redraw(panel);
  return G_SOURCE_CONTINUE;
}

static void panel_setup_timers(Panel* panel, guint status_interval) {
  panel_clock_tick(panel);
  panel->clock_source = g_timeout_add_seconds(1, panel_clock_tick, panel);

  if (panel->status_command && *panel->status_command) {
    panel_status_tick(panel);
    panel->status_source = g_timeout_add_seconds(MAX(1, status_interval), panel_status_tick, panel);
  }
  if (panel->show_volume) {
    panel_volume_tick(panel);
    panel->volume_source = g_timeout_add_seconds(2, panel_volume_tick, panel);
  }
}

static PanelTask* panel_task_at(Panel* panel, gdouble x) {
  if (!panel->task_regions || panel->task_regions->len == 0)
    return NULL;
  for (guint i = 0; i < panel->task_regions->len; ++i) {
    PanelTaskRegion region = g_array_index(panel->task_regions, PanelTaskRegion, i);
    if (x >= region.x && x <= region.x + region.width) {
      if (i < panel->visible_tasks->len)
        return g_ptr_array_index(panel->visible_tasks, i);
      break;
    }
  }
  return NULL;
}

static gboolean point_in_region(const PanelTaskRegion* region, gdouble x, gdouble y, gint height) {
  if (region->width <= 0)
    return FALSE;
  if (y < 0 || y > height)
    return FALSE;
  return (x >= region->x && x <= region->x + region->width);
}

static void panel_activate_window(Panel* panel, Window win) {
  if (!win || win == None)
    return;
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = win;
  ev.xclient.message_type = panel->atoms.net_active_window;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = 1;
  ev.xclient.data.l[1] = CurrentTime;
  XSendEvent(panel->dpy, panel->root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  XFlush(panel->dpy);
}

static void panel_handle_button(Panel* panel, XButtonEvent* ev) {
  const gdouble x = ev->x;
  const gdouble y = ev->y;
  if (point_in_region(&panel->volume_region, x, y, panel->height)) {
#ifdef HAVE_ALSA
    if (ev->button == Button1) {
      mixer_toggle_mute(panel);
    }
    else if (ev->button == Button4)
      mixer_set_volume(panel, 5);
    else if (ev->button == Button5)
      mixer_set_volume(panel, -5);
    panel_volume_tick(panel);
#endif
    return;
  }
  PanelTask* task = panel_task_at(panel, x);
  if (!task)
    return;
  if (ev->button == Button1)
    panel_activate_window(panel, task->window);
}

static void panel_prepare_cr(Panel* panel) {
  cairo_set_source_rgb(panel->cr, PANEL_BG[0], PANEL_BG[1], PANEL_BG[2]);
  cairo_paint(panel->cr);
  cairo_select_font_face(panel->cr, panel->font_family ? panel->font_family : "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(panel->cr, panel->font_size > 0 ? panel->font_size : 11.0);
}

static gdouble panel_draw_text(cairo_t* cr, const gchar* text, gdouble x, gdouble y) {
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, text);
  cairo_new_path(cr);
  return x;
}

static gdouble module_box_width(cairo_t* cr, const gchar* text, gdouble padding) {
  if (!text || !*text)
    return 0;
  cairo_text_extents_t extents;
  cairo_text_extents(cr, text, &extents);
  return extents.width + padding * 2;
}

static void draw_module_box(cairo_t* cr, gdouble x, gdouble width, gdouble height, const gdouble color[3]) {
  cairo_rectangle(cr, x, 4, width, height - 8);
  cairo_set_source_rgba(cr, color[0], color[1], color[2], 0.85);
  cairo_fill(cr);
}

static void draw_module_label(cairo_t* cr, const gchar* text, gdouble x, gdouble height) {
  cairo_text_extents_t extents;
  cairo_text_extents(cr, text, &extents);
  const gdouble text_x = x + 8;
  const gdouble text_y = height / 2.0 + extents.height / 2.0;
  cairo_set_source_rgb(cr, PANEL_FG[0], PANEL_FG[1], PANEL_FG[2]);
  panel_draw_text(cr, text, text_x, text_y);
}

static gchar* ellipsize_text(cairo_t* cr, const gchar* text, gdouble max_width) {
  if (!text || max_width <= 0)
    return NULL;
  cairo_text_extents_t ext;
  cairo_text_extents(cr, text, &ext);
  if (ext.width <= max_width)
    return NULL;
  cairo_text_extents(cr, "...", &ext);
  const gdouble ellipsis = ext.width;
  gchar* tmp = g_strdup(text);
  gsize len = g_utf8_strlen(tmp, -1);
  while (len > 0) {
    gchar* pos = g_utf8_offset_to_pointer(tmp, len);
    *pos = '\0';
    cairo_text_extents(cr, tmp, &ext);
    if (ext.width + ellipsis <= max_width) {
      gchar* truncated = g_strdup_printf("%s...", tmp);
      g_free(tmp);
      return truncated;
    }
    --len;
  }
  g_free(tmp);
  return g_strdup("...");
}

static void panel_layout_modules(Panel* panel, gdouble* right_edge) {
  const gdouble padding = 8.0;
  const gdouble spacing = 6.0;
  const gdouble height = panel->height;

  panel->clock_region.width = 0;
  panel->status_region.width = 0;
  panel->volume_region.width = 0;

  if (panel->clock_text[0]) {
    const gdouble width = module_box_width(panel->cr, panel->clock_text, padding);
    const gdouble x = *right_edge - width;
    panel->clock_region.x = x;
    panel->clock_region.width = width;
    draw_module_box(panel->cr, x, width, height, PANEL_ACTIVE);
    draw_module_label(panel->cr, panel->clock_text, x, height);
    *right_edge = x - spacing;
  }

  if (panel->volume_text && *panel->volume_text) {
    const gdouble width = module_box_width(panel->cr, panel->volume_text, padding);
    const gdouble x = *right_edge - width;
    panel->volume_region.x = x;
    panel->volume_region.width = width;
    draw_module_box(panel->cr, x, width, height, PANEL_DIM);
    draw_module_label(panel->cr, panel->volume_text, x, height);
    *right_edge = x - spacing;
  }

  if (panel->status_text && *panel->status_text) {
    const gdouble width = module_box_width(panel->cr, panel->status_text, padding);
    const gdouble x = *right_edge - width;
    panel->status_region.x = x;
    panel->status_region.width = width;
    draw_module_box(panel->cr, x, width, height, PANEL_DIM);
    draw_module_label(panel->cr, panel->status_text, x, height);
    *right_edge = x - spacing;
  }
}

static void draw_task(Panel* panel, PanelTask* task, PanelTaskRegion* region) {
  const gdouble padding = 8.0;
  const gdouble y = 4.0;
  const gdouble h = panel->height - 8.0;
  const gboolean focused = (task->window == panel->active);
  const gdouble* color = PANEL_DIM;
  if (focused)
    color = PANEL_ACTIVE;
  else if (task->urgent)
    color = PANEL_URGENT;
  cairo_rectangle(panel->cr, region->x, y, region->width, h);
  cairo_set_source_rgba(panel->cr, color[0], color[1], color[2], focused ? 0.95 : 0.4);
  cairo_fill(panel->cr);

  cairo_text_extents_t ext;
  cairo_text_extents(panel->cr, task->title ? task->title : "", &ext);
  const gdouble text_x = region->x + padding;
  const gdouble text_y = panel->height / 2.0 + ext.height / 2.0;
  cairo_set_source_rgb(panel->cr, PANEL_FG[0], PANEL_FG[1], PANEL_FG[2]);
  if (task->hidden)
    cairo_set_source_rgb(panel->cr, PANEL_DIM[0], PANEL_DIM[1], PANEL_DIM[2]);

  const gdouble available = region->width - padding * 2;
  gchar* truncated = ellipsize_text(panel->cr, task->title, available);
  const gchar* label = truncated ? truncated : (task->title ? task->title : "");
  if (!label)
    label = "";
  panel_draw_text(panel->cr, label, text_x, text_y);
  g_free(truncated);
}

static void panel_draw_tasks(Panel* panel, gdouble left, gdouble right) {
  if (!panel->visible_tasks || panel->visible_tasks->len == 0) {
    g_array_set_size(panel->task_regions, 0);
    return;
  }
  const gdouble spacing = 4.0;
  const gdouble width = MAX(0.0, right - left);
  const guint count = panel->visible_tasks->len;
  const gdouble available = width - ((count > 0) ? (spacing * (count - 1)) : 0);
  if (available <= 0) {
    g_array_set_size(panel->task_regions, 0);
    return;
  }
  const gdouble base_width = available / count;
  g_array_set_size(panel->task_regions, count);
  gdouble cursor = left;
  for (guint i = 0; i < count; ++i) {
    PanelTask* task = g_ptr_array_index(panel->visible_tasks, i);
    PanelTaskRegion region = {.win = task->window, .x = cursor, .width = base_width};
    g_array_index(panel->task_regions, PanelTaskRegion, i) = region;
    draw_task(panel, task, &region);
    cursor += base_width + spacing;
  }
}

static gboolean panel_redraw(Panel* panel) {
  if (!panel->cr)
    return FALSE;
  panel_prepare_cr(panel);
  gdouble right_edge = panel->width - 8.0;
  panel_layout_modules(panel, &right_edge);
  const gdouble tasks_left = 8.0;
  panel_draw_tasks(panel, tasks_left, right_edge);
  cairo_surface_flush(panel->surface);
  XFlush(panel->dpy);
  return TRUE;
}

static gboolean panel_queue_redraw(Panel* panel) {
  panel_redraw(panel);
  return TRUE;
}

static gboolean panel_handle_xevent(GIOChannel* chan, GIOCondition cond, gpointer data) {
  Panel* panel = data;
  if (!(cond & (G_IO_IN | G_IO_PRI)))
    return G_SOURCE_CONTINUE;
  while (XPending(panel->dpy)) {
    XEvent ev;
    XNextEvent(panel->dpy, &ev);
    switch (ev.type) {
      case Expose:
        if (ev.xexpose.count == 0)
          panel_redraw(panel);
        break;
      case ConfigureNotify:
        if (ev.xconfigure.window == panel->root)
          panel_update_geometry(panel, FALSE);
        else if (ev.xconfigure.window == panel->win) {
          const gint expected_y = panel_target_y(panel);
          if (ev.xconfigure.x != 0 || ev.xconfigure.y != expected_y || ev.xconfigure.width != panel->width ||
              ev.xconfigure.height != panel->height) {
            panel_update_geometry(panel, TRUE);
          }
        }
        break;
      case DestroyNotify:
        panel_remove_task(panel, ev.xdestroywindow.window);
        panel_refresh_tasks(panel);
        panel_queue_redraw(panel);
        break;
      case PropertyNotify: {
        Atom atom = ev.xproperty.atom;
        if (ev.xproperty.window == panel->root &&
            (atom == panel->atoms.net_client_list || atom == panel->atoms.net_active_window ||
             atom == panel->atoms.net_current_desktop || atom == panel->atoms.net_number_of_desktops)) {
          panel_update_root_state(panel);
          panel_refresh_tasks(panel);
          panel_queue_redraw(panel);
        }
        else {
          PanelTask* task = panel_find_task(panel, ev.xproperty.window);
          if (task) {
            if (atom == panel->atoms.net_wm_state)
              panel_task_update_state(panel, task);
            else if (atom == panel->atoms.net_wm_desktop)
              panel_task_update_desktop(task);
            else if (atom == panel->atoms.net_wm_visible_name || atom == panel->atoms.net_wm_name ||
                     atom == panel->atoms.wm_name)
              panel_task_update_title(panel, task);
            else if (atom == panel->atoms.wm_hints)
              panel_task_update_state(panel, task);
            panel_refresh_tasks(panel);
            panel_queue_redraw(panel);
          }
        }
        break;
      }
      case ButtonPress:
        panel_handle_button(panel, &ev.xbutton);
        break;
      default:
        break;
    }
  }
  return G_SOURCE_CONTINUE;
}

static gboolean panel_signal_quit(gpointer data) {
  Panel* panel = data;
  if (panel->loop)
    g_main_loop_quit(panel->loop);
  return G_SOURCE_REMOVE;
}

static void panel_run(Panel* panel) {
  panel_update_root_state(panel);
  panel_refresh_tasks(panel);
  panel_setup_timers(panel, panel->opts.status_interval ? panel->opts.status_interval : 5);
  panel_redraw(panel);

  panel->loop = g_main_loop_new(NULL, FALSE);
  GIOChannel* channel = g_io_channel_unix_new(ConnectionNumber(panel->dpy));
  g_io_add_watch(channel, G_IO_IN, panel_handle_xevent, panel);
  g_io_channel_unref(channel);
  g_unix_signal_add(SIGINT, panel_signal_quit, panel);
  g_unix_signal_add(SIGTERM, panel_signal_quit, panel);
  g_main_loop_run(panel->loop);
}

static void panel_destroy(Panel* panel) {
  if (!panel)
    return;
  if (panel->clock_source)
    g_source_remove(panel->clock_source);
  if (panel->status_source)
    g_source_remove(panel->status_source);
  if (panel->volume_source)
    g_source_remove(panel->volume_source);

  if (panel->loop)
    g_main_loop_unref(panel->loop);

  if (panel->task_lookup)
    g_hash_table_destroy(panel->task_lookup);
  if (panel->visible_tasks)
    g_ptr_array_free(panel->visible_tasks, TRUE);
  if (panel->task_regions)
    g_array_free(panel->task_regions, TRUE);

#ifdef HAVE_ALSA
  if (panel->mixer.handle)
    snd_mixer_close(panel->mixer.handle);
#endif

  g_clear_pointer(&panel->status_text, g_free);
  g_clear_pointer(&panel->status_command, g_free);
  g_clear_pointer(&panel->clock_format, g_free);
  g_clear_pointer(&panel->volume_text, g_free);
  g_clear_pointer(&panel->font_family, g_free);

  if (panel->cr)
    cairo_destroy(panel->cr);
  if (panel->surface)
    cairo_surface_destroy(panel->surface);
  if (panel->win)
    XDestroyWindow(panel->dpy, panel->win);
  if (panel->dpy) {
    XCloseDisplay(panel->dpy);
    obt_display = NULL;
  }

  g_free(panel->opts.display_name);
  g_free(panel->opts.font_spec);
  g_free(panel->opts.clock_format);
  g_free(panel->opts.status_command);
  g_free(panel->opts.mixer_device);
  g_free(panel->opts.mixer_element);

  g_free(panel);
}

int main(int argc, char** argv) {
  g_set_prgname("obpanel");

  PanelOptions opts = {0};
  opts.height = 32;
  opts.edge = PANEL_EDGE_TOP;
  opts.status_interval = 5;
  opts.show_all_tasks = TRUE;

  gchar* edge_option = NULL;
  gchar* tasks_mode = NULL;
  GOptionEntry entries[] = {
      {"display", 0, 0, G_OPTION_ARG_STRING, &opts.display_name, "X display to connect to", "DISPLAY"},
      {"height", 0, 0, G_OPTION_ARG_INT, &opts.height, "Panel height in pixels", "N"},
      {"edge", 0, 0, G_OPTION_ARG_STRING, &edge_option, "Edge to anchor to (top/bottom)", "EDGE"},
      {"font", 0, 0, G_OPTION_ARG_STRING, &opts.font_spec, "Cairo font description", "FONT"},
      {"clock-format", 0, 0, G_OPTION_ARG_STRING, &opts.clock_format, "strftime format for the clock", "FMT"},
      {"status-command", 0, 0, G_OPTION_ARG_STRING, &opts.status_command, "Shell command for status text", "CMD"},
      {"status-interval", 0, 0, G_OPTION_ARG_INT, &opts.status_interval, "Seconds between status refreshes", "N"},
      {"mixer-device", 0, 0, G_OPTION_ARG_STRING, &opts.mixer_device, "ALSA mixer device (default)", "NAME"},
      {"mixer-element", 0, 0, G_OPTION_ARG_STRING, &opts.mixer_element, "ALSA mixer element (Master)", "NAME"},
      {"tasks", 0, 0, G_OPTION_ARG_STRING, &tasks_mode, "Task visibility scope (all/current)", "MODE"},
      {NULL}};

  GError* error = NULL;
  GOptionContext* context = g_option_context_new("- lightweight Openbox panel");
  g_option_context_add_main_entries(context, entries, NULL);
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_printerr("Failed to parse options: %s\n", error->message);
    g_error_free(error);
    g_option_context_free(context);
    g_free(tasks_mode);
    g_free(edge_option);
    panel_options_clear(&opts);
    return EXIT_FAILURE;
  }
  g_option_context_free(context);

  if (!opts.font_spec)
    opts.font_spec = g_strdup("Sans 11");
  if (!opts.clock_format)
    opts.clock_format = g_strdup("%a %d %b %H:%M");
  if (!opts.mixer_device)
    opts.mixer_device = g_strdup("default");
  if (!opts.mixer_element)
    opts.mixer_element = g_strdup("Master");
  if (tasks_mode) {
    if (g_ascii_strcasecmp(tasks_mode, "all") == 0)
      opts.show_all_tasks = TRUE;
    else if (g_ascii_strcasecmp(tasks_mode, "current") == 0)
      opts.show_all_tasks = FALSE;
    else
      g_printerr("Unknown tasks mode '%s', using 'all'\n", tasks_mode);
    g_free(tasks_mode);
  }

  if (edge_option) {
    if (g_ascii_strcasecmp(edge_option, "top") == 0)
      opts.edge = PANEL_EDGE_TOP;
    else if (g_ascii_strcasecmp(edge_option, "bottom") == 0)
      opts.edge = PANEL_EDGE_BOTTOM;
    else
      g_printerr("Unknown edge '%s', using bottom\n", edge_option);
    g_free(edge_option);
  }

  Panel* panel = panel_create(&opts);
  panel_options_clear(&opts);
  if (!panel) {
    g_printerr("Unable to start obpanel\n");
    return EXIT_FAILURE;
  }

  panel_run(panel);
  panel_destroy(panel);
  return EXIT_SUCCESS;
}

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/cursorfont.h>
#include <assert.h>
#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>

#define PROP_MAX_BYTES (8 * 1024 * 1024)

static gboolean get_all(xcb_connection_t* conn,
                        xcb_window_t win,
                        xcb_atom_t prop,
                        xcb_atom_t* type,
                        gint* size,
                        guchar** data,
                        guint* num);

gint fail(const gchar* s) {
  if (s)
    fprintf(stderr, "%s\n", s);
  else
    fprintf(stderr,
            "Usage: obxprop [OPTIONS] [--] [PROPERTIES ...]\n\n"
            "Options:\n"
            "    --help              Display this help and exit\n"
            "    --display DISPLAY   Connect to this X display\n"
            "    --id ID             Show the properties for this window\n"
            "    --root              Show the properties for the root window\n");
  return 1;
}

gint parse_hex(gchar* s) {
  gint result = 0;
  while (*s) {
    gint add;
    if (*s >= '0' && *s <= '9')
      add = *s - '0';
    else if (*s >= 'A' && *s <= 'F')
      add = *s - 'A' + 10;
    else if (*s >= 'a' && *s <= 'f')
      add = *s - 'a' + 10;
    else
      break;

    result *= 16;
    result += add;
    ++s;
  }
  return result;
}

static xcb_atom_t intern_atom(xcb_connection_t* conn, const char* name, gboolean only_if_exists) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_if_exists, strlen(name), name);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

static gchar* atom_name_dup(xcb_connection_t* conn, xcb_atom_t atom) {
  xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(conn, atom);
  xcb_get_atom_name_reply_t* reply = xcb_get_atom_name_reply(conn, cookie, NULL);
  gchar* name = NULL;

  if (reply) {
    const int len = xcb_get_atom_name_name_length(reply);
    name = g_strndup(xcb_get_atom_name_name(reply), len);
  }

  free(reply);
  return name;
}

Window find_client(Display* d, xcb_connection_t* conn, Window win, xcb_atom_t state) {
  Window r, *children;
  guint n, i;
  Window found = None;

  XQueryTree(d, win, &r, &r, &children, &n);
  for (i = 0; i < n; ++i) {
    found = find_client(d, conn, children[i], state);
    if (found)
      break;
  }
  if (children)
    XFree(children);
  if (found)
    return found;

  {
    xcb_atom_t ret_type = XCB_ATOM_NONE;
    guchar* data = NULL;
    guint items = 0;
    gint format = 0;

    if (state != XCB_ATOM_NONE && get_all(conn, win, state, &ret_type, &format, &data, &items)) {
      g_free(data);
      if (ret_type == state && items >= 1)
        return win;
    }
  }
  return None;
}

static gboolean get_all(xcb_connection_t* conn,
                        xcb_window_t win,
                        xcb_atom_t prop,
                        xcb_atom_t* type,
                        gint* size,
                        guchar** data,
                        guint* num) {
  gboolean ret = FALSE;
  xcb_get_property_cookie_t cookie;
  xcb_get_property_reply_t* reply = NULL;
  const uint8_t* raw;
  gsize bytes;

  g_return_val_if_fail(conn != NULL, FALSE);
  g_return_val_if_fail(type != NULL, FALSE);
  g_return_val_if_fail(size != NULL, FALSE);
  g_return_val_if_fail(data != NULL, FALSE);
  g_return_val_if_fail(num != NULL, FALSE);

  *data = NULL;
  *num = 0;
  *type = XCB_ATOM_NONE;
  *size = 0;

  cookie = xcb_get_property(conn, 0, win, prop, XCB_GET_PROPERTY_TYPE_ANY, 0, PROP_MAX_BYTES / 4);
  reply = xcb_get_property_reply(conn, cookie, NULL);
  if (!reply)
    return FALSE;

  if (reply->bytes_after)
    goto out;
  if (reply->format != 8 && reply->format != 16 && reply->format != 32)
    goto out;

  *type = reply->type;
  *size = reply->format;
  *num = reply->value_len;
  bytes = reply->value_len * (reply->format / 8);

  if (bytes) {
    raw = xcb_get_property_value(reply);
    if (!raw)
      goto out;

    *data = g_malloc(bytes);
    memcpy(*data, raw, bytes);
  }

  ret = TRUE;

out:
  free(reply);
  return ret;
}

GString* append_string(GString* before, gchar* after, gboolean quote) {
  const gchar* q = quote ? "\"" : "";
  if (before)
    g_string_append_printf(before, ", %s%s%s", q, after, q);
  else
    g_string_append_printf(before = g_string_new(NULL), "%s%s%s", q, after, q);
  return before;
}

GString* append_int(GString* before, guint after) {
  if (before)
    g_string_append_printf(before, ", %u", after);
  else
    g_string_append_printf(before = g_string_new(NULL), "%u", after);
  return before;
}

gchar* read_strings(gchar* val, guint n, gboolean utf8) {
  GSList *strs = NULL, *it;
  GString* ret;
  gchar* p;
  guint i;

  p = val;
  while (p < val + n) {
    strs = g_slist_append(strs, g_strndup(p, n - (p - val)));
    p += strlen(p) + 1; /* next string */
  }

  ret = NULL;
  for (i = 0, it = strs; it; ++i, it = g_slist_next(it)) {
    char* data;

    if (utf8) {
      if (g_utf8_validate(it->data, -1, NULL))
        data = g_strdup(it->data);
      else
        data = g_strdup("");
    }
    else
      data = g_locale_to_utf8(it->data, -1, NULL, NULL, NULL);

    ret = append_string(ret, data, TRUE);
    g_free(data);
  }

  while (strs) {
    g_free(strs->data);
    strs = g_slist_delete_link(strs, strs);
  }
  if (ret)
    return g_string_free(ret, FALSE);
  return NULL;
}

gchar* read_atoms(xcb_connection_t* conn, guchar* val, guint n) {
  GString* ret;
  guint i;

  ret = NULL;
  for (i = 0; i < n; ++i) {
    gchar* name = atom_name_dup(conn, ((guint32*)val)[i]);
    ret = append_string(ret, name ? name : "(unknown)", FALSE);
    g_free(name);
  }
  if (ret)
    return g_string_free(ret, FALSE);
  return NULL;
}

gchar* read_numbers(guchar* val, guint n, guint size) {
  GString* ret;
  guint i;

  ret = NULL;
  for (i = 0; i < n; ++i)
    switch (size) {
      case 8:
        ret = append_int(ret, ((guint8*)val)[i]);
        break;
      case 16:
        ret = append_int(ret, ((guint16*)val)[i]);
        break;
      case 32:
        ret = append_int(ret, ((guint32*)val)[i]);
        break;
      default:
        g_assert_not_reached(); /* unhandled size */
    }

  if (ret)
    return g_string_free(ret, FALSE);
  return NULL;
}

gboolean read_prop(xcb_connection_t* conn, Window w, xcb_atom_t prop, gchar** type, gchar** val) {
  guchar* ret = NULL;
  guint nret = 0;
  gint size = 0;
  xcb_atom_t ret_type = XCB_ATOM_NONE;

  if (!get_all(conn, w, prop, &ret_type, &size, &ret, &nret))
    return FALSE;

  *type = atom_name_dup(conn, ret_type);
  if (!*type)
    *type = g_strdup("(unknown)");

  if (!ret || nret == 0) {
    *val = NULL;
    g_free(ret);
    return TRUE;
  }

  if (g_strcmp0(*type, "STRING") == 0)
    *val = read_strings((gchar*)ret, nret, FALSE);
  else if (g_strcmp0(*type, "UTF8_STRING") == 0)
    *val = read_strings((gchar*)ret, nret, TRUE);
  else if (g_strcmp0(*type, "ATOM") == 0) {
    g_assert(size == 32);
    *val = read_atoms(conn, ret, nret);
  }
  else
    *val = read_numbers(ret, nret, size);

  g_free(ret);
  return TRUE;
}

void show_properties(xcb_connection_t* conn, Window w, int argc, char** argv) {
  xcb_list_properties_cookie_t cookie = xcb_list_properties(conn, w);
  xcb_list_properties_reply_t* reply = xcb_list_properties_reply(conn, cookie, NULL);

  if (!reply)
    return;

  int n = xcb_list_properties_atoms_length(reply);
  xcb_atom_t* props = xcb_list_properties_atoms(reply);

  for (int i = 0; i < n; ++i) {
    gchar *type = NULL, *val = NULL;
    gchar* name = atom_name_dup(conn, props[i]);

    if (read_prop(conn, w, props[i], &type, &val)) {
      gboolean found = (argc == 0);
      if (!found && name) {
        for (int j = 0; j < argc; j++) {
          if (!strcmp(name, argv[j])) {
            found = TRUE;
            break;
          }
        }
      }
      if (found)
        g_print("%s(%s) = %s\n", name ? name : "(unknown)", type ? type : "(unknown)", val ? val : "");
    }

    g_free(type);
    g_free(val);
    g_free(name);
  }

  free(reply);
}

int main(int argc, char** argv) {
  Display* d;
  Window id, userid = None;
  int i;
  char* dname = NULL;
  gboolean root = FALSE;

  for (i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--help")) {
      return fail(NULL);
    }
    else if (!strcmp(argv[i], "--root"))
      root = TRUE;
    else if (!strcmp(argv[i], "--id")) {
      if (++i == argc)
        return fail(NULL);
      if (argv[i][0] == '0' && argv[i][1] == 'x') {
        /* hex */
        userid = parse_hex(argv[i] + 2);
      }
      else {
        /* decimal */
        userid = atoi(argv[i]);
      }
      if (!userid)
        return fail("Unable to parse argument to --id.");
    }
    else if (!strcmp(argv[i], "--display")) {
      if (++i == argc)
        return fail(NULL);
      dname = argv[i];
    }
    else if (*argv[i] != '-')
      break;
    else if (!strcmp(argv[i], "--")) {
      i++;
      break;
    }
    else
      return fail(NULL);
  }

  d = XOpenDisplay(dname);
  if (!d) {
    return fail(
        "Unable to find an X display. "
        "Ensure you have permission to connect to the display.");
  }
  xcb_connection_t* conn = XGetXCBConnection(d);
  if (!conn) {
    XCloseDisplay(d);
    return fail("Unable to obtain XCB connection");
  }
  xcb_atom_t state_atom = intern_atom(conn, "WM_STATE", True);

  if (root)
    userid = RootWindow(d, DefaultScreen(d));

  if (userid == None) {
    int j;
    j = XGrabPointer(d, RootWindow(d, DefaultScreen(d)), False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None,
                     XCreateFontCursor(d, XC_crosshair), CurrentTime);
    if (j != GrabSuccess)
      return fail("Unable to grab the pointer device");
    while (1) {
      XEvent ev;

      XNextEvent(d, &ev);
      if (ev.type == ButtonPress) {
        XUngrabPointer(d, CurrentTime);
        userid = ev.xbutton.subwindow;
        break;
      }
    }
    id = find_client(d, conn, userid, state_atom);
  }
  else
    id = userid; /* they picked this one */

  if (id == None)
    return fail("Unable to find window with the requested ID");

  show_properties(conn, id, argc - i, &argv[i]);

  XCloseDisplay(d);

  return 0;
}

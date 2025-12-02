/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */

#include "openbox/x11/x11.h"

#include <glib.h>

static void assert_success(const char* data,
                           gsize len,
                           gsize min_strings,
                           gsize max_strings,
                           const char* const* expected,
                           gsize expected_len) {
  ObX11PropertyValue value = {(guchar*)data, len, 8, None};
  ObX11StringList list;
  gboolean ok = ob_x11_property_to_string_list(&value, min_strings, max_strings, &list);

  g_assert_true(ok);
  g_assert_cmpuint(list.n_items, ==, expected_len);
  for (gsize i = 0; i < expected_len; ++i)
    g_assert_cmpstr(list.items[i], ==, expected[i]);
  ob_x11_string_list_clear(&list);
}

static void assert_failure(const char* data, gsize len, gsize min_strings, gsize max_strings) {
  ObX11PropertyValue value = {(guchar*)data, len, 8, None};
  ObX11StringList list;
  gboolean ok = ob_x11_property_to_string_list(&value, min_strings, max_strings, &list);

  g_assert_false(ok);
}

static void test_two_strings(void) {
  static const char data[] = {'o', 'n', 'e', '\0', 't', 'w', 'o', '\0'};
  static const char* expected[] = {"one", "two"};

  assert_success(data, sizeof(data), 0, 0, expected, 2);
}

static void test_trailing_nul_ignored(void) {
  static const char data[] = {'o', 'n', 'e', '\0', '\0'};
  static const char* expected[] = {"one"};

  assert_success(data, sizeof(data), 0, 0, expected, 1);
}

static void test_missing_terminator_fails(void) {
  static const char data[] = {'o', 'n', 'e'};
  assert_failure(data, sizeof(data), 0, 0);
}

static void test_missing_final_nul_between_strings_fails(void) {
  static const char data[] = {'o', 'n', 'e', '\0', 't', 'w', 'o'};
  assert_failure(data, sizeof(data), 0, 0);
}

static void test_respects_max_strings(void) {
  static const char data[] = {'a', '\0', 'b', '\0', 'c', '\0'};
  assert_failure(data, sizeof(data), 0, 2);
}

static void test_min_strings_enforced(void) {
  assert_failure(NULL, 0, 1, 0);
}

static void test_empty_allowed(void) {
  assert_success(NULL, 0, 0, 0, NULL, 0);
}

int main(int argc, char** argv) {
  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/x11/property/string_list/two_strings", test_two_strings);
  g_test_add_func("/x11/property/string_list/trailing_nul", test_trailing_nul_ignored);
  g_test_add_func("/x11/property/string_list/missing_terminator", test_missing_terminator_fails);
  g_test_add_func("/x11/property/string_list/missing_final_nul", test_missing_final_nul_between_strings_fails);
  g_test_add_func("/x11/property/string_list/max_strings", test_respects_max_strings);
  g_test_add_func("/x11/property/string_list/min_strings", test_min_strings_enforced);
  g_test_add_func("/x11/property/string_list/empty_allowed", test_empty_allowed);

  return g_test_run();
}

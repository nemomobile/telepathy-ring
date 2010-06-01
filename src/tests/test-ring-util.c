/*
 * test-ring-util.c - Test cases for ring utility functions
 *
 * Copyright (C) 2009 Nokia Corporation
 *   @author Pekka Pessi <first.surname@nokia.com>
 *
 * This work is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <dbus/dbus-glib.h>

#include <ring-util.h>
#include "test-ring.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void setup(void)
{
  g_type_init();
  (void)dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
}

static void teardown(void)
{
}

START_TEST(test_normalize_isdn)
{
  gchar *s;

  s = ring_normalize_isdn("(1) 2.3-4");
  fail_unless(s && strcmp(s, "1234") == 0);

  s = ring_normalize_isdn("1+2");
  fail_unless(s == NULL);

  s = ring_normalize_isdn("+123");
  fail_unless(s && strcmp(s, "+123") == 0);

  s = ring_normalize_isdn("+12345678901234567890");
  fail_unless(s && strcmp(s, "+12345678901234567890") == 0);

  s = ring_normalize_isdn("12A");
  fail_unless(s == NULL);

  s = ring_normalize_isdn("(.)(.)");
  fail_unless(s && strcmp(s, "") == 0);
}
END_TEST

START_TEST(test_str_starts_with)
{
  gchar *s;

  s = ring_str_starts_with_case("humppa", "HU");
  fail_if(s == NULL);
  fail_if(strcmp("mppa", s));
}
END_TEST

START_TEST(test_str_has_token)
{
  fail_if(ring_str_has_token("no-priv", "priv"));
}
END_TEST

static GHashTable *
some_fixed_properties(void)
{
  GHashTable *hash;
  gchar const *key;
  GValue *value;

  hash = g_hash_table_new(g_str_hash, g_str_equal);

  key = "uint";
  value = tp_g_value_slice_new(G_TYPE_UINT);
  g_value_set_uint(value, 13);

  g_hash_table_insert(hash, (gpointer)key, value);

  key = "string";
  value = tp_g_value_slice_new(G_TYPE_STRING);
  g_value_set_static_string(value, "value");

  g_hash_table_insert(hash, (gpointer)key, value);

  key = "boolean";
  value = tp_g_value_slice_new(G_TYPE_BOOLEAN);
  g_value_set_boolean(value, TRUE);

  g_hash_table_insert(hash, (gpointer)key, value);

  return hash;
}


START_TEST(test_properties_satisfy)
{
  gchar const * const extra[] = { "foo", "bar", "baz", NULL };

  GHashTable *fixed = some_fixed_properties();
  GHashTable *props = some_fixed_properties();

  fail_unless(ring_properties_satisfy(props, fixed, extra));

  gchar const *key;
  GValue *value;

  key = "foo";
  value = tp_g_value_slice_new(G_TYPE_STRING);
  g_value_set_static_string(value, "foo-value");
  g_hash_table_insert(props, (gpointer)key, value);

  fail_unless(ring_properties_satisfy(props, fixed, extra));
  fail_if(ring_properties_satisfy(fixed, props, extra));

  key = "uint";
  value = tp_g_value_slice_new(G_TYPE_UCHAR);
  g_value_set_uchar(value, 13);
  g_hash_table_insert(props, (gpointer)key, value);
  fail_unless(ring_properties_satisfy(props, fixed, extra));

  value = tp_g_value_slice_new(G_TYPE_UINT64);
  g_value_set_uint64(value, 13);
  g_hash_table_insert(props, (gpointer)key, value);
  fail_unless(ring_properties_satisfy(props, fixed, extra));

  value = tp_g_value_slice_new(G_TYPE_INT64);
  g_value_set_int64(value, 13);
  g_hash_table_insert(props, (gpointer)key, value);
  fail_unless(ring_properties_satisfy(props, fixed, extra));

  value = tp_g_value_slice_new(G_TYPE_INT);
  g_value_set_int(value, 13);
  g_hash_table_insert(props, (gpointer)key, value);
  fail_unless(ring_properties_satisfy(props, fixed, extra));

  value = tp_g_value_slice_new(G_TYPE_UINT);
  g_value_set_uint(value, 0xfffffffe);
}
END_TEST

static TCase *
ring_util_tcase(void)
{
  TCase *tc = tcase_create("Test for ring-util");

  tcase_add_checked_fixture(tc, setup, teardown);

  tcase_add_test(tc, test_normalize_isdn);
  tcase_add_test(tc, test_str_starts_with);
  tcase_add_test(tc, test_str_has_token);
  tcase_add_test(tc, test_properties_satisfy);

  tcase_set_timeout(tc, 5);

  return tc;
}

struct test_cases ring_tcases[] = {
  DECLARE_TEST_CASE(ring_util_tcase),
  LAST_TEST_CASE
};

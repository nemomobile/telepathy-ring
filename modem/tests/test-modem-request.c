/*
 * test-modem-request.c - Test cases for ModemRequest
 *
 * Copyright (C) 2008-2010 Nokia Corporation
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

#include "modem/request.c"

#include "modem/ofono.h"
#include "modem/service.h"

#include "test-modem.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

GMainLoop *mainloop;

static void setup(void)
{
  if (getenv("SBOX_UNAME_MACHINE"))
    setenv("DBUS_LOOPBACK", "1", 0);

  g_type_init();

  mainloop = g_main_loop_new (NULL, FALSE);
  (void)dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
}

static void teardown(void)
{
  g_main_loop_unref(mainloop), mainloop = NULL;
}

static void
weaknotify(gpointer user_data, GObject *object)
{
  *(GObject **)user_data = NULL;
}

static void
callback(gpointer _object)
{
  (void)_object;
}

static void
unref_object(gpointer _object)
{
  fail_unless(G_IS_OBJECT(_object));
  g_object_unref(_object);
}


static void
unref_proxy(gpointer _proxy)
{
  fail_unless(DBUS_IS_G_PROXY(_proxy));
  g_object_unref(_proxy);
}

static void
callback_to_timeout(DBusGProxy *proxy,
  DBusGProxyCall *call,
  void *_request)
{
  g_assert(proxy == NULL);
}

static unsigned cancel_notified;

static void
cancel_notify(gpointer _request)
{
  cancel_notified++;
  modem_request_cancel(_request);
}

START_TEST(make_call_request)
{
  GObject *object = g_object_new(G_TYPE_OBJECT, NULL);
  DBusGProxy *proxy;

  proxy = dbus_g_proxy_new_for_name (dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL),
      OFONO_BUS_NAME,
      "/",
      MODEM_OFACE_MANAGER);

  (void)callback_to_timeout;

  g_object_weak_ref(object, weaknotify, &object);
  g_object_weak_ref(G_OBJECT(proxy), weaknotify, &proxy);

  fail_if(!object);
  fail_if(!proxy);

  ModemRequest *request = _modem_request_new(
    object, proxy, G_CALLBACK(callback), &object);
  ModemRequestPrivate *priv;

  fail_if(!request);
  fail_if(!(priv = request->priv));
  fail_unless(priv->object == object);
  fail_unless(priv->callback == G_CALLBACK(callback));
  fail_unless(priv->user_data == &object);

  fail_unless(priv->proxy == proxy);
  fail_unless(priv->call == NULL);

  fail_unless(modem_request_object(request) == object);
  fail_unless(modem_request_callback(request) == callback);
  fail_unless(modem_request_user_data(request) == &object);

  modem_request_add_cancel_notify(request, cancel_notify);

  g_object_unref(object);
  g_object_unref(proxy);
  fail_if(!object);
  fail_if(!proxy);

  modem_request_cancel(request);

  fail_unless(!object);
  fail_unless(!proxy);
}
END_TEST

GError *return_error;

static void
reply_to_invalid(DBusGProxy *proxy,
  DBusGProxyCall *call,
  void *_request)
{
  fail_unless(!dbus_g_proxy_end_call(proxy, call, &return_error, G_TYPE_INVALID));
  g_main_loop_quit(mainloop);
}

START_TEST(make_request_to_invalid)
{
  GObject *object = g_object_new(G_TYPE_OBJECT, NULL);
  DBusGProxy *proxy = dbus_g_proxy_new_for_name(
    dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL),
    "com.nokia.invalid.server", "/invalid/server",
    "com.nokia.invalid.Server");

  fail_if(!object);
  fail_if(!proxy);

  g_object_weak_ref(object, weaknotify, &object);
  g_object_weak_ref(G_OBJECT(proxy), weaknotify, &proxy);

  ModemRequest *request = modem_request_begin(
    object, proxy, "invalid", reply_to_invalid,
    G_CALLBACK(callback), &object,
    G_TYPE_STRING, "kuik", G_TYPE_INVALID);
  ModemRequestPrivate *priv;

  fail_if(!request);
  fail_if(!(priv = request->priv));
  fail_unless(priv->object == object);
  fail_unless(priv->callback == G_CALLBACK(callback));
  fail_unless(priv->user_data == &object);

  fail_unless(priv->proxy == proxy);
  fail_unless(priv->call != NULL);

  fail_unless(modem_request_object(request) == object);
  fail_unless(modem_request_callback(request) == callback);
  fail_unless(modem_request_user_data(request) == &object);

  g_main_loop_run(mainloop);

  fail_unless(return_error != NULL);
  g_clear_error(&return_error);

  g_message("You will get a SIGSEGV (process:%u) if you have dbus-glib < 0.88",
      (unsigned)getpid());

  request = modem_request_begin(object, proxy,
            "invalid", reply_to_invalid,
            G_CALLBACK(callback), NULL,
            G_TYPE_STRING, "kuik", G_TYPE_INVALID);

  fail_if(!request);

  g_object_unref(object);
  fail_if(!object);

  g_object_unref(proxy);
  fail_if(!proxy);

  modem_request_cancel(request);

  fail_unless(!object);
  fail_unless(!proxy);
}
END_TEST

static gpointer notify_data;

static void notified(gpointer user_data)
{
  notify_data = user_data;
}

START_TEST(notify_in_call_request)
{
  GObject *object = g_object_new(G_TYPE_OBJECT, NULL);
  DBusGProxy *proxy;

  proxy = dbus_g_proxy_new_for_name (dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL),
      OFONO_BUS_NAME,
      "/",
      MODEM_OFACE_MANAGER);

  g_object_weak_ref(object, weaknotify, &object);
  g_object_weak_ref(G_OBJECT(proxy), weaknotify, &proxy);

  fail_if(!object);
  fail_if(!proxy);

  ModemRequest *request = _modem_request_new(
    object, proxy, G_CALLBACK(callback), object);
  ModemRequestPrivate *priv;
  gpointer data;

  fail_if(!request);
  fail_if(!(priv = request->priv));

  data = modem_request_get_data(request, "request-data");
  fail_if(data != NULL);

  GQuark quark = g_quark_from_string("data");

  fail_unless(modem_request_get_qdata(request, quark) == NULL);
  fail_unless(modem_request_steal_qdata(request, quark) == NULL);

  /* Add some data */
  modem_request_add_data(request, "data", weaknotify);
  modem_request_add_qdata(request, quark, weaknotify);
  modem_request_add_qdatas(request,
    quark, weaknotify, notified,
    quark, weaknotify, NULL,
    NULL);
  fail_unless(notify_data == weaknotify);
  notify_data = NULL;

  fail_unless(modem_request_get_data(request, "atad") == NULL);

  modem_request_add_data_full(request, "data", weaknotify, notified);
  modem_request_add_data_full(request, "data", weaknotify, NULL);
  fail_unless(notify_data == weaknotify);
  notify_data = NULL;

  fail_unless(modem_request_steal_data(request, "atad") == NULL);

  struct _ModemRequestNotify *notify = request->priv->notify;

  fail_if(notify[0].quark != GUINT_TO_POINTER(quark));
  fail_if(notify[0].data != weaknotify);

  modem_request_add_notifys(request, unref_proxy, proxy, NULL);

  notify = request->priv->notify;
  fail_if(notify[1].destroy != unref_proxy);
  fail_if(notify[1].data != proxy);

  modem_request_add_notifys(request,
    unref_object, object,
    unref_object, g_object_ref(object),
    NULL);

  notify = request->priv->notify;
  fail_if(notify[2].destroy != unref_object);
  fail_if(notify[2].data != object);
  fail_if(notify[3].destroy != unref_object);
  fail_if(notify[3].data != object);

  data = modem_request_get_data(request, "data");
  fail_if(data != weaknotify);

  modem_request_add_notifys(request, unref_proxy, g_object_ref(proxy), NULL);

  notify = request->priv->notify;
  fail_if(notify[4].destroy != unref_proxy);
  fail_if(notify[4].data != proxy);

  modem_request_add_notifys(request, unref_proxy, g_object_ref(proxy), NULL);

  notify = request->priv->notify;
  fail_if(notify[5].destroy != unref_proxy);
  fail_if(notify[5].data != proxy);

  data = modem_request_steal_data(request, "data");
  fail_if(data != weaknotify);

  data = modem_request_steal_data(request, "data");
  fail_if(data != NULL);

  modem_request_cancel(request);

  fail_unless(!object);
  fail_unless(!proxy);
}
END_TEST


static TCase *
tcase_for_modem_request(void)
{
  TCase *tc = tcase_create("Test for ModemRequest");

  tcase_add_checked_fixture(tc, setup, teardown);
  tcase_add_test(tc, make_call_request);
  tcase_add_test(tc, make_request_to_invalid);
  tcase_add_test(tc, notify_in_call_request);
  tcase_set_timeout(tc, 5);
  return tc;
}

/* ====================================================================== */
/* Test driver for glib objects */

#define CHECK_DEBUG_BASE 0
#define CHECK_DEBUG_DERIVED 0

#include "base.h"
#include "derived.h"

static int n_tags;
static char const *tags[1024];

void checktag(char const *tag)
{
  fail_if(n_tags >= G_N_ELEMENTS(tags));
  tags[n_tags++] = tag;
}

#define check_step(n)                                   \
  fail_if(strcmp(tags[i++], #n), "Expected " #n)

START_TEST(g_object_assumptions)
{
  int i = 0;

  n_tags = 0;

  GType type = TYPE_BASE;

  fail_if(g_type_class_peek(type) != NULL);

  Base *base = g_object_new(type, "base-readwrite", TRUE, NULL);

  check_step(base_class_init);
  check_step(base_constructor);
  check_step(base_init);
  check_step(base_set_property);
  check_step(base_set_property);
  check_step(base_constructed);
  check_step(base_set_property);
  fail_unless(i == n_tags);

  g_object_unref(base);

  check_step(base_dispose);
  check_step(base_finalize);
  fail_unless(i == n_tags);

  Derived *derived = g_object_new(
    TYPE_DERIVED, "derived-readwrite", TRUE, "base-readwrite", TRUE, NULL);

  check_step(derived_class_init);
  check_step(derived_constructor);
  check_step(base_constructor);
  check_step(base_init);
  check_step(derived_init);
  check_step(base_set_property);
  check_step(base_set_property);
  check_step(derived_set_property);
  check_step(derived_set_property);
  check_step(derived_constructed);
  check_step(base_constructed);

  check_step(derived_set_property); /* derived-readwrite */
  check_step(base_set_property); /* base-readwrite */
  fail_unless(i == n_tags);

  g_object_unref(derived);

  check_step(derived_dispose);
  check_step(base_dispose);
  check_step(derived_finalize);
  check_step(base_finalize);
  fail_unless(i == n_tags);
}
END_TEST

START_TEST(proxy_assumptions)
{
  DBusGProxy *proxy = dbus_g_proxy_new_for_name(
    dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL),
    "com.nokia.phone.SIM",
    "/com/nokia/phone/SIM",
    "Phone.Sim");

  g_object_run_dispose((GObject*)proxy);
  g_object_run_dispose((GObject*)proxy);

  g_object_unref(proxy);
}
END_TEST

START_TEST(free_assumptions)
{
  GPtrArray *array;
  GError *error = NULL;
  guint i;

  g_free(NULL);
  mark_point();

  fail_if(g_strdup(NULL) != NULL);

  g_strfreev(NULL);
  mark_point();
  fail_if(g_strdupv(NULL) != NULL);

  array = g_ptr_array_new();
  fail_if(g_ptr_array_free(array, TRUE) != NULL);
  array = g_ptr_array_new();
  fail_if(g_ptr_array_free(array, FALSE) != NULL);

  mark_point();

  array = g_ptr_array_new();

  for (i = 0; i < 3; i++)
    g_ptr_array_add(array, g_strdup("kuik"));
  g_ptr_array_add(array, NULL);
  g_strfreev((char **)g_ptr_array_free(array, FALSE));

  array = g_ptr_array_new();
  g_strfreev((char **)g_ptr_array_free(array, FALSE));

  array = g_ptr_array_new();
  g_ptr_array_add(array, NULL);
  g_strfreev((char **)g_ptr_array_free(array, FALSE));

  g_clear_error(&error);
}
END_TEST


static TCase *
assumptions_tcase(void)
{
  TCase *tc = tcase_create("Test Glib assumptions");

  tcase_add_checked_fixture(tc, g_type_init, NULL);

  tcase_add_test(tc, g_object_assumptions);
  tcase_add_test(tc, proxy_assumptions);
  tcase_add_test(tc, free_assumptions);

  return tc;
}

/* ====================================================================== */

struct test_cases modem_requests_tcases[] = {
  DECLARE_TEST_CASE(assumptions_tcase),
  DECLARE_TEST_CASE(tcase_for_modem_request),
  LAST_TEST_CASE
};


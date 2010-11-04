/*
 * test-sim.c - Test cases for the SIM service
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

#include "modem/sim-service.c"
#include <modem/errors.h>

#include <dbus/dbus-glib.h>

#include "test-modem.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static TCase *
modem_sim_state_tcase(void)
{
  TCase *tc = tcase_create("modem-sim-state");

  tcase_add_checked_fixture(tc, NULL, NULL);
  //tcase_add_test(tc, test_modem_sim_state_mapping);

  return tc;
}

/* ====================================================================== */

#if XXX

static GMainLoop *mainloop = NULL;

static void
modem_sim_api_init(void)
{
  g_type_init();
  (void)dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
  mainloop = g_main_loop_new (NULL, FALSE);
}

GError *reply_error;

#if 0
static void
reply_to_sim_state_request(ModemSIMService *sim,
  ModemRequest *request,
  guint state,
  GError *error,
  gpointer user_data)
{
  if (error)
    reply_error = g_error_copy(error);
  else
    g_clear_error(&reply_error);

  g_main_loop_quit(mainloop);
}
#endif

static int got_sim_connected;

static void
on_sim_connected(ModemSIMService *sim,
  gpointer user_data)
{
  got_sim_connected++;
  g_main_loop_quit(mainloop);
}

START_TEST(modem_sim_api)
{
  char const *imsi;
  char *s;
  guint state;
  GError *error;
  ModemSIMService *sim;

  fail_if(modem_sim_service_is_connected(NULL));
  fail_if(modem_sim_service_is_connecting(NULL));
  fail_if(modem_sim_get_state(NULL) != MODEM_SIM_STATE_UNKNOWN);
  fail_if(modem_sim_get_imsi(NULL) != NULL);

  sim = g_object_new(MODEM_TYPE_SIM_SERVICE, NULL);
  fail_unless(sim != NULL);
  error = NULL;

  fail_if(modem_sim_service_is_connected(sim));
  fail_if(modem_sim_service_is_connecting(sim));
  fail_if(modem_sim_get_state(sim) != MODEM_SIM_STATE_UNKNOWN);
  imsi = modem_sim_get_imsi(sim);
  fail_if(strcmp(imsi, ""));

  g_signal_connect(sim, "connected",
    G_CALLBACK(on_sim_connected),
    &got_sim_connected);

  fail_unless(modem_sim_service_connect(sim));
  fail_unless(modem_sim_service_is_connecting(sim));
  g_main_loop_run(mainloop);
  fail_unless(modem_sim_service_is_connected(sim));

  fail_unless(modem_sim_service_connect(sim));
  fail_unless(!modem_sim_service_is_connecting(sim));

  fail_if(modem_sim_get_state(sim) == MODEM_SIM_STATE_UNKNOWN);
  imsi = modem_sim_get_imsi(sim);
  fail_unless(strcmp(imsi, ""));

  g_object_get(sim, "imsi", &s, "state", &state, NULL);

  fail_unless(state == modem_sim_get_state(sim));
  fail_unless(strcmp(s, modem_sim_get_imsi(sim)) == 0);

  g_free(s);

#if 0
  request = modem_sim_request_state(
    sim, reply_to_sim_state_request, &reply_error);
#endif
  g_main_loop_run(mainloop);
  fail_if(reply_error);

  fail_unless(modem_sim_get_state(sim) != MODEM_SIM_STATE_UNKNOWN);

  got_sim_connected = 0;
  modem_sim_service_disconnect(sim);
  fail_unless(got_sim_connected);

  g_object_unref(sim);
}
END_TEST

START_TEST(modem_sim_api2)
{
  ModemSIMService *sim;

  sim = g_object_new(MODEM_TYPE_SIM_SERVICE, NULL);
  fail_unless(sim != NULL);
  modem_sim_service_connect(sim);
  g_object_unref(sim);

  sim = g_object_new(MODEM_TYPE_SIM_SERVICE, NULL);
  fail_unless(sim != NULL);
  modem_sim_service_connect(sim);
  modem_sim_service_disconnect(sim);
  fail_if(modem_sim_service_is_connected(sim));
  fail_if(modem_sim_service_is_connecting(sim));
  modem_sim_service_connect(sim);
  fail_if(modem_sim_service_is_connected(sim));
  fail_if(modem_sim_service_is_connecting(sim));
  modem_sim_service_disconnect(sim);
  g_object_unref(sim);

  sim = g_object_new(MODEM_TYPE_SIM_SERVICE, NULL);
  fail_unless(sim != NULL);
  modem_sim_service_connect(sim);
  g_object_run_dispose(G_OBJECT(sim));
  fail_if(modem_sim_service_is_connected(sim));
  fail_if(modem_sim_service_is_connecting(sim));
  modem_sim_service_connect(sim);
  g_object_unref(sim);
}
END_TEST

START_TEST(modem_sim_requests)
{
  ModemSIMService *sim;

  sim = g_object_new(MODEM_TYPE_SIM_SERVICE, NULL);

  g_signal_connect(sim, "connected",
    G_CALLBACK(on_sim_connected),
    &got_sim_connected);
  fail_if(0); 
  /* XXX/KV: convert to base class methods */
#if 0
  g_object_unref(sim->priv->proxy);
  sim->priv->proxy = dbus_g_proxy_new_for_name(
    dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL),
    "invalid.server", "/invalid/server", "invalid.Server");
#endif

  fail_unless(modem_sim_service_connect(sim));
  fail_unless(modem_sim_service_is_connecting(sim));
  g_main_loop_run(mainloop);
  fail_if(modem_sim_service_is_connected(sim));

  g_object_unref(sim);
}
END_TEST

#endif

static TCase *
modem_sim_api_tcase(void)
{
  TCase *tc = tcase_create("modem-sim");

#if XXX

  tcase_add_checked_fixture(tc, modem_sim_api_init, NULL);

  {
    tcase_add_test(tc, modem_sim_api);
    tcase_add_test(tc, modem_sim_api2);
    tcase_add_test(tc, modem_sim_requests);
  }

  tcase_set_timeout(tc, 40);

#endif

  return tc;
}

struct test_cases modem_sim_tcases[] = {
  DECLARE_TEST_CASE(modem_sim_state_tcase),
  DECLARE_TEST_CASE_OFF_BY_DEFAULT(modem_sim_api_tcase),
  LAST_TEST_CASE
};

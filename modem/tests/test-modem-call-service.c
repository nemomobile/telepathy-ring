/*
 * test-modem-call-service.c - Tests cases for the call service
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

#include <modem/call.h>
#include <modem/ofono.h>

#include "test-modem.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Valid digits are 0123456789abc*# */
START_TEST(test_modem_call_validate_address)
{
  GError *error = NULL;

  fail_if(modem_call_is_valid_address(NULL));
  fail_if(modem_call_is_valid_address(""));
  fail_if(modem_call_validate_address("", &error));
  fail_if(error == NULL);
  g_error_free(error);

  fail_if(modem_call_validate_address("", NULL));
  fail_if(modem_call_validate_address(" ", NULL));
  fail_if(modem_call_validate_address("w#123", NULL));

  fail_if(!modem_call_is_valid_address("1"));
  fail_if(!modem_call_is_valid_address("+1"));

  /* Internation prefix */
  fail_if(modem_call_is_valid_address("+"));
  fail_if(modem_call_is_valid_address("+1+"));

  /* Almost too long */
  fail_if(!modem_call_is_valid_address("+35899123456789012345"));
  fail_if(!modem_call_is_valid_address("*31#+35899123456789012345"));
  fail_if(!modem_call_is_valid_address("#31#+35899123456789012345"));

  /* Too long */
  fail_if(modem_call_is_valid_address("+358991234567890123456"));

  /* Dialstrings */
  fail_if(!modem_call_is_valid_address("+358718008000w123#*abc"));
  fail_if(!modem_call_is_valid_address("+358718008000abc#*w123#*abc"));
  fail_if(modem_call_is_valid_address("+358718008000abc#*w123#*abcd"));
  fail_if(!modem_call_is_valid_address("+358718008000p123"));
  fail_if(modem_call_is_valid_address("+358718008000p"));

  /* Prefixes only */
  fail_if(modem_call_is_valid_address("*31#+"));

  /* Invalid prefixes */
  fail_if(modem_call_is_valid_address("*31##31#+1"));

  /* Alphanumeric */
  fail_if(modem_call_is_valid_address("tilulilu"));

  fail_if(!modem_call_is_valid_address("abc#*cba"));

  /* SOS URNs */
  fail_if(!modem_call_is_valid_address("urn:service:sos"));
  fail_if(!modem_call_is_valid_address("URN:seRVICE:SoS"));
  fail_if(modem_call_is_valid_address("urn:service:sossoo"));
  fail_if(!modem_call_is_valid_address("urn:service:sos.soo"));
}
END_TEST

static TCase *
tcase_for_modem_call_address_validator(void)
{
  TCase *tc = tcase_create("Test for modem call address validation");

  tcase_add_checked_fixture(tc, g_type_init, NULL);

  tcase_add_test(tc, test_modem_call_validate_address);

  tcase_set_timeout(tc, 5);
  return tc;
}

/* Speaking Clock in NTN */
char const *destination = /*"+3584544"*/ "99901";

static GMainLoop *mainloop = NULL;

static void setup(void)
{
  if (getenv("TEST_MODEM_CALL_DESTINATION"))
    destination = getenv("TEST_MODEM_CALL_DESTINATION");

  g_type_init();
  (void)dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
  mainloop = g_main_loop_new (NULL, FALSE);
}

static void teardown(void)
{
  g_main_loop_unref(mainloop), mainloop = NULL;
}

static GError *reply_error = NULL;
static ModemCall *created;

static void
reply_to_dial_request(ModemCallService *call_service,
  ModemRequest *request,
  ModemCall *ci,
  GError *error,
  gpointer user_data)
{
  if (error)
    reply_error = g_error_copy(error);
  else
    g_clear_error(&reply_error);

  created = ci;

  g_main_loop_quit(mainloop);
}


static void
reply_to_call_service_request(ModemCallService *call_service,
  ModemRequest *request,
  GError *error,
  gpointer user_data)
{
  if (error)
    reply_error = g_error_copy(error);
  else
    g_clear_error(&reply_error);

  g_main_loop_quit(mainloop);
}

static void
reply_to_call_request(ModemCall *call,
  ModemRequest *request,
  GError *error,
  gpointer user_data)
{
  if (error)
    reply_error = g_error_copy(error);
  else
    g_clear_error(&reply_error);

  g_main_loop_quit(mainloop);
}

static int got_call_connected;

static void on_call_connected(ModemCallService *call_service,
  gpointer user_data)
{
  got_call_connected++;
  g_main_loop_quit(mainloop);
}

#if nomore
static void on_emergency_numbers(ModemCallService *call_service,
  char **strv,
  gpointer user_data)
{
  char ***return_numbers = user_data;

  *return_numbers = g_strdupv(strv);

  g_main_loop_quit(mainloop);
}
#endif

static int got_call_state;
ModemCall *state;

static void on_call_state(ModemCallService *call_service,
  gpointer user_data)
{
  got_call_state++;
  g_main_loop_quit(mainloop);
}

START_TEST(modem_call_api)
{
  GError *error;
  ModemCallService *call_service;
  ModemCall **calls;
  ModemRequest *request;
  char **strv;
  char *emergency_before, *emergency_after;
#if nomore
  char **updated;
  char *add[] = { "012", "987", NULL };
#endif
  guint i;

  fail_if(modem_call_service_is_connected(NULL));
  fail_if(modem_call_service_is_connecting(NULL));

  call_service = g_object_new(MODEM_TYPE_CALL_SERVICE, NULL);
  fail_unless(call_service != NULL);
  error = NULL;

  fail_if(modem_call_service_is_connected(call_service));
  fail_if(modem_call_service_is_connecting(call_service));

  strv = (gpointer)modem_call_get_emergency_numbers(call_service);
  fail_unless(strv != NULL);
  emergency_before = g_strjoinv(" ", strv);
  fail_unless(strcmp("112 911 118 119 000 110 08 999", emergency_before) == 0);

  g_signal_connect(call_service, "connected",
    G_CALLBACK(on_call_connected),
    &got_call_connected);

  fail_unless(modem_call_service_connect(call_service, "/"));
  fail_unless(modem_call_service_is_connecting(call_service));
  g_main_loop_run(mainloop);
  fail_unless(modem_call_service_is_connected(call_service));

  strv = (gpointer)modem_call_get_emergency_numbers(call_service);
  fail_unless(strv != NULL);
  emergency_after = g_strjoinv(" ", strv);
  fail_unless(strcmp(emergency_before, emergency_after) != 0);
  g_free(emergency_before), g_free(emergency_after);

  g_clear_error(&reply_error);

#if nomore
  modem_call_remove_emergency_numbers(call_service, reply_to_call_service_request, &reply_error);
  g_main_loop_run(mainloop);
  g_clear_error(&reply_error);

  strv = (gpointer)modem_call_get_emergency_numbers(call_service);
  fail_unless(strv != NULL);
  emergency_before = g_strjoinv(" ", strv);


  updated = NULL;
  g_signal_connect(call_service, "emergency-numbers-changed",
    G_CALLBACK(on_emergency_numbers),
    &updated);

  modem_call_add_emergency_numbers(call_service, (gpointer)add, NULL, NULL);

  g_main_loop_run(mainloop);
  fail_unless(updated != NULL);
  emergency_after = g_strjoinv(" ", updated);
  fail_unless(strcmp(emergency_before, emergency_after));
  g_free(emergency_before);
  emergency_before = emergency_after;
  g_strfreev(updated), updated = NULL;

  modem_call_remove_emergency_numbers(call_service, NULL, NULL);

  g_main_loop_run(mainloop);
  fail_unless(updated != NULL);
  emergency_after = g_strjoinv(" ", updated);
  fail_unless(strcmp(emergency_before, emergency_after));
  g_free(emergency_before), g_free(emergency_after);

  g_strfreev(updated), updated = NULL;
#endif

  calls = modem_call_service_get_calls(call_service);
  fail_unless(calls != NULL);

  for (i = 0; calls[i]; i++) {
    g_signal_connect(calls[i], "state", G_CALLBACK(on_call_state), NULL);
  }

  for (i = 0; calls[i]; i++) {
    if (modem_call_get_state(calls[i])) {
      request = modem_call_request_release(calls[i],
                reply_to_call_request,
                &reply_error);
      g_main_loop_run(mainloop);
      fail_unless(reply_error == NULL);
      g_main_loop_run(mainloop);
    }
  }

  for (i = 0; calls[i]; i++) {
    if (modem_call_get_state(calls[i])) {
      g_main_loop_run(mainloop);
    }
  }

  g_free(calls), calls = NULL;

  fail_unless(modem_call_service_connect(call_service, "/"));
  fail_unless(!modem_call_service_is_connecting(call_service));

  request = modem_call_request_conference(
    call_service, reply_to_call_service_request, &reply_error);
  g_main_loop_run(mainloop);
  fail_unless(reply_error != NULL);
  g_clear_error(&reply_error);

  request = modem_call_request_dial(
    call_service, "tilulilu", 0, reply_to_dial_request, &reply_error);
  g_main_loop_run(mainloop);
  fail_unless(reply_error != NULL);
  g_clear_error(&reply_error);

  request = modem_call_request_dial(
    call_service, "tilulilu", 0, reply_to_dial_request, &reply_error);
  modem_request_cancel(request);

  request = modem_call_request_dial(
    call_service, destination, 0, reply_to_dial_request, &reply_error);
  g_main_loop_run(mainloop);

  if (!reply_error) {
    fail_unless(created != NULL);

    while (modem_call_get_state(created) < MODEM_CALL_STATE_ACTIVE)
      g_main_loop_run(mainloop);

    request = modem_call_request_release(created, reply_to_call_request, &reply_error);
    g_main_loop_run(mainloop);
    fail_unless(reply_error == NULL);

    while (modem_call_get_state(created) != 0)
      g_main_loop_run(mainloop);

    created = NULL;
  }

  got_call_connected = 0;
  modem_call_service_disconnect(call_service);
  fail_unless(got_call_connected);

  g_object_unref(call_service);
}
END_TEST

START_TEST(modem_call_api2)
{
  ModemCallService *call_service;

  call_service = g_object_new(MODEM_TYPE_CALL_SERVICE, NULL);
  fail_unless(call_service != NULL);
  fail_unless(modem_call_service_connect(call_service, "/"));
  modem_call_service_disconnect(call_service);
  fail_if(modem_call_service_is_connected(call_service));
  fail_if(modem_call_service_is_connecting(call_service));
  g_object_unref(call_service);

  call_service = g_object_new(MODEM_TYPE_CALL_SERVICE, NULL);
  fail_unless(call_service != NULL);
  fail_unless(modem_call_service_connect(call_service, "/"));
  g_object_run_dispose(G_OBJECT(call_service));
  fail_if(modem_call_service_is_connected(call_service));
  fail_if(modem_call_service_is_connecting(call_service));
  g_object_unref(call_service);

  call_service = g_object_new(MODEM_TYPE_CALL_SERVICE, NULL);
  fail_unless(call_service != NULL);
  g_object_run_dispose(G_OBJECT(call_service));
  fail_if(modem_call_service_connect(call_service, "/"));
  fail_if(modem_call_service_is_connected(call_service));
  fail_if(modem_call_service_is_connecting(call_service));
  g_object_unref(call_service);
}
END_TEST

#include "modem/call-service.c"

START_TEST(modem_call_internal)
{
  ModemCallService *call_service;

  call_service = g_object_new(MODEM_TYPE_CALL_SERVICE, NULL);
  fail_unless(call_service != NULL);

  fail_if(modem_call_service_is_connected(call_service));
  fail_if(modem_call_service_is_connecting(call_service));

  g_signal_connect(call_service, "connected",
    G_CALLBACK(on_call_connected),
    &got_call_connected);

  fail_unless(modem_call_service_connect(call_service, "/"));
  fail_unless(modem_call_service_is_connecting(call_service));
  g_main_loop_run(mainloop);
  fail_unless(modem_call_service_is_connected(call_service));

  got_call_connected = 0;
  modem_call_service_disconnect(call_service);
  fail_unless(got_call_connected != 0);

  g_object_unref(call_service);
}
END_TEST

static TCase *
tcase_for_modem_call_service(void)
{
  TCase *tc = tcase_create("Test for ModemCallService");

  tcase_add_checked_fixture(tc, setup, teardown);

  tcase_add_test(tc, modem_call_api);
  tcase_add_test(tc, modem_call_api2);
  tcase_add_test(tc, modem_call_internal);

  tcase_set_timeout(tc, 60);
  return tc;
}


/* ====================================================================== */

struct test_cases modem_call_service_tcases[] = {
  DECLARE_TEST_CASE(tcase_for_modem_call_address_validator),
  DECLARE_TEST_CASE_OFF_BY_DEFAULT(tcase_for_modem_call_service),
  LAST_TEST_CASE
};


/*
 * test-modem-call.c - Tests cases for call instances
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

#include <modem/ofono.h>
#include <modem/call.h>
#include <modem/errors.h>
#include <modem/service.h>

#include <dbus/dbus-glib.h>

#include "test-modem.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void setup(void)
{
  g_type_init();
  (void)dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
  modem_service();
}

static void teardown(void)
{
}


START_TEST(modem_call_properties)
{
  ModemCall *ci = g_object_new(MODEM_TYPE_CALL,
      "object-path", "/path",
      "call-service", NULL,
      NULL);

  ModemCallService *client = (gpointer)-1;
  char *remote = (gpointer)-1;
  char *emergency = (gpointer)-1;
  unsigned state = (unsigned)-1;
  gboolean originating = (gboolean)-1, terminating = (gboolean)-1;
  gboolean onhold = (gboolean)-1, member = (gboolean)-1;

  g_object_get(ci,
      "call-service", &client,
      "remote", &remote,
      "state", &state,
      "originating", &originating,
      "terminating", &terminating,
      "emergency", &emergency,
      "onhold", &onhold,
      "multiparty", &member,
    NULL);

  fail_if(client == (gpointer)-1);
  fail_if(remote == (gpointer)-1);
  fail_if(state == (unsigned)-1);
  fail_if(originating == (gboolean)-1);
  fail_if(terminating == (gboolean)-1);
  fail_if(emergency == (gpointer)-1);
  fail_if(onhold == (gboolean)-1);
  fail_if(member == (gboolean)-1);

  fail_unless(client == NULL);
  fail_unless(remote == NULL);
  fail_unless(state == MODEM_CALL_STATE_INVALID);
  fail_unless(originating == FALSE);
  fail_unless(terminating == FALSE);
  fail_unless(emergency == NULL);
  fail_unless(onhold == FALSE);
  fail_unless(member == FALSE);

  g_free(remote);

#if XXX
  g_object_set(ci,
    "remote", "99001",
    "state", MODEM_CALL_STATE_ACTIVE,
    "originating", 1,
    "terminating", 0,
    "emergency", "urn:service:sos",
    "onhold", 1,
    "multiparty", 1,
    NULL);

  g_object_get(ci,
    "remote", &remote,
    "state", &state,
    "originating", &originating,
    "terminating", &terminating,
    "emergency", &emergency,
    "onhold", &onhold,
    "multiparty", &member,
    NULL);

  fail_unless(remote && strcmp(remote, "99001") == 0);
  fail_unless(state == MODEM_CALL_STATE_ACTIVE);
  fail_unless(originating == TRUE);
  fail_unless(terminating == FALSE);
  fail_unless(emergency && strcmp(emergency, "urn:service:sos") == 0);
  fail_unless(onhold == TRUE);
  fail_unless(member == TRUE);

  g_free(remote);
  g_free(emergency);
#endif
}
END_TEST

static TCase *
modem_call_tcase(void)
{
  TCase *tc = tcase_create("Test for ModemCall");

  tcase_add_checked_fixture(tc, setup, teardown);

  tcase_add_test(tc, modem_call_properties);

  tcase_set_timeout(tc, 5);
  return tc;
}

/* ====================================================================== */

START_TEST(test_modem_error_fix)
{
  GError *error = NULL, *unfixed;

  modem_error_fix(&error);
  fail_unless(error == NULL);

  error = unfixed =
    g_error_new(DBUS_GERROR, DBUS_GERROR_REMOTE_EXCEPTION,
      "message%c%s.%s", '\0',
      "org.ofono.Bogus.Error",
      "Generic");

  modem_error_fix(&error);
  fail_unless(error == unfixed);
  g_clear_error(&error);

  error = unfixed =
    g_error_new(DBUS_GERROR, DBUS_GERROR_REMOTE_EXCEPTION,
      "message%c%s.%s", '\0',
      "org.ofono.Error",
      "NoSuchError");
  modem_error_fix(&error);
  fail_unless(error == unfixed);

  g_clear_error(&error);

  error = unfixed =
    g_error_new(DBUS_GERROR, DBUS_GERROR_REMOTE_EXCEPTION,
      "message%c%s.%s", '\0',
      "org.ofono.Error",
      "Failed");
  modem_error_fix(&error);
  fail_unless(error != unfixed);

  g_clear_error(&error);
}
END_TEST


START_TEST(test_call_error_conversion)
{
  GError *error, *dbus, *fixed;

  guint causetype, cause;
  char ebuffer[16];

  for (causetype = 1; causetype <= 4; causetype++) {
    for (cause = 0; cause < 128; cause++) {
      error = modem_call_new_error(causetype, cause, "test");
      fail_unless(error != NULL);
      fail_if(modem_error_name(error, ebuffer, sizeof ebuffer) == ebuffer);
      dbus = g_error_new(DBUS_GERROR, DBUS_GERROR_REMOTE_EXCEPTION,
             "%s%c%s.%s",
             error->message, '\0',
             modem_error_domain_prefix(error->domain),
             modem_error_name(error, ebuffer, sizeof ebuffer));
      fail_unless(dbus != NULL);
      fixed = dbus;
      modem_error_fix(&fixed);
      fail_if(fixed == dbus);
      fail_if(fixed->domain != error->domain);
      fail_if(fixed->code != error->code);

      g_clear_error(&error);
      g_clear_error(&fixed);
    }
  }
}
END_TEST


START_TEST(test_modem_error_name)
{
  GError *error;
  char ebuffer[16];
  char const *name;

  name = modem_error_name(NULL, ebuffer, sizeof ebuffer);
  fail_unless(name == ebuffer);

  error = g_error_new(MODEM_CALL_ERRORS, 1024, "message");
  name = modem_error_name(error, ebuffer, sizeof ebuffer);
  fail_unless(name == ebuffer);

  g_clear_error(&error);
}
END_TEST

#define tst(x) {                                                        \
    error.code = x;                                                     \
    fail_if(modem_error_name(&error, buffer, sizeof buffer) == buffer); \
    remote = g_error_new(DBUS_GERROR, DBUS_GERROR_REMOTE_EXCEPTION,     \
             "message%c%s.%s", '\0',                                    \
             modem_error_domain_prefix(error.domain),                   \
             modem_error_name(&error, NULL, 0));                        \
    fail_if(remote == NULL);                                            \
    fixed = remote;                                                     \
    modem_error_fix(&fixed);                                            \
    fail_if(remote == fixed, "fixing " #x);                             \
    fail_if(fixed->domain != error.domain);                             \
    fail_if(fixed->code != x);                                          \
  } while(0)


START_TEST(test_modem_call_error_prefix)
{
  GError error = { MODEM_CALL_ERRORS, 0, "error" };
  GError *remote, *fixed;
  char buffer[16];

  tst(MODEM_CALL_ERROR_NO_ERROR);
  tst(MODEM_CALL_ERROR_NO_CALL);
  tst(MODEM_CALL_ERROR_RELEASE_BY_USER);
  tst(MODEM_CALL_ERROR_BUSY_USER_REQUEST);
  tst(MODEM_CALL_ERROR_ERROR_REQUEST);
  tst(MODEM_CALL_ERROR_CALL_ACTIVE);
  tst(MODEM_CALL_ERROR_NO_CALL_ACTIVE);
  tst(MODEM_CALL_ERROR_INVALID_CALL_MODE);
  tst(MODEM_CALL_ERROR_TOO_LONG_ADDRESS);
  tst(MODEM_CALL_ERROR_INVALID_ADDRESS);
  tst(MODEM_CALL_ERROR_EMERGENCY);
  tst(MODEM_CALL_ERROR_NO_SERVICE);
  tst(MODEM_CALL_ERROR_NO_COVERAGE);
  tst(MODEM_CALL_ERROR_CODE_REQUIRED);
  tst(MODEM_CALL_ERROR_NOT_ALLOWED);
  tst(MODEM_CALL_ERROR_DTMF_ERROR);
  tst(MODEM_CALL_ERROR_CHANNEL_LOSS);
  tst(MODEM_CALL_ERROR_FDN_NOT_OK);
  tst(MODEM_CALL_ERROR_BLACKLIST_BLOCKED);
  tst(MODEM_CALL_ERROR_BLACKLIST_DELAYED);
  tst(MODEM_CALL_ERROR_EMERGENCY_FAILURE);
  tst(MODEM_CALL_ERROR_NO_SIM);
  tst(MODEM_CALL_ERROR_DTMF_SEND_ONGOING);
  tst(MODEM_CALL_ERROR_CS_INACTIVE);
  tst(MODEM_CALL_ERROR_NOT_READY);
  tst(MODEM_CALL_ERROR_INCOMPATIBLE_DEST);

  fail_if(modem_error_domain_prefix(error.domain) == NULL);
  fail_if(strcmp(MODEM_CALL_ERROR_PREFIX, modem_error_domain_prefix(error.domain)));
}
END_TEST


START_TEST(test_modem_call_error)
{
  GError *error;

  int causes[] = {
    MODEM_CALL_ERROR_NO_CALL,
    MODEM_CALL_ERROR_RELEASE_BY_USER,
    MODEM_CALL_ERROR_BUSY_USER_REQUEST,
    MODEM_CALL_ERROR_ERROR_REQUEST,
    MODEM_CALL_ERROR_CALL_ACTIVE,
    MODEM_CALL_ERROR_NO_CALL_ACTIVE,
    MODEM_CALL_ERROR_INVALID_CALL_MODE,
    MODEM_CALL_ERROR_TOO_LONG_ADDRESS,
    MODEM_CALL_ERROR_INVALID_ADDRESS,
    MODEM_CALL_ERROR_EMERGENCY,
    MODEM_CALL_ERROR_NO_SERVICE,
    MODEM_CALL_ERROR_NO_COVERAGE,
    MODEM_CALL_ERROR_CODE_REQUIRED,
    MODEM_CALL_ERROR_NOT_ALLOWED,
    MODEM_CALL_ERROR_DTMF_ERROR,
    MODEM_CALL_ERROR_CHANNEL_LOSS,
    MODEM_CALL_ERROR_FDN_NOT_OK,
    MODEM_CALL_ERROR_BLACKLIST_BLOCKED,
    MODEM_CALL_ERROR_BLACKLIST_DELAYED,
    MODEM_CALL_ERROR_EMERGENCY_FAILURE,
    MODEM_CALL_ERROR_NO_SIM,
    MODEM_CALL_ERROR_DTMF_SEND_ONGOING,
    MODEM_CALL_ERROR_CS_INACTIVE,
    MODEM_CALL_ERROR_NOT_READY,
    MODEM_CALL_ERROR_INCOMPATIBLE_DEST,
    MODEM_CALL_ERROR_NO_ERROR,
  };

  guint i = 0;

  for (i = 0; causes[i]; i++) {
    error = modem_call_new_error(MODEM_CALL_CAUSE_TYPE_LOCAL, causes[i], "kuik");
    fail_if(error == NULL);
    fail_if(error->domain != MODEM_CALL_ERRORS);
    fail_if(error->code != causes[i]);
    fail_if(error->message == NULL || strlen(error->message) < 1);
    fail_if(strncmp(error->message, "kuik: ", 6));
    g_clear_error(&error);
  }

  /* Try unknown cause type */
  error = modem_call_new_error(4, 1, "kuik");
  fail_if(error == NULL);
  fail_if(error->domain != MODEM_CALL_ERRORS);
  fail_if(error->code != MODEM_CALL_ERROR_GENERIC);
  fail_if(error->message == NULL || strlen(error->message) < 1);
  fail_if(strncmp(error->message, "kuik: ", 6));
  g_clear_error(&error);

  /* Try unknown cause code (and no prefix) */
  error = modem_call_new_error(MODEM_CALL_CAUSE_TYPE_LOCAL, 255, NULL);
  fail_if(error == NULL);
  fail_if(error->domain != MODEM_CALL_ERRORS);
  fail_if(error->code != MODEM_CALL_ERROR_GENERIC);
  fail_if(error->message == NULL || strlen(error->message) < 1);
  g_clear_error(&error);
}
END_TEST

START_TEST(test_modem_call_net_error_prefix)
{
  GError error = { MODEM_CALL_NET_ERRORS, 0, "error" };
  GError *remote, *fixed;
  char buffer[16];

  /* Try to re-register MODEM_CALL_NET_ERRORS */
  modem_error_register_mapping(MODEM_CALL_NET_ERRORS,
    MODEM_CALL_NET_ERROR_PREFIX,
    MODEM_TYPE_CALL_NET_ERROR);

  fail_if(modem_error_domain_prefix(error.domain) == NULL);
  fail_if(strcmp(MODEM_CALL_NET_ERROR_PREFIX,
      modem_error_domain_prefix(error.domain)));

  tst(MODEM_CALL_NET_ERROR_UNASSIGNED_NUMBER);
  tst(MODEM_CALL_NET_ERROR_NO_ROUTE);
  tst(MODEM_CALL_NET_ERROR_CH_UNACCEPTABLE);
  tst(MODEM_CALL_NET_ERROR_OPER_BARRING);
  tst(MODEM_CALL_NET_ERROR_NORMAL);
  tst(MODEM_CALL_NET_ERROR_USER_BUSY);
  tst(MODEM_CALL_NET_ERROR_NO_USER_RESPONSE);
  tst(MODEM_CALL_NET_ERROR_ALERT_NO_ANSWER);
  tst(MODEM_CALL_NET_ERROR_CALL_REJECTED);
  tst(MODEM_CALL_NET_ERROR_NUMBER_CHANGED);
  tst(MODEM_CALL_NET_ERROR_NON_SELECT_CLEAR);
  tst(MODEM_CALL_NET_ERROR_DEST_OUT_OF_ORDER);
  tst(MODEM_CALL_NET_ERROR_INVALID_NUMBER);
  tst(MODEM_CALL_NET_ERROR_FACILITY_REJECTED);
  tst(MODEM_CALL_NET_ERROR_RESP_TO_STATUS);
  tst(MODEM_CALL_NET_ERROR_NORMAL_UNSPECIFIED);
  tst(MODEM_CALL_NET_ERROR_NO_CHANNEL);
  tst(MODEM_CALL_NET_ERROR_NETW_OUT_OF_ORDER);
  tst(MODEM_CALL_NET_ERROR_TEMPORARY_FAILURE);
  tst(MODEM_CALL_NET_ERROR_CONGESTION);
  tst(MODEM_CALL_NET_ERROR_ACCESS_INFO_DISC);
  tst(MODEM_CALL_NET_ERROR_CHANNEL_NA);
  tst(MODEM_CALL_NET_ERROR_RESOURCES_NA);
  tst(MODEM_CALL_NET_ERROR_QOS_NA);
  tst(MODEM_CALL_NET_ERROR_FACILITY_UNSUBS);
  tst(MODEM_CALL_NET_ERROR_COMING_BARRED_CUG);
  tst(MODEM_CALL_NET_ERROR_BC_UNAUTHORIZED);
  tst(MODEM_CALL_NET_ERROR_BC_NA);
  tst(MODEM_CALL_NET_ERROR_SERVICE_NA);
  tst(MODEM_CALL_NET_ERROR_BEARER_NOT_IMPL);
  tst(MODEM_CALL_NET_ERROR_ACM_MAX);
  tst(MODEM_CALL_NET_ERROR_FACILITY_NOT_IMPL);
  tst(MODEM_CALL_NET_ERROR_ONLY_RDI_BC);
  tst(MODEM_CALL_NET_ERROR_SERVICE_NOT_IMPL);
  tst(MODEM_CALL_NET_ERROR_INVALID_TI);
  tst(MODEM_CALL_NET_ERROR_NOT_IN_CUG);
  tst(MODEM_CALL_NET_ERROR_INCOMPATIBLE_DEST);
  tst(MODEM_CALL_NET_ERROR_INV_TRANS_NET_SEL);
  tst(MODEM_CALL_NET_ERROR_SEMANTICAL_ERR);
  tst(MODEM_CALL_NET_ERROR_INVALID_MANDATORY);
  tst(MODEM_CALL_NET_ERROR_MSG_TYPE_INEXIST);
  tst(MODEM_CALL_NET_ERROR_MSG_TYPE_INCOMPAT);
  tst(MODEM_CALL_NET_ERROR_IE_NON_EXISTENT);
  tst(MODEM_CALL_NET_ERROR_COND_IE_ERROR);
  tst(MODEM_CALL_NET_ERROR_MSG_INCOMPATIBLE);
  tst(MODEM_CALL_NET_ERROR_TIMER_EXPIRY);
  tst(MODEM_CALL_NET_ERROR_PROTOCOL_ERROR);
  tst(MODEM_CALL_NET_ERROR_INTERWORKING);

}
END_TEST

START_TEST(test_modem_call_net_error)
{
  GError *error;

  int causes[] = {
    MODEM_CALL_NET_ERROR_UNASSIGNED_NUMBER,
    MODEM_CALL_NET_ERROR_NO_ROUTE,
    MODEM_CALL_NET_ERROR_CH_UNACCEPTABLE,
    MODEM_CALL_NET_ERROR_OPER_BARRING,
    MODEM_CALL_NET_ERROR_NORMAL,
    MODEM_CALL_NET_ERROR_USER_BUSY,
    MODEM_CALL_NET_ERROR_NO_USER_RESPONSE,
    MODEM_CALL_NET_ERROR_ALERT_NO_ANSWER,
    MODEM_CALL_NET_ERROR_CALL_REJECTED,
    MODEM_CALL_NET_ERROR_NUMBER_CHANGED,
    MODEM_CALL_NET_ERROR_NON_SELECT_CLEAR,
    MODEM_CALL_NET_ERROR_DEST_OUT_OF_ORDER,
    MODEM_CALL_NET_ERROR_INVALID_NUMBER,
    MODEM_CALL_NET_ERROR_FACILITY_REJECTED,
    MODEM_CALL_NET_ERROR_RESP_TO_STATUS,
    MODEM_CALL_NET_ERROR_NORMAL_UNSPECIFIED,
    MODEM_CALL_NET_ERROR_NO_CHANNEL,
    MODEM_CALL_NET_ERROR_NETW_OUT_OF_ORDER,
    MODEM_CALL_NET_ERROR_TEMPORARY_FAILURE,
    MODEM_CALL_NET_ERROR_CONGESTION,
    MODEM_CALL_NET_ERROR_ACCESS_INFO_DISC,
    MODEM_CALL_NET_ERROR_CHANNEL_NA,
    MODEM_CALL_NET_ERROR_RESOURCES_NA,
    MODEM_CALL_NET_ERROR_QOS_NA,
    MODEM_CALL_NET_ERROR_FACILITY_UNSUBS,
    MODEM_CALL_NET_ERROR_COMING_BARRED_CUG,
    MODEM_CALL_NET_ERROR_BC_UNAUTHORIZED,
    MODEM_CALL_NET_ERROR_BC_NA,
    MODEM_CALL_NET_ERROR_SERVICE_NA,
    MODEM_CALL_NET_ERROR_BEARER_NOT_IMPL,
    MODEM_CALL_NET_ERROR_ACM_MAX,
    MODEM_CALL_NET_ERROR_FACILITY_NOT_IMPL,
    MODEM_CALL_NET_ERROR_ONLY_RDI_BC,
    MODEM_CALL_NET_ERROR_SERVICE_NOT_IMPL,
    MODEM_CALL_NET_ERROR_INVALID_TI,
    MODEM_CALL_NET_ERROR_NOT_IN_CUG,
    MODEM_CALL_NET_ERROR_INCOMPATIBLE_DEST,
    MODEM_CALL_NET_ERROR_INV_TRANS_NET_SEL,
    MODEM_CALL_NET_ERROR_SEMANTICAL_ERR,
    MODEM_CALL_NET_ERROR_INVALID_MANDATORY,
    MODEM_CALL_NET_ERROR_MSG_TYPE_INEXIST,
    MODEM_CALL_NET_ERROR_MSG_TYPE_INCOMPAT,
    MODEM_CALL_NET_ERROR_IE_NON_EXISTENT,
    MODEM_CALL_NET_ERROR_COND_IE_ERROR,
    MODEM_CALL_NET_ERROR_MSG_INCOMPATIBLE,
    MODEM_CALL_NET_ERROR_TIMER_EXPIRY,
    MODEM_CALL_NET_ERROR_PROTOCOL_ERROR,
    MODEM_CALL_NET_ERROR_INTERWORKING,
    0
  };

  guint i;

  for (i = 0; causes[i]; i++) {
    error = modem_call_new_error(MODEM_CALL_CAUSE_TYPE_NETWORK, causes[i], "kuik");
    fail_if(error == NULL);
    fail_if(error->domain != MODEM_CALL_NET_ERRORS);
    fail_if(error->code != causes[i]);
    fail_if(error->message == NULL || strlen(error->message) < 1);
    fail_if(strncmp(error->message, "kuik: ", 6));
    g_clear_error(&error);
  }

  /* Try unknown cause code */
  error = modem_call_new_error(MODEM_CALL_CAUSE_TYPE_NETWORK, 126, "kuik");
  fail_if(error == NULL);
  fail_if(error->domain != MODEM_CALL_NET_ERRORS);
  fail_if(error->code != MODEM_CALL_NET_ERROR_INTERWORKING);
  fail_if(error->message == NULL || strlen(error->message) < 1);
  fail_if(strncmp(error->message, "kuik: ", 6));
  g_clear_error(&error);
}
END_TEST


START_TEST(test_modem_call_state)
{
  guint i;
  for (i = 0; i <= MODEM_CALL_STATE_DISCONNECTED; i++) {
    fail_unless(strcmp(modem_call_get_state_name(i), "UNKNOWN"));
  }
  fail_if(strcmp(modem_call_get_state_name(i), "UNKNOWN"));
}
END_TEST


static TCase *
modem_call_error_tcase(void)
{
  TCase *tc = tcase_create("Test for ModemCallError");

  tcase_add_checked_fixture(tc, setup, teardown);

  tcase_add_test(tc, test_modem_error_fix);
  tcase_add_test(tc, test_call_error_conversion);
  tcase_add_test(tc, test_modem_error_name);
  tcase_add_test(tc, test_modem_call_error_prefix);
  tcase_add_test(tc, test_modem_call_error);
  tcase_add_test(tc, test_modem_call_net_error_prefix);
  tcase_add_test(tc, test_modem_call_net_error);
  tcase_add_test(tc, test_modem_call_state);

  tcase_set_timeout(tc, 5);
  return tc;
}

/* ====================================================================== */

struct test_cases modem_call_tcases[] = {
  DECLARE_TEST_CASE(modem_call_error_tcase),
  DECLARE_TEST_CASE(modem_call_tcase),
  LAST_TEST_CASE
};

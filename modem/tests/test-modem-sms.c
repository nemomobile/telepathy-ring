/*
 * test-modem-sms.c - Test cases for ModemSMSService
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

#define MODEM_DEBUG_FLAG (-1)
#define _ATFILE_SOURCE (1)

#include "modem/debug.h"
#include <modem/sms.h>
#include <modem/errors.h>

#include <sms-glib/submit.h>
#include <sms-glib/deliver.h>
#include <sms-glib/errors.h>

#include <dbus/dbus-glib.h>

#include "test-modem.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

gchar const default_smsc[] = "+3584544" "00046";
gchar const national_smsc[] = "04544" "00046";

static GMainLoop *mainloop = NULL;
static ModemSMSService *sms_service;

static struct event {
  enum {
    SEND_REPLY = 1,
    INCOMING,
    OUTGOING_COMPLETE,
    OUTGOING_ERROR,
    STATUS_REPORT,
    CONNECTED,
  } reason;
  gchar *id;
  gchar *destination;
  GError *error;
  SMSGStatusReport *sr;
  SMSGDeliver *deliver;
} events[4 * 256];

static int reported = 0, steps;

static guint64 client_connected;

static void
on_connected(ModemSMSService *modem_sms_service,
  gpointer _self)
{
  memset(&events[reported], 0, sizeof events[reported]);
  events[reported].reason = CONNECTED;
  reported++;

  client_connected = modem_sms_service_time_connected(modem_sms_service);

  assert(reported < 1024);              /* abort() if not in events */
}

static void
on_deliver(ModemSMSService *modem_sms_service,
  SMSGDeliver *received,
  gpointer _self)
{
  memset(&events[reported], 0, sizeof events[reported]);
  events[reported].reason = INCOMING;
  events[reported].id = g_strdup(sms_g_deliver_get_message_token(received));
  events[reported].deliver = g_object_ref(received);
  reported++;

  assert(reported < 1024);              /* abort() if not in events */
}

static void send_reply(ModemSMSService *sms_service,
  ModemRequest *call,
  gchar const *message_id,
  GError const *error,
  gpointer userdata)
{
  (void)sms_service, (void)call;
  memset(&events[reported], 0, sizeof events[reported]);
  events[reported].reason = SEND_REPLY;
  events[reported].id = message_id ? g_strdup(message_id) : NULL;
  events[reported].error = error ? g_error_copy(error) : NULL;
  reported++;
  assert(reported < 1024);              /* abort() if not in events */
}

static void
on_outgoing_complete(
  ModemSMSService *sms_service,
  gchar const *message_id,
  gchar const *destination,
  gpointer user_data)
{
  memset(&events[reported], 0, sizeof events[reported]);
  events[reported].reason = OUTGOING_COMPLETE;
  events[reported].id = message_id ? g_strdup(message_id) : NULL;
  events[reported].destination = g_strdup(destination);
  reported++;
  assert(reported < 1024);              /* abort() if not in events */
}

void
on_outgoing_error(
  ModemSMSService *self,
  gchar const *message_id,
  gchar const *destination,
  GError const *error,
  gpointer user_data)
{
  memset(&events[reported], 0, sizeof events[reported]);
  events[reported].reason = OUTGOING_ERROR;
  events[reported].id = message_id ? g_strdup(message_id) : NULL;
  events[reported].destination = g_strdup(destination);
  events[reported].error = g_error_copy(error);
  reported++;
  assert(reported < 1024);              /* abort() if not in events */
}

void on_status_report(
  ModemSMSService *self,
  SMSGStatusReport *status_report,
  gpointer user_data)
{
  memset(&events[reported], 0, sizeof events[reported]);
  events[reported].reason = STATUS_REPORT;
  events[reported].id = g_strdup(sms_g_status_report_get_message_token(status_report));
  events[reported].sr = g_object_ref(status_report);
  reported++;
  assert(reported < 1024);              /* abort() if not in events */
}

static void zap_events(void)
{
  guint i;

  for (i = 0; i < reported; i++) {
    g_free(events[i].id);
    if (events[i].error) g_error_free(events[i].error);
    if (events[i].sr) g_object_unref(events[i].sr);
    if (events[i].deliver) g_object_unref(events[i].deliver);
    if (events[i].destination) g_free(events[i].destination);
  }

  reported = 0;
}

static struct {
  gpointer original;
  size_t   size;
  guint8   data[16];
} quitter;

static gboolean quit_loop (gpointer pointer)
{
  (void)pointer;
  if (memcmp(quitter.original, quitter.data, quitter.size)) {
    g_main_loop_quit (mainloop);
    return FALSE;
  }
  return TRUE;
}

static void run_until(gpointer change, size_t size)
{
  assert(size <= sizeof quitter.data);
  memcpy(quitter.data, quitter.original = change, quitter.size = size);

#if 1
  g_idle_add_full(300,
    quit_loop,
    (gpointer)quit_loop,
    NULL);
#else
  g_timeout_add(20, quit_loop, (gpointer)quit_loop);
#endif
  g_main_loop_run (mainloop);
}

gulong
id_connected,
  id_deliver, id_outgoing_complete,
  id_outgoing_error, id_status_report;

char *test_spooldir;

static void echo_setup(void)
{
  setenv("SMS_LOOPBACK", "1", 1);

  /* sms_g_debug_set_flags(0xffffffff); */

  g_type_init();
  (void)dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
  mainloop = g_main_loop_new (NULL, FALSE);

  char *spooldir, *expect =
    g_strdup_printf("/tmp/test-modem-sms-%u/srr/spool", (unsigned)getpid());

  sms_service = g_object_new(MODEM_TYPE_SMS_SERVICE,
                "srr-spool-directory", expect,
                NULL);

  g_object_get(sms_service, "srr-spool-directory", &spooldir, NULL);

  fail_if(strcmp(spooldir, expect));
  test_spooldir = spooldir;
  g_free(expect);

  id_deliver =
    modem_sms_connect_to_deliver(
      sms_service, on_deliver, events);

  id_outgoing_complete =
    modem_sms_connect_to_outgoing_complete(
      sms_service, on_outgoing_complete, events);

  id_outgoing_error =
    modem_sms_connect_to_outgoing_error(
      sms_service, on_outgoing_error, events);

  id_status_report =
    modem_sms_connect_to_status_report(
      sms_service, on_status_report, events);

  id_connected =
    modem_sms_connect_to_connected(
      sms_service, on_connected, events);

  fail_unless(modem_sms_service_connect(sms_service));
  run_until(&client_connected, sizeof client_connected);
  fail_unless(modem_sms_service_time_connected(sms_service) != 0);
  zap_events();

  g_object_unref(sms_service);
}

static void echo_teardown(void)
{
  if (id_connected != 0 &&
    g_signal_handler_is_connected(sms_service, id_connected))
    g_signal_handler_disconnect(sms_service, id_connected);
  id_connected = 0;

  if (id_deliver != 0 &&
    g_signal_handler_is_connected(sms_service, id_deliver))
    g_signal_handler_disconnect(sms_service, id_deliver);
  id_deliver = 0;

  if (id_outgoing_complete != 0 &&
    g_signal_handler_is_connected(sms_service, id_outgoing_complete))
    g_signal_handler_disconnect(sms_service, id_outgoing_complete);
  id_outgoing_complete = 0;

  if (id_outgoing_error != 0 &&
    g_signal_handler_is_connected(sms_service, id_outgoing_error))
    g_signal_handler_disconnect(sms_service, id_outgoing_error);
  id_outgoing_error = 0;

  if (id_status_report != 0 &&
    g_signal_handler_is_connected(sms_service, id_status_report))
    g_signal_handler_disconnect(sms_service, id_status_report);
  id_status_report = 0;

  zap_events();

  if (sms_service)
    g_object_unref((GObject *)sms_service), sms_service = NULL;

  if (test_spooldir) {
    DIR *dir = opendir(test_spooldir);
    struct dirent *d;
    if (dir) {
      for (d = readdir(dir); d; d = readdir(dir)) {
        char *path = g_strdup_printf("%s/%s", test_spooldir, d->d_name);
        unlink(path);
        g_free(path);
      }
      closedir(dir);
    }

    char *spooldir = test_spooldir;

    while (spooldir && strcmp(spooldir, "/tmp")) {
      rmdir(spooldir);
      char * slash = strrchr(spooldir, '/');
      if (!slash || slash == spooldir)
        break;
      *slash = '\0';
    }

    g_free(test_spooldir), test_spooldir = NULL;
  }

  g_main_loop_unref (mainloop), mainloop = NULL;
}

/* ---------------------------------------------------------------------- */

static
void echo_text_message(gchar const *text, gchar const *destination)
{
  SMSGSubmit *sms = sms_g_submit_new();
  ModemRequest *sending;
  SMSGDeliver *echo;
  GError *error = NULL;
  GPtrArray const *tpdus;

  if (destination == NULL)
    destination = "045441041099";

  sms_g_submit_set_destination(sms, destination);

  tpdus = sms_g_submit_text(sms, text, &error);
  fail_unless(tpdus != NULL);
  fail_unless(error == NULL);

  sending = modem_sms_request_send(sms_service, sms, send_reply, events);
  fail_unless(sending != NULL);

  g_object_unref(sms), sms = NULL;

  for (steps = 0; reported < 3 && steps < 100; steps++)
    run_until(&reported, sizeof reported);

  fail_unless(events[0].reason == SEND_REPLY);
  fail_unless(events[0].id != NULL);

  fail_unless(events[1].reason == OUTGOING_COMPLETE);
  fail_unless(events[1].id != NULL);
  fail_if(strcmp(events[1].destination, destination));

  fail_unless(events[2].reason == INCOMING);
  fail_unless(events[2].id != NULL);
  fail_unless(events[2].deliver != NULL);

  echo = events[2].deliver;

  fail_if(strcmp(sms_g_deliver_get_originator(echo), destination));
  fail_if(strcmp(sms_g_deliver_get_text(echo), text));
  fail_unless(sms_g_deliver_is_text(echo));
  fail_unless(sms_g_deliver_is_type(echo, "text/plain"));
}

START_TEST(test_modem_sms_echo_text)
{
  echo_text_message("echo text message via loopback", NULL);
  zap_events();
  echo_text_message("Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters",
    "05055578901234567899");
  zap_events();
#if notyet
  /* This fails! */
  echo_text_message("Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters..",
    "05055578901234567899");
  zap_events();

  char text[] =
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters.."
    "Diipa daapa with exactly 40 characters..";
  int i;

  for (i = sizeof(text); i >= 0; i--) {
    text[i] = '\0';
    echo_text_message(text, "05055578901234567899");
    zap_events();
  }
#endif
}
END_TEST

START_TEST(test_modem_sms_echo_empty)
{
  echo_text_message("", NULL);
}
END_TEST

START_TEST(test_modem_sms_echo_with_content_type)
{
  SMSGSubmit *sms = sms_g_submit_new_type("text/x-vcard");
  ModemRequest *sending;
  SMSGDeliver *echo;
  GError *error = NULL;
  GPtrArray const *tpdus;
  gchar const text[] = "echo text message via loopback";

  sms_g_submit_set_destination(sms, "04544104109");

  tpdus = sms_g_submit_text(sms, text, &error);
  fail_unless(tpdus != NULL);
  fail_unless(error == NULL);

  sending = modem_sms_request_send(sms_service, sms, send_reply, events);
  fail_unless(sending != NULL);

  g_object_unref(sms), sms = NULL;

  for (steps = 0; reported < 3 && steps < 100; steps++)
    run_until(&reported, sizeof reported);

  fail_unless(events[0].reason == SEND_REPLY);
  fail_unless(events[0].id != NULL);

  fail_unless(events[1].reason == OUTGOING_COMPLETE);
  fail_unless(events[1].id != NULL);
  fail_if(strcmp(events[1].destination, "04544104109"));

  fail_unless(events[2].reason == INCOMING);
  fail_unless(events[2].id != NULL);
  fail_unless(events[2].deliver != NULL);

  echo = events[2].deliver;

  fail_if(strcmp(sms_g_deliver_get_originator(echo), "04544104109"));
  GArray const *binary = sms_g_deliver_get_binary(echo);
  fail_if(memcmp(text, binary->data, binary->len));
  fail_unless(sms_g_deliver_is_type(echo, "text/x-vcard"));
}
END_TEST

START_TEST(test_modem_sms_echo_binary)
{
  SMSGSubmit *sms = sms_g_submit_new();
  ModemRequest *sending;
  SMSGDeliver *echo;
  GError *error = NULL;
  GPtrArray const *tpdus;
  gchar const binary[] = "echo binary message via loopback";
  GArray bsend[1] = {{ (gpointer)binary, sizeof binary }};
  GArray *becho;
  guint sport, dport;

  sms_g_submit_set_destination(sms, "04544104109");
  sms_g_submit_set_sms_class(sms, 1);
  sms_g_submit_set_dport(sms, 0xe4);
  sms_g_submit_set_sport(sms, 243);

  tpdus = sms_g_submit_binary(sms, bsend, &error);
  fail_unless(tpdus != NULL);
  fail_unless(error == NULL);

  sending = modem_sms_request_send(sms_service, sms, send_reply, events);
  fail_unless(sending != NULL);

  g_object_unref(sms), sms = NULL;

  for (steps = 0; reported < 3 && steps < 100; steps++)
    run_until(&reported, sizeof reported);

  fail_unless(events[0].reason == SEND_REPLY);
  fail_unless(events[0].id != NULL);

  fail_unless(events[1].reason == OUTGOING_COMPLETE);
  fail_unless(events[1].id != NULL);
  fail_if(strcmp(events[1].destination, "04544104109"));

  fail_unless(events[2].reason == INCOMING);
  fail_unless(events[2].id != NULL);
  fail_unless(events[2].deliver != NULL);

  echo = events[2].deliver;

  fail_if(strcmp(sms_g_deliver_get_originator(echo), "04544104109"));

  g_object_get(echo,
    "binary", &becho,
    "destination-port", &dport,
    "source-port", &sport,
    NULL);
  fail_if(!becho);
  fail_if(becho->len != sizeof binary);
  fail_if(memcmp(becho->data, binary, sizeof binary));
  g_boxed_free(DBUS_TYPE_G_UCHAR_ARRAY, becho);
  fail_unless(dport == 0xe4);
  fail_unless(sport == 243);

  fail_unless(sms_g_deliver_is_type(echo, "text/x-vcalendar"));

}
END_TEST

START_TEST(test_modem_sms_echo_large_binary)
{
  SMSGSubmit *sms = sms_g_submit_new();
  ModemRequest *sending;
  SMSGDeliver *echo;
  GError *error = NULL;
  GPtrArray const *tpdus;
  gchar binary[512] = "echo binary message via loopback";
  GArray bsend[1] = {{ (gpointer)binary, sizeof binary }};
  GArray *becho;
  guint sport, dport;
  guint i;

  for (i = strlen(binary) + 1; i < sizeof binary; i++)
    binary[i] = (i % 64) + 32;

  sms_g_submit_set_destination(sms, "04544104109");
  sms_g_submit_set_sms_class(sms, 1);
  sms_g_submit_set_dport(sms, 226);
  sms_g_submit_set_sport(sms, 243);

  for (i = 1; i < sizeof binary; i++) {
    bsend->len = i;
    tpdus = sms_g_submit_binary(sms, bsend, &error);
    fail_unless(tpdus != NULL);
    fail_unless(error == NULL);

    sending = modem_sms_request_send(sms_service, sms, send_reply, events);
    fail_unless(sending != NULL);

    for (steps = 0; reported < 3 && steps < 1000; steps++)
      run_until(&reported, sizeof reported);

    fail_unless(events[0].reason == SEND_REPLY);
    fail_unless(events[0].id != NULL);

    fail_unless(events[1].reason == OUTGOING_COMPLETE);
    fail_unless(events[1].id != NULL);
    fail_if(strcmp(events[1].destination, "04544104109"));

    fail_unless(events[2].reason == INCOMING);
    fail_unless(events[2].id != NULL);
    fail_unless(events[2].deliver != NULL);

    echo = events[2].deliver;

    fail_if(strcmp(sms_g_deliver_get_originator(echo), "04544104109"));

    g_object_get(echo,
      "binary", &becho,
      "destination-port", &dport,
      "source-port", &sport,
      NULL);
    fail_if(!becho);
    if (becho->len != i) {
      printf("sent %u bytes, received %u\n", i, becho->len);
    }
    fail_if(becho->len != i);
    fail_if(memcmp(becho->data, binary, i));
    fail_unless(dport == 226);
    fail_unless(sport == 243);

    g_boxed_free(DBUS_TYPE_G_UCHAR_ARRAY, becho);

    fail_unless(sms_g_deliver_is_vcard(echo));
    fail_unless(sms_g_deliver_is_type(echo, "text/x-vcard"));

    zap_events();
  }

  g_object_unref(sms), sms = NULL;
}
END_TEST

START_TEST(test_modem_sms_echo_srr)
{
  char text[] = "wait for status report";
  char destination[] = "0505556666";
  SMSGSubmit *sms;
  ModemRequest *sending;
  SMSGDeliver *echo;
  GError *error = NULL;
  GPtrArray const *tpdus;

  sms = sms_g_submit_new();
  sms_g_submit_set_destination(sms, destination);
  g_object_set(sms, "status-report-request", 1, NULL);

  tpdus = sms_g_submit_text(sms, text, &error);
  fail_unless(tpdus != NULL);
  fail_unless(error == NULL);

  sending = modem_sms_request_send(sms_service, sms, send_reply, events);
  fail_unless(sending != NULL);

  g_object_unref(sms), sms = NULL;

  for (steps = 0; reported < 4 && steps < 100; steps++)
    run_until(&reported, sizeof reported);

  fail_unless(events[0].reason == SEND_REPLY);
  fail_unless(events[0].id != NULL);

  fail_unless(events[1].reason == OUTGOING_COMPLETE);
  fail_unless(events[1].id != NULL);
  fail_if(strcmp(events[1].destination, destination));

  fail_unless(events[2].reason == INCOMING);
  fail_unless(events[2].id != NULL);
  fail_unless(events[2].deliver != NULL);

  fail_unless(events[3].reason == STATUS_REPORT);

  echo = events[2].deliver;

  fail_if(strcmp(sms_g_deliver_get_originator(echo), destination));
  fail_if(strcmp(sms_g_deliver_get_text(echo), text));
  fail_unless(sms_g_deliver_is_text(echo));
  fail_unless(sms_g_deliver_is_type(echo, "text/plain"));
}
END_TEST

static TCase *
echo_tcase(void)
{
  TCase *tc = tcase_create("2 - Loopback tests");

  tcase_add_checked_fixture(tc, echo_setup, echo_teardown);

  {
    tcase_add_test(tc, test_modem_sms_echo_text);
    tcase_add_test(tc, test_modem_sms_echo_empty);
    tcase_add_test(tc, test_modem_sms_echo_with_content_type);
    tcase_add_test(tc, test_modem_sms_echo_binary);
    tcase_add_test(tc, test_modem_sms_echo_large_binary);
    tcase_add_test(tc, test_modem_sms_echo_srr);
  }

  tcase_set_timeout(tc, 40);
  return tc;
}

/* ====================================================================== */

static char *test_content_types[] = { "text/x-vcalendar", NULL };

static void
sms_setup(void)
{
  g_type_init();
  (void)dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
  mainloop = g_main_loop_new (NULL, FALSE);
}

static void
sms_teardown(void)
{
  g_main_loop_unref(mainloop), mainloop = NULL;
}

START_TEST(sms_service_defaults)
{
  char *spooldir = NULL;
  char **content_types = NULL;
  char *smsc = NULL;
  unsigned validity_period = 0;

  sms_service = g_object_new(MODEM_TYPE_SMS_SERVICE,
                "content-types", test_content_types,
                "service-centre", "+0454400046",
                "validity-period", 300,
                NULL);

  g_object_get(sms_service,
    "content-types", &content_types,
    "service-centre", &smsc,
    "validity-period", &validity_period,
    NULL);

  fail_if(content_types == NULL);
  fail_if(content_types[0] == NULL);
  fail_if(content_types[1] != NULL);
  fail_if(strcmp(content_types[0], test_content_types[0]));

  fail_if(smsc == NULL);
  fail_if(strlen(smsc) == 0);
  fail_if(strcmp(smsc, "+0454400046") != 0);

  fail_if(validity_period != 300);

  g_strfreev(content_types);
  g_free(smsc);

  /* Try to set too long validity-period */
  g_object_set(sms_service, "validity-period", 64 * 7 * 24 * 60 * 60, NULL);

  g_object_get(sms_service, "validity-period", &validity_period, NULL);

  fail_if(validity_period != 300);

  g_object_unref(sms_service);

  sms_service = g_object_new(MODEM_TYPE_SMS_SERVICE, "srr-spool-directory", "", NULL);
  g_object_get(sms_service, "srr-spool-directory", &spooldir, NULL);
  fail_if(spooldir != NULL);
  g_object_run_dispose(G_OBJECT(sms_service));
  fail_if(modem_sms_service_connect(sms_service));
  g_object_unref(sms_service);
}
END_TEST

static TCase *
tcase_for_modem_sms_defaults(void)
{
  TCase *tc = tcase_create("3 - Default values");

  tcase_add_checked_fixture(tc, sms_setup, sms_teardown);

  {
    tcase_add_test(tc, sms_service_defaults);
  }

  tcase_set_timeout(tc, 40);

  return tc;
}


/* ====================================================================== */

static void modem_sms_api_init(void)
{
  g_type_init();
  (void)dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
  mainloop = g_main_loop_new (NULL, FALSE);
}

static void modem_sms_api_teardown(void)
{
  g_main_loop_unref(mainloop), mainloop = NULL;
}

static int got_sms_connected;

static void on_sms_connected(ModemSMSService *sms_service,
  gpointer user_data)
{
  got_sms_connected++;
  g_main_loop_quit(mainloop);
}

#if notyet
GError *reply_error;

static void reply_to_sms_request(ModemSMSService *self,
  ModemRequest *request,
  GError *error,
  gpointer user_data)
{
  reply_error = g_error_copy(error);
  g_main_loop_quit(mainloop);
}
#endif

START_TEST(modem_sms_api)
{
  char **strv;
  gpointer p;
  GError *error;

  fail_if(modem_sms_service_is_connected(NULL));
  fail_if(modem_sms_service_is_connecting(NULL));

  sms_service = g_object_new(MODEM_TYPE_SMS_SERVICE,
                "content-types", test_content_types,
                NULL);
  error = NULL;

  fail_if(modem_sms_service_time_connected(sms_service) != 0);
  fail_if(modem_sms_service_is_connected(sms_service));
  fail_if(modem_sms_service_is_connecting(sms_service));

  strv = modem_sms_list_stored(sms_service);
  fail_if(strv == NULL);
  g_strfreev(strv);

  p = modem_sms_get_stored_message(sms_service, "/kuik");
  fail_if(p != NULL);

  modem_sms_request_expunge(sms_service, "/kuik", NULL, NULL);
  mark_point();

  g_signal_connect(sms_service, "connected",
    G_CALLBACK(on_sms_connected),
    &got_sms_connected);

  fail_unless(modem_sms_service_connect(sms_service));
  fail_unless(modem_sms_service_is_connecting(sms_service));
  g_main_loop_run(mainloop);
  fail_unless(modem_sms_service_is_connected(sms_service));

  g_object_unref(sms_service);
}
END_TEST

static TCase *
tcase_for_modem_sms_api(void)
{
  TCase *tc = tcase_create("modem-sms-api");

  tcase_add_checked_fixture(tc, modem_sms_api_init, modem_sms_api_teardown);

  {
    tcase_add_test(tc, modem_sms_api);
  }

  tcase_set_timeout(tc, 40);

  return tc;
}

/* ====================================================================== */
#undef MODEM_DEBUG_FLAG

#include "modem/sms-service.c"

static void modem_sms_error_init(void)
{
  g_type_init();
  (void)dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
}

static void modem_sms_error_teardown(void)
{
}

START_TEST(test_modem_sms_errors)
{
  GError *error;
  guint code;

  for (code = 1000; code <= 1050; code ++) {
    error = modem_sms_new_error(code, NULL);
    fail_unless(error != NULL);
    fail_unless(error->domain == MODEM_SMS_NET_ERRORS);
    g_error_free(error);
  }

  for (;code <= 1070; code ++) {
    error = modem_sms_new_error(code, NULL);
    fail_unless(error != NULL);
    fail_unless(error->domain == MODEM_SMS_ERRORS,
      "libsmserror %u: %s.%s\n", code,
      g_quark_to_string(error->domain), modem_error_name(error, "???", 0));
    g_error_free(error);
  }

  for (;code <= 1082; code ++) {
    error = modem_sms_new_error(code, NULL);
    fail_unless(error != NULL);
    fail_unless(error->domain == MODEM_SMS_ERRORS,
      "libsmserror %u: %s.%s\n", code,
      g_quark_to_string(error->domain), modem_error_name(error, "???", 0));
    g_error_free(error);
  }

  for (code = 1200; code <= 1208; code ++) {
    error = modem_sms_new_error(code, NULL);
    fail_unless(error != NULL);
    fail_unless(error->domain == MODEM_SIM_ERRORS,
      "libsmserror %u: %s.%s\n", code,
      g_quark_to_string(error->domain), modem_error_name(error, "???", 0));
    g_error_free(error);
  }

  /* 1210 is duplicate,
     1211 is unknown generic sms error */
  for (;code <= 1210; code ++) {
    error = modem_sms_new_error(code, NULL);
    fail_unless(error != NULL);
    if (error->domain != MODEM_SMS_ERRORS)
      printf("libsmserror %u: %s.%s\n",
        code, g_quark_to_string(error->domain), modem_error_name(error, "???", 0));
    fail_unless(error->domain == MODEM_SMS_ERRORS);
    g_error_free(error);
  }

  error = modem_sms_new_error(SMS_CAUSE_ROUTING_FAILED, NULL);
  fail_unless(error != NULL);
  fail_unless(modem_sms_error_is_temporary(error));
  g_error_free(error);
}
END_TEST


START_TEST(test_sms_error_conversion)
{
  GError *error, *dbus, *fixed;

  guint i;
  char ebuffer[16];

  for (i = 980; i < 1200; i++) {
    error = modem_sms_new_error(i, "test");
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
END_TEST

static TCase *
modem_sms_errors_tcase(void)
{
  TCase *tc = tcase_create("modem-sms-errors");

  tcase_add_checked_fixture(tc, modem_sms_error_init, modem_sms_error_teardown);

  {
    tcase_add_test(tc, test_modem_sms_errors);
    tcase_add_test(tc, test_sms_error_conversion);
  }

  tcase_set_timeout(tc, 5);

  return tc;
}

/* ====================================================================== */

struct test_cases modem_sms_tcases[] = {
  DECLARE_TEST_CASE_OFF_BY_DEFAULT(echo_tcase),
  DECLARE_TEST_CASE_OFF_BY_DEFAULT(tcase_for_modem_sms_defaults),
  DECLARE_TEST_CASE_OFF_BY_DEFAULT(tcase_for_modem_sms_api),
  DECLARE_TEST_CASE(modem_sms_errors_tcase),
  LAST_TEST_CASE
};

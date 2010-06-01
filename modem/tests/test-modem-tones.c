/*
 * test-modem-tones.c - Test cases for ModemTones
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
#include <modem/tones.h>

#include "test-modem.h"
#include "modem/debug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static GMainLoop *mainloop = NULL;

static void setup(void)
{
  g_type_init();
  mainloop = g_main_loop_new (NULL, FALSE);
}

static void teardown(void)
{
  g_main_loop_unref(mainloop), mainloop = NULL;
}

static void tone_stopped(ModemTones *tones, guint source, gpointer data)
{
  fail_if(data == NULL);
  *(int *)data = 0;
  g_main_loop_quit(mainloop);
}

START_TEST(test_modem_tones)
{
  ModemTones *tones;
  int volume = 0;
  gint event;
  guint playing;

  tones = g_object_new(MODEM_TYPE_TONES, NULL);

  g_object_get(tones, "volume", &volume, NULL);
  fail_if(volume >= 0);

  g_object_set(tones, "volume", -63, NULL);
  g_object_get(tones, "volume", &volume, NULL);
  fail_if(volume != -63);

  modem_tones_stop(tones, 0);
  modem_tones_user_connection(tones, 0);

  fail_if(0 > (event = modem_call_event_tone(MODEM_CALL_STATE_ALERTING, 0, 0)));
  fail_if(!(playing = modem_tones_start(tones, event, 0)));
  modem_tones_stop(tones, (unsigned)-1);
  fail_unless(modem_tones_is_playing(tones, playing));

  /* NW cause Busy - no tone with MO-RELEASE */
  event = modem_call_event_tone(MODEM_CALL_STATE_DISCONNECTED, 3, 17);
  fail_if(event >= 0);
  fail_if(playing = modem_tones_start(tones, event, 0));

  /* NW cause Busy - BUSY with MT-RELEASE */
  event = modem_call_event_tone(MODEM_CALL_STATE_DISCONNECTED, 3, 17);
  fail_unless(event == TONES_EVENT_BUSY);
  fail_unless(playing = modem_tones_start_full(tones, event, 0, 100, tone_stopped, &playing));
  g_main_loop_run(mainloop);
  fail_if(playing);

  fail_unless(playing = modem_tones_start_full(tones, event, 0, 100, tone_stopped, &playing));
  modem_tones_stop(tones, playing);
  fail_if(modem_tones_is_playing (tones, playing));

  fail_unless(playing = modem_tones_start_full(tones, event, 0, 100, tone_stopped, &playing));
  modem_tones_stop(tones, playing ^ 13);
  fail_unless(playing);
  modem_tones_stop(tones, 0);
  fail_if(modem_tones_is_playing (tones, playing));

  fail_unless(playing = modem_tones_start_full(tones, event, 0, 100, tone_stopped, &playing));
  fail_unless(playing);
  g_object_run_dispose(G_OBJECT(tones));
  fail_if(modem_tones_is_playing (tones, playing));
  g_message("Expect a **CRITICAL** message from the following line:");
  fail_if(playing = modem_tones_start_full(tones, event, 0, 100, tone_stopped, &playing));

  g_message("Expect a **CRITICAL** message from the following line:");
  modem_tones_stop(tones, 0);
  g_object_unref(tones);

  tones = g_object_new(MODEM_TYPE_TONES, NULL);
  fail_unless(playing = modem_tones_start_full(tones, event, 0, 100, tone_stopped, &playing));
  /* modem_tones_start() will always stop the tone */
  fail_if(modem_tones_start_full(tones, -1, 0, 100, tone_stopped, &playing));
  fail_if(modem_tones_is_playing (tones, playing));

#if 0

  fail_if(playing);
  if (status == MODEM_CALL_STATE_MT_RELEASE)
    fail_unless(playing);
  if (status == MODEM_CALL_STATE_MO_RELEASE)
    fail_unless(modem_tones_is_playing(tones) == call);
  modem_tones_for_call_event(tones, call,
    status, 1, MODEM_CALL_ERROR_BLACKLIST_DELAYED);
  fail_unless(modem_tones_is_playing(tones) == call);
}
modem_tones_user_connection(tones, 1);
modem_tones_stop(tones, NULL);
}

for (causetype = 1; causetype <= 3; causetype++) {
  for (cause = 1; cause <= 127; cause++) {
    modem_tones_user_connection(tones, 0);
    modem_tones_for_call_event(tones, call,
      MODEM_CALL_STATE_MT_RELEASE, causetype, cause);
    modem_tones_for_call_event(tones, call,
      MODEM_CALL_STATE_TERMINATED, causetype, cause);
    modem_tones_for_call_event(tones, call, MODEM_CALL_STATE_IDLE, 0, 0);
    modem_tones_user_connection(tones, 1);
  }
}

got_call_connected = 0;
modem_call_service_disconnect(call_service);
fail_unless(got_call_connected != 0);
#endif
g_object_unref(tones);
}
END_TEST

static TCase *
tcase_for_modem_tones(void)
{
  TCase *tc = tcase_create("Test for ModemCallService");

  tcase_add_checked_fixture(tc, setup, teardown);

  tcase_add_test(tc, test_modem_tones);
  tcase_set_timeout(tc, 10);

  return tc;
}


/* ====================================================================== */

struct test_cases modem_tones_tcases[] = {
  DECLARE_TEST_CASE_OFF_BY_DEFAULT(tcase_for_modem_tones),
  LAST_TEST_CASE
};

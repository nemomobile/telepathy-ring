/*
 * modem/tones.c - Call signaling tones handling
 * Copyright (C) 2008 Nokia Corporation
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

#define MODEM_DEBUG_FLAG MODEM_SERVICE_TONES

#include "modem/debug.h"
#include "modem/tones.h"
#include "modem/request-private.h"

#include "modem/errors.h"

#include <dbus/dbus-glib.h>

#include <string.h>

G_DEFINE_TYPE(ModemTones, modem_tones, G_TYPE_OBJECT);

struct _ModemTonesPrivate
{
  DBusGProxy *proxy;
  GTimer *timer;

  int volume;
  guint source;

  guint playing;
  int event;
  int evolume;
  guint duration;

  /* Timeout context */
  guint timeout;
  ModemTonesStoppedNotify *notify;
  gpointer data;

  GQueue stop_requests[1];

  unsigned user_connection:1;
  unsigned dispose_has_run:2;
};

static void
modem_tones_init(ModemTones *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, MODEM_TYPE_TONES, ModemTonesPrivate);
  self->priv->timer = g_timer_new();
  self->priv->proxy =
    dbus_g_proxy_new_for_name(dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL),
      "com.Nokia.Telephony.Tones",
      "/com/Nokia/Telephony/Tones",
      "com.Nokia.Telephony.Tones");
  g_queue_init(self->priv->stop_requests);
}

static void
modem_tones_dispose(GObject *object)
{
  ModemTones *self = MODEM_TONES(object);
  ModemTonesPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = 1;
  modem_tones_stop(self, 0);
  priv->dispose_has_run = 2;
  while (!g_queue_is_empty(priv->stop_requests)) {
    modem_request_cancel(g_queue_pop_head(priv->stop_requests));
  }
  g_assert(!priv->playing);
  g_object_run_dispose(G_OBJECT(priv->proxy));

  if (G_OBJECT_CLASS(modem_tones_parent_class)->dispose)
    G_OBJECT_CLASS(modem_tones_parent_class)->dispose(object);
}

static void
modem_tones_finalize(GObject *object)
{
  ModemTones *self = MODEM_TONES(object);
  ModemTonesPrivate *priv = self->priv;

  g_object_unref(priv->proxy);
  g_timer_destroy(priv->timer);

  memset(priv, 0, (sizeof *priv));

  G_OBJECT_CLASS(modem_tones_parent_class)->finalize(object);
}

/* Properties */
enum
{
  PROP_NONE,
  PROP_VOLUME,
  LAST_PROPERTY
};

static void
modem_tones_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  ModemTones *self = MODEM_TONES(object);
  ModemTonesPrivate *priv = self->priv;

  switch(property_id) {
    case PROP_VOLUME:
      g_value_set_int(value, priv->volume);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
modem_tones_set_property(GObject *obj,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  ModemTones *self = MODEM_TONES(obj);
  ModemTonesPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_VOLUME:
      priv->volume = g_value_get_int(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
modem_tones_class_init(ModemTonesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  g_type_class_add_private(klass, sizeof (ModemTonesPrivate));

  object_class->get_property = modem_tones_get_property;
  object_class->set_property = modem_tones_set_property;
  object_class->dispose = modem_tones_dispose;
  object_class->finalize = modem_tones_finalize;

  /* Properties */
  g_object_class_install_property(
    object_class, PROP_VOLUME,
    g_param_spec_int("volume",
      "Volume in dBm0",
      "Volume describes the power level of the tone, "
      "expressed in dBm0. "
      "Power levels range from 0 to -63 dBm0.",
      /* min */ -63, /* max  */ 0, /* default */ -9,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS));
}

/* ------------------------------------------------------------------------- */

static gboolean modem_tones_timeout(gpointer _self);
static void modem_tones_timeout_removed(gpointer _self);
static void reply_to_stop_tone(DBusGProxy *proxy,
  DBusGProxyCall *call,
  void *_request);

static gboolean modem_tones_suppress(int event)
{
  return event < 0 ||
    (event > TONES_EVENT_DTMF_D && event < TONES_EVENT_RADIO_PATH_ACK);
}

guint
modem_tones_start_full(ModemTones *self,
  int event,
  int volume,
  unsigned duration,
  ModemTonesStoppedNotify *notify,
  gpointer data)
{
  ModemTonesPrivate *priv = self->priv;

  g_return_val_if_fail(!priv->dispose_has_run, 0);

  volume += priv->volume;

  if (volume > 0)
    volume = 0;
  else if (volume < -63)
    volume = -63;

  if (event == TONES_EVENT_DROPPED) {
    if (duration > 1200)
      duration = 1200;
  }

  modem_tones_stop(self, 0);

  if (event < 0)
    return 0;

  if (priv->source == 0)
    priv->source++;

  priv->playing = priv->source++;
  priv->event = event;
  priv->evolume = volume;
  priv->duration = duration;

  if (duration) {
    priv->timeout = g_timeout_add_full(G_PRIORITY_DEFAULT, duration,
                    modem_tones_timeout, self, modem_tones_timeout_removed);
  }

  priv->data = data;
  priv->notify = notify;

  g_timer_start(priv->timer);

  DEBUG("%scalling StartEventTone(%u, %d, %u) with %u",
    priv->user_connection ? "not " : "",
    priv->event, priv->evolume, priv->duration, priv->playing);

  if (!priv->user_connection || !modem_tones_suppress(priv->event)) {
    dbus_g_proxy_call_no_reply(priv->proxy,
      "StartEventTone",
      G_TYPE_UINT, priv->event,
      G_TYPE_INT, priv->evolume,
      G_TYPE_UINT, priv->duration,
      G_TYPE_INVALID);
  }

  return priv->playing;
}

guint
modem_tones_start(ModemTones *self,
  int event,
  unsigned duration)
{
  return modem_tones_start_full(self, event, 0, duration, NULL, NULL);
}

guint
modem_tones_is_playing(ModemTones const *self, guint playing)
{
  double played;

  if (!MODEM_IS_TONES(self) || !self->priv->playing)
    return 0;
  if (playing != 0 && playing != self->priv->playing)
    return 0;

  played = 1000 * g_timer_elapsed(self->priv->timer, NULL) + 0.5;

  if (played < 1.0)
    return 1;
  if (played < (double)UINT_MAX)
    return (guint)played;
  else
    return UINT_MAX;
}

int
modem_tones_playing_event(ModemTones const *self, guint playing)
{
  if (!MODEM_IS_TONES(self) || !self->priv->playing)
    return TONES_NONE;
  if (playing != 0 && playing != self->priv->playing)
    return TONES_NONE;
  return self->priv->event;
}

static gboolean
modem_tones_timeout(gpointer _self)
{
  MODEM_TONES(_self)->priv->timeout = 0;
  modem_tones_stop(MODEM_TONES(_self), 0);
  return FALSE;
}

static void
modem_tones_timeout_removed(gpointer _self)
{
  MODEM_TONES(_self)->priv->timeout = 0;
}

void
modem_tones_stop(ModemTones *self,
  guint source)
{
  ModemTonesPrivate *priv;
  ModemTonesStoppedNotify *notify;
  gpointer data;
  ModemRequest *stopping;

  DEBUG("(%p, %u)", self, source);

  g_return_if_fail(self);
  priv = self->priv;
  g_return_if_fail(priv->dispose_has_run <= 1);

  if (!priv->playing)
    return;
  if (source && priv->playing != source)
    return;
  if (priv->timeout)
    g_source_remove(priv->timeout);
  g_assert(priv->timeout == 0);

  source = priv->playing, priv->playing = 0;
  notify = priv->notify; data = priv->data;
  priv->notify = NULL, priv->data = NULL;

  if (notify) {
    stopping = modem_request_with_timeout(
      self, priv->proxy, "StopTone",
      reply_to_stop_tone,
      G_CALLBACK(notify), data, 5000,
      G_TYPE_INVALID);
    g_queue_push_tail(priv->stop_requests, stopping);
    modem_request_add_data(stopping, "modem-tones-stop-source",
      GUINT_TO_POINTER(source));
  }
}

static void
reply_to_stop_tone(DBusGProxy *proxy,
  DBusGProxyCall *call,
  void *_request)
{
  ModemRequest *request = _request;
  ModemTones *self = modem_request_object(request);
  ModemTonesStoppedNotify *notify = modem_request_callback(request);
  guint source = GPOINTER_TO_UINT(
    modem_request_get_data(request, "modem-tones-stop-source"));
  gpointer data = modem_request_user_data(request);

  GError *error = NULL;

  if (!dbus_g_proxy_end_call(proxy, call, &error, G_TYPE_INVALID)) {
    g_error_free(error);
  }

  g_queue_remove(self->priv->stop_requests, _request);

  notify(self, source, data);
}

void
modem_tones_user_connection(ModemTones *self,
  gboolean user_connection)
{
  ModemTonesPrivate *priv = self->priv;

  user_connection = !!user_connection;

  DEBUG("(%p, %u)", self, user_connection);

  g_return_if_fail(self);
  g_return_if_fail(!self->priv->dispose_has_run);

  self->priv->user_connection = user_connection;

  if (user_connection) {
    if (priv->playing)
      dbus_g_proxy_call_no_reply(priv->proxy,
        "StopTone",
        G_TYPE_INVALID);
  }
  else {
    if (priv->playing && priv->duration &&
      modem_tones_suppress(priv->event)) {
      double should_have_been_playing = g_timer_elapsed(priv->timer, NULL);

      if (1000.0 * priv->duration > 2 * should_have_been_playing) {
        dbus_g_proxy_call_no_reply(priv->proxy,
          "StartEventTone",
          G_TYPE_UINT, priv->event,
          G_TYPE_INT, priv->evolume,
          G_TYPE_UINT, priv->duration - (guint)(should_have_been_playing * 1000.0),
          G_TYPE_INVALID);
      }
    }
  }
}

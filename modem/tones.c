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

#define MODEM_DEBUG_FLAG MODEM_LOG_AUDIO

#include "modem/debug.h"
#include "modem/tones.h"
#include "modem/request-private.h"

#include "modem/errors.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

#include <libngf/ngf.h>
#include <libngf/proplist.h>

#include <string.h>

G_DEFINE_TYPE(ModemTones, modem_tones, G_TYPE_OBJECT);

struct _ModemTonesPrivate
{
  DBusGConnection *connection;
  GTimer *timer;

  NgfClient *ngf;
  uint32_t event_id;

  int volume;

  int event;
  int evolume;
  guint duration;

  /* Timeout context */
  guint timeout;
  ModemTonesStoppedNotify *notify;
  gpointer data;

  GHashTable *notify_map;

  unsigned dispose_has_run:2;
};

struct _ModemNotifyData
{
  uint32_t event_id;
  ModemTonesStoppedNotify *notify;
  ModemTones *modem;
  gpointer data;
};

typedef struct _ModemNotifyData ModemNotifyData;

static void
ngf_callback(NgfClient *client, uint32_t id, NgfEventState state, void *userdata)
{
  ModemTones *self = (ModemTones*) userdata;
  ModemNotifyData *notify_data = NULL;

  switch (state) {
    /* Both FAILED and COMPLETED mean that the event isn't playing anymore.
     * So with both cases check if tone user wants to be notified. */
    case NGF_EVENT_FAILED:
      DEBUG("NGFD event failed.");
      /* fall through */
    case NGF_EVENT_COMPLETED:
      notify_data = g_hash_table_lookup(self->priv->notify_map, &id);
      break;

    case NGF_EVENT_PLAYING:
      break;

    case NGF_EVENT_PAUSED:
      break;

  }

  if (notify_data) {
    notify_data->notify(self, notify_data->event_id, notify_data->data);
    g_hash_table_remove(self->priv->notify_map, &id);
  }
}

static void
modem_tones_init(ModemTones *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, MODEM_TYPE_TONES, ModemTonesPrivate);
  self->priv->connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
  self->priv->timer = g_timer_new();
  self->priv->ngf = ngf_client_create(NGF_TRANSPORT_DBUS,
                                      dbus_g_connection_get_connection(self->priv->connection));
  ngf_client_set_callback(self->priv->ngf, ngf_callback, self);
  self->priv->event_id = 0;
  self->priv->notify_map = g_hash_table_new_full(g_int_hash,
                                                 g_int_equal,
                                                 NULL,
                                                 g_free);
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
  g_assert(!priv->event_id);

  if (G_OBJECT_CLASS(modem_tones_parent_class)->dispose)
    G_OBJECT_CLASS(modem_tones_parent_class)->dispose(object);
}

static void
modem_tones_finalize(GObject *object)
{
  ModemTones *self = MODEM_TONES(object);
  ModemTonesPrivate *priv = self->priv;

  g_timer_destroy(priv->timer);
  ngf_client_destroy(priv->ngf);
  g_hash_table_destroy(priv->notify_map);
  dbus_g_connection_unref(priv->connection);

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

guint
modem_tones_start_full(ModemTones *self,
  int event,
  int volume,
  unsigned duration,
  ModemTonesStoppedNotify *notify,
  gpointer data)
{
  ModemTonesPrivate *priv = self->priv;
  NgfProplist *proplist;

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

  proplist = ngf_proplist_new();

  if (event > TONES_EVENT_DTMF_D)
    ngf_proplist_set_as_unsigned(proplist, "tonegen.pattern", event);
  else
    ngf_proplist_set_as_unsigned(proplist, "tonegen.value", event);

  ngf_proplist_set_as_integer(proplist, "tonegen.dbm0", volume);

  if (duration > 0)
    ngf_proplist_set_as_unsigned(proplist, "tonegen.duration", duration);

  if (event > TONES_EVENT_DTMF_D)
    priv->event_id = ngf_client_play_event(priv->ngf, "indicator", proplist);
  else
    priv->event_id = ngf_client_play_event(priv->ngf, "dtmf", proplist);

  ngf_proplist_free(proplist);

  DEBUG("called StartEventTone(%u, %d, %u) with %u",
    priv->event, priv->evolume, priv->duration, priv->event_id);

  return priv->event_id;
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

  if (!MODEM_IS_TONES(self) || !self->priv->event_id)
    return 0;
  if (playing != 0 && playing != self->priv->event_id)
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
  if (!MODEM_IS_TONES(self) || !self->priv->event_id)
    return TONES_NONE;
  if (playing != 0 && playing != self->priv->event_id)
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

static gboolean
stop_cb(gpointer userdata)
{
    ModemNotifyData *notify_data = (ModemNotifyData *) userdata;

    ngf_client_stop_event(notify_data->modem->priv->ngf, notify_data->event_id);

    return FALSE;
}

void
modem_tones_stop(ModemTones *self,
  guint event_id)
{
  ModemTonesPrivate *priv;
  ModemTonesStoppedNotify *notify;
  gpointer data;
  ModemRequest *stopping;

  DEBUG("(%p, %u)", self, event_id);

  g_return_if_fail(self);
  priv = self->priv;
  g_return_if_fail(priv->dispose_has_run <= 1);

  if (!priv->event_id)
    return;
  if (event_id && priv->event_id != event_id)
    return;
  if (priv->timeout)
    g_source_remove(priv->timeout);
  g_assert(priv->timeout == 0);

  event_id = priv->event_id, priv->event_id = 0;
  notify = priv->notify; data = priv->data;
  priv->notify = NULL, priv->data = NULL;

  if (notify) {
    ModemNotifyData *notify_data = g_new0(ModemNotifyData, 1);
    notify_data->event_id = event_id;
    notify_data->notify = notify;
    notify_data->modem = self;
    notify_data->data = data;

    g_hash_table_insert(priv->notify_map, &notify_data->event_id, notify_data);
    g_timeout_add_seconds(5, stop_cb, notify_data);
  }
}

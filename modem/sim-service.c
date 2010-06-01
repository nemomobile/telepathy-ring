/*
 * modem/sim-service.c - ModemSIMService class
 *
 * Copyright (C) 2008-2010 Nokia Corporation
 *   @author Pekka Pessi <first.surname@nokia.com>
 *   @author Lassi Syrjala <first.surname@nokia.com>
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

#define MODEM_DEBUG_FLAG MODEM_SERVICE_SIM

#include "debug.h"

#include "modem/sim.h"
#include "modem/request-private.h"
#include "modem/errors.h"
#include "modem/ofono.h"

#include <dbus/dbus-glib.h>

#include "signals-marshal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

/* ------------------------------------------------------------------------ */

G_DEFINE_TYPE(ModemSIMService, modem_sim_service, G_TYPE_OBJECT);

/* Signals we send */
enum {
  SIGNAL_CONNECTED,
  SIGNAL_STATUS,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

/* Properties */
enum {
  PROP_NONE,
  PROP_OBJECT_PATH,
  PROP_STATUS,
  PROP_IMSI,
  LAST_PROPERTY
};

/* private data */
struct _ModemSIMServicePrivate
{
  guint state;
  char *imsi;
  char *object_path;

  GQueue queue[1];

  DBusGProxy *proxy;

  unsigned dispose_has_run:1, connected:1, signals:1, disconnected:1;
  unsigned connection_error:1;
  unsigned :0;
};

/* ------------------------------------------------------------------------ */
/* Local functions */

static void on_sim_property_changed(
  DBusGProxy *, char const *, GValue const *, gpointer);

static ModemOfonoPropsReply reply_to_sim_get_properties;

/* ------------------------------------------------------------------------ */

static void
modem_sim_service_init(ModemSIMService *self)
{
  DEBUG("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, MODEM_TYPE_SIM_SERVICE, ModemSIMServicePrivate);
}

static void
modem_sim_service_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  ModemSIMService *self = MODEM_SIM_SERVICE(object);
  ModemSIMServicePrivate *priv = self->priv;

  switch(property_id) {
    case PROP_OBJECT_PATH:
      g_value_set_string(value, priv->object_path);
      break;

    case PROP_STATUS:
      g_value_set_uint(value, priv->state);
      break;

    case PROP_IMSI:
      g_value_set_string(value, priv->imsi);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
modem_sim_service_set_property(GObject *object,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  ModemSIMService *self = MODEM_SIM_SERVICE(object);
  ModemSIMServicePrivate *priv = self->priv;
  gpointer old;

  switch(property_id) {
    case PROP_OBJECT_PATH:
      priv->object_path = g_value_dup_string(value);
      break;

    case PROP_STATUS:
      priv->state = g_value_get_uint(value);
      break;

    case PROP_IMSI:
      old = priv->imsi;
      priv->imsi = g_value_dup_string(value);
      g_free(old);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
modem_sim_service_constructed(GObject *object)
{
  ModemSIMService *self = MODEM_SIM_SERVICE(object);
  ModemSIMServicePrivate *priv = self->priv;

  priv->proxy = modem_ofono_proxy(priv->object_path, OFONO_IFACE_SIM);
}

static void
modem_sim_service_dispose(GObject *object)
{
  ModemSIMService *self = MODEM_SIM_SERVICE(object);
  ModemSIMServicePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;
  priv->disconnected = TRUE;
  priv->connected = FALSE;
  priv->signals = FALSE;

  g_object_set(self, "state", MODEM_SIM_STATE_UNKNOWN, NULL);

  while (!g_queue_is_empty(priv->queue))
    modem_request_cancel(g_queue_pop_head(priv->queue));

  g_object_run_dispose(G_OBJECT(priv->proxy));

  if (G_OBJECT_CLASS(modem_sim_service_parent_class)->dispose)
    G_OBJECT_CLASS(modem_sim_service_parent_class)->dispose(object);
}

static void
modem_sim_service_finalize(GObject *object)
{
  ModemSIMService *self = MODEM_SIM_SERVICE(object);
  ModemSIMServicePrivate *priv = self->priv;

  DEBUG("enter");

  /* Free any data held directly by the object here */
  g_free(priv->imsi);
  g_free(priv->object_path);
  g_object_unref(priv->proxy);

  G_OBJECT_CLASS(modem_sim_service_parent_class)->finalize(object);
}


static void
modem_sim_service_class_init(ModemSIMServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  DEBUG("enter");

  object_class->get_property = modem_sim_service_get_property;
  object_class->set_property = modem_sim_service_set_property;
  object_class->constructed = modem_sim_service_constructed;
  object_class->dispose = modem_sim_service_dispose;
  object_class->finalize = modem_sim_service_finalize;

  g_object_class_install_property(
    object_class, PROP_OBJECT_PATH,
    g_param_spec_string("object-path",
      "Modem object path",
      "D-Bus object path used to identify the modem",
      "/", /* default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_STATUS,
    g_param_spec_uint("state",
      "SIM Status",
      "Current state of Subscriber identity module.",
      MODEM_SIM_STATE_UNKNOWN,
      LAST_MODEM_SIM_STATE - 1,
      MODEM_SIM_STATE_UNKNOWN,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_IMSI,
    g_param_spec_string("imsi",
      "IMSI",
      "Internation Mobile Subscriber Identity",
      "", /* default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS));

  signals[SIGNAL_CONNECTED] =
    g_signal_new("connected",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[SIGNAL_STATUS] =
    g_signal_new("state",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__UINT,
      G_TYPE_NONE, 1,
      G_TYPE_UINT);

  g_type_class_add_private(klass, sizeof (ModemSIMServicePrivate));

  modem_error_domain_prefix(0); /* Init errors */
}


/* --------------------------------------------------------------------------------- */
/* modem_sim_service interface */

/* Connect to SIM service */
gboolean
modem_sim_service_connect(ModemSIMService *self)
{
  ModemSIMServicePrivate *priv = self->priv;

  DEBUG("enter");

  if (self->priv->connected) {
    DEBUG("already connected");
    return TRUE;
  }

  if (self->priv->disconnected) {
    DEBUG("already disconnected");
    return TRUE;
  }

  modem_ofono_proxy_connect_to_property_changed(
    priv->proxy, on_sim_property_changed, self);

  priv->signals = TRUE;
  priv->connection_error = FALSE;

  g_queue_push_tail(
    priv->queue, modem_ofono_proxy_request_properties(
      priv->proxy, reply_to_sim_get_properties, self, NULL));

  DEBUG("connecting");

  return TRUE;
}


static void
modem_sim_check_connected(ModemSIMService *self,
  ModemRequest *request,
  GError const **error)
{
  ModemSIMServicePrivate *priv = self->priv;

  if (g_queue_find(priv->queue, request)) {
    g_queue_remove(priv->queue, request);

    if (*error) {
      GError const *e = *error;

      modem_critical(MODEM_SERVICE_SIM, GERROR_MSG_FMT,
        GERROR_MSG_CODE(e));

      /* We are 'connected' even if the SIM is not ready */
      if (e->domain == DBUS_GERROR &&
        e->code != DBUS_GERROR_REMOTE_EXCEPTION)
        priv->connection_error = TRUE;
    }

    if (g_queue_is_empty(priv->queue)) {
      DEBUG("got connected (error = %d), emitting", priv->connection_error);
      self->priv->connected = TRUE;
      g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
    }
  }
}

gboolean
modem_sim_service_is_connected(ModemSIMService *self)
{
  return MODEM_IS_SIM_SERVICE(self) && self->priv->connected &&
    !self->priv->connection_error;
}

gboolean
modem_sim_service_is_connecting(ModemSIMService *self)
{
  return MODEM_IS_SIM_SERVICE(self) &&
    !g_queue_is_empty(self->priv->queue);
}

void
modem_sim_service_disconnect(ModemSIMService *self)
{
  ModemSIMServicePrivate *priv = self->priv;
  int was_connected = priv->connected;

  if (priv->disconnected)
    return;

  DEBUG("enter");

  priv->disconnected = TRUE;
  priv->connected = FALSE;

  g_object_set(self, "state", MODEM_SIM_STATE_UNKNOWN, NULL);

  if (priv->signals) {
    priv->signals = FALSE;
    modem_ofono_proxy_disconnect_from_property_changed(
      priv->proxy, on_sim_property_changed, self);
  }

  while (!g_queue_is_empty(priv->queue))
    modem_request_cancel(g_queue_pop_head(priv->queue));

  if (was_connected)
    g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
}

/* ---------------------------------------------------------------------- */

static void
reply_to_sim_get_properties(gpointer _self,
  ModemRequest *request,
  GHashTable *properties,
  GError const *error,
  gpointer user_data)
{
  ModemSIMService *self = MODEM_SIM_SERVICE(_self);

  DEBUG("enter");

  if (!error) {
    GValue *value;

    value = g_hash_table_lookup(properties, "SubscriberIdentity");
    if (value)
      g_object_set_property(G_OBJECT(self), "imsi", value);
  }

  modem_sim_check_connected(self, request, &error);
}

static void
on_sim_property_changed(DBusGProxy *proxy,
  char const *property,
  GValue const *value,
  gpointer _self)
{
  ModemSIMService *self = MODEM_SIM_SERVICE(_self);

  if (!strcmp (property, "SubscriberIdentity"))
    g_object_set_property(G_OBJECT(self), "imsi", value);
}

ModemSIMState
modem_sim_get_state(ModemSIMService const *self)
{
  return MODEM_IS_SIM_SERVICE(self) ? self->priv->state : 0;
}

/* ---------------------------------------------------------------------- */

char const *
modem_sim_get_imsi(ModemSIMService const *self)
{
  return MODEM_IS_SIM_SERVICE(self) ? self->priv->imsi : NULL;
}

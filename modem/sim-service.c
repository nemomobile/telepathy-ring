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

G_DEFINE_TYPE (ModemSIMService, modem_sim_service, MODEM_TYPE_OFACE);

/* Signals we send */
enum
{
  SIGNAL_CONNECTED,
  SIGNAL_STATUS,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

/* Properties */
enum
{
  PROP_NONE,
  PROP_STATUS,
  PROP_IMSI,
  LAST_PROPERTY
};

/* private data */
struct _ModemSIMServicePrivate
{
  guint state;
  char *imsi;

  GQueue queue[1];

  unsigned dispose_has_run:1, connected:1, signals:1, disconnected:1;
  unsigned connection_error:1;
  unsigned :0;
};

/* ------------------------------------------------------------------------ */
/* Local functions */

/* ------------------------------------------------------------------------ */

static void
modem_sim_service_init (ModemSIMService *self)
{
  DEBUG ("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      MODEM_TYPE_SIM_SERVICE, ModemSIMServicePrivate);
}

static void
modem_sim_service_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  ModemSIMService *self = MODEM_SIM_SERVICE (object);
  ModemSIMServicePrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_STATUS:
      g_value_set_uint (value, priv->state);
      break;

    case PROP_IMSI:
      g_value_set_string (value, priv->imsi);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_sim_service_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  ModemSIMService *self = MODEM_SIM_SERVICE (object);
  ModemSIMServicePrivate *priv = self->priv;
  gpointer old;

  switch (property_id)
    {
    case PROP_STATUS:
      priv->state = g_value_get_uint (value);
      break;

    case PROP_IMSI:
      old = priv->imsi;
      priv->imsi = g_value_dup_string (value);
      g_free (old);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_sim_service_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (modem_sim_service_parent_class)->constructed)
    G_OBJECT_CLASS (modem_sim_service_parent_class)->constructed (object);

  if (modem_oface_dbus_proxy (MODEM_OFACE (object)) == NULL)
	  g_warning("object created without dbus-proxy set");
}

static void
modem_sim_service_dispose (GObject *object)
{
  ModemSIMService *self = MODEM_SIM_SERVICE (object);
  ModemSIMServicePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;
  priv->disconnected = TRUE;
  priv->connected = FALSE;
  priv->signals = FALSE;

  g_object_set (self, "state", MODEM_SIM_STATE_UNKNOWN, NULL);

  while (!g_queue_is_empty (priv->queue))
    modem_request_cancel (g_queue_pop_head (priv->queue));

  if (G_OBJECT_CLASS (modem_sim_service_parent_class)->dispose)
    G_OBJECT_CLASS (modem_sim_service_parent_class)->dispose (object);
}

static void
modem_sim_service_finalize (GObject *object)
{
  ModemSIMService *self = MODEM_SIM_SERVICE (object);
  ModemSIMServicePrivate *priv = self->priv;

  DEBUG ("enter");

  /* Free any data held directly by the object here */
  g_free (priv->imsi);

  G_OBJECT_CLASS (modem_sim_service_parent_class)->finalize (object);
}

/* ------------------------------------------------------------------------- */
/* ModemOface interface */

static char const *
modem_sim_service_property_mapper (char const *name)
{
  if (!strcmp (name, "SubscriberIdentity"))
    return "imsi";
  if (!strcmp(name, "Present"))
      return NULL;
  if (!strcmp (name, "CardIdentifier"))
    return NULL;
  if (!strcmp (name, "MobileCountryCode"))
    return NULL;
  if (!strcmp (name, "MobileNetworkCode"))
    return NULL;
  if (!strcmp (name, "SubscriberNumbers"))
    return NULL;
  if (!strcmp (name, "ServiceNumbers"))
    return NULL;
  if (!strcmp (name, "PinRequired"))
    return NULL;
  if (!strcmp (name, "LockedPins"))
    return NULL;
  if (!strcmp (name, "FixedDialing"))
    return NULL;
  if (!strcmp (name, "BarredDialing"))
    return NULL;
  return NULL;
}

static void
modem_sim_service_connect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  modem_oface_connect_properties (_self, TRUE);
}

static void
modem_sim_service_connected (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);
}

static void
modem_sim_service_disconnect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  modem_oface_disconnect_properties (_self);
}

static void
modem_sim_service_class_init (ModemSIMServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ModemOfaceClass *oface_class = MODEM_OFACE_CLASS (klass);

  DEBUG ("enter");

  object_class->get_property = modem_sim_service_get_property;
  object_class->set_property = modem_sim_service_set_property;
  object_class->constructed = modem_sim_service_constructed;
  object_class->dispose = modem_sim_service_dispose;
  object_class->finalize = modem_sim_service_finalize;

  oface_class->property_mapper = modem_sim_service_property_mapper;
  oface_class->connect = modem_sim_service_connect;
  oface_class->connected = modem_sim_service_connected;
  oface_class->disconnect = modem_sim_service_disconnect;

  /* Properties */
  g_object_class_install_property (object_class, PROP_STATUS,
      g_param_spec_uint ("state",
          "SIM Status",
          "Current state of Subscriber identity module.",
          MODEM_SIM_STATE_UNKNOWN,
          LAST_MODEM_SIM_STATE - 1,
          MODEM_SIM_STATE_UNKNOWN,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_IMSI,
      g_param_spec_string ("imsi",
          "IMSI",
          "Internation Mobile Subscriber Identity",
          "", /* default value */
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));

  signals[SIGNAL_STATUS] =
    g_signal_new ("state",
        G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__UINT,
        G_TYPE_NONE, 1,
        G_TYPE_UINT);

  g_type_class_add_private (klass, sizeof (ModemSIMServicePrivate));

  modem_error_domain_prefix (0); /* Init errors */
}

ModemSIMState
modem_sim_get_state (ModemSIMService const *self)
{
  return MODEM_IS_SIM_SERVICE (self) ? self->priv->state : 0;
}

char const *
modem_sim_get_imsi (ModemSIMService const *self)
{
  return MODEM_IS_SIM_SERVICE (self) ? self->priv->imsi : NULL;
}

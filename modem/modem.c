/*
 * modem/modem.c - oFono modem
 *
 * Copyright (C) 2009, 2010 Nokia Corporation
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

#define MODEM_DEBUG_FLAG MODEM_SERVICE_MODEM

#include "debug.h"

#include "modem/modem.h"
#include "modem/request-private.h"
#include "modem/ofono.h"
#include "modem/errors.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/util.h>

#include "signals-marshal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

/* ------------------------------------------------------------------------ */

G_DEFINE_TYPE (Modem, modem, G_TYPE_OBJECT);

enum
{
  PROP_OBJECT_PATH = 1,
  PROP_POWERED,
  PROP_ONLINE,
  PROP_NAME,
  PROP_MANUFACTURER,
  PROP_MODEL,
  PROP_REVISION,
  PROP_SERIAL,
  PROP_FEATURES,
  PROP_INTERFACES,
  N_PROPS
};

/* private data */
struct _ModemPrivate
{
  DBusGProxy *proxy;

  /* Properties */
  gchar *object_path;
  gboolean powered;
  gboolean online;
  gchar *name;
  gchar *manufacturer;
  gchar *model;
  gchar *revision;
  gchar *serial;
  gchar **features;
  gchar **interfaces;

  unsigned dispose_has_run:1;
  unsigned :0;
};

/* ------------------------------------------------------------------------ */
/* Local functions */

static void on_modem_property_changed (DBusGProxy *, char const *,
    GValue const *, gpointer);

/* ------------------------------------------------------------------------ */

static void
modem_init (Modem *self)
{
  DEBUG ("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      MODEM_TYPE_MODEM, ModemPrivate);
}

static void
modem_constructed (GObject *object)
{
  Modem *self = MODEM_MODEM (object);
  ModemPrivate *priv = self->priv;

  priv->proxy = modem_ofono_proxy (priv->object_path, OFONO_IFACE_MODEM);

  if (priv->proxy)
    {
      modem_ofono_proxy_connect_to_property_changed (priv->proxy,
          on_modem_property_changed, self);
    }
  else
    g_error ("Unable to proxy oFono modem %s", priv->object_path);
}

static void
modem_dispose (GObject *object)
{
  Modem *self = MODEM_MODEM (object);
  ModemPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  modem_ofono_proxy_disconnect_from_property_changed (priv->proxy,
        on_modem_property_changed, self);

  g_object_run_dispose (G_OBJECT (priv->proxy));

  if (G_OBJECT_CLASS (modem_parent_class)->dispose)
    G_OBJECT_CLASS (modem_parent_class)->dispose (object);
}

static void
modem_finalize (GObject *object)
{
  Modem *self = MODEM_MODEM (object);
  ModemPrivate *priv = self->priv;

  DEBUG ("enter");

  /* Free any data held directly by the object here */

  g_object_unref (priv->proxy);

  g_free (priv->object_path);
  g_free (priv->name);
  g_free (priv->manufacturer);
  g_free (priv->model);
  g_free (priv->revision);
  g_free (priv->serial);
  g_strfreev (priv->features);
  g_strfreev (priv->interfaces);

  G_OBJECT_CLASS (modem_parent_class)->finalize (object);
}

static void
modem_set_property (GObject *obj,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
  Modem *self = MODEM_MODEM (obj);
  ModemPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_OBJECT_PATH:
      g_free (priv->object_path);
      priv->object_path = g_value_dup_boxed (value);
      break;

    case PROP_POWERED:
      priv->powered = g_value_get_boolean (value);
      break;

    case PROP_ONLINE:
      priv->online = g_value_get_boolean (value);
      break;

    case PROP_NAME:
      g_free (priv->name);
      priv->name = g_value_dup_string (value);
      break;

    case PROP_MANUFACTURER:
      g_free (priv->manufacturer);
      priv->manufacturer = g_value_dup_string (value);
      break;

    case PROP_MODEL:
      g_free (priv->model);
      priv->model = g_value_dup_string (value);
      break;

    case PROP_REVISION:
      g_free (priv->revision);
      priv->revision = g_value_dup_string (value);
      break;

    case PROP_SERIAL:
      g_free (priv->serial);
      priv->serial = g_value_dup_string (value);
      break;

    case PROP_FEATURES:
      g_strfreev (priv->features);
      priv->features = g_value_dup_boxed (value);
      break;

    case PROP_INTERFACES:
      g_strfreev (priv->interfaces);
      priv->interfaces = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
modem_get_property (GObject *obj,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
  Modem *self = MODEM_MODEM (obj);
  ModemPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_OBJECT_PATH:
      g_value_set_boxed (value, priv->object_path);
      break;

    case PROP_POWERED:
      g_value_set_boolean (value, priv->powered);
      break;

    case PROP_ONLINE:
      g_value_set_boolean (value, priv->online);
      break;

    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    case PROP_MANUFACTURER:
      g_value_set_string (value, priv->manufacturer);
      break;

    case PROP_MODEL:
      g_value_set_string (value, priv->model);
      break;

    case PROP_REVISION:
      g_value_set_string (value, priv->revision);
      break;

    case PROP_SERIAL:
      g_value_set_string (value, priv->serial);
      break;

    case PROP_FEATURES:
      g_value_set_boxed (value, priv->features);
      break;

    case PROP_INTERFACES:
      g_value_set_boxed (value, priv->interfaces);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
modem_class_init (ModemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  DEBUG ("enter");

  object_class->constructed = modem_constructed;
  object_class->dispose = modem_dispose;
  object_class->finalize = modem_finalize;
  object_class->get_property = modem_get_property;
  object_class->set_property = modem_set_property;

  g_type_class_add_private (klass, sizeof (ModemPrivate));

  g_object_class_install_property (object_class, PROP_OBJECT_PATH,
      g_param_spec_boxed ("object-path",
          "Object Path",
          "Object path of the modem on oFono",
          DBUS_TYPE_G_OBJECT_PATH,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_POWERED,
      g_param_spec_boolean ("powered",
          "Powered",
          "The power state of the modem device",
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ONLINE,
      g_param_spec_boolean ("online",
          "Online",
          "The radio state of the modem. Online is false in flight mode.",
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name",
          "Name",
          "Friendly name of the modem device.",
          "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MANUFACTURER,
      g_param_spec_string ("manufacturer",
          "Manufacturer",
          "The manufacturer of the modem device.",
          "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MODEL,
      g_param_spec_string ("model",
          "Model",
          "The model of the modem device.",
          "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_REVISION,
      g_param_spec_string ("revision",
          "Revision",
          "The revision of the modem device.",
          "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_SERIAL,
      g_param_spec_string ("serial",
          "Serial",
          "The serial number (IMEI) of the modem device.",
          "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FEATURES,
      g_param_spec_boxed ("features",
          "Features",
          "List of currently enabled features with simple "
          "string abbreviations like 'sms', 'sim' etc.",
          G_TYPE_STRV,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_INTERFACES,
      g_param_spec_boxed ("interfaces",
          "Interfaces",
          "Set of interfaces currently supported by the modem. "
          "The set depends on the state of the device "
          "(registration status, SIM inserted status, "
          "network capabilities, device capabilities, etc.)",
          G_TYPE_STRV,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}


/* -------------------------------------------------------------------------- */
/* modem interface */

char const *
modem_property_name_by_ofono_name (char const *name)
{
  if (!strcmp (name, "Powered"))
    return "powered";
  if (!strcmp (name, "Online"))
    return "online";
  if (!strcmp (name, "Name"))
    return "name";
  if (!strcmp (name, "Manufacturer"))
    return "manufacturer";
  if (!strcmp (name, "Model"))
    return "model";
  if (!strcmp (name, "Revision"))
    return "revision";
  if (!strcmp (name, "Serial"))
    return "serial";
  if (!strcmp (name, "Features"))
    return "features";
  if (!strcmp (name, "Interfaces"))
    return "interfaces";
  return NULL;
}

static void
on_modem_property_changed (DBusGProxy *proxy,
                           char const *property,
                           GValue const *value,
                           gpointer _self)
{
  property = modem_property_name_by_ofono_name (property);

  if (property)
    g_object_set_property (G_OBJECT (_self), property, value);
}

char const *
modem_get_modem_path (Modem const *self)
{
  if (!MODEM_IS_MODEM (self))
    return NULL;

  return dbus_g_proxy_get_path (self->priv->proxy);
}

gboolean
modem_is_powered (Modem const *self)
{
  g_return_val_if_fail (MODEM_IS_MODEM (self), FALSE);

  return self->priv->powered;
}

gboolean
modem_is_online (Modem const *self)
{
  g_return_val_if_fail (MODEM_IS_MODEM (self), FALSE);

  return self->priv->online;
}

gboolean
modem_has_interface (Modem const *self, char const *interface)
{
  guint i;

  g_return_val_if_fail (MODEM_IS_MODEM (self), FALSE);

  if (!self->priv->interfaces)
    return FALSE;

  for (i = 0; self->priv->interfaces[i]; i++)
    if (!strcmp (self->priv->interfaces[i], interface))
      return TRUE;

  return FALSE;
}

gboolean
modem_supports_sim (Modem const *self)
{
  return modem_has_interface (self, OFONO_IFACE_SIM);
}

gboolean
modem_supports_call (Modem const *self)
{
  return modem_has_interface (self, OFONO_IFACE_CALL_MANAGER);
}

gboolean
modem_supports_sms (Modem const *self)
{
  return modem_has_interface (self, OFONO_IFACE_SMS);
}

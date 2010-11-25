/*
 * modem/modem.c - oFono modem
 *
 * Copyright (C) 2009, 2010 Nokia Corporation
 *
 * @author Pekka Pessi <first.surname@nokia.com>
 * @author Kai Vehmanen <first.surname@nokia.com>
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
#include "modem/oface.h"
#include "modem/ofono.h"
#include "modem/call.h"
#include "modem/sim.h"
#include "modem/sms.h"
#include "modem/errors.h"
#include "modem/oface.h"

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

G_DEFINE_TYPE (Modem, modem, MODEM_TYPE_OFACE);

enum
{
  PROP_NONE,
  PROP_POWERED,
  PROP_ONLINE,
  PROP_NAME,
  PROP_MANUFACTURER,
  PROP_MODEL,
  PROP_REVISION,
  PROP_IMEI,
  PROP_FEATURES,
  PROP_INTERFACES,
  N_PROPS
};

enum
{
  SIGNAL_INTERFACE_ADDED,
  SIGNAL_INTERFACE_REMOVED,
  SIGNAL_IMSI_ADDED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

/* private data */
struct _ModemPrivate
{
  /* Properties */
  gboolean powered;
  gboolean online;
  gchar *name;
  gchar *manufacturer;
  gchar *model;
  gchar *revision;
  gchar *imei;
  gchar **features;
  gchar **interfaces;

  GHashTable *ifhash;
  GHashTable *connecting;
  GHashTable *ofaces;
};

/* ------------------------------------------------------------------------ */
/* Local functions */

static void on_notify_interfaces (Modem *, GParamSpec *, Modem *);
static void modem_update_interfaces (Modem *);
static void on_sim_notify_imsi (ModemSIMService *, GParamSpec *, Modem *);
static void on_oface_connected  (ModemOface *, gboolean, Modem *);

/* ------------------------------------------------------------------------ */

static void
modem_init (Modem *self)
{
  DEBUG ("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      MODEM_TYPE_MODEM, ModemPrivate);

  self->priv->ifhash = g_hash_table_new (g_str_hash, g_str_equal);
  self->priv->connecting = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);
  self->priv->ofaces = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);
}

static void
modem_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (modem_parent_class)->constructed)
    G_OBJECT_CLASS (modem_parent_class)->constructed (object);
}

static void
modem_dispose (GObject *object)
{
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

  g_free (priv->name);
  g_free (priv->manufacturer);
  g_free (priv->model);
  g_free (priv->revision);
  g_free (priv->imei);
  g_strfreev (priv->features);
  g_strfreev (priv->interfaces);

  g_hash_table_destroy (priv->ifhash);
  g_hash_table_destroy (priv->ofaces);
  g_hash_table_destroy (priv->connecting);

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

    case PROP_IMEI:
      g_free (priv->imei);
      priv->imei = g_value_dup_string (value);
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

    case PROP_IMEI:
      g_value_set_string (value, priv->imei);
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

/* ------------------------------------------------------------------------- */

/** Maps properties of org.ofono.Modem to matching Modem properties */
char const *
modem_property_mapper (char const *name)
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
    return "imei";
  if (!strcmp (name, "Features"))
    return "features";
  if (!strcmp (name, "Interfaces"))
    return "interfaces";
  return NULL;
}

static void
modem_connect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  modem_oface_connect_properties (_self, FALSE);

  g_signal_connect (_self, "notify::interfaces",
      G_CALLBACK(on_notify_interfaces), _self);

  modem_update_interfaces ( MODEM_MODEM(_self) );
}

static void
modem_disconnect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  Modem *self = MODEM_MODEM (_self);
  ModemPrivate *priv = self->priv;
  GHashTableIter iter[1];
  char *interface;
  ModemOface *oface;

  g_signal_handlers_disconnect_by_func (_self,
      G_CALLBACK(on_notify_interfaces), _self);

  for (g_hash_table_iter_init (iter, priv->connecting);
       g_hash_table_iter_next (iter, (gpointer)&interface, (gpointer)&oface);)
    {
      g_signal_handlers_disconnect_by_func (oface,
          on_oface_connected, self);
    }

  for (g_hash_table_iter_init (iter, priv->ofaces);
       g_hash_table_iter_next (iter, (gpointer)&interface, (gpointer)&oface);)
    {
      g_signal_handlers_disconnect_by_func (oface,
          on_oface_connected, self);

      if (MODEM_IS_SIM_SERVICE (oface))
        g_signal_handlers_disconnect_by_func (oface,
            on_sim_notify_imsi, self);
    }
}

static void
modem_class_init (ModemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ModemOfaceClass *oface_class = MODEM_OFACE_CLASS (klass);

  DEBUG ("enter");

  object_class->constructed = modem_constructed;
  object_class->dispose = modem_dispose;
  object_class->finalize = modem_finalize;
  object_class->get_property = modem_get_property;
  object_class->set_property = modem_set_property;

  oface_class->ofono_interface = MODEM_OFACE_MODEM;
  oface_class->property_mapper = modem_property_mapper;
  oface_class->connect = modem_connect;
  oface_class->disconnect = modem_disconnect;

  g_type_class_add_private (klass, sizeof (ModemPrivate));

  signals[SIGNAL_INTERFACE_ADDED] = g_signal_new ("interface-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, G_TYPE_OBJECT);

  signals[SIGNAL_INTERFACE_REMOVED] = g_signal_new ("interface-removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, G_TYPE_OBJECT);

  signals[SIGNAL_IMSI_ADDED] = g_signal_new ("imsi-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__STRING,
      G_TYPE_NONE, 1, G_TYPE_STRING);

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

  g_object_class_install_property (object_class, PROP_IMEI,
      g_param_spec_string ("imei",
          "Serial",
          "The IMEI (serial number) of the modem device.",
          "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FEATURES,
      g_param_spec_boxed ("features",
          "Features",
          "List of currently enabled features with simple "
          "string abbreviations like 'sms', 'sim' etc.",
          G_TYPE_STRV,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* XXX/KV: should this be removed as this is oFono-specific */
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

static void
on_sim_notify_imsi (ModemSIMService *sim,
                    GParamSpec *dummy,
                    Modem *self)
{
  gchar *imsi = NULL;

  DEBUG ("enter");

  g_object_get (sim, "imsi", &imsi, NULL);
  if (imsi == NULL)
    return;

  if (strlen (imsi))
    {
      DEBUG ("emitting imsi-added \"%s\" for %s", imsi,
          modem_oface_object_path (MODEM_OFACE (sim)));
      g_signal_emit (self, signals[SIGNAL_IMSI_ADDED], 0, imsi);
    }

  g_free (imsi);
}

static void
on_oface_connected (ModemOface *oface,
                    gboolean connected,
                    Modem *self)
{
  ModemPrivate *priv = self->priv;
  char *interface;

  DEBUG ("enter");

  g_object_get (oface, "interface", &interface, NULL);

  DEBUG ("%s %s interface = %s, oface = %p",
      connected ? "connected" : "disconnected",
      modem_oface_object_path (oface),
      interface, oface);

  if (!connected)
    {
      g_signal_handlers_disconnect_by_func (oface,
          on_oface_connected, self);

      if (MODEM_IS_SIM_SERVICE (oface))
        g_signal_handlers_disconnect_by_func (oface,
            on_sim_notify_imsi, self);

      if (g_hash_table_lookup (priv->connecting, interface))
        {
          g_hash_table_remove (priv->connecting, interface);
        }
      else if (g_hash_table_lookup (priv->ofaces, interface))
        {
          DEBUG("emitting interface-removed for %s", interface);
          g_signal_emit (self, signals[SIGNAL_INTERFACE_REMOVED], 0, oface);
          g_hash_table_remove (priv->ofaces, interface);
        }

      g_free (interface);

      return;
    }

  g_hash_table_insert (priv->ofaces, interface, g_object_ref (oface));
  g_hash_table_remove (priv->connecting, interface);

  DEBUG ("emitting interface-added for %s", interface);

  g_signal_emit (self, signals[SIGNAL_INTERFACE_ADDED], 0, oface);

  if (MODEM_IS_SIM_SERVICE (oface))
    {
      g_signal_connect (oface, "notify::imsi",
          G_CALLBACK(on_sim_notify_imsi), self);
      on_sim_notify_imsi (MODEM_SIM_SERVICE (oface), NULL, self);
    }
}

static void
modem_update_interfaces (Modem *self)
{
  ModemPrivate *priv = self->priv;
  ModemOface *oface;
  ModemOface *already;
  GHashTableIter iter[1];
  char const *object_path;
  GHashTable *prev_ifhash;
  char *interface;
  guint i;

  DEBUG ("enter");

  prev_ifhash = priv->ifhash;
  priv->ifhash = g_hash_table_new (g_str_hash, g_str_equal);

  for (i = 0; priv->interfaces[i]; i++)
    {
      interface = g_strdup (priv->interfaces[i]);
      g_hash_table_insert (priv->ifhash, interface, GUINT_TO_POINTER(1));
    }

  object_path = modem_oface_object_path (MODEM_OFACE (self));

  for (i = 0; priv->interfaces[i]; i++)
    {
      interface = priv->interfaces[i];

      if (g_hash_table_lookup (priv->ofaces, interface))
        continue;
      if (g_hash_table_lookup (prev_ifhash, interface))
        continue;

      oface = modem_oface_new (interface, object_path);
      if (oface == NULL) {
        DEBUG("Modem %s ignoring interface %s", object_path, interface);
        continue;
      }

      DEBUG("Modem %s adding interface %s", object_path, interface);

      already = g_hash_table_lookup (priv->connecting, interface);
      if (already)
        {
          /* interface was added, removed and added before it got connected */
          g_signal_handlers_disconnect_by_func (already,
              on_oface_connected, self);
        }

      g_hash_table_insert (priv->connecting, g_strdup (interface), oface);

      g_signal_connect (oface, "connected",
          G_CALLBACK (on_oface_connected), self);

      modem_oface_connect (oface);
    }

  g_hash_table_unref (prev_ifhash);

  DEBUG("All modem %s interfaces added", object_path);

 redo_connecting:

  for (g_hash_table_iter_init (iter, priv->connecting);
       g_hash_table_iter_next (iter, (gpointer)&interface, (gpointer)&oface);)
    {
      if (g_hash_table_lookup (priv->ifhash, interface) == NULL)
        {
          modem_oface_disconnect (oface);
          goto redo_connecting;
        }
    }

 redo_ofaces:

  for (g_hash_table_iter_init (iter, priv->ofaces);
       g_hash_table_iter_next (iter, (gpointer)&interface, (gpointer)&oface);)
    {
      if (g_hash_table_lookup (priv->ifhash, interface) == NULL)
        {
          modem_oface_disconnect (oface);
          goto redo_ofaces;
        }
    }
}

static void
on_notify_interfaces (Modem *self,
                      GParamSpec *dummy1,
                      Modem *dummy2)
{
  modem_update_interfaces (self);
}

/* -------------------------------------------------------------------------- */
/* modem interface */

char const *
modem_get_modem_path (Modem const *self)
{
  return modem_oface_object_path (MODEM_OFACE (self));
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

ModemOface *
modem_get_interface (Modem const *self, char const *interface)
{
  g_return_val_if_fail (MODEM_IS_MODEM (self), FALSE);

  return g_hash_table_lookup (self->priv->ofaces, interface);
}

gboolean
modem_has_interface (Modem const *self, char const *interface)
{
  g_return_val_if_fail (MODEM_IS_MODEM (self), FALSE);

  if (!self->priv->ifhash)
    return FALSE;

  return g_hash_table_lookup (self->priv->ifhash, interface) != NULL;
}

ModemOface **
modem_list_interfaces (Modem const *self)
{
  ModemPrivate *priv;
  GPtrArray *array;
  char *key;
  ModemOface *oface;
  GHashTableIter iter[1];

  g_return_val_if_fail (MODEM_IS_MODEM (self), g_new (ModemOface *, 1));

  priv = self->priv;
  array = g_ptr_array_sized_new (g_hash_table_size (priv->ofaces) + 1);

  for (g_hash_table_iter_init (iter, priv->ofaces);
       g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&oface);)
    {
      g_ptr_array_add(array, oface);
    }

  g_ptr_array_add (array, NULL);

  return (ModemOface **)g_ptr_array_free (array, FALSE);
}

gboolean
modem_supports_sim (Modem const *self)
{
  return modem_get_interface (self, MODEM_OFACE_SIM) != NULL;
}

gboolean
modem_supports_call (Modem const *self)
{
  return modem_get_interface (self, MODEM_OFACE_CALL_MANAGER) != NULL;
}

gboolean
modem_supports_sms (Modem const *self)
{
  return modem_get_interface (self, MODEM_OFACE_SMS) != NULL;
}

gboolean
modem_has_imsi (Modem const *self, gchar const *imsi)
{
  ModemSIMService *sim;
  gchar *sim_imsi;
  gboolean match;

  g_return_val_if_fail (MODEM_IS_MODEM (self), FALSE);
  g_return_val_if_fail (imsi != NULL, FALSE);

  sim = (ModemSIMService *)modem_get_interface (self, MODEM_OFACE_SIM);
  if (sim == NULL)
    return FALSE;

  g_object_get(sim, "imsi", &sim_imsi, NULL);

  match = strcmp(sim_imsi, imsi) == 0;

  g_free(sim_imsi);

  return match;
}

gboolean
modem_has_imei (Modem const *self, gchar const *imei)
{
  g_return_val_if_fail (MODEM_IS_MODEM (self), FALSE);
  g_return_val_if_fail (imei != NULL, FALSE);

  return g_strcmp0(self->priv->imei, imei) == 0;
}

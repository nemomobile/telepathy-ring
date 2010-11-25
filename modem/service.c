/*
 * modem/service.c - oFono modem service
 *
 * Copyright (C) 2009 Nokia Corporation
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

#include "modem/oface.h"
#include "modem/service.h"
#include "modem/request-private.h"
#include "modem/ofono.h"
#include "modem/errors.h"
#include "modem/modem.h"
#include "modem/sim.h"
#include "modem/call.h"
#include "modem/sms.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

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

G_DEFINE_TYPE(ModemService, modem_service, MODEM_TYPE_OFACE);

enum {
  SIGNAL_MODEM_ADDED,
  SIGNAL_MODEM_POWERED,
  SIGNAL_MODEM_REMOVED,
  SIGNAL_IMEI_ADDED,
  SIGNAL_IMSI_ADDED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

struct _ModemServicePrivate
{
  GHashTable *modems;

  unsigned signals:1, subscribed:1;
  unsigned :0;
};

/* ------------------------------------------------------------------------ */
/* Local functions */

static void reply_to_get_modems (ModemOface *_self, ModemRequest *request,
    GPtrArray *modems, GError const *error, gpointer user_data);
static void on_modem_added (DBusGProxy *, char const *, GHashTable *, gpointer);
static void on_modem_notify_imei (Modem *, GParamSpec *, ModemService *);
static void on_modem_notify_powered (Modem *, GParamSpec *, ModemService *);
static void on_modem_imsi_added (Modem *, char const *imsi, ModemService *);
static void on_modem_removed (DBusGProxy *, char const *, gpointer);

/* ------------------------------------------------------------------------ */

static void
modem_service_init(ModemService *self)
{
  DEBUG("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, MODEM_TYPE_SERVICE,
      ModemServicePrivate);

  self->priv->modems = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);
}

static void
modem_service_constructed(GObject *object)
{
  if (G_OBJECT_CLASS(modem_service_parent_class)->constructed)
    G_OBJECT_CLASS(modem_service_parent_class)->constructed(object);
}

static void
modem_service_dispose(GObject *object)
{
  if (G_OBJECT_CLASS(modem_service_parent_class)->dispose)
    G_OBJECT_CLASS(modem_service_parent_class)->dispose(object);
}

static void
modem_service_finalize(GObject *object)
{
  ModemService *self = MODEM_SERVICE(object);
  ModemServicePrivate *priv = self->priv;

  DEBUG("enter");

  /* Free any data held directly by the object here */
  g_hash_table_unref (priv->modems);

  G_OBJECT_CLASS(modem_service_parent_class)->finalize(object);
}

/* ------------------------------------------------------------------------- */
/* ModemOface implementation */

static void
reply_to_get_modems (ModemOface *_self,
                     ModemRequest *request,
                     GPtrArray *modem_list,
                     GError const *error,
                     gpointer user_data)
{
  DEBUG ("enter");

  if (modem_list)
    {
      guint i;

      for (i = 0; i < modem_list->len; i++)
        {
          GValueArray *va = g_ptr_array_index (modem_list, i);
          char const *path = g_value_get_boxed (va->values + 0);
          GHashTable *properties = g_value_get_boxed (va->values + 1);

          on_modem_added (NULL, path, properties, _self);
        }
    }

  modem_oface_check_connected (_self, request, error);
}

void
modem_service_connect (ModemOface *_self)
{
  ModemService *self = MODEM_SERVICE (_self);
  ModemServicePrivate *priv = self->priv;

  DEBUG("enter");

  if (!priv->signals)
    {
      DBusGProxy *proxy = modem_oface_dbus_proxy (_self);

      priv->signals = TRUE;

#define CONNECT(p, handler, name, signature...) \
    dbus_g_proxy_add_signal (p, (name), ##signature); \
    dbus_g_proxy_connect_signal (p, (name), G_CALLBACK (handler), self, NULL)

      CONNECT (proxy, on_modem_added, "ModemAdded",
          DBUS_TYPE_G_OBJECT_PATH, MODEM_TYPE_DBUS_DICT, G_TYPE_INVALID);

      CONNECT (proxy, on_modem_removed, "ModemRemoved",
          DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);

#undef CONNECT
    }

  if (!priv->subscribed)
    {
      DBusConnection *bus = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);

      /* Pre-subscribe to all interesting signals */

      if (bus)
        {
          priv->subscribed = 1;

          dbus_bus_add_match (bus,
              "type='signal',"
              "sender='org.ofono',"
              "interface='org.ofono.Modem',"
              "member='PropertyChanged'",
              NULL);
          dbus_bus_add_match (bus,
              "type='signal',"
              "sender='org.ofono',"
              "interface='org.ofono.SimManager',"
              "member='PropertyChanged'",
              NULL);
          dbus_bus_add_match (bus,
              "type='signal',"
              "sender='org.ofono',"
              "interface='org.ofono.MessageManager',"
              "member='PropertyChanged'",
              NULL);
          dbus_bus_add_match (bus,
              "type='signal',"
              "sender='org.ofono',"
              "interface='org.ofono.MessageManager',"
              "member='MessageAdded'",
              NULL);
          dbus_bus_add_match (bus,
              "type='signal',"
              "sender='org.ofono',"
              "interface='org.ofono.MessageManager',"
              "member='MessageRemoved'",
              NULL);
          dbus_bus_add_match (bus,
              "type='signal',"
              "sender='org.ofono',"
              "interface='org.ofono.VoiceCallManager',"
              "member='PropertyChanged'",
              NULL);
          dbus_bus_add_match (bus,
              "type='signal',"
              "sender='org.ofono',"
              "interface='org.ofono.VoiceCallManager',"
              "member='CallAdded'",
              NULL);
          dbus_bus_add_match (bus,
              "type='signal',"
              "sender='org.ofono',"
              "interface='org.ofono.VoiceCallManager',"
              "member='CallRemoved'",
              NULL);
        }
    }

  modem_oface_add_connect_request (_self,
      modem_oface_request_managed (_self, "GetModems",
          reply_to_get_modems, NULL));
}

void
modem_service_disconnect (ModemOface *_self)
{
  ModemService *self = MODEM_SERVICE (_self);
  ModemServicePrivate *priv = self->priv;

  DEBUG("enter");

  if (priv->signals)
    {
      DBusGProxy *proxy = modem_oface_dbus_proxy (_self);

      priv->signals = FALSE;

      dbus_g_proxy_disconnect_signal (proxy, "ModemAdded",
          G_CALLBACK (on_modem_added), self);
      dbus_g_proxy_disconnect_signal (proxy, "ModemRemoved",
          G_CALLBACK (on_modem_removed), self);
    }

  DEBUG("leave");
}

static void
modem_service_class_init(ModemServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  ModemOfaceClass *oface_class = MODEM_OFACE_CLASS (klass);

  DEBUG("enter");

  object_class->constructed = modem_service_constructed;
  object_class->dispose = modem_service_dispose;
  object_class->finalize = modem_service_finalize;

  oface_class->ofono_interface = MODEM_OFACE_MANAGER;
  oface_class->connect = modem_service_connect;
  oface_class->disconnect = modem_service_disconnect;

  signals[SIGNAL_MODEM_ADDED] = g_signal_new ("modem-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, MODEM_TYPE_MODEM);

  signals[SIGNAL_MODEM_POWERED] = g_signal_new ("modem-powered",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, MODEM_TYPE_MODEM);

  signals[SIGNAL_MODEM_REMOVED] = g_signal_new ("modem-removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, MODEM_TYPE_MODEM);

  signals[SIGNAL_IMEI_ADDED] = g_signal_new ("imei-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _modem__marshal_VOID__OBJECT_STRING,
      G_TYPE_NONE,
      2, MODEM_TYPE_MODEM, G_TYPE_STRING);

  signals[SIGNAL_IMSI_ADDED] = g_signal_new ("imsi-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _modem__marshal_VOID__OBJECT_STRING,
      G_TYPE_NONE,
      2, MODEM_TYPE_MODEM, G_TYPE_STRING);

  g_type_class_add_private(klass, sizeof (ModemServicePrivate));

  dbus_g_object_register_marshaller (_modem__marshal_VOID__STRING_BOXED,
      G_TYPE_NONE,
      G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);

  dbus_g_object_register_marshaller (_modem__marshal_VOID__BOXED_BOXED,
      G_TYPE_NONE,
      DBUS_TYPE_G_OBJECT_PATH, MODEM_TYPE_DBUS_DICT, G_TYPE_INVALID);

  dbus_g_object_register_marshaller (_modem__marshal_VOID__BOXED,
      G_TYPE_NONE,
      DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);

  modem_error_domain_prefix (0); /* Init errors */

  modem_ofono_init_quarks ();
}

/* ------------------------------------------------------------------------ */
/* modem_service interface */

/* -------------------------------------------------------------------------- */

ModemService *modem_service (void)
{
  static ModemService *service;

  if (!service) {
    modem_oface_register_type (MODEM_TYPE_SERVICE);
    modem_oface_register_type (MODEM_TYPE_MODEM);
    modem_oface_register_type (MODEM_TYPE_SIM_SERVICE);
    modem_oface_register_type (MODEM_TYPE_SMS_SERVICE);
    modem_oface_register_type (MODEM_TYPE_CALL_SERVICE);

    service = MODEM_SERVICE (modem_oface_new (MODEM_OFACE_MANAGER, "/"));
  }

  return service;
}

void
modem_service_refresh (ModemService *self)
{
  modem_oface_connect (MODEM_OFACE (self));
}

Modem *
modem_service_find_by_path (ModemService *self, char const *object_path)
{
  g_return_val_if_fail (MODEM_IS_SERVICE (self), NULL);

  if (object_path)
    return g_hash_table_lookup (self->priv->modems, object_path);
  else
    return NULL;
}

Modem *
modem_service_find_best (ModemService *self)
{
  char *key;
  Modem *modem;
  GHashTableIter iter[1];

  g_return_val_if_fail (MODEM_IS_SERVICE (self), NULL);

  for (g_hash_table_iter_init (iter, self->priv->modems);
       g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&modem);)
    {
      if (modem_is_online (modem))
        return modem;
    }

  for (g_hash_table_iter_init (iter, self->priv->modems);
       g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&modem);)
    {
      if (modem_is_powered (modem))
        return modem;
    }

  return NULL;
}

Modem *
modem_service_find_by_imsi (ModemService *self, char const *imsi)
{
  char *key;
  Modem *modem;
  GHashTableIter iter[1];

  g_return_val_if_fail (MODEM_IS_SERVICE (self), NULL);

  for (g_hash_table_iter_init (iter, self->priv->modems);
       g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&modem);)
    {
      if (modem_has_imsi(modem, imsi))
        return modem;
    }

  return NULL;
}

Modem *
modem_service_find_by_imei (ModemService *self, char const *imei)
{
  char *key;
  Modem *modem = NULL;
  GHashTableIter iter[1];

  g_return_val_if_fail (MODEM_IS_SERVICE (self), NULL);

  for (g_hash_table_iter_init (iter, self->priv->modems);
       g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&modem);)
    {
      if (modem_has_imei (modem, imei))
        return modem;
    }

  return NULL;
}

Modem **
modem_service_get_modems (ModemService *self)
{
  ModemServicePrivate *priv;
  GPtrArray *array;
  char *key;
  Modem *modem;
  GHashTableIter iter[1];

  g_return_val_if_fail (MODEM_IS_SERVICE (self), NULL);

  priv = self->priv;
  array = g_ptr_array_sized_new (g_hash_table_size (priv->modems) + 1);

  for (g_hash_table_iter_init (iter, priv->modems);
       g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&modem);)
    {
      g_ptr_array_add(array, modem);
    }

  g_ptr_array_add (array, NULL);

  return (Modem **)g_ptr_array_free (array, FALSE);
}

static void
on_modem_added (DBusGProxy *_dummy,
                char const *object_path,
                GHashTable *properties,
                gpointer userdata)
{
  ModemService *self = userdata;
  ModemServicePrivate *priv = self->priv;
  Modem *modem;

  if (DEBUGGING)
    modem_ofono_debug_managed ("ModemAdded", object_path, properties);

  modem = g_hash_table_lookup (priv->modems, object_path);
  if (modem)
    {
      DEBUG ("Modem %s already has object %p", object_path, (void *)modem);
      return;
    }

  modem = MODEM_MODEM (modem_oface_new (OFONO_IFACE_MODEM, object_path));
  if (modem == NULL)
    {
      DEBUG ("Cannot create modem object %s", object_path);
      return;
    }

  g_hash_table_insert (priv->modems, g_strdup (object_path), modem);

  modem_oface_update_properties (MODEM_OFACE (modem), properties);

  g_signal_connect (modem, "notify::imei",
      G_CALLBACK (on_modem_notify_imei), self);

  g_signal_connect (modem, "notify::powered",
      G_CALLBACK (on_modem_notify_powered), self);

  g_signal_connect (modem, "imsi-added",
      G_CALLBACK (on_modem_imsi_added), self);

  modem_oface_connect (MODEM_OFACE (modem));

  g_assert (modem_oface_is_connected (MODEM_OFACE (modem)));

  g_signal_emit (self, signals[SIGNAL_MODEM_ADDED], 0, modem);

  on_modem_notify_powered (modem, NULL, self);

  on_modem_notify_imei (modem, NULL, self);
}

static void
on_modem_notify_imei (Modem *modem,
                      GParamSpec *dummy,
                      ModemService *self)
{
  gchar *imei = NULL;

  g_object_get(modem, "imei", &imei, NULL);

  if (imei && strcmp (imei, ""))
    {
      DEBUG ("emitting \"%s\" with modem=%p (%s) imei=%s", "imei-added",
          modem, modem_oface_object_path (MODEM_OFACE (modem)), imei);
      g_signal_emit (self, signals[SIGNAL_IMEI_ADDED], 0, modem, imei);
    }

  g_free (imei);
}

static void
on_modem_notify_powered (Modem *modem,
                         GParamSpec *dummy,
                         ModemService *self)
{
  gboolean powered;

  g_object_get(modem, "powered", &powered, NULL);

  if (powered) {
    DEBUG ("emitting \"%s\" with modem=%p (%s)", "modem-powered",
        modem, modem_oface_object_path (MODEM_OFACE (modem)));
    g_signal_emit (self, signals[SIGNAL_MODEM_POWERED], 0, modem);
  }
}

static void
on_modem_imsi_added (Modem *modem,
                     char const *imsi,
                     ModemService *self)
{
  DEBUG ("emitting \"%s\" with modem=%p (%s) imsi=%s", "imsi-added",
      modem, modem_oface_object_path (MODEM_OFACE (modem)), imsi);
  g_signal_emit (self, signals[SIGNAL_IMSI_ADDED], 0, modem, imsi);
}

static void
on_modem_removed (DBusGProxy *proxy,
                  char const *object_path,
                  gpointer userdata)
{
  ModemService *self = userdata;
  ModemServicePrivate *priv = self->priv;
  Modem *modem;

  DEBUG ("ModemRemoved(%s)", object_path);

  modem = g_hash_table_lookup (priv->modems, object_path);
  if (!modem)
    return;

  modem_oface_disconnect (MODEM_OFACE (modem));

  g_signal_emit (self, signals[SIGNAL_MODEM_REMOVED], 0, modem);

  g_signal_handlers_disconnect_by_func (modem,
      G_CALLBACK (on_modem_imsi_added), self);

  g_signal_handlers_disconnect_by_func (modem,
      G_CALLBACK (on_modem_notify_imei), self);

  g_signal_handlers_disconnect_by_func (modem,
      G_CALLBACK (on_modem_notify_powered), self);

  g_hash_table_remove (priv->modems, object_path);
}

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

#include "modem/service.h"
#include "modem/request-private.h"
#include "modem/ofono.h"
#include "modem/errors.h"

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

G_DEFINE_TYPE(ModemService, modem_service, G_TYPE_OBJECT);

enum {
  SIGNAL_MODEM_ADDED,
  SIGNAL_MODEM_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

struct _ModemServicePrivate
{
  ModemRequest *refresh;

  DBusGProxy *proxy;

  GHashTable *modems;

  unsigned dispose_has_run:1, signals:1, subscribed:1;
  unsigned :0;
};

/* ------------------------------------------------------------------------ */
/* Local functions */

static void reply_to_get_modems (gpointer _self, ModemRequest *request,
    GPtrArray *modems, GError const *error, gpointer user_data);
static void on_modem_added (DBusGProxy *, char const *, GHashTable *, gpointer);
static void on_modem_removed (DBusGProxy *, char const *, gpointer);

/* ------------------------------------------------------------------------ */

static void
modem_service_init(ModemService *self)
{
  DEBUG("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, MODEM_TYPE_SERVICE, ModemServicePrivate);

  self->priv->modems = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);
}

static void
modem_service_constructed(GObject *object)
{
  ModemService *self = MODEM_SERVICE(object);
  ModemServicePrivate *priv = self->priv;

  priv->proxy = modem_ofono_proxy("/", OFONO_IFACE_MANAGER);

  if (!priv->proxy) {
    g_error("Unable to proxy oFono");
  }
}

static void
modem_service_dispose(GObject *object)
{
  ModemService *self = MODEM_SERVICE(object);
  ModemServicePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;
  priv->signals = FALSE;

  if (priv->refresh)
    modem_request_cancel (priv->refresh);
  priv->refresh = NULL;

  g_object_run_dispose(G_OBJECT(priv->proxy));

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

  g_object_unref (priv->proxy);

  g_hash_table_unref (priv->modems);

  G_OBJECT_CLASS(modem_service_parent_class)->finalize(object);
}

static void
modem_service_class_init(ModemServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  DEBUG("enter");

  object_class->constructed = modem_service_constructed;
  object_class->dispose = modem_service_dispose;
  object_class->finalize = modem_service_finalize;

  signals[SIGNAL_MODEM_ADDED] = g_signal_new ("modem-added",
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

  g_type_class_add_private(klass, sizeof (ModemServicePrivate));

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

ModemService *modem_service (void)
{
  static ModemService *service;

  if (!service)
    service = g_object_new (MODEM_TYPE_SERVICE, NULL);

  return service;
}

Modem *modem_service_find_modem (ModemService *self, char const *object_path)
{
  ModemServicePrivate *priv;
  char *key;
  Modem *modem;
  GHashTableIter iter[1];

  g_return_val_if_fail (MODEM_IS_SERVICE (self), NULL);

  priv = self->priv;

  if (object_path)
    return g_hash_table_lookup (priv->modems, object_path);

  for (g_hash_table_iter_init (iter, priv->modems);
       g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&modem);)
    {
      if (modem_is_online (modem))
        return modem;
    }

  for (g_hash_table_iter_init (iter, priv->modems);
       g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&modem);)
    {
      if (modem_is_powered (modem))
        return modem;
    }

  return NULL;
}

Modem **modem_service_get_modems(ModemService *self)
{
  ModemServicePrivate *priv = self->priv;
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

void
modem_service_refresh (ModemService *self)
{
  ModemServicePrivate *priv = self->priv;

  DEBUG("enter");

  if (!priv->signals)
    {
      priv->signals = TRUE;

      dbus_g_proxy_add_signal (priv->proxy,
          "ModemAdded", DBUS_TYPE_G_OBJECT_PATH, MODEM_TYPE_DBUS_DICT,
          G_TYPE_INVALID);
      dbus_g_proxy_connect_signal (priv->proxy,
          "ModemAdded", G_CALLBACK (on_modem_added), self, NULL);

      dbus_g_proxy_add_signal (priv->proxy,
          "ModemRemoved", DBUS_TYPE_G_OBJECT_PATH,
          G_TYPE_INVALID);
      dbus_g_proxy_connect_signal (priv->proxy,
          "ModemRemoved", G_CALLBACK (on_modem_removed), self, NULL);
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

  if (priv->refresh)
    return;

  priv->refresh = modem_ofono_request_descs (self,
      priv->proxy, "GetModems",
      reply_to_get_modems, NULL);
}

static void
reply_to_get_modems (gpointer _self,
                     ModemRequest *request,
                     GPtrArray *modem_list,
                     GError const *error,
                     gpointer user_data)
{
  ModemService *self = MODEM_SERVICE (_self);
  ModemServicePrivate *priv = self->priv;
  guint i;

  DEBUG ("enter");

  priv->refresh = NULL;

  if (error)
    return;

  for (i = 0; i < modem_list->len; i++) {
    GValueArray *va = g_ptr_array_index (modem_list, i);
    char const *path = g_value_get_boxed (va->values + 0);
    GHashTable *properties = g_value_get_boxed (va->values + 1);

    on_modem_added (priv->proxy, path, properties, self);
  }
}

static void
on_modem_added (DBusGProxy *proxy,
                char const *object_path,
                GHashTable *properties,
                gpointer userdata)
{
  ModemService *self = userdata;
  ModemServicePrivate *priv = self->priv;
  Modem *modem;
  GHashTableIter iter[1];
  char *name;
  GValue *value;

  if (DEBUGGING)
    modem_ofono_debug_desc ("ModemAdded", object_path, properties);

  modem = g_hash_table_lookup (priv->modems, object_path);
  if (modem)
    {
      DEBUG ("Modem %s already has object %p", object_path, (void *)modem);
      return;
    }

  modem = g_object_new (MODEM_TYPE_MODEM, "object-path", object_path, NULL);
  if (!modem)
    return;

  for (g_hash_table_iter_init (iter, properties);
       g_hash_table_iter_next (iter, (gpointer)&name, (gpointer)&value);)
    {
      char const *property = modem_property_name_by_ofono_name (name);

      if (property)
        g_object_set_property (G_OBJECT (modem), property, value);
    }

  g_hash_table_insert (priv->modems, g_strdup (object_path), modem);

  g_signal_emit (self, signals[SIGNAL_MODEM_ADDED], 0, modem);
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

  g_signal_emit (self, signals[SIGNAL_MODEM_REMOVED], 0, modem);

  g_hash_table_remove (priv->modems, object_path);
}

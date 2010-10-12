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

/* Signals we send */
enum {
  SIGNAL_CONNECTED,
  SIGNAL_MODEM_ADDED,
  SIGNAL_MODEM_REMOVED,
  N_SIGNALS
};

enum {
  PROP_OBJECT_PATH = 1,
  N_PROPS
};

static guint signals[N_SIGNALS] = {0};

/* private data */
struct _ModemServicePrivate
{
  struct {
    ModemRequest *request;
    GError *error;
  } connecting;

  DBusGProxy *proxy;
  DBusGProxy *modem;

  gchar *object_path;

  gchar **interfaces;

  unsigned dispose_has_run:1, connected:1, signals:1, disconnected:1;
  unsigned modem_online:1;
  unsigned have_call_manager:1;
  unsigned :0;
};

/* ------------------------------------------------------------------------ */
/* Local functions */

static void reply_to_get_modems(gpointer _self, ModemRequest *request,
    GPtrArray *modems, GError const *error, gpointer user_data);
static void on_modem_added(DBusGProxy *, char const *, GHashTable *, gpointer);
static void on_modem_removed(DBusGProxy *, char const *, gpointer);
static void modem_service_check_connected(ModemService *self);
static void on_modem_property_changed(DBusGProxy *, char const *,
  GValue const *, gpointer);

/* ------------------------------------------------------------------------ */

static void
modem_service_init(ModemService *self)
{
  DEBUG("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, MODEM_TYPE_SERVICE, ModemServicePrivate);
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
  priv->disconnected = TRUE;
  priv->connected = FALSE;
  priv->signals = FALSE;

  if (priv->connecting.request)
    modem_request_cancel(priv->connecting.request);
  priv->connecting.request = NULL;

  g_object_run_dispose(G_OBJECT(priv->proxy));
  if (priv->modem)
    g_object_run_dispose(G_OBJECT(priv->modem));

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

  g_object_unref(priv->proxy);
  if (priv->modem)
    g_object_unref(priv->modem);

  if (priv->connecting.error)
    g_clear_error(&priv->connecting.error);

  G_OBJECT_CLASS(modem_service_parent_class)->finalize(object);
}

static void
modem_service_set_property(GObject *obj,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  ModemService *self = MODEM_SERVICE(obj);
  ModemServicePrivate *priv = self->priv;

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_free(priv->object_path);
      priv->object_path = g_value_dup_boxed(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
modem_service_get_property(GObject *obj,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  ModemService *self = MODEM_SERVICE(obj);
  ModemServicePrivate *priv = self->priv;

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_value_set_boxed(value, priv->object_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
modem_service_class_init(ModemServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  DEBUG("enter");

  object_class->constructed = modem_service_constructed;
  object_class->dispose = modem_service_dispose;
  object_class->finalize = modem_service_finalize;
  object_class->get_property = modem_service_get_property;
  object_class->set_property = modem_service_set_property;

  signals[SIGNAL_CONNECTED] = g_signal_new("connected",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  g_type_class_add_private(klass, sizeof (ModemServicePrivate));

  dbus_g_object_register_marshaller(_modem__marshal_VOID__BOXED_BOXED,
      G_TYPE_NONE,
      DBUS_TYPE_G_OBJECT_PATH, MODEM_TYPE_DBUS_DICT, G_TYPE_INVALID);

  dbus_g_object_register_marshaller(_modem__marshal_VOID__BOXED,
      G_TYPE_NONE,
      DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);

  g_object_class_install_property(
    object_class, PROP_OBJECT_PATH,
    g_param_spec_boxed("object-path",
      "Object Path",
      "Object path of the modem on oFono",
      DBUS_TYPE_G_OBJECT_PATH,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  modem_error_domain_prefix(0); /* Init errors */

  modem_ofono_init_quarks();
}

/* ------------------------------------------------------------------------ */
/* modem_service interface */

/* Connect to service */
gboolean
modem_service_connect(ModemService *self)
{
  ModemServicePrivate *priv = self->priv;

  DEBUG("enter");

  if (self->priv->connected) {
    DEBUG("already connected");
    return TRUE;
  }

  if (self->priv->disconnected) {
    DEBUG("already disconnected");
    return TRUE;
  }

  priv->signals = TRUE;

  dbus_g_proxy_add_signal(priv->proxy,
      "ModemAdded", DBUS_TYPE_G_OBJECT_PATH, MODEM_TYPE_DBUS_DICT,
      G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(priv->proxy,
      "ModemAdded", G_CALLBACK(on_modem_added), self, NULL);

  dbus_g_proxy_add_signal(priv->proxy,
      "ModemRemoved", DBUS_TYPE_G_OBJECT_PATH,
      G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(priv->proxy,
      "ModemRemoved", G_CALLBACK(on_modem_removed), self, NULL);

  priv->connecting.request = modem_ofono_request_descs(self,
      priv->proxy, "GetModems",
      reply_to_get_modems, NULL);

  DEBUG("connecting");

  return TRUE;
}

static void
reply_to_get_modems(gpointer _self,
                    ModemRequest *request,
                    GPtrArray *modem_list,
                    GError const *error,
                    gpointer user_data)
{
  ModemService *self = MODEM_SERVICE(_self);
  ModemServicePrivate *priv = self->priv;
  guint i;

  DEBUG("enter");

  priv->connecting.request = NULL;

  if (error) {
    if (priv->connecting.error)
      g_clear_error(&priv->connecting.error);
    priv->connecting.error = g_error_copy(error);
    priv->connected = TRUE;
    g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
    return;
  }

  for (i = 0; i < modem_list->len; i++) {
    GValueArray *va = g_ptr_array_index(modem_list, i);
    char const *path = g_value_get_boxed(va->values + 0);
    GHashTable *properties = g_value_get_boxed(va->values + 1);

    on_modem_added(priv->proxy, path, properties, self);
  }
}

static void
on_modem_added(DBusGProxy *proxy,
               char const *object_path,
               GHashTable *properties,
               gpointer userdata)
{
  ModemService *self = userdata;
  ModemServicePrivate *priv = self->priv;
  GValue *value;

  if (DEBUGGING)
    modem_ofono_debug_desc("Modem", object_path, properties);

  if (priv->modem)
    return;

  if (priv->object_path && strcmp(object_path, priv->object_path))
    return;

  priv->modem = modem_ofono_proxy(object_path, OFONO_IFACE_MODEM);

  modem_ofono_proxy_connect_to_property_changed(priv->modem,
      on_modem_property_changed, self);

  value = g_hash_table_lookup(properties, "Powered");
  if (value && !g_value_get_boolean(value)) {
    GValue v[1];
    g_value_init (memset (v, 0, sizeof v), G_TYPE_BOOLEAN);
    g_value_set_boolean (v, TRUE);

    modem_ofono_proxy_set_property(priv->modem,
        "Powered", v,
        NULL, NULL, NULL);
  }

  value = g_hash_table_lookup(properties, "Interfaces");
  if (value)
    priv->interfaces = g_value_dup_boxed(value);

  value = g_hash_table_lookup(properties, "Online");
  if (value) {
    priv->modem_online = g_value_get_boolean(value);

    modem_service_check_connected(self);
  }
}

static void
on_modem_removed(DBusGProxy *proxy,
                 char const *object_path,
                 gpointer userdata)
{
  ModemService *self = userdata;
  ModemServicePrivate *priv = self->priv;

  DEBUG("ModemRemoved(%s)", object_path);

  if (priv->modem == NULL)
    return;

  if (strcmp(object_path, dbus_g_proxy_get_path(priv->modem)))
    return;

  if (modem_service_is_connecting(self)) {
    modem_ofono_proxy_disconnect_from_property_changed(priv->modem,
        on_modem_property_changed, self);
    g_object_run_dispose(G_OBJECT(priv->modem));
    priv->modem = NULL;

    if (priv->connecting.request)
      return;

    /* Try another? */
    priv->connecting.request = modem_ofono_request_descs(self,
        priv->proxy, "GetModems",
        reply_to_get_modems, NULL);

    DEBUG("re-connecting");
  }
  else if (priv->connected)
    modem_service_disconnect(self);
}

static void
modem_service_check_connected(ModemService *self)
{
  ModemServicePrivate *priv = self->priv;

  if (priv->connected || priv->disconnected)
    return;

  if (modem_service_is_connecting(self))
    return;

  int i;
  for (i = 0; priv->interfaces[i]; i++)
    if (!strcmp(priv->interfaces[i], OFONO_IFACE_SIM))
      break;

  if (priv->interfaces[i])
    priv->connected = TRUE;
  else
    modem_service_disconnect(self);

  g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
}

gboolean
modem_service_is_connected(ModemService *self)
{
  return MODEM_IS_SERVICE(self)
    && self->priv->connected
    && !self->priv->connecting.error;
}

gboolean
modem_service_is_connecting(ModemService *self)
{
  ModemServicePrivate *priv;

  if (!MODEM_IS_SERVICE(self))
    return FALSE;

  priv = self->priv;

  if (priv->connected || priv->disconnected)
    return FALSE;

  if (priv->connecting.request)
    return TRUE;

  if (!priv->modem)
    return TRUE;

  if (!priv->modem_online)
    return TRUE;

  if (!priv->interfaces)
    return TRUE;

  return FALSE;
}

void
modem_service_disconnect(ModemService *self)
{
  ModemServicePrivate *priv = self->priv;
  int was_connected;

  if (priv->disconnected)
    return;

  DEBUG("enter");

  was_connected = priv->connected;
  priv->disconnected = TRUE;
  priv->connected = FALSE;

  if (priv->signals) {
    dbus_g_proxy_disconnect_signal(priv->proxy, "ModemAdded",
	G_CALLBACK(on_modem_added), self);
    dbus_g_proxy_disconnect_signal(priv->proxy, "ModemRemoved",
	G_CALLBACK(on_modem_removed), self);

    priv->signals = FALSE;
  }

  if (priv->modem) {
    modem_ofono_proxy_disconnect_from_property_changed(priv->modem,
        on_modem_property_changed, self);
  }

  if (was_connected)
    g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
}

char const *
modem_service_get_modem_path(ModemService *self)
{
  g_return_val_if_fail(MODEM_IS_SERVICE(self), NULL);

  if (!self->priv->modem)
    return NULL;

  return dbus_g_proxy_get_path(self->priv->modem);
}

gboolean
modem_service_supports_call(ModemService *self)
{
  g_return_val_if_fail(MODEM_IS_SERVICE(self), FALSE);

  ModemServicePrivate *priv = self->priv;
  int i;

  if (priv->interfaces == NULL)
    return FALSE;

  for (i = 0; priv->interfaces[i]; i++)
    if (!strcmp(priv->interfaces[i], OFONO_IFACE_CALL_MANAGER))
      return TRUE;

  return FALSE;
}

gboolean
modem_service_supports_sms(ModemService *self)
{
  g_return_val_if_fail(MODEM_IS_SERVICE(self), FALSE);

  ModemServicePrivate *priv = self->priv;
  int i;

  if (priv->interfaces == NULL)
    return FALSE;

  for (i = 0; priv->interfaces[i]; i++)
    if (!strcmp(priv->interfaces[i], OFONO_IFACE_SMS))
      return TRUE;

  return FALSE;
}

static void
on_modem_property_changed(DBusGProxy *proxy,
  char const *property,
  GValue const *value,
  gpointer _self)
{
  ModemService *self = MODEM_SERVICE(_self);
  ModemServicePrivate *priv = self->priv;

  if (!strcmp(property, "Interfaces")) {
    g_strfreev(priv->interfaces);
    priv->interfaces = g_value_dup_boxed(value);
    modem_service_check_connected(self);
    return;
  }

  if (!strcmp(property, "Powered")) {
    gboolean powered = g_value_get_boolean(value);

    DEBUG("Powered = %d", powered);
    if (!powered)
      modem_service_disconnect(self);
    return;
  }

  if (!strcmp(property, "Online")) {
    priv->modem_online = g_value_get_boolean(value);

    modem_service_check_connected(self);
    return;
  }
}

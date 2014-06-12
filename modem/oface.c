/*
 * modem/oface.c - Abstract oFono D-Bus interface class
 *
 * Handle GetProperties, SetProperty and PropertyChanged signal
 *
 * Copyright (C) 2010 Nokia Corporation
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

#define MODEM_DEBUG_FLAG MODEM_LOG_MODEM

#include "debug.h"

#include "modem/oface.h"
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

G_DEFINE_TYPE (ModemOface, modem_oface, G_TYPE_OBJECT);

/* Properties */
enum
{
  PROP_NONE,
  PROP_INTERFACE,
  PROP_OBJECT_PATH,
  LAST_PROPERTY
};

/* Signals */
enum
{
  SIGNAL_CONNECTED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

/* private data */
struct _ModemOfacePrivate
{
  DBusGProxy *proxy;

  struct
  {
    GQueue queue[1];
    GError *error;
  } connecting;

  unsigned dispose_has_run:1, connected:1, signals:1, disconnected:1, :0;
};

/* ------------------------------------------------------------------------ */
/* Local functions */

static void modem_oface_set_object_path (ModemOface *self, char const *path);
static void on_property_changed (DBusGProxy *,
    char const *, GValue const *, gpointer);

/* ------------------------------------------------------------------------ */

static void
modem_oface_init (ModemOface *self)
{
  DEBUG ("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      MODEM_TYPE_OFACE, ModemOfacePrivate);

  g_queue_init (self->priv->connecting.queue);
}

static void
modem_oface_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  ModemOface *self = MODEM_OFACE (object);
  ModemOfacePrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_INTERFACE:
      g_value_set_string (value, dbus_g_proxy_get_interface (priv->proxy));
      break;

    case PROP_OBJECT_PATH:
      g_value_set_string (value, dbus_g_proxy_get_path (priv->proxy));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_oface_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  ModemOface *self = MODEM_OFACE (object);

  switch (property_id)
    {
    case PROP_OBJECT_PATH:
      modem_oface_set_object_path (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_oface_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (modem_oface_parent_class)->constructed)
    G_OBJECT_CLASS (modem_oface_parent_class)->constructed (object);

  if (MODEM_OFACE (object)->priv->proxy == NULL)
    g_warning("object created without dbus-proxy");
}

static void
modem_oface_dispose (GObject *object)
{
  ModemOface *self = MODEM_OFACE (object);
  ModemOfacePrivate *priv = self->priv;

  DEBUG ("enter");

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;

  if (!priv->disconnected)
    modem_oface_disconnect (self);

  g_clear_error (&priv->connecting.error);

  g_object_run_dispose (G_OBJECT (priv->proxy));

  if (G_OBJECT_CLASS (modem_oface_parent_class)->dispose)
    G_OBJECT_CLASS (modem_oface_parent_class)->dispose (object);
}

static void
modem_oface_finalize (GObject *object)
{
  ModemOface *self = MODEM_OFACE (object);

  DEBUG ("enter");

  /* Free any data held directly by the object here */
  if (self->priv->proxy)
    g_object_unref (self->priv->proxy);
  g_clear_error (&self->priv->connecting.error);

  G_OBJECT_CLASS (modem_oface_parent_class)->finalize (object);
}

static void
modem_oface_class_init (ModemOfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  DEBUG ("enter");

  object_class->get_property = modem_oface_get_property;
  object_class->set_property = modem_oface_set_property;
  object_class->dispose = modem_oface_dispose;
  object_class->constructed = modem_oface_constructed;
  object_class->finalize = modem_oface_finalize;

  g_type_class_add_private (klass, sizeof (ModemOfacePrivate));

  /* Properties */
  g_object_class_install_property (object_class, PROP_INTERFACE,
      g_param_spec_string ("interface",
          "Interface name",
          "D-Bus interface name",
          ".", /* default value */
          G_PARAM_READABLE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_OBJECT_PATH,
      g_param_spec_string ("object-path",
          "Modem object path",
          "D-Bus object path used to identify the modem",
          "/", /* default value */
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
          G_PARAM_STATIC_STRINGS));

  /* Signals to emit */
  signals[SIGNAL_CONNECTED] =
    g_signal_new ("connected", G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/* ------------------------------------------------------------------------- */
/* ModemOface factory */

static DBusGConnection *
modem_oface_get_bus (void)
{
  static DBusGConnection *bus = NULL;

  if (G_UNLIKELY (bus == NULL))
    bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);

  return bus;
}

static void
modem_oface_set_object_path (ModemOface *self,
                             char const *object_path)
{
  DBusGConnection *bus = modem_oface_get_bus ();
  char const *interface = MODEM_OFACE_GET_CLASS (self)->ofono_interface;

  self->priv->proxy =
    dbus_g_proxy_new_for_name (bus, OFONO_BUS_NAME, object_path, interface);
}

static GHashTable *modem_oface_types;

char const *
modem_oface_get_interface_name_by_type (GType type)
{
  ModemOfaceClass *klass;
  char const *interface = NULL;

  g_return_val_if_fail (G_TYPE_IS_OBJECT (type), NULL);

  klass = g_type_class_peek_static (type);
  if (klass)
    {
      g_return_val_if_fail (MODEM_IS_OFACE_CLASS (klass), NULL);
      interface = klass->ofono_interface;
    }
  else
    {
      klass = g_type_class_ref (type);

      if (MODEM_IS_OFACE_CLASS (klass))
        interface = klass->ofono_interface;
      else
        (void) MODEM_OFACE_CLASS (klass);

      g_type_class_unref (klass);
    }

  return interface;
}

GType
modem_oface_get_type_by_interface_name (char const *interface)
{
  gpointer type = g_hash_table_lookup (modem_oface_types, interface);

  if (type != NULL)
    return (GType) type;

  return G_TYPE_INVALID;
}

void
modem_oface_register_type (GType type)
{
  static gsize once = 0;
  gpointer interface;

  if (g_once_init_enter (&once))
    {
      modem_oface_types = g_hash_table_new_full (g_str_hash, g_str_equal,
          NULL, NULL);
      g_once_init_leave (&once, 1);
    }

  interface = (gpointer) modem_oface_get_interface_name_by_type (type);

  g_return_if_fail (interface);

  g_hash_table_insert (modem_oface_types, interface, (gpointer)type);
}


ModemOface *
modem_oface_new (char const *interface, char const *object_path)
{
  GType type = modem_oface_get_type_by_interface_name (interface);

  if (type != G_TYPE_INVALID)
    {
      return g_object_new (type, "object-path", object_path, NULL);
    }
  else
    {
      return NULL;
    }
}

/* ------------------------------------------------------------------------- */
/* Connecting API */

static gboolean modem_oface_set_connected (ModemOface *, gboolean connected);

gboolean
modem_oface_connect (ModemOface *self)
{
  g_return_val_if_fail (MODEM_IS_OFACE (self), FALSE);
  g_return_val_if_fail (self->priv->connected == FALSE, FALSE);
  g_return_val_if_fail (self->priv->disconnected == FALSE, FALSE);
  g_return_val_if_fail (self->priv->dispose_has_run == FALSE, FALSE);

  ModemOfacePrivate *priv = self->priv;
  void (*implement_connect) (ModemOface *);

  DEBUG ("enter");

  g_clear_error (&priv->connecting.error);

  implement_connect = MODEM_OFACE_GET_CLASS (self)->connect;
  if (implement_connect)
    {
      implement_connect (self);
    }

  if (g_queue_is_empty (priv->connecting.queue))
    {
      modem_oface_set_connected (self, priv->connecting.error == NULL);
    }

  DEBUG ("leave%s", priv->connected ? " already connected" :
      ! g_queue_is_empty (priv->connecting.queue) ? " while connecting" : "");

  return priv->connected || ! g_queue_is_empty (priv->connecting.queue);
}

static gboolean
modem_oface_set_connected (ModemOface *self, gboolean connected)
{
  g_return_val_if_fail (MODEM_IS_OFACE (self), FALSE);
  g_return_val_if_fail (self->priv->disconnected == FALSE, FALSE);
  g_return_val_if_fail (self->priv->dispose_has_run == FALSE, FALSE);

  ModemOfacePrivate *priv = self->priv;

  if (priv->connected || priv->disconnected || priv->dispose_has_run)
    return FALSE;

  priv->connected = connected != FALSE;

  if (connected && MODEM_OFACE_GET_CLASS (self)->connected)
    {
      MODEM_OFACE_GET_CLASS (self)->connected (self);
    }

  g_signal_emit (self, signals[SIGNAL_CONNECTED], 0, connected != FALSE);

  return TRUE;
}

void
modem_oface_add_connect_request (ModemOface *self,
                                 ModemRequest *request)
{
  DEBUG("enter");
  g_queue_push_tail (self->priv->connecting.queue, request);
}

void
modem_oface_check_connected (ModemOface *self,
                             ModemRequest *request,
                             GError const *error)
{
  ModemOfacePrivate *priv = self->priv;

  if (!g_queue_find (priv->connecting.queue, request))
    return;
  g_queue_remove (priv->connecting.queue, request);

  if (error)
    modem_oface_set_connecting_error (self, error);

  if (g_queue_is_empty (priv->connecting.queue))
    {
      modem_oface_set_connected (self, priv->connecting.error == NULL);
    }
}

void
modem_oface_set_connecting_error (ModemOface *self,
                                  GError const *error)
{
  ModemOfacePrivate *priv = self->priv;

  if (!priv->connecting.error ||
      priv->connecting.error->domain == DBUS_GERROR)
    {
      g_clear_error (&priv->connecting.error);
      g_set_error (&priv->connecting.error,
          error->domain, error->code, "%s", error->message);
    }
}

/** Disconnect from call service */
void
modem_oface_disconnect (ModemOface *self)
{
  DEBUG ("(%p): enter%s", self,
      self->priv->disconnected ? " (already done)" : "");

  ModemOfacePrivate *priv = self->priv;
  void (*implement_disconnect)(ModemOface *);
  unsigned was_connected = priv->connected;

  if (priv->disconnected)
    return;

  priv->connected = FALSE;
  priv->disconnected = TRUE;

  implement_disconnect = MODEM_OFACE_GET_CLASS (self)->disconnect;
  if (implement_disconnect)
    {
      implement_disconnect (self);
    }

  while (!g_queue_is_empty (priv->connecting.queue))
    {
      modem_request_cancel (g_queue_pop_head (priv->connecting.queue));
    }

  if (was_connected)
    g_signal_emit (self, signals[SIGNAL_CONNECTED], 0, FALSE);
}

static void
reply_to_connect_properties (ModemOface *self,
                             ModemRequest *request,
                             GHashTable *properties,
                             GError const *error,
                             gpointer dummy)
{
  if (properties)
    {
      modem_oface_update_properties (self, properties);
    }

  /* XXX/PP: GetProperties might return error randomly */
  /* XXX/PP: we should retry on error */
  modem_oface_check_connected (self, request, error);
}

void
modem_oface_connect_properties (ModemOface *self,
                                gboolean get_all)
{
  ModemOfacePrivate *priv = self->priv;

  if (!priv->signals)
    {
      priv->signals = TRUE;
      dbus_g_proxy_add_signal (priv->proxy, "PropertyChanged",
          G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
      dbus_g_proxy_connect_signal (priv->proxy, "PropertyChanged",
	  G_CALLBACK (on_property_changed), self, NULL);
    }

  if (!get_all)
    return;

  modem_oface_add_connect_request (self,
      modem_oface_request_properties (self,
          reply_to_connect_properties, NULL));
}

void
modem_oface_disconnect_properties (ModemOface *self)
{
  ModemOfacePrivate *priv = self->priv;

  if (priv->signals)
    {
      priv->signals = FALSE;

      dbus_g_proxy_disconnect_signal (priv->proxy, "PropertyChanged",
          G_CALLBACK (on_property_changed), self);
    }
}

gboolean
modem_oface_is_connected (ModemOface const *self)
{
  return MODEM_IS_OFACE (self) && self->priv->connected;
}

gboolean
modem_oface_is_connecting (ModemOface const *self)
{
  return MODEM_IS_OFACE (self)
    && !g_queue_is_empty (self->priv->connecting.queue);
}

static void
reply_to_get_properties (DBusGProxy *proxy,
			 DBusGProxyCall *call,
			 gpointer _request)
{
  GHashTable *properties = NULL;
  ModemRequest *request = _request;
  gpointer object = modem_request_object (request);
  ModemOfacePropertiesReply *callback = modem_request_callback (request);
  gpointer user_data = modem_request_user_data (request);
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error,
	  MODEM_TYPE_DBUS_DICT, &properties,
	  G_TYPE_INVALID))
    {
      modem_error_fix (&error);
    }

  if (callback)
    callback (object, request, properties, error, user_data);

  if (error)
    g_error_free (error);
  if (properties)
    g_hash_table_unref (properties);
}

ModemRequest *
modem_oface_request_properties (ModemOface *self,
                                ModemOfacePropertiesReply *callback,
                                gpointer user_data)
{
  DEBUG("enter");

  g_return_val_if_fail (MODEM_IS_OFACE(self), NULL);

  return modem_request_begin (self, self->priv->proxy,
      "GetProperties",
      reply_to_get_properties,
      G_CALLBACK (callback), user_data,
      G_TYPE_INVALID);
}

static void
on_property_changed (DBusGProxy *proxy,
                     char const *property,
                     GValue const *value,
                     gpointer _self)
{
  ModemOface *self = MODEM_OFACE (_self);
  char const *(*property_mapper)(char const *ofono_property);
  char const *gname;

  property_mapper = MODEM_OFACE_GET_CLASS (self)->property_mapper;
  if (!property_mapper)
    return;

  gname = property_mapper (property);

  if (DEBUGGING)
    {
      char *s = g_strdup_value_contents (value);
      DEBUG("%s = %s (as %s)", property, s, gname);
      g_free(s);
    }

  if (!gname)
    return;

  g_object_set_property (G_OBJECT (self), gname, value);
}

void
modem_oface_update_properties (ModemOface *self,
                               GHashTable *properties)
{
  GHashTableIter iter[1];
  char *name;
  GValue *value;

  DEBUG ("enter");

  for (g_hash_table_iter_init (iter, properties);
       g_hash_table_iter_next (iter, (gpointer)&name, (gpointer)&value);)
    {
      on_property_changed (NULL, name, value, self);
    }
}

static void
reply_to_set_property (DBusGProxy *proxy,
                       DBusGProxyCall *call,
                       void *_request)
{
  ModemRequest *request = _request;
  ModemOface *self = modem_request_object (request);
  ModemOfaceVoidReply *callback = modem_request_callback (request);
  gpointer user_data = modem_request_user_data (request);

  GError *error = NULL;

  dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID);
  if (callback)
    callback (self, request, error, user_data);
  g_clear_error (&error);
}

ModemRequest *
modem_oface_set_property_req (ModemOface *self,
                              char const *property,
                              GValue *value,
                              ModemOfaceVoidReply *callback,
                              gpointer user_data)
{
  ModemOfacePrivate *priv = self->priv;

  if (DEBUGGING)
    {
      char *s = g_strdup_value_contents (value);
      DEBUG ("%s.%s (%s, %s)",
          dbus_g_proxy_get_interface (priv->proxy), "SetProperty",
          property, s);
      g_free (s);
    }

  return modem_request_begin (self, priv->proxy, "SetProperty",
      reply_to_set_property,
      G_CALLBACK (callback), user_data,
      G_TYPE_STRING, property,
      G_TYPE_VALUE, value,
      G_TYPE_INVALID);
}

/* ------------------------------------------------------------------------- */
/* Managed */

static void
reply_to_get_managed (DBusGProxy *proxy,
                      DBusGProxyCall *call,
                      gpointer _request)
{
  GPtrArray *array = NULL;
  ModemRequest *request = _request;
  gpointer object = modem_request_object (request);
  ModemOfaceManagedReply *callback = modem_request_callback (request);
  gpointer user_data = modem_request_user_data (request);
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error,
          MODEM_TYPE_DBUS_MANAGED_ARRAY, &array,
          G_TYPE_INVALID))
    modem_error_fix (&error);

  if (callback)
    callback (object, request, array, error, user_data);

  if (error)
    g_error_free (error);
  if (array)
    g_ptr_array_free (array, TRUE);
}

ModemRequest *
modem_oface_request_managed (ModemOface *oface,
                             char const *method,
                             ModemOfaceManagedReply *callback,
                             gpointer userdata)
{
  ModemOfacePrivate *priv = oface->priv;
  return modem_request_begin (oface,
      priv->proxy, method, reply_to_get_managed,
      G_CALLBACK (callback), userdata,
      G_TYPE_INVALID);
}

/** Returns pointer to DBusGProxy.
 *
 * If you want to have a new reference, use g_object_get_property ().
 */
DBusGProxy *
modem_oface_dbus_proxy (ModemOface *self)
{
  return MODEM_OFACE (self)->priv->proxy;
}

/** Returns object path for the oface object.
 *
 * If you want to have a newly copied string, use g_object_get_property ().
 */
char const *
modem_oface_object_path (ModemOface *self)
{
  return dbus_g_proxy_get_path (MODEM_OFACE (self)->priv->proxy);
}

char const *
modem_oface_interface (ModemOface *self)
{
  return dbus_g_proxy_get_interface (MODEM_OFACE (self)->priv->proxy);
}


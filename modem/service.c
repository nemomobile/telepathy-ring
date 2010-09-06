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
    GQueue queue[1];
    GError *error;
  } connecting;

  DBusGProxy *manager;
  DBusGProxy *modem;

  gchar *object_path;

  time_t seldom;

  unsigned dispose_has_run:1, connected:1, signals:1, disconnected:1;
  unsigned modem_powered:1, mandatory_ifaces_satisfied:1;
  unsigned have_call_manager:1;
  unsigned :0;
};

/* ------------------------------------------------------------------------ */
/* Local functions */

static ModemOfonoPropsReply reply_to_manager_get_properties;
static ModemOfonoPropsReply reply_to_modem_get_properties;
static void on_manager_property_changed(DBusGProxy *, char const *,
  GValue const *, gpointer);
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

  priv->manager =
    modem_ofono_proxy("/", OFONO_IFACE_MANAGER);

  if (!priv->manager) {
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

  while (!g_queue_is_empty(priv->connecting.queue))
    modem_request_cancel(g_queue_pop_head(priv->connecting.queue));

  g_object_run_dispose(G_OBJECT(priv->manager));
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

  g_object_unref(priv->manager);
  if (priv->modem)
    g_object_unref(priv->modem);

  if (priv->connecting.error)
    g_clear_error(&priv->connecting.error);

  g_free (priv->object_path);

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

  signals[SIGNAL_CONNECTED] =
    g_signal_new("connected",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  g_type_class_add_private(klass, sizeof (ModemServicePrivate));

  dbus_g_object_register_marshaller
    (_modem__marshal_VOID__STRING_BOXED,
      G_TYPE_NONE, G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);

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


/* --------------------------------------------------------------------------------- */
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
  modem_ofono_proxy_connect_to_property_changed(
    priv->manager, on_manager_property_changed, self);

  g_queue_push_tail(
    priv->connecting.queue,
    modem_ofono_proxy_request_properties(
      priv->manager, reply_to_manager_get_properties, self, NULL));

  DEBUG("connecting");

  return TRUE;
}


static void
modem_service_check_interfaces(ModemService *self,
  char const **ifaces)
{
  int i;
  gboolean sim, call, sms;
  ModemServicePrivate *priv = self->priv;

  sim = call = sms = FALSE;

  if (ifaces) {
    for (i = 0; ifaces[i]; i++) {
      GQuark q = g_quark_try_string(ifaces[i]);

      if (q == 0)
        ;
      else if (q == OFONO_IFACE_QUARK_SIM)
        sim = TRUE;
      else if (q == OFONO_IFACE_QUARK_CALL_MANAGER)
        call = TRUE;
      else if (q == OFONO_IFACE_QUARK_SMS)
        sms = TRUE;
    }
  }

  priv->mandatory_ifaces_satisfied = sim;
  priv->have_call_manager = call;

  if (priv->mandatory_ifaces_satisfied) {
    DEBUG("interfaces satisfied (%sincluding CallManager)", call ? "" : "NOT ");

    if (!priv->connected &&
      !priv->connecting.error &&
      g_queue_is_empty (priv->connecting.queue)) {
      DEBUG("connected and interfaces satisfied");
      priv->connected = TRUE;
      g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
    }

  }
  else if (priv->connected) {
    modem_service_disconnect(self);
  }
}

static void
modem_service_check_connected(ModemService *self,
  ModemRequest *request,
  const GError **error)
{
  ModemServicePrivate *priv = self->priv;

  if (g_queue_find(priv->connecting.queue, request)) {
    g_queue_remove(priv->connecting.queue, request);

    if (error && *error) {
      if (priv->connecting.error)
        g_clear_error(&priv->connecting.error);
      priv->connecting.error = g_error_copy(*error);

      modem_critical(MODEM_SERVICE_MODEM, GERROR_MSG_FMT,
        GERROR_MSG_CODE(priv->connecting.error));
    }

    if (g_queue_is_empty(priv->connecting.queue)) {
      gboolean emit = TRUE;

      if (priv->connecting.error)
        priv->connected = FALSE;
      else if (priv->mandatory_ifaces_satisfied)
        priv->connected = TRUE;
      else {
        DEBUG("connected but interfaces not satisfied yet");
        priv->connected = FALSE;
        emit = FALSE;
      }

      if (emit)
        g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
    }
  }
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
  return MODEM_IS_SERVICE(self) &&
    !g_queue_is_empty(self->priv->connecting.queue);
}

static void
reply_to_modem_set_powered(gpointer _self,
  ModemRequest *request,
  const GError *error,
  gpointer user_data)
{
  ModemService *self = MODEM_SERVICE(_self);

  DEBUG("enter");

  if (!error)
    DEBUG("success");

  modem_service_check_connected(self, request, &error);
}

static ModemRequest *
request_modem_be_powered(ModemService *self, gboolean powered)
{
  GValue v[1];
  ModemServicePrivate *priv = self->priv;

  g_value_init (memset (v, 0, sizeof v), G_TYPE_BOOLEAN);
  g_value_set_boolean (v, powered);

  return
    modem_ofono_proxy_set_property(
      priv->modem, "Powered", v, reply_to_modem_set_powered,
      self, NULL);
}

void
modem_service_disconnect(ModemService *self)
{
  ModemServicePrivate *priv = self->priv;
  int was_connected = priv->connected;

  if (priv->disconnected)
    return;

  DEBUG("enter");

  priv->disconnected = TRUE;
  priv->connected = FALSE;

  if (priv->signals) {
    modem_ofono_proxy_disconnect_from_property_changed(
      priv->manager, on_manager_property_changed, self);
    priv->signals = FALSE;
  }

  while (!g_queue_is_empty(priv->connecting.queue))
    modem_request_cancel(g_queue_pop_head(priv->connecting.queue));

  if (was_connected)
    g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
}

char const *
modem_service_get_modem_path(ModemService *self)
{
  if (!MODEM_IS_SERVICE(self))
    return NULL;

  return dbus_g_proxy_get_path(self->priv->modem);
}

gboolean
modem_service_supports_call(ModemService *self)
{
  g_return_val_if_fail(MODEM_IS_SERVICE(self), FALSE);

  return self->priv->have_call_manager;
}

/* ---------------------------------------------------------------------- */

static void
reply_to_modem_get_properties(gpointer _self,
  ModemRequest *request,
  GHashTable *properties,
  GError const *error,
  gpointer user_data)
{
  ModemService *self = MODEM_SERVICE(_self);
  ModemServicePrivate *priv = self->priv;

  DEBUG("enter");

  if (!error) {
    char *key;
    GValue *value;
    GHashTableIter iter[1];

    value = g_hash_table_lookup(properties, "Powered");
    if (value) {
      priv->modem_powered = g_value_get_boolean(value);
      if (!priv->modem_powered) {
        g_queue_push_tail(priv->connecting.queue,
          request_modem_be_powered(self, TRUE));
      }
    }

    value = g_hash_table_lookup(properties, "Interfaces");
    if (value) {
      modem_service_check_interfaces(
        self, g_value_get_boxed(value));
    }

    g_hash_table_iter_init(iter, properties);
    while (g_hash_table_iter_next(iter, (gpointer)&key, (gpointer)&value)) {
      char *s = g_strdup_value_contents(value);
      DEBUG("%s = %s", key, s);
      g_free(s);
    }
  }

  modem_service_check_connected(self, request, &error);
}

static void
reply_to_manager_get_properties(gpointer _self,
  ModemRequest *request,
  GHashTable *properties,
  GError const *error,
  gpointer user_data)
{
  ModemService *self = MODEM_SERVICE(_self);
  ModemServicePrivate *priv = self->priv;
  GError *error0 = NULL;

  DEBUG("enter");

  if (!error) {
    GPtrArray *paths = tp_asv_get_boxed (properties, "Modems",
        TP_ARRAY_TYPE_OBJECT_PATH_LIST);

    if (paths == NULL || paths->len == 0) {
      error0 = g_error_new_literal (MODEM_OFONO_ERRORS,
          MODEM_OFONO_ERROR_FAILED, "No modems found");
    } else if (priv->object_path != NULL) {
      /* We know which modem we want; check it exists. */
      guint i;
      gboolean found = FALSE;

      for (i = 0; !found && i < paths->len; i++) {
        const gchar *path = g_ptr_array_index (paths, i);

        found = !tp_strdiff (path, priv->object_path);
      }

      if (!found)
        error0 = g_error_new (MODEM_OFONO_ERRORS, MODEM_OFONO_ERROR_FAILED,
            "Modem '%s' not found", priv->object_path);
    } else {
      /* Just take the first one. */
      priv->object_path = g_strdup (g_ptr_array_index (paths, 0));
    }

    if (error0 == NULL) {
      DEBUG("Modem = %s", priv->object_path);
      priv->modem = modem_ofono_proxy(priv->object_path, OFONO_IFACE_MODEM);
      modem_ofono_proxy_connect_to_property_changed(
        priv->modem, on_modem_property_changed, self);

      g_queue_push_tail(
        priv->connecting.queue,
        modem_ofono_proxy_request_properties(
          priv->modem, reply_to_modem_get_properties, self, NULL));
    }
  }

  if (error == NULL && error0 != NULL)
    error = error0;

  modem_service_check_connected(self, request, &error);

  if (error0 != NULL)
    g_clear_error (&error0);
}

static void
on_manager_property_changed(DBusGProxy *proxy,
  char const *property,
  GValue const *value,
  gpointer _self)
{
  char *s;

  s = g_strdup_value_contents(value);
  DEBUG("%s = %s", property, s);
  g_free(s);

  if (!strcmp(property, "Modems")) {
    /* TODO: if the modem went away, disconnect? */
  }
}

static void
on_modem_property_changed(DBusGProxy *proxy,
  char const *property,
  GValue const *value,
  gpointer _self)
{
  char *s;
  ModemService *self = MODEM_SERVICE(_self);
  ModemServicePrivate *priv = self->priv;

  if (!strcmp(property, "Interfaces")) {
    modem_service_check_interfaces(
      self, g_value_get_boxed(value));

  }
  else if (!strcmp(property, "Powered")) {
    gboolean powered = g_value_get_boolean(value);

    DEBUG("Powered = %d", powered);
    if (priv->modem_powered != powered) {
      priv->modem_powered = powered;
      if (!powered)
        modem_service_disconnect(self);
    }
  }
  else {
    s = g_strdup_value_contents(value);
    DEBUG("%s = %s", property, s);
    g_free(s);
  }
}

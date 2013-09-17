/*
 * ring-text-manager.c - Manager for text channels
 *
 * Copyright (C) 2007-2010 Nokia Corporation
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

/*
 * Based on telepathy-glib/examples/cm/echo/factory.c with notice:
 *
 * """
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 * """
 */

#include "config.h"

#define DEBUG_FLAG RING_DEBUG_CONNECTION
#include "ring-debug.h"

#include "ring-text-manager.h"
#include "ring-text-channel.h"

#include "ring-connection.h"
#include "ring-param-spec.h"
#include "ring-util.h"

#include <modem/sms.h>
#include <modem/oface.h>

#include <dbus/dbus-glib.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>

#include <uuid/uuid.h>

#include <string.h>

static void channel_manager_iface_init(gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE(
  RingTextManager,
  ring_text_manager,
  G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_MANAGER,
    channel_manager_iface_init));

enum
{
  PROP_NONE,
  PROP_CONNECTION,
  PROP_SMS_SERVICE,
  PROP_CAPABILITY_FLAGS,      /**< Channel-type-specific capabilities */
  PROP_SMSC,                  /**< SMSC address */
  PROP_SMS_VALID,             /**< SMS validity period in seconds */
  PROP_SMS_REDUCED_CHARSET,   /**< SMS reduced character set */
  N_PROPS
};

struct _RingTextManagerPrivate
{
  RingConnection *connection;
  char *smsc;
  guint sms_valid;
  guint capability_flags;

  /* object_path => RingTextChannel */
  GHashTable *channels;

  TpConnectionStatus status, cstatus;

  ModemSMSService *sms_service;

  guint sms_reduced_charset :1;

  struct {
    gulong incoming_message, immediate_message;
    gulong outgoing_sms_complete, outgoing_sms_error;
    gulong receiving_sms_status_report;
#if nomore
    gulong receiving_sms_deliver;
#endif
    gulong status_changed;
  } signals;
};

/* ------------------------------------------------------------------------ */

static void ring_text_manager_set_sms_service (RingTextManager *,
    ModemSMSService *);

static void ring_text_manager_connected(RingTextManager *self);

static void ring_text_manager_disconnect(RingTextManager *self);

static void on_connection_status_changed (TpBaseConnection *conn,
    guint status, guint reason,
    RingTextManager *self);

static void foreach_dispose (gpointer, gpointer, gpointer);

static gboolean ring_text_requestotron(RingTextManager *self,
  gpointer request,
  GHashTable *properties,
  gboolean require_mine);

static RingTextChannel *ring_text_manager_request(RingTextManager *self,
  gpointer request,
  TpHandle initiator,
  TpHandle target,
  gboolean require_mine,
  gboolean class0);

static gboolean tp_asv_get_sms_channel (GHashTable *properties);

static void on_text_channel_closed(RingTextChannel *, RingTextManager *);

static void on_sms_service_outgoing_complete(ModemSMSService *,
  char const *token,
  char const *destination,
  gpointer _self);

static void on_sms_service_outgoing_error(ModemSMSService *,
  char const *token,
  char const *destination,
  GError const *error,
  gpointer _self);

static void on_sms_service_status_report(ModemSMSService *,
  char const *destination,
  char const *token,
  gboolean const *success,
  gpointer _self);

static void ring_text_manager_receive_status_report(RingTextManager *self,
  char const *destination,
  char const *token,
  gboolean const *success);

#if nomore
static void on_sms_service_deliver(ModemSMSService *,
  SMSGDeliver *, gpointer _self);
static void ring_text_manager_receive_deliver(
  RingTextManager *, SMSGDeliver *);
static void ring_text_manager_receive_status_report(
  RingTextManager *, SMSGStatusReport *);
#endif

static void on_incoming_message (ModemSMSService *,
    gchar const *message,
    GHashTable *info,
    gpointer user_data);
static void on_immediate_message (ModemSMSService *,
    gchar const *message,
    GHashTable *info,
    gpointer user_data);

/* ------------------------------------------------------------------------ */
/* GObject interface */

static void
ring_text_manager_constructed(GObject *object)
{
  RingTextManager *self = RING_TEXT_MANAGER(object);
  RingTextManagerPrivate *priv = self->priv;

  priv->signals.status_changed = g_signal_connect (priv->connection,
      "status-changed", (GCallback) on_connection_status_changed, self);

  if (G_OBJECT_CLASS(ring_text_manager_parent_class)->constructed)
    G_OBJECT_CLASS(ring_text_manager_parent_class)->constructed(object);
}

static void
ring_text_manager_init (RingTextManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, RING_TYPE_TEXT_MANAGER,
               RingTextManagerPrivate);

  self->priv->channels = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, g_object_unref);
}

static void
ring_text_manager_dispose(GObject *object)
{
  RingTextManager *self = RING_TEXT_MANAGER(object);
  RingTextManagerPrivate *priv = self->priv;

  ring_signal_disconnect (priv->connection, &priv->signals.status_changed);

  ring_text_manager_disconnect (self);

  g_hash_table_foreach (priv->channels, foreach_dispose, NULL);
  g_hash_table_remove_all (priv->channels);

  G_OBJECT_CLASS(ring_text_manager_parent_class)->dispose(object);
}

static void
ring_text_manager_finalize(GObject *object)
{
  RingTextManager *self = RING_TEXT_MANAGER(object);
  RingTextManagerPrivate *priv = self->priv;

  /* Free any data held directly by the object here */
  g_free(priv->smsc);
  g_hash_table_destroy (priv->channels);

  G_OBJECT_CLASS(ring_text_manager_parent_class)->finalize(object);
}

static void
ring_text_manager_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  RingTextManager *self = RING_TEXT_MANAGER(object);
  RingTextManagerPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CONNECTION:
      g_value_set_object(value, priv->connection);
      break;
    case PROP_SMS_SERVICE:
      g_value_set_pointer (value, priv->sms_service);
      break;
    case PROP_SMSC:
      g_value_set_string(value, priv->smsc ? priv->smsc : "");
      break;
    case PROP_SMS_VALID:
      g_value_set_uint(value, priv->sms_valid);
      break;
    case PROP_SMS_REDUCED_CHARSET:
      g_value_set_boolean(value, priv->sms_reduced_charset);
      break;
    case PROP_CAPABILITY_FLAGS:
      g_value_set_uint(value, priv->capability_flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
ring_text_manager_set_property(GObject *object,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  RingTextManager *self = RING_TEXT_MANAGER(object);
  RingTextManagerPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CONNECTION:
      /* We don't ref the connection, because it owns a reference to the
       * manager, and it guarantees that the manager's lifetime is
       * less than its lifetime */
      priv->connection = g_value_get_object(value);
      break;

    case PROP_SMS_SERVICE:
      ring_text_manager_set_sms_service (self, g_value_get_pointer (value));
      break;
    case PROP_SMSC:
      priv->smsc = g_value_dup_string(value);
      if (priv->sms_service)
        g_object_set(priv->sms_service, "service-centre", priv->smsc, NULL);
      break;
    case PROP_SMS_VALID:
      priv->sms_valid = g_value_get_uint(value);
      if (priv->sms_service)
        g_object_set(priv->sms_service, "validity-period", priv->sms_valid, NULL);
      break;
    case PROP_SMS_REDUCED_CHARSET:
      priv->sms_reduced_charset = g_value_get_boolean(value);
      if (priv->sms_service)
        g_object_set(priv->sms_service, "reduced-charset",
          priv->sms_reduced_charset, NULL);
      break;
    case PROP_CAPABILITY_FLAGS:
      priv->capability_flags = g_value_get_uint(value) &
        RING_TEXT_CHANNEL_CAPABILITY_FLAGS;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
ring_text_manager_class_init(RingTextManagerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private(klass, sizeof (RingTextManagerPrivate));

  object_class->constructed = ring_text_manager_constructed;
  object_class->get_property = ring_text_manager_get_property;
  object_class->set_property = ring_text_manager_set_property;
  object_class->dispose = ring_text_manager_dispose;
  object_class->finalize = ring_text_manager_finalize;

  g_object_class_install_property(
    object_class, PROP_CONNECTION, ring_param_spec_connection());
  g_object_class_install_property (object_class,
      PROP_SMS_SERVICE, ring_param_spec_sms_service (0));
  g_object_class_install_property(
    object_class, PROP_SMSC, ring_param_spec_smsc());
  g_object_class_install_property(
    object_class, PROP_SMS_VALID, ring_param_spec_sms_valid());
  g_object_class_install_property(
    object_class, PROP_SMS_REDUCED_CHARSET,
    ring_param_spec_sms_reduced_charset());
  g_object_class_install_property(object_class, PROP_CAPABILITY_FLAGS,
    ring_param_spec_type_specific_capability_flags(G_PARAM_CONSTRUCT,
      RING_TEXT_CHANNEL_CAPABILITY_FLAGS));
}

/* ---------------------------------------------------------------------- */

static void
ring_text_manager_set_sms_service (RingTextManager *self,
                                   ModemSMSService *service)
{
  RingTextManagerPrivate *priv = self->priv;

  if (priv->sms_service)
    {
      ring_text_manager_disconnect (self);
    }

  if (service)
    {
      priv->sms_service = g_object_ref (MODEM_SMS_SERVICE (service));
      ring_text_manager_connected (self);
    }
}

gboolean
ring_text_manager_is_connected (void const * _self)
{
  RingTextManager const *self = _self;

  if (RING_IS_TEXT_MANAGER (_self))
    return self->priv->sms_service != NULL;

  return FALSE;
}

static void
ring_text_manager_connected (RingTextManager *self)
{
  RingTextManagerPrivate *priv = self->priv;
  ModemSMSService *sms = priv->sms_service;

  priv->signals.incoming_message =
    modem_sms_connect_to_incoming_message (sms,
        on_incoming_message, self);
  priv->signals.immediate_message =
    modem_sms_connect_to_immediate_message (sms,
        on_immediate_message, self);

  priv->signals.outgoing_sms_complete =
    modem_sms_connect_to_outgoing_complete (sms,
        on_sms_service_outgoing_complete, self);

  priv->signals.outgoing_sms_error =
    modem_sms_connect_to_outgoing_error (sms,
        on_sms_service_outgoing_error, self);

  priv->signals.receiving_sms_status_report =
    modem_sms_connect_to_status_report (sms,
        on_sms_service_status_report, self);

#if nomore
  priv->signals.receiving_sms_deliver =
    modem_sms_connect_to_deliver (sms, on_sms_service_deliver, self);

#endif
}

static void
ring_text_manager_disconnect (RingTextManager *self)
{
  RingTextManagerPrivate *priv = self->priv;
  ModemSMSService *sms = priv->sms_service;

  ring_signal_disconnect (sms, &priv->signals.incoming_message);
  ring_signal_disconnect (sms, &priv->signals.immediate_message);
  ring_signal_disconnect (sms, &priv->signals.outgoing_sms_complete);
  ring_signal_disconnect (sms, &priv->signals.outgoing_sms_error);
  ring_signal_disconnect (sms, &priv->signals.receiving_sms_status_report);

#if nomore
  ring_signal_disconnect (sms, &priv->signals.receiving_sms_deliver);
  ring_signal_disconnect (sms, &priv->signals.outgoing_sms_complete);
  ring_signal_disconnect (sms, &priv->signals.outgoing_sms_error);
#endif

  if (priv->sms_service)
    g_object_unref (priv->sms_service);
  priv->sms_service = NULL;
}

static void
on_connection_status_changed (TpBaseConnection *conn,
                              guint status,
                              guint reason,
                              RingTextManager *self)
{
  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      ring_text_manager_dispose (G_OBJECT (self));
    }
}

static void
foreach_dispose (gpointer key,
                 gpointer _channel,
                 gpointer user_data)
{
  /* Ensure "closed" has been emitted */
  if (!tp_base_channel_is_destroyed (_channel))
    {
      g_object_run_dispose (_channel);
    }
}

/* ---------------------------------------------------------------------- */
/* Insert channel-type specific capabilities into array */

void
ring_text_manager_add_capabilities(RingTextManager *self,
  guint handle,
  GPtrArray *returns)
{
  RingTextManagerPrivate *priv = RING_TEXT_MANAGER(self)->priv;
  char const *id = ring_connection_inspect_contact(priv->connection, handle);
  guint selfhandle = tp_base_connection_get_self_handle(
    (TpBaseConnection *)priv->connection);
  char *destination;

  if (id == NULL)
    return;

  /* Some UIs create channels even if they do not intend to send anything */
  /* Allow them to do so */

  destination = ring_text_channel_destination(id);

  if (handle == selfhandle || modem_sms_is_valid_address (destination)) {
    g_ptr_array_add(returns,
      ring_contact_capability_new(handle,
        TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_CONNECTION_CAPABILITY_FLAG_CREATE,
        RING_TEXT_CHANNEL_CAPABILITY_FLAGS));
  }

  g_free(destination);
}

/* ---------------------------------------------------------------------- */
/* TpChannelManagerIface interface */

static char const * const ring_text_channel_fixed_properties_list[] =
{
  TP_IFACE_CHANNEL ".TargetHandleType", /* Contact */
  TP_IFACE_CHANNEL ".ChannelType",      /* Text */
  NULL
};

static char const * const ring_text_channel_allowed_properties[] =
{
  TP_IFACE_CHANNEL ".TargetHandle",
  TP_IFACE_CHANNEL ".TargetID",
#if HAVE_TP_SMS_CHANNEL
  TP_IFACE_CHANNEL_INTERFACE_SMS ".SMSChannel",
#endif
  NULL
};


static GHashTable *
ring_text_channel_fixed_properties(void)
{
  static GHashTable *hash;

  if (hash)
    return hash;

  hash = g_hash_table_new(g_str_hash, g_str_equal);

  char const *key;
  GValue *value;

  key = TP_IFACE_CHANNEL ".TargetHandleType";
  value = tp_g_value_slice_new(G_TYPE_UINT);
  g_value_set_uint(value, TP_HANDLE_TYPE_CONTACT);

  g_hash_table_insert(hash, (gpointer)key, value);

  key = TP_IFACE_CHANNEL ".ChannelType";
  value = tp_g_value_slice_new(G_TYPE_STRING);
  g_value_set_static_string(value, TP_IFACE_CHANNEL_TYPE_TEXT);

  g_hash_table_insert(hash, (gpointer)key, value);

  return hash;
}

static void
ring_text_manager_foreach_channel_class(TpChannelManager *_self,
  TpChannelManagerChannelClassFunc func,
  gpointer userdata)
{
  /* Allow clients to request channels even when not connected */
  func(_self,
    ring_text_channel_fixed_properties(),
    ring_text_channel_allowed_properties,
    userdata);
}

static void
ring_text_manager_foreach_channel(TpChannelManager *_self,
  TpExportableChannelFunc func,
  gpointer user_data)
{
  RingTextManager *self = RING_TEXT_MANAGER(_self);
  GHashTableIter i[1];
  gpointer channel;

  for (g_hash_table_iter_init(i, self->priv->channels);
       g_hash_table_iter_next(i, NULL, &channel);)
    func(channel, user_data);
}

/** Create a new RingTextChannel */
static gboolean
ring_text_manager_create_channel(TpChannelManager *_self,
  gpointer request,
  GHashTable *properties)
{
  return ring_text_requestotron(RING_TEXT_MANAGER(_self), request, properties, 1);
}

/** Request a RingTextChannel */
static gboolean
ring_text_manager_request_channel(TpChannelManager *_self,
  gpointer request,
  GHashTable *properties)
{
  return ring_text_requestotron(RING_TEXT_MANAGER(_self), request, properties, 0);
}

static gboolean
ring_text_manager_ensure_channel(TpChannelManager *_self,
  gpointer request,
  GHashTable *properties)
{
  return ring_text_requestotron(RING_TEXT_MANAGER(_self), request, properties, 0);
}

static void
channel_manager_iface_init(gpointer ifacep,
  gpointer data)
{
  TpChannelManagerIface *iface = ifacep;

#define IMPLEMENT(x) iface->x = ring_text_manager_##x

  IMPLEMENT(foreach_channel);
  IMPLEMENT(foreach_channel_class);
  IMPLEMENT(create_channel);
  IMPLEMENT(request_channel);
  IMPLEMENT(ensure_channel);

#undef IMPLEMENT
}

static gboolean
ring_text_requestotron(RingTextManager *self,
  gpointer request,
  GHashTable *properties,
  gboolean require_mine)
{
  RingTextManagerPrivate *priv = self->priv;
  TpHandle initiator, target;

  initiator = priv->connection->parent.self_handle;
  target = tp_asv_get_uint32(properties, TP_IFACE_CHANNEL ".TargetHandle", NULL);

  if (target == 0 ||
    target == priv->connection->parent.self_handle ||
    target == priv->connection->anon_handle)
    return FALSE;

  if (!ring_properties_satisfy(properties,
      ring_text_channel_fixed_properties(),
      ring_text_channel_allowed_properties))
    return FALSE;

  if (!tp_asv_get_sms_channel (properties))
    return FALSE;

  ring_text_manager_request(self, request, initiator, target, require_mine, 0);
  return TRUE;
}

static gboolean tp_asv_get_sms_channel (GHashTable *properties)
{
  GValue *value = g_hash_table_lookup (properties,
      TP_IFACE_CHANNEL_INTERFACE_SMS ".SMSChannel");

  if (value == NULL)
    return TRUE;
  else if (!G_VALUE_HOLDS_BOOLEAN (value))
    return FALSE;
  else
    return g_value_get_boolean (value);
}


/* ---------------------------------------------------------------------- */
/* RingTextManager interface */

gpointer
ring_text_manager_lookup(RingTextManager *self,
  char const *object_path)
{
  RingTextManagerPrivate *priv = self->priv;
  return (RingTextChannel *)g_hash_table_lookup(priv->channels, object_path);
}

static RingTextChannel *
ring_text_manager_request(RingTextManager *self,
  gpointer request,
  TpHandle initiator,
  TpHandle handle,
  gboolean require_mine,
  gboolean class0)
{
  RingTextManagerPrivate *priv = self->priv;
  RingTextChannel *channel;
  char *object_path;

  object_path = g_strdup_printf("%s/%s%u",
                priv->connection->parent.object_path,
                class0 ? "flash" : "text",
                (unsigned)handle);

  channel = ring_text_manager_lookup(self, object_path);

  if (channel) {
    g_free(object_path);

    if (require_mine) {
      char *message;
      channel = NULL;
      message = g_strdup_printf("Cannot create: "
                "channel with target '%s' already exists",
                ring_connection_inspect_contact(priv->connection, handle));
      DEBUG("%s", message);
      if (request)
        tp_channel_manager_emit_request_failed(self,
          request, TP_ERRORS, TP_ERROR_NOT_AVAILABLE, message);
      g_assert(request);
      g_free(message);
      return NULL;
    }
    else {
      if (request)
        tp_channel_manager_emit_request_already_satisfied(
          self, request, TP_EXPORTABLE_CHANNEL(channel));
      return channel;
    }
  }

  channel = g_object_new (RING_TYPE_TEXT_CHANNEL,
      "connection", self->priv->connection,
      "object-path", object_path,
      "handle-type", TP_HANDLE_TYPE_CONTACT,
      "handle", handle,
      "initiator-handle", initiator,
      "requested", request != NULL,
      "sms-flash", class0,
      NULL);
  g_free(object_path);

  g_object_get(channel, "object_path", &object_path, NULL);

  g_hash_table_insert(priv->channels, object_path, channel);

  g_signal_connect(channel, "closed", (GCallback)on_text_channel_closed, self);

  GSList *requests = request ? g_slist_prepend(NULL, request) : NULL;

  tp_channel_manager_emit_new_channel(
    self, TP_EXPORTABLE_CHANNEL(channel), requests);

  DEBUG("New channel emitted");

  g_slist_free(requests);

  return channel;
}

static void
on_text_channel_closed(RingTextChannel *channel, RingTextManager *self)
{
  char *object_path;
  gboolean really_destroyed;
  guint handle;

  g_object_get(channel,
    "object-path", &object_path,
    "channel-destroyed", &really_destroyed,
    "handle", &handle,
    NULL);

  if (self->priv->channels == NULL)
    really_destroyed = TRUE;

  DEBUG(": %s is %s", object_path,
    really_destroyed ? "destroyed" : "about to be recycled");

  tp_channel_manager_emit_channel_closed(self, object_path);

  if (self->priv->channels == NULL)
    ;
  else if (really_destroyed) {
    g_hash_table_remove(self->priv->channels, object_path);
  }
  else {
    g_object_set(channel,
      "initiator", handle,
      "requested", FALSE,
      NULL);
    tp_channel_manager_emit_new_channel(self,
      (TpExportableChannel *)channel,
      NULL);
  }
  g_free(object_path);
}

static RingTextChannel *
get_text_channel(RingTextManager *self,
  char const *address,
  gboolean class0,
  gboolean self_invoked)
{
  TpHandleRepoIface *repo;
  RingTextChannel *channel = NULL;
  TpHandle handle, initiator;
  GError *error = NULL;

  g_return_val_if_fail (address != NULL, NULL);

  repo = tp_base_connection_get_handles(
    (TpBaseConnection *)self->priv->connection, TP_HANDLE_TYPE_CONTACT);

  error = NULL;
  handle = tp_handle_ensure(repo, address,
           ring_network_normalization_context(), &error);
  if (handle == 0) {
    DEBUG("tp_handle_ensure: %s: %s (%d@%s)", address, GERROR_MSG_CODE(error));
    g_clear_error(&error);
    /* Xyzzy */
    return NULL;
  }

  initiator = self_invoked ? self->priv->connection->parent.self_handle : handle;

  channel = ring_text_manager_request(self, NULL, initiator, handle, 0, class0);

  if (channel == NULL)
    tp_handle_unref(repo, handle);

  return channel;
}

static void
on_sms_service_outgoing_complete(ModemSMSService *service,
  char const *destination,
  char const *token,
  gpointer _self)
{
  RingTextManager *self = _self;
  RingTextManagerPrivate *priv = self->priv;
  RingTextChannel *channel;

  DEBUG("Outgoing complete to %s with %s", destination, token);

  if (priv->cstatus != TP_CONNECTION_STATUS_CONNECTED) {
    DEBUG("not yet connected, ignoring");
    return;
  }

  channel = get_text_channel(self, destination, 0, 1);

  if (channel)
    ring_text_channel_outgoing_sms_complete(channel, token);
}


static void
on_sms_service_outgoing_error(ModemSMSService *service,
  char const *destination,
  char const *token,
  GError const *error,
  gpointer _self)
{
  RingTextManager *self = _self;
  RingTextManagerPrivate *priv = self->priv;
  RingTextChannel *channel;

  DEBUG("Outgoing error to %s with %s", destination, token);

  if (priv->cstatus != TP_CONNECTION_STATUS_CONNECTED) {
    DEBUG("not yet connected, ignoring");
    return;
  }

  channel = get_text_channel(self, destination, 0, 1);

  if (channel)
    ring_text_channel_outgoing_sms_error(channel, token, error);
}

static void
on_sms_service_status_report(ModemSMSService *sms_service,
  char const *destination,
  char const *token,
  gboolean const *success,
  gpointer _self)
{
  ring_text_manager_receive_status_report(
    RING_TEXT_MANAGER(_self), destination, token, success);
}

#if nomore
static void
on_sms_service_deliver(ModemSMSService *sms_service,
  SMSGDeliver *deliver,
  gpointer _self)
{
  ring_text_manager_receive_deliver(RING_TEXT_MANAGER(_self), deliver);
}


#endif

/* ---------------------------------------------------------------------- */

static void
ring_text_manager_receive_status_report(RingTextManager *self,
		  char const *destination,
		  char const *token,
		  gboolean const *success)
{
  RingTextManagerPrivate *priv = self->priv;
  RingTextChannel *channel;

  if (priv->cstatus != TP_CONNECTION_STATUS_CONNECTED) {
    /* Uh-oh */
    DEBUG("not yet connected, ignoring");
    return;
  }

  channel = get_text_channel(self, destination, 0, 0);

  if (channel)
    ring_text_channel_receive_status_report(channel, token, success);
}

#if nomore
static void
ring_text_manager_receive_deliver(RingTextManager *self,
  SMSGDeliver *deliver)
{
  RingTextManagerPrivate *priv = self->priv;
  RingTextChannel *channel;

  char const *originator = sms_g_deliver_get_originator(deliver);
  glong delivered = sms_g_deliver_get_delivered(deliver);

  DEBUG("SMS-DELIVER from %s%s", originator, delivered ? " from spool" : "");

  if (!ring_text_channel_can_handle(deliver)) {
    DEBUG("cannot handle, ignoring");
    return;
  }

  if (priv->cstatus != TP_CONNECTION_STATUS_CONNECTED) {
    DEBUG("not yet connected, ignoring");
    return;
  }

  int class0 = sms_g_deliver_get_sms_class(deliver) == 0;

  channel = get_text_channel(self, originator, class0, 0);

  if (channel)
    ring_text_channel_receive_deliver(channel, deliver);
}

#endif

static char *
generate_token (void)
{
  char *token;
  uuid_t uu;

  token = g_new (gchar, 37);
  uuid_generate_random (uu);
  uuid_unparse_lower (uu, token);

  return token;
}

static void
receive_text (RingTextManager *self,
              RingTextChannel *channel,
              gchar const *message,
              GHashTable *info,
              guint32 sms_class)
{
  char const *sent;
  char *token;
  gint64 message_sent = 0;
  gint64 message_received = (gint64) time(NULL);

  sent = tp_asv_get_string (info, "SentTime");
  if (sent)
    message_sent = modem_sms_parse_time (sent);
  if (!message_sent)
    message_sent = message_received;

  token = generate_token ();
  ring_text_channel_receive_text (channel,
      token, message, message_sent, message_received, sms_class);
  g_free (token);
}

static void
on_incoming_message (ModemSMSService *sms,
                     gchar const *message,
                     GHashTable *info,
                     gpointer _self)
{
  RingTextManager *self = RING_TEXT_MANAGER (_self);
  char const *sender;
  RingTextChannel *channel;

  g_return_if_fail (info != NULL);
  g_return_if_fail (message != NULL);

  sender = tp_asv_get_string (info, "Sender");
  g_return_if_fail (sender != NULL);

  channel = get_text_channel (self, sender, 0, 0);
  g_return_if_fail (channel != NULL);

  receive_text (self, channel, message, info, G_MAXUINT32);
}

static void
on_immediate_message (ModemSMSService *sms,
                      gchar const *message,
                      GHashTable *info,
                      gpointer _self)
{
  RingTextManager *self = RING_TEXT_MANAGER (_self);
  char const *sender;
  RingTextChannel *channel;

  g_return_if_fail (info != NULL);
  g_return_if_fail (message != NULL);

  sender = tp_asv_get_string (info, "Sender");
  g_return_if_fail (sender != NULL);

  channel = get_text_channel (self, sender, 0, 0);
  g_return_if_fail (channel != NULL);

  receive_text (self, channel, message, info, 0);
}

/* ---------------------------------------------------------------------- */
/* StoredMessages interface */

#if nomore
static void
ring_text_manager_not_connected(gpointer context)
{
  GError error =
    { TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "SMS service is not available" };
  dbus_g_method_return_error(context, &error);
}

void
ring_text_manager_deliver_stored_messages(RingTextManager *self,
  char const **messages,
  gpointer context)
{
#if nomore
  /* XXX/KV: use ring_text_manager_is_connected () */
  if (self->priv->status == TP_CONNECTION_STATUS_CONNECTED) {
    int i;
    ModemSMSService *service = self->priv->sms_service;

    for (i = 0; messages[i]; i++) {
      gpointer m = modem_sms_get_stored_message(service, messages[i]);

      if (m == NULL) {
        DEBUG("%s: not in received", messages[i]);
        continue;
      }

      if (SMS_G_IS_DELIVER(m)) {
        ring_text_manager_receive_deliver(self, m);
      }
      else if (SMS_G_IS_STATUS_REPORT(m)) {
        ring_text_manager_receive_status_report(self, m);
      }
      else {
        DEBUG("unknown %s in received", G_OBJECT_TYPE_NAME(m));
      }
    }

    dbus_g_method_return(context);
  }
  else {
    ring_text_manager_not_connected(context);
  }
#endif
}

void
ring_text_manager_expunge_messages(RingTextManager *self,
  char const **messages,
  gpointer context)
{
  /* XXX/KV: use ring_text_manager_is_connected () */
  if (self->priv->status == TP_CONNECTION_STATUS_CONNECTED) {
    int i;
    ModemSMSService *service = self->priv->sms_service;
    GPtrArray *expunged = g_ptr_array_new();

    for (i = 0; messages[i]; i++) {
      if (modem_sms_request_expunge(service, messages[i], NULL, NULL)) {
        g_ptr_array_add(expunged, (gpointer)g_strdup(messages[i]));
      }
    }

    g_ptr_array_add(expunged, NULL);

    dbus_g_method_return(context);

    rtcom_tp_svc_connection_interface_stored_messages_emit_messages_expunged(
      self->priv->connection,
      (char const **)expunged->pdata);

    g_strfreev((char **)g_ptr_array_free(expunged, FALSE));
  }
  else {
    ring_text_manager_not_connected(context);
  }
}

static ModemSMSServiceReply ring_text_manager_set_storage_status_reply;

void
ring_text_manager_set_storage_status(RingTextManager *self,
  gboolean out_of_storage,
  gpointer context)
{
  /* XXX/KV: use ring_text_manager_is_connected () */
  if (self->priv->status == TP_CONNECTION_STATUS_CONNECTED)
    modem_sms_request_out_of_memory(
      self->priv->sms_service,
      out_of_storage,
      ring_text_manager_set_storage_status_reply,
      context);
  else
    ring_text_manager_not_connected(context);
}

static void
ring_text_manager_set_storage_status_reply(ModemSMSService *service,
  ModemRequest *request,
  GError *error,
  gpointer context)
{
  if (!error)
    dbus_g_method_return(context);
  else
    dbus_g_method_return_error(context, error);
}

char **
ring_text_manager_list_stored_messages(RingTextManager const *self)
{
  return modem_sms_list_stored(self->priv->sms_service);
}
#endif

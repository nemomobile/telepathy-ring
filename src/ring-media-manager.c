/*
 * ring-media-manager.c - Manager for media channels
 *
 * Copyright (C) 2007-2010 Nokia Corporation
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

#include "ring-media-manager.h"
#include "ring-media-channel.h"
#include "ring-call-channel.h"
#include "ring-connection.h"
#include "ring-param-spec.h"
#include "ring-util.h"

#include "ring-extensions/ring-extensions.h"

#include "modem/call.h"
#include "modem/tones.h"

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include <dbus/dbus-glib.h>

#include <string.h>

static void channel_manager_iface_init(gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE(
  RingMediaManager,
  ring_media_manager,
  G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_MANAGER,
    channel_manager_iface_init));

enum
{
  PROP_NONE,
  PROP_CONNECTION,
  PROP_CALL_SERVICE,
  PROP_ANON_MODES,
  PROP_CAPABILITY_FLAGS,
  N_PROPS
};

typedef enum
{
  METHOD_COMPATIBLE,
  METHOD_CREATE,
  METHOD_ENSURE
} RequestotronMethod;

/* ---------------------------------------------------------------------- */

static void ring_media_manager_set_call_service (RingMediaManager *self,
    ModemCallService *service);

static void ring_media_manager_connected (RingMediaManager *self);

static void ring_media_manager_disconnect(RingMediaManager *self);

static void on_connection_status_changed (TpBaseConnection *conn,
    guint status, guint reason,
    RingMediaManager *self);

static gboolean ring_media_requestotron(RingMediaManager *,
  gpointer,
  GHashTable *,
  RequestotronMethod what);

static char *ring_media_manager_new_object_path(RingMediaManager const *self,
  char const *type);

static gboolean ring_media_manager_outgoing_call(RingMediaManager *self,
  gpointer request,
  guint target,
  guint initial_remote,
  char const *emergency,
  gboolean initial_audio);

static void on_media_channel_closed(GObject *chan, RingMediaManager *self);

static void foreach_dispose (gpointer, gpointer, gpointer);

static gpointer ring_media_manager_lookup_by_peer(RingMediaManager *self,
  TpHandle handle);

static void on_modem_call_emergency_numbers_changed (ModemCallService *call_service,
    GParamSpec *dummy,
    RingMediaManager *self);

static void on_modem_call_incoming(ModemCallService *call_service,
  ModemCall *ci,
  char const *originator,
  RingMediaManager *self);

static void on_modem_call_created(ModemCallService *call_service,
  ModemCall *ci,
  char const *destination,
  RingMediaManager *self);

static void on_modem_call_user_connection(ModemCallService *call_service,
  gboolean active,
  RingMediaManager *self);

static void on_modem_call_removed (ModemCallService *, ModemCall *,
    RingMediaManager *);

#define METHOD(i, x) (i ## _ ## x)

/* ---------------------------------------------------------------------- */
/* GObject interface */

struct _RingMediaManagerPrivate
{
  RingConnection *connection;
  guint anon_modes;
  guint capability_flags;

  /* Hash by object path */
  GHashTable *channels;

  ModemCallService *call_service;
  ModemTones *tones;

  struct {
    gulong incoming, created, removed;
    gulong emergency_numbers, joined, user_connection;
    gulong status_changed;
  } signals;

  unsigned dispose_has_run:1, :0;
};

static void
ring_media_manager_init(RingMediaManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, RING_TYPE_MEDIA_MANAGER, RingMediaManagerPrivate);

  self->priv->channels = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, g_object_unref);

  self->priv->tones = g_object_new(MODEM_TYPE_TONES, NULL);
}

static void
ring_media_manager_constructed(GObject *object)
{
  RingMediaManager *self = RING_MEDIA_MANAGER(object);
  RingMediaManagerPrivate *priv = self->priv;

  priv->signals.status_changed = g_signal_connect (priv->connection,
      "status-changed", (GCallback) on_connection_status_changed, self);

  if (G_OBJECT_CLASS(ring_media_manager_parent_class)->constructed)
    G_OBJECT_CLASS(ring_media_manager_parent_class)->constructed(object);
}

static void
ring_media_manager_dispose(GObject *object)
{
  RingMediaManager *self = RING_MEDIA_MANAGER(object);
  RingMediaManagerPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;

  ring_signal_disconnect (priv->connection, &priv->signals.status_changed);

  ring_media_manager_disconnect (self);

  g_object_run_dispose (G_OBJECT (priv->tones));

  ((GObjectClass *) ring_media_manager_parent_class)->dispose(object);
}

static void
ring_media_manager_finalize(GObject *object)
{
  RingMediaManager *self = RING_MEDIA_MANAGER(object);
  RingMediaManagerPrivate *priv = self->priv;

  g_object_unref (priv->tones);
  g_hash_table_destroy (priv->channels);
}

static void
ring_media_manager_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  RingMediaManager *self = RING_MEDIA_MANAGER(object);
  RingMediaManagerPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CONNECTION:
      g_value_set_object(value, priv->connection);
      break;
    case PROP_CALL_SERVICE:
      g_value_set_pointer (value, priv->call_service);
      break;
    case PROP_ANON_MODES:
      g_value_set_uint(value, priv->anon_modes);
      break;
    case PROP_CAPABILITY_FLAGS:
      g_value_set_uint(value, priv->capability_flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
ring_media_manager_set_property(GObject *object,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  RingMediaManager *self = RING_MEDIA_MANAGER(object);
  RingMediaManagerPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CONNECTION:
      /* We don't ref the connection, because it owns a reference to the
       * manager, and it guarantees that the manager's lifetime is
       * less than its lifetime */
      priv->connection = g_value_get_object(value);
      break;

    case PROP_CALL_SERVICE:
      ring_media_manager_set_call_service (self, g_value_get_pointer (value));
      break;

    case PROP_ANON_MODES:
      priv->anon_modes = g_value_get_uint(value);
      break;

    case PROP_CAPABILITY_FLAGS:
      priv->capability_flags = g_value_get_uint(value) &
        RING_MEDIA_CHANNEL_CAPABILITY_FLAGS;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
ring_media_manager_class_init(RingMediaManagerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private(klass, sizeof (RingMediaManagerPrivate));

  object_class->constructed = ring_media_manager_constructed;
  object_class->dispose = ring_media_manager_dispose;
  object_class->finalize = ring_media_manager_finalize;
  object_class->get_property = ring_media_manager_get_property;
  object_class->set_property = ring_media_manager_set_property;

  g_object_class_install_property(object_class, PROP_CONNECTION,
    ring_param_spec_connection());

  g_object_class_install_property (object_class, PROP_CALL_SERVICE,
      g_param_spec_pointer ("call-service",
          "Call Manager Object",
          "oFono Call Manager",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(object_class, PROP_ANON_MODES,
    ring_param_spec_anon_modes());

  g_object_class_install_property(object_class, PROP_CAPABILITY_FLAGS,
    ring_param_spec_type_specific_capability_flags(G_PARAM_CONSTRUCT,
      RING_MEDIA_CHANNEL_CAPABILITY_FLAGS));
}

/* ---------------------------------------------------------------------- */
/* RingMediaManager interface */

static void
ring_media_manager_set_call_service (RingMediaManager *self,
                                     ModemCallService *service)
{
  RingMediaManagerPrivate *priv = self->priv;

  if (priv->call_service)
    {
      ring_media_manager_disconnect (self);
    }

  if (service)
    {
      priv->call_service = g_object_ref (MODEM_CALL_SERVICE (service));
      ring_media_manager_connected (self);
    }
}

static void
ring_media_manager_connected (RingMediaManager *self)
{
  RingMediaManagerPrivate *priv = self->priv;

  priv->signals.incoming =
    g_signal_connect(priv->call_service, "incoming",
      G_CALLBACK(on_modem_call_incoming), self);

  priv->signals.created =
    g_signal_connect(priv->call_service, "created",
      G_CALLBACK(on_modem_call_created), self);

  priv->signals.removed =
    g_signal_connect(priv->call_service, "removed",
      G_CALLBACK(on_modem_call_removed), self);

  priv->signals.user_connection =
    g_signal_connect(priv->call_service, "user-connection",
      G_CALLBACK(on_modem_call_user_connection), self);

  priv->signals.emergency_numbers =
    g_signal_connect(priv->call_service, "notify::emergency-numbers",
      G_CALLBACK(on_modem_call_emergency_numbers_changed), self);

  modem_call_service_resume (priv->call_service);
}

/** Disconnect from call service */
static void
ring_media_manager_disconnect(RingMediaManager *self)
{
  RingMediaManagerPrivate *priv = self->priv;

  ring_signal_disconnect (priv->call_service, &priv->signals.incoming);
  ring_signal_disconnect (priv->call_service, &priv->signals.created);
  ring_signal_disconnect (priv->call_service, &priv->signals.user_connection);
  ring_signal_disconnect (priv->call_service, &priv->signals.emergency_numbers);

  g_hash_table_foreach (priv->channels, foreach_dispose, NULL);
  g_hash_table_remove_all (priv->channels);

  if (priv->call_service)
    g_object_unref (priv->call_service);
  priv->call_service = NULL;
}

static gboolean
ring_media_manager_is_connected (RingMediaManager *self)
{
  return RING_IS_MEDIA_MANAGER (self) && self->priv->call_service != NULL;
}

static void
on_connection_status_changed (TpBaseConnection *conn,
                              guint status,
                              guint reason,
                              RingMediaManager *self)
{
  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      ring_media_manager_disconnect (self);
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

RingEmergencyServiceInfoList *
ring_media_manager_emergency_services(RingMediaManager *self)
{
  RingMediaManagerPrivate *priv = RING_MEDIA_MANAGER(self)->priv;
  char const * const * numbers = NULL;

  /*
   * If the list is queried without valid call_service,
   * default emergency number list is returned
   */

  numbers = modem_call_get_emergency_numbers (priv->call_service);

  return ring_emergency_service_info_list_default ((char **)numbers);
}

static void
on_modem_call_emergency_numbers_changed (ModemCallService *call_service,
                                         GParamSpec *dummy,
                                         RingMediaManager *self)
{
  RingMediaManagerPrivate *priv = RING_MEDIA_MANAGER(self)->priv;
  TpBaseConnection *base = TP_BASE_CONNECTION(priv->connection);
  gchar **numbers;
  RingEmergencyServiceInfoList *services;

  if (base->status != TP_CONNECTION_STATUS_CONNECTED)
    return;

  g_object_get (call_service, "emergency-numbers", &numbers, NULL);
  services = ring_emergency_service_info_list_default (numbers);

  METHOD (tp_svc_connection_interface_service_point,
      emit_service_points_changed) (priv->connection, services);

  ring_emergency_service_info_list_free(services);
  g_strfreev (numbers);
}

/* ---------------------------------------------------------------------- */
/* Insert channel-type specific capabilities into array */

void
ring_media_manager_add_capabilities(RingMediaManager *self,
  guint handle,
  GPtrArray *returns)
{
  RingMediaManagerPrivate *priv = RING_MEDIA_MANAGER(self)->priv;
  char const *id = ring_connection_inspect_contact(priv->connection, handle);
  guint selfhandle = tp_base_connection_get_self_handle(
    (TpBaseConnection *)priv->connection);

  if (id == NULL)
    return;

  if (!ring_media_manager_is_connected (self))
    return;

  /* XXX - should check if we are in emergency call mode only status */

  if (handle == selfhandle) {
    if (priv->capability_flags)
      g_ptr_array_add(returns,
        ring_contact_capability_new(handle,
          TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
          TP_CONNECTION_CAPABILITY_FLAG_CREATE |
          TP_CONNECTION_CAPABILITY_FLAG_INVITE,
          RING_MEDIA_CHANNEL_CAPABILITY_FLAGS));
  }
  else if (modem_call_is_valid_address (id)) {
    g_ptr_array_add(returns,
      ring_contact_capability_new(handle,
        TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
        TP_CONNECTION_CAPABILITY_FLAG_CREATE |
        TP_CONNECTION_CAPABILITY_FLAG_INVITE,
        RING_MEDIA_CHANNEL_CAPABILITY_FLAGS));
  }
}

/* ---------------------------------------------------------------------- */
/* TpChannelManagerIface interface */

static GHashTable *
ring_call_channel_fixed_properties(void)
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
  g_value_set_static_string(value, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA);

  g_hash_table_insert(hash, (gpointer)key, value);

  return hash;
}

static char const * const ring_call_channel_allowed_properties[] =
{
  TP_IFACE_CHANNEL ".TargetHandle",
  TP_IFACE_CHANNEL ".TargetID",
  TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA ".InitialAudio",
  TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA ".InitialVideo",
  NULL
};

static GHashTable *
ring_anon_channel_fixed_properties(void)
{
  static GHashTable *hash;

  if (hash)
    return hash;

  hash = g_hash_table_new(g_str_hash, g_str_equal);

  char const *key;
  GValue *value;

  key = TP_IFACE_CHANNEL ".TargetHandleType";
  value = tp_g_value_slice_new(G_TYPE_UINT);
  g_value_set_uint(value, TP_HANDLE_TYPE_NONE);

  g_hash_table_insert(hash, (gpointer)key, value);

  key = TP_IFACE_CHANNEL ".ChannelType";
  value = tp_g_value_slice_new(G_TYPE_STRING);
  g_value_set_static_string(value, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA);

  g_hash_table_insert(hash, (gpointer)key, value);

  return hash;
}

static char const * const ring_anon_channel_allowed_properties[] =
{
  NULL
};

static void
ring_media_manager_foreach_channel_class(TpChannelManager *_self,
  TpChannelManagerChannelClassFunc func,
  gpointer userdata)
{
  RingMediaManager *self = RING_MEDIA_MANAGER(_self);

  /* If we're not connected, calls aren't supported. */
  if (self->priv->call_service == NULL)
    return;

  func(_self,
    ring_call_channel_fixed_properties(),
    ring_call_channel_allowed_properties,
    userdata);

#if nomore
  /* anon channels are only for compatibility for RequestChannel */
  func(_self,
    ring_anon_channel_fixed_properties(),
    ring_anon_channel_allowed_properties,
    userdata);
#endif
}

static void
ring_media_manager_foreach_channel(TpChannelManager *_self,
  TpExportableChannelFunc func,
  gpointer user_data)
{
  RingMediaManager *self = RING_MEDIA_MANAGER(_self);
  GHashTableIter i[1];
  gpointer channel;

  for (g_hash_table_iter_init(i, self->priv->channels);
       g_hash_table_iter_next(i, NULL, &channel);)
    func(channel, user_data);
}

/** Request a RingMediaChannel. */
static gboolean
ring_media_manager_request_channel(TpChannelManager *_self,
  gpointer request,
  GHashTable *properties)
{
  return ring_media_requestotron(
    RING_MEDIA_MANAGER(_self), request, properties, METHOD_COMPATIBLE);
}

/** Create a new RingMediaChannel. */
static gboolean
ring_media_manager_create_channel(TpChannelManager *_self,
  gpointer request,
  GHashTable *properties)
{
  return ring_media_requestotron(
    RING_MEDIA_MANAGER(_self), request, properties, METHOD_CREATE);
}

/** Ensure a new RingMediaChannel. */
static gboolean
ring_media_manager_ensure_channel(TpChannelManager *_self,
  gpointer request,
  GHashTable *properties)
{
  return ring_media_requestotron(
    RING_MEDIA_MANAGER(_self), request, properties, METHOD_ENSURE);
}

static void
channel_manager_iface_init(gpointer ifacep,
  gpointer data)
{
  TpChannelManagerIface *iface = ifacep;

#define IMPLEMENT(x) iface->x = ring_media_manager_##x

  IMPLEMENT(foreach_channel);
  IMPLEMENT(foreach_channel_class);
  IMPLEMENT(create_channel);
  IMPLEMENT(request_channel);
  IMPLEMENT(ensure_channel);

#undef IMPLEMENT
}

/* ---------------------------------------------------------------------- */

static gboolean
ring_media_requestotron(RingMediaManager *self,
  gpointer request,
  GHashTable *properties,
  RequestotronMethod kind)
{
  RingMediaManagerPrivate *priv = self->priv;
  TpHandle handle;

  /* If we're not connected, calls aren't supported. */
  if (self->priv->call_service == NULL)
    return FALSE;

  handle = tp_asv_get_uint32 (properties,
      TP_IFACE_CHANNEL ".TargetHandle", NULL);

  if (kind == METHOD_COMPATIBLE &&
    handle == 0 &&
    ring_properties_satisfy(properties,
      ring_anon_channel_fixed_properties(),
      ring_anon_channel_allowed_properties)) {
    return ring_media_manager_outgoing_call(self, request, 0, 0, NULL, FALSE);
  }

  if (tp_asv_get_initial_video (properties, FALSE))
    {
      tp_channel_manager_emit_request_failed (self, request,
          TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Video calls are not supported");
      return TRUE;
    }

  if (handle != 0 &&
    ring_properties_satisfy(properties,
      ring_call_channel_fixed_properties(),
      ring_call_channel_allowed_properties)) {
    RingCallChannel *channel;
    char const *target_id;
    GError *error = NULL;

    target_id = ring_connection_inspect_contact(priv->connection, handle);

    if (!modem_call_validate_address(target_id, &error)) {
      tp_channel_manager_emit_request_failed(
        self, request, TP_ERRORS, TP_ERROR_INVALID_HANDLE, error->message);
      g_error_free(error);
      return TRUE;
    }
    /* We do not yet support 'w' */
    else if (strchr(target_id, 'w')) {
      tp_channel_manager_emit_request_failed(
        self, request,
        TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
        "Dial strings containing 'w' are not supported");
      return TRUE;
    }

    if (kind == METHOD_ENSURE) {
      channel = ring_media_manager_lookup_by_peer(self, handle);
      if (channel) {
        tp_channel_manager_emit_request_already_satisfied(
          self, request, TP_EXPORTABLE_CHANNEL(channel));
        return TRUE;
      }
    }

    return ring_media_manager_outgoing_call(self, request,
      handle,
      kind == METHOD_COMPATIBLE ? handle : 0,
      modem_call_get_emergency_service(priv->call_service, target_id),
      tp_asv_get_initial_audio(properties, FALSE));
  }

  return FALSE;
}

gboolean
ring_media_manager_validate_initial_members(RingMediaManager *self,
  RingInitialMembers *initial,
  GError **error)
{
  if (initial == NULL) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "No initial members");
    return FALSE;
  }

  if (initial->len != 2) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Expecting exactly two initial members");
    return FALSE;
  }

  RingMemberChannel *ch1 = (RingMemberChannel *)
    ring_media_manager_lookup(self, initial->odata[0]);
  RingMemberChannel *ch2 = (RingMemberChannel *)
    ring_media_manager_lookup(self, initial->odata[1]);

  if (ch1 == ch2) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Expecting distinct initial members");
    return FALSE;
  }

  if (!RING_IS_MEMBER_CHANNEL(ch1) ||
    !RING_IS_MEMBER_CHANNEL(ch2)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Initial member is not a MediaStream channel");
    return FALSE;
  }

  if (!ring_member_channel_can_become_member(ch1, error)) {
    g_prefix_error(error, "First initial: ");
    return FALSE;
  }

  if (!ring_member_channel_can_become_member(ch2, error)) {
    g_prefix_error(error, "Second initial: ");
    return FALSE;
  }

  return TRUE;
}

static char *
ring_media_manager_new_object_path(RingMediaManager const *self,
  char const *type)
{
  RingMediaManagerPrivate const *priv = self->priv;
  char const *base_path = TP_BASE_CONNECTION(priv->connection)->object_path;

  static unsigned media_index;
  static unsigned media_index_init;

  if (!media_index_init) {
    media_index = (guint)(time(NULL) - (38 * 365 * 24 * 60 * 60)) * 10;
    media_index_init = 1;
  }

  /* Find an unique D-Bus object_path */
  for (;;) {
    char *path = g_strdup_printf("%s/%s%u", base_path, type, ++media_index);
    if (!g_hash_table_lookup(priv->channels, path)) {
      return path;
    }
    g_free(path);
  }
}

static const gchar*
get_nick(TpBaseChannel *channel)
{
  return RING_MEDIA_CHANNEL (channel)->nick;
}

void
ring_media_manager_emit_new_channel(RingMediaManager *self,
  gpointer request,
  gpointer _channel,
  GError *error)
{
  DEBUG("%s(%p, %p, %p, %p) called", __func__, self, request, _channel, error);

  RingMediaManagerPrivate *priv = RING_MEDIA_MANAGER(self)->priv;
  TpBaseChannel *channel = _channel ? TP_BASE_CHANNEL (_channel) : NULL;
  GSList *requests = request ? g_slist_prepend(NULL, request) : NULL;

  if (error == NULL) {
    char *object_path = NULL;

    g_signal_connect(
      channel, "closed", G_CALLBACK(on_media_channel_closed), self);

    g_object_get(channel, "object-path", &object_path, NULL);

    DEBUG("got new channel %p nick %s type %s",
	  channel, get_nick (channel), G_OBJECT_TYPE_NAME (channel));

    g_hash_table_insert(priv->channels, object_path, channel);

    tp_channel_manager_emit_new_channel(self,
      TP_EXPORTABLE_CHANNEL(channel), requests);

    /* Emit Group and StreamedMedia signals */
    ring_media_channel_emit_initial(RING_MEDIA_CHANNEL (channel));
  }
  else {
    DEBUG("new channel %p nick %s type %s failed with " GERROR_MSG_FMT,
	  channel, get_nick (channel), G_OBJECT_TYPE_NAME (channel),
      GERROR_MSG_CODE(error));

    if (request) {
      tp_channel_manager_emit_request_failed(self,
        request, error->domain, error->code, error->message);
    }
    if (_channel)
      g_object_unref(_channel);
  }

  g_slist_free(requests);
}


static gboolean
ring_media_manager_outgoing_call(RingMediaManager *self,
  gpointer request,
  TpHandle target,
  TpHandle initial_remote,
  char const *emergency,
  gboolean initial_audio)
{
  RingMediaManagerPrivate *priv = self->priv;
  TpHandleType htype = target ? TP_HANDLE_TYPE_CONTACT : TP_HANDLE_TYPE_NONE;
  char *object_path = ring_media_manager_new_object_path(self, "outgoing");
  RingCallChannel *channel;
  TpHandle initiator;

  initiator = tp_base_connection_get_self_handle(
    TP_BASE_CONNECTION(priv->connection));

  channel = (RingCallChannel *)
    g_object_new(RING_TYPE_CALL_CHANNEL,
      "connection", priv->connection,
      "tones", priv->tones,
      "object-path", object_path,
      "initiator", initiator,
      "handle-type", htype,
      "handle", target,
      "peer", target,
      "initial-remote", initial_remote,
      "requested", TRUE,
      "initial-audio", initial_audio,
      "anon-modes", priv->anon_modes,
      "initial-emergency-service", emergency,
      NULL);

  g_free(object_path);

  if (initial_audio)
    ring_call_channel_initial_audio(channel, self, request);
  else
    ring_media_manager_emit_new_channel(self, request, channel, NULL);

  return TRUE;
}

static void
on_media_channel_closed(GObject *chan, RingMediaManager *self)
{
  if (self->priv->channels != NULL) {
    gchar *object_path;

    g_object_get (chan, "object-path", &object_path, NULL);
    g_hash_table_remove (self->priv->channels, object_path);
    tp_channel_manager_emit_channel_closed (self, object_path);
    g_free (object_path);
  }
}

/** Find a RingMediaChannel by object_path. */
RingMediaChannel *
ring_media_manager_lookup(RingMediaManager *self,
  char const *object_path)
{
  if (self && object_path)
    return g_hash_table_lookup(self->priv->channels, object_path);
  else
    return NULL;
}

/** Find a RingMediaChannel by peer handle. */
static gpointer
ring_media_manager_lookup_by_peer(RingMediaManager *self,
  TpHandle handle)
{
  GHashTableIter i[1];
  gpointer channel;

  for (g_hash_table_iter_init(i, self->priv->channels);
       g_hash_table_iter_next(i, NULL, &channel);) {
    TpHandle peer = 0;
    g_object_get(channel, "peer", &peer, NULL);
    if (handle == peer)
      return channel;
  }

  return NULL;
}


static void
on_modem_call_incoming(ModemCallService *call_service,
  ModemCall *modem_call,
  char const *originator,
  RingMediaManager *self)
{
  RingMediaManagerPrivate *priv = self->priv;
  TpHandleRepoIface *repo;
  RingCallChannel *channel;
  TpHandle handle;
  GError *error = NULL;

  if (!ring_media_manager_is_connected (self))
    return;

  channel = modem_call_get_handler(modem_call);
  if (channel) {
    modem_call_set_handler(modem_call, NULL);
    ring_critical("Call instance %s already associated with channel.",
      modem_call_get_name(modem_call));
    ring_critical("Closing old channel %s.", RING_MEDIA_CHANNEL(channel)->nick);
    ring_media_channel_close(RING_MEDIA_CHANNEL(channel));
  }

  repo = tp_base_connection_get_handles(
    (TpBaseConnection *)(self->priv->connection), TP_HANDLE_TYPE_CONTACT);
  error = NULL;
  handle = tp_handle_ensure(repo, originator,
           ring_network_normalization_context(), &error);

  if (handle == 0) {
    DEBUG("tp_handle_ensure:" GERROR_MSG_FMT, GERROR_MSG_CODE(error));
    if (error) g_error_free(error);
    /* Xyzzy - modem_call_request_release() ?? */
    return;
  }

  /* Incoming call - pass call and handle ownership to new media channel */
  char *object_path = ring_media_manager_new_object_path(self, "incoming");

  channel = (RingCallChannel *)
    g_object_new(RING_TYPE_CALL_CHANNEL,
      "connection", priv->connection,
      "tones", priv->tones,
      "object-path", object_path,
      "initiator", handle,
      "handle-type", TP_HANDLE_TYPE_CONTACT,
      "handle", handle,
      "peer", handle,
      "requested", FALSE,
      "initial-audio", TRUE,
      "anon-modes", priv->anon_modes,
      "call-instance", modem_call,
      "terminating", TRUE,
      NULL);

  g_free(object_path);

  ring_media_manager_emit_new_channel(self, NULL, channel, NULL);
  ring_media_channel_set_state(RING_MEDIA_CHANNEL(channel),
    MODEM_CALL_STATE_INCOMING, 0, 0);
}

static void
on_modem_call_created(ModemCallService *call_service,
  ModemCall *modem_call,
  char const *destination,
  RingMediaManager *self)
{
  RingMediaManagerPrivate *priv = self->priv;
  TpHandleRepoIface *repo;
  RingCallChannel *channel;
  TpHandle handle;
  char const *sos;
  GError *error = NULL;

  if (!ring_media_manager_is_connected (self))
    return;

  channel = modem_call_get_handler(modem_call);
  if (channel)
    return; /* Call created by ring, nothing to do */

  /* This is a call created outside ring, create a channel for it */
  DEBUG("Freshly created call instance %s not associated with channel.",
    modem_call_get_name(modem_call));

  repo = tp_base_connection_get_handles(
    (TpBaseConnection *)(self->priv->connection), TP_HANDLE_TYPE_CONTACT);
  error = NULL;
  handle = tp_handle_ensure(repo, destination,
           ring_network_normalization_context(), &error);

  if (handle == 0) {
    ring_warning("tp_handle_ensure:" GERROR_MSG_FMT, GERROR_MSG_CODE(error));
    if (error) g_error_free(error);
    /* Xyzzy - modem_call_request_release() ?? */
    return;
  }

  sos = modem_call_get_emergency_service(priv->call_service, destination);

  char *object_path = ring_media_manager_new_object_path(self, "created");

  channel = (RingCallChannel *)
    g_object_new(RING_TYPE_CALL_CHANNEL,
      "connection", priv->connection,
      "tones", priv->tones,
      "object-path", object_path,
      "initiator", tp_base_connection_get_self_handle(
        TP_BASE_CONNECTION(priv->connection)),
      "handle-type", TP_HANDLE_TYPE_CONTACT,
      "handle", handle,
      "peer", handle,
      "requested", TRUE,
      "initial-remote", handle,
      "initial-audio", TRUE,
      "anon-modes", priv->anon_modes,
      "call-instance", modem_call,
      "originating", TRUE,
      sos ? "initial-emergency-service" : NULL, sos,
      NULL);

  g_free(object_path);

  ring_media_manager_emit_new_channel(self, NULL, channel, NULL);
  ring_media_channel_set_state(RING_MEDIA_CHANNEL(channel),
    MODEM_CALL_STATE_DIALING, 0, 0);
}

static void
on_modem_call_removed (ModemCallService *call_service,
                       ModemCall *modem_call,
                       RingMediaManager *self)
{
  RingCallChannel *channel;

  channel = modem_call_get_handler (modem_call);
  if (channel)
    g_object_set (channel, "call-instance", NULL, NULL);
}
/* ---------------------------------------------------------------------- */

static void
on_modem_call_user_connection(ModemCallService *call_service,
  gboolean active,
  RingMediaManager *self)
{
  DEBUG("user-connection %s", active ? "attached" : "detached");
  modem_tones_user_connection(RING_MEDIA_MANAGER(self)->priv->tones, active);
}

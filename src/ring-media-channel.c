/*
 * ring-media-channel.c - Source for RingMediaChannel
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
 * Based on telepathy-glib/examples/cm/echo/chan.c:
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

#define DEBUG_FLAG RING_DEBUG_MEDIA
#include "ring-debug.h"

#include "ring-media-channel.h"
#include "ring-util.h"

#include "modem/call.h"
#include "modem/errors.h"
#include "modem/tones.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-generic.h>

#include <ring-extensions/ring-extensions.h>

#include "ring-connection.h"

#include "ring-param-spec.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct _RingMediaChannelPrivate
{
  TpBaseConnection *connection;

  gchar *object_path;

  GQueue requests[1];           /* Requests towards the modem */

  uint8_t state;

  struct {
    uint8_t state;
    uint8_t reason;
    uint8_t requested;          /* Hold state requested by client */
  } hold;

  unsigned requested:1;
  unsigned initial_audio:1;     /* property */

  unsigned disposed:1;
  unsigned closing:1, closed:1;
  unsigned :0;

  ModemRequest *control;
  guint playing;
  ModemTones *tones;

  guint close_timer;

  struct {
    ModemRequest *request;
    guchar digit;
  } dtmf;

  struct {
    char *string;       /* Dialstring */
    unsigned playing:1, canceled:1, stopped:1, :0;
  } dial;

  struct {
    gulong state, terminated;
    gulong dtmf_tone, dialstring;
  } signals;
};

/* properties */
enum {
  PROP_NONE,
  /* telepathy-glib properties */
  PROP_OBJECT_PATH,
  PROP_CHANNEL_PROPERTIES,
  PROP_CHANNEL_DESTROYED,
  PROP_CHANNEL_TYPE,
  PROP_TARGET,
  PROP_TARGET_ID,
  PROP_TARGET_TYPE,

  /* DBUs properties */
  PROP_INTERFACES,

  PROP_REQUESTED,
  PROP_INITIATOR,
  PROP_INITIATOR_ID,

  PROP_HOLD_STATE,              /* o.f.T.Channel.Interface.Hold */
  PROP_HOLD_REASON,             /* o.f.T.Channel.Interface.Hold */

  PROP_INITIAL_AUDIO,           /* o.f.T.Ch.T.StreamedMedia.InitialAudio */
  PROP_INITIAL_VIDEO,           /* o.f.T.Ch.T.StreamedMedia.InitialVideo */
  PROP_IMMUTABLE_STREAMS,       /* o.f.T.Ch.T.StreamedMedia.ImmutableStreams */

  /* ring-specific properties */
  PROP_PEER,
  PROP_CONNECTION,
  PROP_CALL_INSTANCE,
  PROP_TONES,

  LAST_PROPERTY
};

static TpDBusPropertiesMixinIfaceImpl
ring_media_channel_dbus_property_interfaces[];
static void ring_media_channel_channel_iface_init(gpointer, gpointer);
static void ring_media_channel_dtmf_iface_init(gpointer, gpointer);
static void ring_channel_hold_iface_init(gpointer, gpointer);
#if nomore
static void ring_channel_dial_strings_iface_init(gpointer, gpointer);
#endif

G_DEFINE_TYPE_WITH_CODE(
  RingMediaChannel, ring_media_channel, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_DBUS_PROPERTIES,
    tp_dbus_properties_mixin_iface_init);
G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL,
    ring_media_channel_channel_iface_init);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_DTMF,
    ring_media_channel_dtmf_iface_init);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_HOLD,
    ring_channel_hold_iface_init);
#if nomore
  /* XXX: waiting for upstream tp-glib to provide a similar interface */
  G_IMPLEMENT_INTERFACE(RTCOM_TYPE_TP_SVC_CHANNEL_INTERFACE_DIAL_STRINGS,
    ring_channel_dial_strings_iface_init);
#endif
  G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_STREAMED_MEDIA,
    ring_streamed_media_mixin_iface_init);
  G_IMPLEMENT_INTERFACE(TP_TYPE_EXPORTABLE_CHANNEL, NULL);
  G_IMPLEMENT_INTERFACE(TP_TYPE_CHANNEL_IFACE, NULL);
  );

static void ring_media_channel_set_call_instance(RingMediaChannel *self,
  ModemCall *ci);
static void on_modem_call_state(ModemCall *, ModemCallState,
  RingMediaChannel *);
static void on_modem_call_state_dialing(RingMediaChannel *self);
static void on_modem_call_state_incoming(RingMediaChannel *self);
static void on_modem_call_state_waiting(RingMediaChannel *self);
static void on_modem_call_state_mo_alerting(RingMediaChannel *self);
#if nomore
static void on_modem_call_state_mt_alerting(RingMediaChannel *self);
static void on_modem_call_state_answered(RingMediaChannel *self);
#endif
static void on_modem_call_state_active(RingMediaChannel *self);
#if nomore
static void on_modem_call_state_hold_initiated(RingMediaChannel *self);
#endif
static void on_modem_call_state_held(RingMediaChannel *self);
#if nomore
static void on_modem_call_state_retrieve_initiated(RingMediaChannel *self);
#endif
static void on_modem_call_state_release(RingMediaChannel *);
#if nomore
static void on_modem_call_state_terminated(RingMediaChannel *);
#endif

static void on_modem_call_terminated(ModemCall *, RingMediaChannel *);
static void on_modem_call_dtmf_tone(ModemCall *call_instance,
  gint tone, RingMediaChannel *self);
static void on_modem_call_dialstring(ModemCall *, char const *,
  RingMediaChannel *);

/* ====================================================================== */
/* GObject interface */

static void
ring_media_channel_init(RingMediaChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, RING_TYPE_MEDIA_CHANNEL, RingMediaChannelPrivate);
  self->priv->hold.requested = -1;

  ring_streamed_media_mixin_init (G_OBJECT(self),
      G_STRUCT_OFFSET(RingMediaChannel, streamed_media));
}

static void
ring_media_channel_constructed(GObject *object)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(object);
  RingMediaChannelPrivate *priv = self->priv;
  TpBaseConnection *connection = TP_BASE_CONNECTION(priv->connection);
  TpDBusDaemon *bus_daemon = tp_base_connection_get_dbus_daemon(connection);

  if (G_OBJECT_CLASS(ring_media_channel_parent_class)->constructed)
    G_OBJECT_CLASS(ring_media_channel_parent_class)->constructed(object);

  DEBUG("(%p) with %s", self, self->nick);

  tp_dbus_daemon_register_object(bus_daemon, priv->object_path, object);
}

static void
ring_media_channel_get_property(GObject *obj,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(obj);
  RingMediaChannelPrivate *priv = self->priv;
  guint initiator;
  gchar const *id;

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_value_set_string(value, priv->object_path);
      break;
    case PROP_CHANNEL_DESTROYED:
      g_value_set_boolean(value, TRUE);
      break;
    case PROP_CHANNEL_PROPERTIES:
      g_value_take_boxed(value, ring_media_channel_properties(self));
      break;
    case PROP_CHANNEL_TYPE:
      g_value_set_static_string(value, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA);
      break;
    case PROP_PEER:
      g_value_set_uint(value, 0);
      break;
    case PROP_TARGET:
      g_value_set_uint(value, 0);
      break;
    case PROP_TARGET_TYPE:
      g_value_set_uint(value, 0);
      break;
    case PROP_TARGET_ID:
      g_value_set_uint(value, 0);
      break;
    case PROP_INITIATOR:
      initiator = tp_base_connection_get_self_handle(priv->connection);
      g_value_set_uint(value, initiator);
      break;
    case PROP_INITIATOR_ID:
      initiator = tp_base_connection_get_self_handle(priv->connection);
      id = ring_connection_inspect_contact(RING_CONNECTION(priv->connection), initiator);
      g_value_set_static_string(value, id);
      break;
    case PROP_REQUESTED:
      g_value_set_boolean(value, priv->requested);
      break;
    case PROP_HOLD_STATE:
      g_value_set_uint(value, priv->hold.state);
      break;
    case PROP_HOLD_REASON:
      g_value_set_uint(value, priv->hold.reason);
      break;
    case PROP_INITIAL_AUDIO:
      g_value_set_boolean(value, priv->initial_audio);
      break;
    case PROP_INITIAL_VIDEO:
      g_value_set_boolean(value, FALSE);
      break;
    case PROP_IMMUTABLE_STREAMS:
      g_value_set_boolean(value, TRUE);
      break;
    case PROP_CONNECTION:
      g_value_set_object(value, priv->connection);
      break;
    case PROP_CALL_INSTANCE:
      g_value_set_pointer(value, self->call_instance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_media_channel_set_property(GObject *obj,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(obj);
  RingMediaChannelPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_OBJECT_PATH:
      priv->object_path = g_strdup(value);
      self->nick = strrchr(priv->object_path, '/');
      if(!self->nick++) self->nick = "";
      DEBUG("(%p) with nick '%s", self, self->nick);
      break;
    case PROP_CHANNEL_TYPE:
    case PROP_INITIATOR:
    case PROP_TARGET:
    case PROP_TARGET_ID:
    case PROP_TARGET_TYPE:
    case PROP_PEER:
      /* these property is writable in the interface, but not actually
       * meaningfully changable on this channel, so we do nothing */
      break;
    case PROP_REQUESTED:
      priv->requested = g_value_get_boolean(value);
      break;
    case PROP_HOLD_STATE:
      priv->hold.state = g_value_get_uint(value);
      break;
    case PROP_HOLD_REASON:
      priv->hold.reason = g_value_get_uint(value);
      break;
    case PROP_INITIAL_AUDIO:
      priv->initial_audio = g_value_get_boolean(value);
      break;
    case PROP_CONNECTION:
      priv->connection = g_value_get_object(value);
      break;
    case PROP_CALL_INSTANCE:
      ring_media_channel_set_call_instance (self, g_value_get_pointer (value));
      break;
    case PROP_TONES:
      /* media manager owns tones as well as a reference to this channel */
      priv->tones = g_value_get_object(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_media_channel_dispose(GObject *object)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL (object);
  RingMediaChannelPrivate *priv = self->priv;

  if (self->priv->disposed)
    return;
  self->priv->disposed = TRUE;

  ring_media_channel_close(self);

  if (priv->close_timer)
    g_source_remove(priv->close_timer), priv->close_timer = 0;

  if (priv->playing)
    modem_tones_stop(priv->tones, priv->playing);

  /* if still holding on to a call instance, disconnect */
  if (self->call_instance)
    g_object_set(self, "call-instance", NULL, NULL);

  ((GObjectClass *)ring_media_channel_parent_class)->dispose(object);
}


static void
ring_media_channel_finalize(GObject *object)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(object);
  RingMediaChannelPrivate *priv = self->priv;
  gchar const *nick = self->nick;
  gchar *object_path = priv->object_path;

  ring_streamed_media_mixin_finalize (object);

  g_free(priv->dial.string);

  G_OBJECT_CLASS(ring_media_channel_parent_class)->finalize(object);

  DEBUG("(%p) on %s", (gpointer)object, nick);
  g_free(object_path);
}

/* ====================================================================== */
/* GObjectClass */

static void
ring_media_channel_class_init(RingMediaChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  g_type_class_add_private(klass, sizeof (RingMediaChannelPrivate));

  object_class->constructed = ring_media_channel_constructed;
  object_class->get_property = ring_media_channel_get_property;
  object_class->set_property = ring_media_channel_set_property;
  object_class->dispose = ring_media_channel_dispose;
  object_class->finalize = ring_media_channel_finalize;

  g_object_class_override_property(
    object_class, PROP_OBJECT_PATH, "object-path");

  g_object_class_override_property(
    object_class, PROP_CHANNEL_PROPERTIES, "channel-properties");

  g_object_class_override_property(
    object_class, PROP_CHANNEL_DESTROYED, "channel-destroyed");

  g_object_class_override_property(
    object_class, PROP_CHANNEL_TYPE, "channel-type");

  g_object_class_override_property(
    object_class, PROP_TARGET_TYPE, "handle-type");

  g_object_class_override_property(
    object_class, PROP_TARGET, "handle");

  g_object_class_install_property(
    object_class, PROP_TARGET_ID, ring_param_spec_handle_id(0));

  g_object_class_install_property(
    object_class, PROP_INTERFACES, ring_param_spec_interfaces());

  g_object_class_install_property(
    object_class, PROP_REQUESTED, ring_param_spec_requested(G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_INITIATOR, ring_param_spec_initiator(0));

  g_object_class_install_property(
    object_class, PROP_INITIATOR_ID, ring_param_spec_initiator_id(0));

  g_object_class_install_property(
    object_class, PROP_HOLD_STATE,
    g_param_spec_uint("hold-state",
      "Hold State",
      "The hold state of the channel.",
      TP_LOCAL_HOLD_STATE_UNHELD,
      TP_LOCAL_HOLD_STATE_PENDING_UNHOLD,
      TP_LOCAL_HOLD_STATE_UNHELD,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_HOLD_REASON,
    g_param_spec_uint("hold-state-reason",
      "Hold State Reason",
      "The reason for the hold state of the channel.",
      TP_LOCAL_HOLD_STATE_REASON_NONE,
      TP_LOCAL_HOLD_STATE_REASON_RESOURCE_NOT_AVAILABLE,
      TP_LOCAL_HOLD_STATE_REASON_NONE,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_CONNECTION, ring_param_spec_connection());

  g_object_class_install_property(
    object_class, PROP_PEER,
    g_param_spec_uint("peer",
      "Peer handle",
      "Peer handle for this channel",
      0, G_MAXUINT, 0,
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_INITIAL_AUDIO,
    g_param_spec_boolean(
      "initial-audio",
      "InitialAudio",
      "True if the audio stream was requested when channel was created.",
      FALSE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_INITIAL_VIDEO,
    g_param_spec_boolean(
      "initial-video",
      "InitialVideo",
      "True if the video stream was requested when channel was created.",
      FALSE,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_IMMUTABLE_STREAMS,
    g_param_spec_boolean("immutable-streams",
      "ImmutableStreams",
      "True if the video stream was requested when channel was created.",
      TRUE,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_CALL_INSTANCE,
      g_param_spec_pointer ("call-instance",
          "ModemCall Object",
          "ModemCall instance for this channel",
          /* MODEM_TYPE_CALL, */
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_TONES,
    g_param_spec_object("tones",
      "ModemTones Object",
      "ModemTones for this channel",
      MODEM_TYPE_TONES,
      G_PARAM_WRITABLE |
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));
}

/* ====================================================================== */
/**
 * org.freedesktop.DBus properties
 */

static TpDBusPropertiesMixinPropImpl channel_properties[] = {
    { "ChannelType", "channel-type", NULL },
    { "Interfaces", "interfaces", NULL },
    { "TargetHandle", "handle", NULL },
    { "TargetID", "handle-id", NULL },
    { "TargetHandleType", "handle-type", NULL },
    { "Requested", "requested", NULL },
    { "InitiatorHandle", "initiator" },
    { "InitiatorId", "initiator-id" },
    { NULL }
};

static TpDBusPropertiesMixinPropImpl media_properties[] = {
    { "InitialAudio", "initial-audio", NULL },
    { "InitialVideo", "initial-video", NULL },
    { NULL }
};

static TpDBusPropertiesMixinIfaceImpl
ring_media_channel_dbus_property_interfaces[] = {
    {
        TP_IFACE_CHANNEL,
        tp_dbus_properties_mixin_getter_gobject_properties,
        NULL,
        channel_properties,
    },
    {
        TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
        tp_dbus_properties_mixin_getter_gobject_properties,
        NULL,
        media_properties,
    },
    { NULL }
};

GHashTable*
ring_media_channel_properties(RingMediaChannel *self)
{
    return tp_dbus_properties_mixin_make_properties_hash(
      G_OBJECT(self),
      TP_IFACE_CHANNEL, "ChannelType",
      TP_IFACE_CHANNEL, "Interfaces",
      TP_IFACE_CHANNEL, "TargetHandle",
      TP_IFACE_CHANNEL, "TargetHandleType",
      TP_IFACE_CHANNEL, "TargetID",
      TP_IFACE_CHANNEL, "InitiatorHandle",
      TP_IFACE_CHANNEL, "InitiatorID",
      TP_IFACE_CHANNEL, "Requested",
      TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, "InitialAudio",
      TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, "InitialVideo",
      TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, "ImmutableStreams",
      NULL
    );
}

/* ====================================================================== */

ModemCallService *
ring_media_channel_get_call_service (RingMediaChannel *self)
{
  RingMediaChannelPrivate *priv = self->priv;
  TpBaseConnection *base_connection = priv->connection;
  RingConnection *connection;
  ModemOface *oface;

  connection = RING_CONNECTION (base_connection);
  oface = ring_connection_get_modem_interface (connection,
      MODEM_OFACE_CALL_MANAGER);

  if (oface)
    return MODEM_CALL_SERVICE (oface);
  else
    return NULL;
}

static gboolean ring_media_channel_emit_closed(RingMediaChannel *self);

void
ring_media_channel_emit_initial(RingMediaChannel *_self)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(_self);
  RingMediaChannelPrivate *priv = self->priv;

  RING_MEDIA_CHANNEL_GET_CLASS(self)->emit_initial(self);

  if (priv->initial_audio) {
    ring_streamed_media_mixin_update_audio (self, 0,
      TP_MEDIA_STREAM_STATE_CONNECTING,
      TP_MEDIA_STREAM_DIRECTION_NONE,
      TP_MEDIA_STREAM_PENDING_LOCAL_SEND |
      TP_MEDIA_STREAM_PENDING_REMOTE_SEND);
  }
}

ModemRequest *
ring_media_channel_queue_request (RingMediaChannel *self,
  ModemRequest *request)
{
  if (request)
    g_queue_push_tail(self->priv->requests, request);
  return request;
}

ModemRequest *
ring_media_channel_dequeue_request(RingMediaChannel *self,
  ModemRequest *request)
{
  if (request)
    g_queue_remove(self->priv->requests, request);
  return request;
}

void
ring_media_channel_close(RingMediaChannel *self)
{
  RingMediaChannelPrivate *priv = self->priv;
  RingMediaChannelClass *cls = RING_MEDIA_CHANNEL_GET_CLASS(self);
  gboolean ready = TRUE;

  if (priv->closed)
    return;

  if (priv->closing)
    return;

  priv->closing = TRUE;

  if (priv->playing)
    modem_tones_stop(priv->tones, priv->playing);

  while (!g_queue_is_empty(priv->requests))
    modem_request_cancel(g_queue_pop_head(priv->requests));

  if (cls->close)
    ready = cls->close(self, FALSE);

  if (ready && self->call_instance)
    g_object_set(self, "call-instance", NULL, NULL);

  if (!priv->playing && ready && !self->call_instance) {
    ring_media_channel_emit_closed(self);
  }
  else if (!priv->close_timer) {
    priv->close_timer = g_timeout_add(32000,
                        (GSourceFunc)ring_media_channel_emit_closed, self);
  }
}

static gboolean
ring_media_channel_emit_closed(RingMediaChannel *self)
{
  RingMediaChannelPrivate *priv = self->priv;
  RingMediaChannelClass *cls = RING_MEDIA_CHANNEL_GET_CLASS(self);

  if (priv->close_timer)
    g_source_remove(priv->close_timer), priv->close_timer = 0;

  if (priv->closed)
    return FALSE;

  priv->closed = TRUE;

  if (priv->playing)
    modem_tones_stop(priv->tones, priv->playing);

  priv->playing = 0;

  cls->close(self, TRUE);

  if (self->call_instance)
    g_object_set(self, "call-instance", NULL, NULL);

  tp_svc_channel_emit_closed((TpSvcChannel*)self);

  DEBUG("emit Closed on %s", self->nick);

  return FALSE;
}

/* ====================================================================== */
/**
 * Telepathy.Channel DBus interface
 *
 * Close() -> nothing
 * GetChannelType() -> s
 * GetHandle() -> u, u
 * GetInterfaces() -> as
 *
 * Signals:
 * -> Closed(
 */

/** DBus method Close ( ) -> nothing
 *
 * Request that the channel be closed. This is not the case until the Closed
 * signal has been emitted, and depending on the connection manager this may
 * simply remove you from the channel on the server, rather than causing it
 * to stop existing entirely. Some channels such as contact list channels
 * may not be closed.
 */
void
ring_media_channel_method_close(TpSvcChannel *iface,
  DBusGMethodInvocation *context)
{
  DEBUG("Close() entered");
  ring_media_channel_close(RING_MEDIA_CHANNEL(iface));
  tp_svc_channel_return_from_close(context);
}

/** DBus method GetChannelType() -> s
 *
 * Returns the interface name for this type of channel.
 */
static void
ring_media_channel_method_get_channel_type(TpSvcChannel *iface,
  DBusGMethodInvocation *context)
{
  DEBUG("GetChannelType() entered");
  gchar *type = NULL;
  g_object_get(iface, "channel-type", &type, NULL);
  tp_svc_channel_return_from_get_channel_type(context, type);
  g_free(type);
}

/** DBus method GetHandle() -> u, u
 *
 * Returns the handle type and number if this channel represents a
 * communication with a particular contact, room or server-stored list, or
 * zero if it is transient and defined only by its' contents.
 */
static void
ring_media_channel_method_get_handle(TpSvcChannel *iface,
  DBusGMethodInvocation *context)
{
  DEBUG("GetHandle() entered");
  guint type = 0, target = 0;
  g_object_get(iface, "handle-type", &type, "handle", &target, NULL);
  tp_svc_channel_return_from_get_handle(context, type, target);
}

/** DBus method GetInterfaces() -> as
 *
 * Get the optional interfaces implemented by this channel.
 */
static void
ring_media_channel_method_get_interfaces(TpSvcChannel *iface,
  DBusGMethodInvocation *context)
{
  DEBUG("GetInterfaces() entered");
  gchar **interfaces = NULL;
  g_object_get(iface, "interfaces", &interfaces, NULL);
  tp_svc_channel_return_from_get_interfaces(
    context,
    (char const **)interfaces);
  g_strfreev(interfaces);
}

static void
ring_media_channel_channel_iface_init(gpointer g_iface, gpointer iface_data)
{
    TpSvcChannelClass *klass = (TpSvcChannelClass*) g_iface;

#define IMPLEMENT(x) \
  tp_svc_channel_implement_##x(klass, ring_media_channel_method_##x)

  IMPLEMENT(close);
  IMPLEMENT(get_channel_type);
  IMPLEMENT(get_handle);
  IMPLEMENT(get_interfaces);

#undef IMPLEMENT
}

/* ====================================================================== */


/* ====================================================================== */
/*
 * Telepathy.Channel.Interface.DTMF DBus interface - version 0.15
 */

#ifdef nomore
static void ring_sending_dial_string(RingMediaChannel *, char const *);
#endif

static ModemCallReply ring_media_channel_dtmf_start_tone_replied;
static ModemCallReply ring_media_channel_dtmf_stop_tone_replied;

static char const ring_media_channel_dtmf_events[16] = "0123456789*#ABCD";

/** DBus method StartTone ( u: stream_id, y: event ) -> nothing
 *
 * Start sending a DTMF tone on this stream. Where possible, the tone will
 * continue until StopTone is called. On certain protocols, it may only be
 * possible to send events with a predetermined length. In this case, the
 * implementation may emit a fixed-length tone, and the StopTone method call
 * should return NotAvailable.
 */
void
ring_media_channel_dtmf_start_tone(TpSvcChannelInterfaceDTMF *iface,
  guint stream_id,
  guchar event,
  DBusGMethodInvocation *context)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(iface);
  RingMediaChannelPrivate *priv = self->priv;
  GError *error = NULL;
  char const *events = ring_media_channel_dtmf_events;

  DEBUG("(%u, %u) on %s", stream_id, event, self->nick);

  if (stream_id == 0 /* XXXX || priv->audio->id != stream_id */) {
    g_set_error(&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "invalid stream id %u", stream_id);
  }
  else if (event >= 16) {
    g_set_error(&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "event %u is not a known DTMF event", event);
  }
  else if (self->call_instance == NULL) {
    g_set_error(&error, TP_ERRORS, TP_ERROR_DISCONNECTED,
      "Channel is not connected");
  }
  else {
    ModemRequest *request;

    if (priv->dtmf.digit || priv->dtmf.request ||
      (priv->dial.string && priv->dial.string[0])) {
      request = modem_call_stop_dtmf(self->call_instance,
                ring_media_channel_dtmf_stop_tone_replied, self);
      ring_media_channel_queue_request(self, request);
    }

#if nomore
    if (!priv->dial.string)
      ring_sending_dial_string(self, "");
#endif

    request = modem_call_start_dtmf(self->call_instance, events[event],
              ring_media_channel_dtmf_start_tone_replied, self);

    if (request) {
      priv->dtmf.request = request;
      modem_request_add_data_full(request,
        "tp-request", context, ring_method_return_internal_error);
      modem_request_add_data(request, "StartTone", GINT_TO_POINTER((gint)events[event]));
      ring_media_channel_queue_request(self, request);
      return;
    }

    g_set_error(&error, TP_ERRORS, TP_ERROR_NETWORK_ERROR,
      "Failed to send request to modem");
  }

  dbus_g_method_return_error(context, error);
  g_error_free(error);
}

static void
ring_media_channel_dtmf_start_tone_replied(ModemCall *call_instance,
  ModemRequest *request,
  GError *error,
  gpointer _self)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(_self);
  RingMediaChannelPrivate *priv = self->priv;
  gpointer context = modem_request_steal_data(request, "tp-request");

  ring_media_channel_dequeue_request(self, request);

  if (priv->dtmf.request == request)
    priv->dtmf.request = NULL;

  if (!error) {
    guchar dtmf = GPOINTER_TO_INT(modem_request_get_data(request, "StartTone"));
    char const *event = strchr(ring_media_channel_dtmf_events, dtmf);

    priv->dtmf.digit = dtmf;

    if (event) {
      ring_media_channel_play_tone(self,
        event - ring_media_channel_dtmf_events,
        -6, 20000);
    }

    tp_svc_channel_interface_dtmf_return_from_start_tone(context);
  }
  else {
    dbus_g_method_return_error(context, error);
  }
}

/** DBus method StopTone ( u: stream_id ) -> nothing
 *
 * Stop sending any DTMF tone which has been started using the StartTone
 * method. If there is no current tone, this method will do nothing.
 */
void
ring_media_channel_dtmf_stop_tone(TpSvcChannelInterfaceDTMF *iface,
  guint stream_id,
  DBusGMethodInvocation *context)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(iface);

  GError *error = NULL;

  DEBUG("(%u) on %s", stream_id, self->nick);

  if (ring_streamed_media_mixin_is_audio_stream (iface, stream_id)) {
    g_set_error(&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "invalid stream id %u", stream_id);
  }
  else if (self->call_instance == NULL) {
    g_set_error(&error, TP_ERRORS, TP_ERROR_DISCONNECTED,
      "Channel is not connected");
  }
  else {
    ModemRequest *request;

    request = modem_call_stop_dtmf(self->call_instance,
              ring_media_channel_dtmf_stop_tone_replied, self);

    if (request) {
      modem_request_add_data_full(request,
        "tp-request", context, ring_method_return_internal_error);
      ring_media_channel_queue_request(self, request);
      return;
    }

    g_set_error(&error, TP_ERRORS, TP_ERROR_NETWORK_ERROR,
      "Failed to send request to modem");
  }

  dbus_g_method_return_error(context, error);

  g_error_free(error);
}

static void
ring_media_channel_dtmf_stop_tone_replied(ModemCall *call_instance,
  ModemRequest *request,
  GError *error,
  gpointer _self)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(_self);
  RingMediaChannelPrivate *priv = self->priv;
  gpointer context = modem_request_steal_data(request, "tp-request");

  ring_media_channel_dequeue_request(self, request);

  priv->dtmf.digit = 0;
  ring_media_channel_stop_playing(self, FALSE);

  if (!context)
    return;

  if (!error) {
    tp_svc_channel_interface_dtmf_return_from_stop_tone(context);
  }
  else {
    dbus_g_method_return_error(context, error);
  }
}

static void
ring_media_channel_dtmf_iface_init(gpointer g_iface, gpointer iface_data)
{
  TpSvcChannelInterfaceDTMFClass *klass = (TpSvcChannelInterfaceDTMFClass *)g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_dtmf_implement_##x(       \
    klass, ring_media_channel_dtmf_##x)
  IMPLEMENT(start_tone);
  IMPLEMENT(stop_tone);
#undef IMPLEMENT
}

/* ---------------------------------------------------------------------- */
/* Implement org.freedesktop.Telepathy.Channel.Interface.Hold */

static int ring_update_hold (RingMediaChannel *self, int hold, int reason);

static
void get_hold_state(TpSvcChannelInterfaceHold *iface,
  DBusGMethodInvocation *context)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(iface);
  RingMediaChannelPrivate *priv = self->priv;

  if (self->call_instance == NULL)
    {
      GError *error = NULL;
      g_set_error(&error, TP_ERRORS, TP_ERROR_DISCONNECTED,
          "Channel is not connected");
      dbus_g_method_return_error(context, error);
      g_error_free(error);
    }
  else
    {
      tp_svc_channel_interface_hold_return_from_get_hold_state (context,
          priv->hold.state, priv->hold.reason);
    }
}

static ModemCallReply response_to_hold;

static
void request_hold (TpSvcChannelInterfaceHold *iface,
                   gboolean hold,
                   DBusGMethodInvocation *context)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL (iface);
  RingMediaChannelPrivate *priv = self->priv;
  ModemCall *instance = self->call_instance;

  GError *error = NULL;
  uint8_t state, next;
  ModemCallState expect;

  DEBUG ("(%u) on %s", hold, self->nick);

  if (hold)
    {
      expect = MODEM_CALL_STATE_ACTIVE;
      state = TP_LOCAL_HOLD_STATE_HELD;
      next = TP_LOCAL_HOLD_STATE_PENDING_HOLD;
    }
  else
    {
      expect = MODEM_CALL_STATE_HELD;
      state = TP_LOCAL_HOLD_STATE_UNHELD;
      next = TP_LOCAL_HOLD_STATE_PENDING_UNHOLD;
    }

  if (instance == NULL)
    {
      g_set_error(&error, TP_ERRORS, TP_ERROR_DISCONNECTED,
          "Channel is not connected");
    }
  else if (state == priv->hold.state || next == priv->hold.state)
    {
      priv->hold.reason = TP_LOCAL_HOLD_STATE_REASON_REQUESTED;
      tp_svc_channel_interface_hold_return_from_request_hold(context);
      return;
    }
  else if (priv->state != expect)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Invalid call state %s",
          modem_call_get_state_name(priv->state));
    }
  else if (priv->control)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Call control operation pending");
    }
  else
    {
      tp_svc_channel_interface_hold_return_from_request_hold(context);

      g_object_ref (self);

      priv->control = modem_call_request_hold (instance, hold,
          response_to_hold, self);
      ring_media_channel_queue_request (self, priv->control);

      priv->hold.requested = state;

      ring_update_hold (self, next, TP_LOCAL_HOLD_STATE_REASON_REQUESTED);
      return;
    }

  DEBUG ("request_hold(%u) on %s: %s", hold, self->nick, error->message);
  dbus_g_method_return_error (context, error);
  g_clear_error (&error);
}

static void
response_to_hold (ModemCall *ci,
                  ModemRequest *request,
                  GError *error,
                  gpointer _self)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL (_self);
  RingMediaChannelPrivate *priv = self->priv;

  if (priv->control == request)
    priv->control = NULL;

  ring_media_channel_dequeue_request (self, request);

  if (error && priv->hold.requested != -1)
    {
      uint8_t next;

      DEBUG ("%s: %s", self->nick, error->message);

      if (priv->hold.requested)
        next = TP_LOCAL_HOLD_STATE_UNHELD;
      else
        next = TP_LOCAL_HOLD_STATE_HELD;

      ring_update_hold (self, next,
          TP_LOCAL_HOLD_STATE_REASON_RESOURCE_NOT_AVAILABLE);

      priv->hold.requested = -1;
    }

  g_object_unref (self);
}


static void
ring_channel_hold_iface_init(gpointer g_iface, gpointer iface_data)
{
  TpSvcChannelInterfaceHoldClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_hold_implement_##x(       \
    klass, x)
  IMPLEMENT(get_hold_state);
  IMPLEMENT(request_hold);
#undef IMPLEMENT
}

static int
ring_update_hold (RingMediaChannel *self,
                  int hold,
                  int reason)
{
  RingMediaChannelPrivate *priv = self->priv;
  unsigned old = priv->hold.state;
  char const *name;

  if (hold == old)
    return 0;

  switch (hold) {
    case TP_LOCAL_HOLD_STATE_UNHELD:
      name = "Unheld";
      if (reason)
        ;
      else if (hold == priv->hold.requested)
        reason = TP_LOCAL_HOLD_STATE_REASON_REQUESTED;
      else if (old == TP_LOCAL_HOLD_STATE_PENDING_HOLD)
        reason = TP_LOCAL_HOLD_STATE_REASON_RESOURCE_NOT_AVAILABLE;
      else
        reason = TP_LOCAL_HOLD_STATE_REASON_NONE;
      priv->hold.requested = -1;
      break;
    case TP_LOCAL_HOLD_STATE_HELD:
      name = "Held";
      if (reason)
        ;
      else if (hold == priv->hold.requested)
        reason = TP_LOCAL_HOLD_STATE_REASON_REQUESTED;
      else if (old == TP_LOCAL_HOLD_STATE_PENDING_UNHOLD)
        reason = TP_LOCAL_HOLD_STATE_REASON_RESOURCE_NOT_AVAILABLE;
      else
        reason = TP_LOCAL_HOLD_STATE_REASON_NONE;
      priv->hold.requested = -1;
      break;
    case TP_LOCAL_HOLD_STATE_PENDING_HOLD:
      name = "Pending_Hold";
      break;
    case TP_LOCAL_HOLD_STATE_PENDING_UNHOLD:
      name = "Pending_Unhold";
      break;
    default:
      name = "Unknown";
      DEBUG("unknown %s(%d)", "HoldStateChanged", hold);
      return -1;
  }

  g_object_set(self,
    "hold-state", hold,
    "hold-state-reason", reason,
    NULL);

  DEBUG("emitting %s(%s) for %s", "HoldStateChanged", name, self->nick);

  tp_svc_channel_interface_hold_emit_hold_state_changed(
    (TpSvcChannelInterfaceHold *)self,
    hold, reason);

  return 0;
}

/* ---------------------------------------------------------------------- */
/* Implement com.Nokia.Telepathy.Channel.Interface.DialStrings */

#if nomore
static void
ring_emit_stopped_dial_string(RingMediaChannel *self);

static void
get_current_dial_strings(RTComTpSvcChannelInterfaceDialStrings *iface,
  DBusGMethodInvocation *context)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(iface);
  RingMediaChannelPrivate *priv = self->priv;

  GHashTable *dialstrings = g_hash_table_new (NULL, NULL);

  if (priv->dial.string) {
    g_hash_table_replace(dialstrings,
      GUINT_TO_POINTER(priv->audio->id),
      priv->dial.string);
  }
  rtcom_tp_svc_channel_interface_dial_strings_return_from_get_current_dial_strings
    (context, dialstrings);

  g_hash_table_destroy(dialstrings);
}


static void
send_dial_string(RTComTpSvcChannelInterfaceDialStrings *iface,
  guint id,
  char const *dialstring,
  guint duration,
  guint pause,
  DBusGMethodInvocation *context)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(iface);

  GError *error = NULL;

  if (ring_media_channel_send_dialstring(self, id, dialstring, duration, pause, &error)) {
    rtcom_tp_svc_channel_interface_dial_strings_return_from_send_dial_string(context);
    return;
  }

  dbus_g_method_return_error(context, error);
  g_error_free(error);
}


static void
cancel_dial_string(RTComTpSvcChannelInterfaceDialStrings *iface,
  guint id,
  DBusGMethodInvocation *context)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(iface);
  RingMediaChannelPrivate *priv = self->priv;

  GError *error = NULL;

  DEBUG("(%u) for %s", id, self->nick);

  if (id != priv->audio->id) {
    g_set_error(&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Invalid stream");
  }
  else if (priv->audio->state != TP_MEDIA_STREAM_STATE_CONNECTED) {
    g_set_error(&error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
      "Stream is not connected");
  }
  else if (!priv->dial.string) {
    g_set_error(&error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
      "Already sending a dial string");
  }
  else if (modem_call_stop_dtmf(self->call_instance, NULL, NULL) < 0) {
    g_set_error(&error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
      "Channel error");
  }
  else {
    priv->dial.canceled = TRUE;
    rtcom_tp_svc_channel_interface_dial_strings_return_from_cancel_dial_string(context);
    return;
  }

  dbus_g_method_return_error(context, error);
  g_error_free(error);
}


static void
ring_channel_dial_strings_iface_init(gpointer g_iface, gpointer iface_data)
{
  RTComTpSvcChannelInterfaceDialStringsClass *klass = g_iface;

#define IMPLEMENT(x) rtcom_tp_svc_channel_interface_dial_strings_implement_##x( \
    klass, x)
  IMPLEMENT(get_current_dial_strings);
  IMPLEMENT(send_dial_string);
  IMPLEMENT(cancel_dial_string);
#undef IMPLEMENT
}

static void
ring_sending_dial_string(RingMediaChannel *self,
  char const *dialstring)
{
  RingMediaChannelPrivate *priv = self->priv;
  char *old;
  unsigned id = priv->audio->id;

  if (priv->dial.string && !priv->dial.stopped && !priv->dial.canceled) {
    if (strcmp(dialstring, priv->dial.string) == 0) {
      DEBUG("avoid emitting duplicate %s(%u, %s) for %s",
        "SendingDialString", id, dialstring, self->nick);
      return;
    }
  }

  DEBUG("emitting %s(%u, %s) for %s",
    "SendingDialString", id, dialstring, self->nick);

  rtcom_tp_svc_channel_interface_dial_strings_emit_sending_dial_string(
    (RTComTpSvcChannelInterfaceDialStrings *)self,
    id, dialstring);

  old = priv->dial.string;
  priv->dial.string = g_strdup(dialstring);
  priv->dial.stopped = FALSE;
  priv->dial.canceled = FALSE;
  g_free(old);
}

static int
ring_stopped_dial_string(RingMediaChannel *self,
  int canceled)
{
  RingMediaChannelPrivate *priv = self->priv;

  priv->dial.stopped = TRUE;
  priv->dial.canceled |= canceled != 0;
  if (!priv->dial.playing)
    ring_emit_stopped_dial_string(self);

  return 0;
}

static void
ring_emit_stopped_dial_string(RingMediaChannel *self)
{
  RingMediaChannelPrivate *priv = self->priv;

  char *old = priv->dial.string;
  unsigned id = priv->audio->id;

  if (old) {
    gboolean canceled = priv->dial.canceled;

    DEBUG("emitting %s(%u, %s) for %s",
      "StoppedDialString", id, canceled ? "True" : "False", self->nick);

    rtcom_tp_svc_channel_interface_dial_strings_emit_stopped_dial_string(
      (RTComTpSvcChannelInterfaceDialStrings *)self,
      id, canceled);

    g_free(old);
  }
  else {
    DEBUG("AVOID EMITTING StoppedDialString for %s", self->nick);
  }

  priv->dial.string = NULL;
  priv->dial.canceled = FALSE;
  priv->dial.stopped = FALSE;
}
#endif

gboolean
ring_media_channel_send_dialstring(RingMediaChannel *self,
  guint id,
  char const *dialstring,
  guint duration,
  guint pause,
  GError **error)
{
  RingMediaChannelPrivate *priv = self->priv;

  DEBUG("(%u, \"%s\", %u, %u) for %s",
    id, dialstring, duration, pause, self->nick);

  (void)duration;
  (void)pause;

  if (ring_streamed_media_mixin_is_audio_stream (self, id)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Invalid stream");
    return FALSE;
  }
  else if (self->call_instance == NULL ||
      !ring_streamed_media_mixin_is_stream_connected (self, id)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
      "Channel is not connected");
    return FALSE;
  }
  else if (priv->dial.string) {
    g_set_error(error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
      "Already sending a dial string");
    return FALSE;
  }
  else if (modem_call_send_dtmf(self->call_instance, dialstring, NULL, NULL) < 0) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Bad dial string");
    return FALSE;
  }
  else {
    priv->dial.canceled = FALSE;
    return TRUE;
  }
}

/* ====================================================================== */
/* Signals from ModemCall */

static void
ring_media_channel_set_call_instance (RingMediaChannel *self,
                                      ModemCall *ci)
{
  RingMediaChannelPrivate *priv = self->priv;
  ModemCall *old = self->call_instance;

  if (ci == old)
    return;

  if (ci) {
    modem_call_set_handler(self->call_instance = MODEM_CALL (ci), self);

#define CONNECT(n, f)                                           \
    g_signal_connect(ci, n, G_CALLBACK(on_modem_call_ ## f), self)

    priv->signals.state = CONNECT("state", state);
    priv->signals.terminated = CONNECT("terminated", terminated);
    priv->signals.dtmf_tone = CONNECT("dtmf-tone", dtmf_tone);
    priv->signals.dialstring = CONNECT("dialstring", dialstring);

#undef CONNECT
  }
  else {
    ModemCall *old = self->call_instance;

    modem_call_set_handler(old, NULL);

#define DISCONNECT(n)                                           \
    if (priv->signals.n &&                                      \
      g_signal_handler_is_connected(old, priv->signals.n)) {    \
      g_signal_handler_disconnect(old, priv->signals.n);        \
    } (priv->signals.n = 0)

    DISCONNECT(state);
    DISCONNECT(terminated);
    DISCONNECT(dtmf_tone);
    DISCONNECT(dialstring);
#undef DISCONNECT
  }

  RING_MEDIA_CHANNEL_GET_CLASS(self)->set_call_instance(self, ci);

  if (old)
    g_object_unref (old);
  self->call_instance = ci;
  if (ci)
    g_object_ref (ci);

  if (ci == NULL && !priv->playing)
    ring_media_channel_close(self);
}

static void
on_modem_call_state(ModemCall *ci,
  ModemCallState state,
  RingMediaChannel *self)
{
  ring_media_channel_set_state(
    RING_MEDIA_CHANNEL(self), state, 0, 0);
}

void
ring_media_channel_set_state(RingMediaChannel *self,
  guint state,
  guint causetype,
  guint cause)
{
  self->priv->state = state;

  switch (state) {
    case MODEM_CALL_STATE_DIALING: on_modem_call_state_dialing(self); break;
    case MODEM_CALL_STATE_INCOMING: on_modem_call_state_incoming(self); break;
    case MODEM_CALL_STATE_ALERTING: on_modem_call_state_mo_alerting(self); break;
    case MODEM_CALL_STATE_WAITING: on_modem_call_state_waiting(self); break;
    case MODEM_CALL_STATE_ACTIVE: on_modem_call_state_active(self); break;
    case MODEM_CALL_STATE_DISCONNECTED: on_modem_call_state_release(self); break;
    case MODEM_CALL_STATE_HELD: on_modem_call_state_held(self); break;
    default:
      break;
  }

  RING_MEDIA_CHANNEL_GET_CLASS(self)->update_state(self, state, causetype, cause);
}

static void on_modem_call_state_incoming(RingMediaChannel *self)
{
  ring_streamed_media_mixin_update_audio (self, 0,
    TP_MEDIA_STREAM_STATE_CONNECTING,
    TP_MEDIA_STREAM_DIRECTION_NONE,
    TP_MEDIA_STREAM_PENDING_LOCAL_SEND |
    TP_MEDIA_STREAM_PENDING_REMOTE_SEND);
}

static void
on_modem_call_state_dialing(RingMediaChannel *self)
{
  ring_streamed_media_mixin_update_audio (self, 0,
    TP_MEDIA_STREAM_STATE_CONNECTING,
    TP_MEDIA_STREAM_DIRECTION_NONE,
    TP_MEDIA_STREAM_PENDING_LOCAL_SEND |
    TP_MEDIA_STREAM_PENDING_REMOTE_SEND);
}

static void
on_modem_call_state_mo_alerting(RingMediaChannel *self)
{
  ring_streamed_media_mixin_update_audio (self, 0,
    TP_MEDIA_STREAM_STATE_CONNECTED,
    TP_MEDIA_STREAM_DIRECTION_RECEIVE,
    TP_MEDIA_STREAM_PENDING_LOCAL_SEND);
}

#if nomore
static void
on_modem_call_state_mt_alerting(RingMediaChannel *self)
{
  /* Audio has been connected - at least locally */
  ring_streamed_media_mixin_update_audio (self, 0,
    TP_MEDIA_STREAM_STATE_CONNECTED,
    TP_MEDIA_STREAM_DIRECTION_SEND,
    TP_MEDIA_STREAM_PENDING_REMOTE_SEND);
}
#endif

static void
on_modem_call_state_waiting(RingMediaChannel *self)
{
  /* Audio has been connected - at least locally */
  ring_streamed_media_mixin_update_audio (self, 0,
    TP_MEDIA_STREAM_STATE_CONNECTED,
    TP_MEDIA_STREAM_DIRECTION_NONE,
    TP_MEDIA_STREAM_PENDING_LOCAL_SEND |
    TP_MEDIA_STREAM_PENDING_REMOTE_SEND);
}

#if nomore
static void
on_modem_call_state_answered(RingMediaChannel *self)
{
  /* Call has been answered we might not have radio channel */
  ring_streamed_media_mixin_update_audio (self, 0,
    TP_MEDIA_STREAM_STATE_CONNECTED,
    TP_MEDIA_STREAM_DIRECTION_SEND,
    TP_MEDIA_STREAM_PENDING_REMOTE_SEND);
}
#endif

static void
on_modem_call_state_active(RingMediaChannel *self)
{
  /* Call should be active now and media channels open. */
  ring_streamed_media_mixin_update_audio (self, 0,
    TP_MEDIA_STREAM_STATE_CONNECTED,
    TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL,
    0);

  ring_update_hold(self, TP_LOCAL_HOLD_STATE_UNHELD, 0);
}

static void
on_modem_call_state_held(RingMediaChannel *self)
{
  ring_streamed_media_mixin_update_audio (self, 0,
    TP_MEDIA_STREAM_STATE_CONNECTED,
    TP_MEDIA_STREAM_DIRECTION_NONE,
    0);

  ring_update_hold(self, TP_LOCAL_HOLD_STATE_HELD, 0);
}

static void
on_modem_call_state_release(RingMediaChannel *self)
{
}

#if nomore
static void
on_modem_call_state_terminated(RingMediaChannel *self)
{
  ring_streamed_media_mixin_update_audio (self, 0,
    TP_MEDIA_STREAM_STATE_DISCONNECTED,
    TP_MEDIA_STREAM_DIRECTION_NONE,
    0);
}
#endif

static void
on_modem_call_terminated(ModemCall *ci,
  RingMediaChannel *_self)
{
  g_object_set(RING_MEDIA_CHANNEL(_self), "call-instance", NULL, NULL);
}

static void
on_modem_call_dialstring(ModemCall *ci,
  char const *dialstring,
  RingMediaChannel *_self)
{
#if nomore
  RingMediaChannel *self = RING_MEDIA_CHANNEL(_self);

  if (dialstring)
    ring_sending_dial_string(self, dialstring);
  else
    ring_stopped_dial_string(self, FALSE);
#endif
}

static void
on_modem_call_dtmf_tone(ModemCall *call_instance,
  gint tone,
  RingMediaChannel *self)
{
  RingMediaChannelPrivate *priv = RING_MEDIA_CHANNEL(self)->priv;

  gboolean sending_dial_string;

  /* When sending single DTMF digits, dial.string is empty */
  sending_dial_string = priv->dial.string && priv->dial.string[0] != '\0';

  DEBUG("modem playing dtmf-tone %d '%c'",
    tone, 0 <= tone && tone <= 15 ? "0123456789*#ABCD"[tone] : '?');

  if (!sending_dial_string)
    return;

  if (0 <= tone && tone <= 14) {
    ring_media_channel_play_tone(RING_MEDIA_CHANNEL(self), tone, -6, 2000);
  }
  else {
    ring_media_channel_stop_playing(RING_MEDIA_CHANNEL(self), FALSE);
  }
}

/* ---------------------------------------------------------------------- */

static void ring_media_channel_stopped_playing(ModemTones *,
  guint source, gpointer _self);

void
ring_media_channel_play_tone(RingMediaChannel *self,
  int tone,
  int volume,
  unsigned duration)
{
  RingMediaChannelPrivate *priv = self->priv;

  if (priv->closing)
    return;

  if (1)
    /* XXX - no tones so far */
    return;

  if ((tone >= 0 && !modem_tones_is_playing(priv->tones, 0))
    || priv->playing) {
    priv->playing = modem_tones_start_full(priv->tones,
                    tone, volume, duration,
                    ring_media_channel_stopped_playing,
                    g_object_ref(self));
  }
}

gboolean
ring_media_channel_is_playing(RingMediaChannel const *self)
{
  return self && RING_MEDIA_CHANNEL(self)->priv->playing != 0;
}

void
ring_media_channel_idle_playing(RingMediaChannel *self)
{
  RingMediaChannelPrivate *priv = self->priv;

  if (priv->playing) {
    int event = modem_tones_playing_event(priv->tones, priv->playing);
    if (event < TONES_EVENT_RADIO_PATH_ACK &&
      modem_tones_is_playing(priv->tones, priv->playing) > 1200)
      ring_media_channel_stop_playing(self, FALSE);
  }
}

void
ring_media_channel_stop_playing(RingMediaChannel *self, gboolean always)
{
  RingMediaChannelPrivate *priv = self->priv;

  if (!priv->playing)
    return;

  modem_tones_stop(priv->tones, priv->playing);
}

static void
ring_media_channel_stopped_playing(ModemTones *tones,
  guint source,
  gpointer _self)
{
  RingMediaChannel *self = RING_MEDIA_CHANNEL(_self);
  RingMediaChannelPrivate *priv = self->priv;

  if (priv->playing == source) {
    priv->playing = 0;

#if nomore
    if (priv->dial.stopped)
      ring_emit_stopped_dial_string(self);
#endif

    if (!self->call_instance) {
      DEBUG("tone ended, closing");
      ring_media_channel_close(RING_MEDIA_CHANNEL(self));
    }
  }

  g_object_unref(self);
}

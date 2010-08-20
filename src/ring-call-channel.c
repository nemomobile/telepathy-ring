/*
 * ring-call-channel.c - Source for RingCallChannel
 * Handles peer-to-peer calls
 *
 * Copyright (C) 2007-2009 Nokia Corporation
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

#include "ring-call-channel.h"
#include "ring-member-channel.h"
#include "ring-emergency-service.h"

#include "ring-util.h"

#include "modem/service.h"
#include "modem/call.h"
#include "modem/tones.h"
#include "modem/errors.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-generic.h>

#include <ring-extensions/ring-extensions.h>

#include "ring-connection.h"
#include "ring-media-manager.h"

#include "ring-param-spec.h"

#if !defined(TP_CHANNEL_CALL_STATE_CONFERENCE_HOST)
/* Added in tp-spec 0.19.11 */
#define TP_CHANNEL_CALL_STATE_CONFERENCE_HOST (32)
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct _RingCallChannelPrivate
{
  guint anon_modes;
  char *dial2nd;
  char *emergency_service;
  char *initial_emergency_service;

  TpHandle peer_handle, initial_remote;
  TpHandle initiator, target;

  char *accepted;

  ModemRequest *creating_call;

  struct {
    char *message;
    TpHandle actor;
    TpChannelGroupChangeReason reason;
    guchar causetype, cause;
  } release;

  struct {
    RingConferenceChannel *conference;
    TpHandle handle;
  } member;

  uint8_t state;

  unsigned constructed:1, released:1, closing:1, disposed:1;

  unsigned call_instance_seen:1;

  unsigned originating:1, terminating:1;

  unsigned :0;

  unsigned call_state; /* Channel.Interface.CallState bits */

  struct {
    gulong emergency, multiparty;
    gulong waiting, on_hold, forwarded;
  } signals;
};

/* properties */
enum
{
  PROP_NONE,
  PROP_CHANNEL_PROPERTIES,
  PROP_INTERFACES,            /* o.f.T.Channel.Interfaces */

  PROP_TARGET,                /* o.f.T.Channel.TargetHandle */
  PROP_TARGET_ID,             /* o.f.T.Channel.TargetID */
  PROP_TARGET_TYPE,           /* o.f.T.Channel.HandleType */

  PROP_INITIATOR,             /* o.f.T.Channel.InitiatorHandle */
  PROP_INITIATOR_ID,          /* o.f.T.Channel.InitiatorID */

  PROP_ANON_MODES,

  PROP_CURRENT_SERVICE_POINT, /* o.f.T.C.I.ServicePoint.CurrentServicePoint */
  PROP_INITIAL_SERVICE_POINT, /* o.f.T.C.I.ServicePoint.InitialServicePoint */

  /* ring-specific properties */
  PROP_EMERGENCY_SERVICE,
  PROP_INITIAL_EMERGENCY_SERVICE,

  PROP_TERMINATING,
  PROP_ORIGINATING,

  PROP_MEMBER,
  PROP_MEMBER_MAP,
  PROP_CONFERENCE,

  PROP_PEER,
  PROP_INITIAL_REMOTE,

  LAST_PROPERTY
};

static void ring_call_channel_implement_media_channel(RingMediaChannelClass *);

static TpDBusPropertiesMixinIfaceImpl
ring_call_channel_dbus_property_interfaces[];
static void ring_channel_call_state_iface_init(gpointer, gpointer);
static void ring_channel_splittable_iface_init(gpointer, gpointer);

static gboolean ring_call_channel_add_member(
  GObject *obj, TpHandle handle, const char *message, GError **error);
static gboolean ring_call_channel_remove_member_with_reason(
  GObject *obj, TpHandle handle, const char *message,
  guint reason,
  GError **error);
static gboolean ring_call_channel_accept_pending(
  GObject *, TpHandle handle, const char *message, GError **error);
static gboolean ring_call_channel_request_remote(
  GObject *, TpHandle handle, const char *message, GError **error);
static gboolean ring_call_channel_remote_pending(
  RingCallChannel *, TpHandle handle, const char *message);

G_DEFINE_TYPE_WITH_CODE(
  RingCallChannel, ring_call_channel, RING_TYPE_MEDIA_CHANNEL,
  G_IMPLEMENT_INTERFACE(RING_TYPE_MEMBER_CHANNEL, NULL)
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_DBUS_PROPERTIES,
    tp_dbus_properties_mixin_iface_init)
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_GROUP,
    tp_group_mixin_iface_init)
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_CALL_STATE,
    ring_channel_call_state_iface_init)
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_SERVICE_POINT,
    NULL);
  G_IMPLEMENT_INTERFACE(RING_TYPE_SVC_CHANNEL_INTERFACE_SPLITTABLE,
    ring_channel_splittable_iface_init));

const char *ring_call_channel_interfaces[] = {
  RING_MEDIA_CHANNEL_INTERFACES,
  TP_IFACE_CHANNEL_INTERFACE_GROUP,
  TP_IFACE_CHANNEL_INTERFACE_CALL_STATE,
  TP_IFACE_CHANNEL_INTERFACE_SERVICE_POINT,
  RING_IFACE_CHANNEL_INTERFACE_SPLITTABLE,
  NULL
};

static GHashTable *ring_call_channel_properties(RingCallChannel *self);

static void ring_call_channel_update_state(RingMediaChannel *_self,
  guint state, guint causetype, guint cause);
static void ring_call_channel_play_error_tone(RingCallChannel *self,
  guint state, guint causetype, guint cause);
static gboolean ring_call_channel_close(RingMediaChannel *_self,
  gboolean immediately);
static void ring_call_channel_set_call_instance(RingMediaChannel *_self,
  ModemCall *ci);
static gboolean ring_call_channel_validate_media_handle(
  RingMediaChannel *_self,
  guint *handle,
  GError **);
static gboolean ring_call_channel_create_streams(RingMediaChannel *_self,
  TpHandle handle,
  gboolean audio,
  gboolean video,
  GError **);

static guint ring_call_channel_get_member_handle(RingCallChannel *self);

static void on_modem_call_state_dialing(RingCallChannel *self);
static void on_modem_call_state_incoming(RingCallChannel *self);
static void on_modem_call_state_waiting(RingCallChannel *self);
static void on_modem_call_state_mo_alerting(RingCallChannel *self);
static void on_modem_call_state_mt_alerting(RingCallChannel *self);
static void on_modem_call_state_answered(RingCallChannel *self);
static void on_modem_call_state_active(RingCallChannel *self);
static void on_modem_call_state_mo_release(RingCallChannel *, guint causetype, guint cause);
static void on_modem_call_state_mt_release(RingCallChannel *, guint causetype, guint cause);
static void on_modem_call_state_terminated(RingCallChannel *, guint causetype, guint cause);

static void ring_call_channel_released(RingCallChannel *self,
  TpHandle actor, TpChannelGroupChangeReason reason, char const *message,
  GError *error, char const *debug);

static void on_modem_call_emergency(ModemCall *, char const *, RingCallChannel *);
static void on_modem_call_on_hold(ModemCall *, int onhold, RingCallChannel *);
static void on_modem_call_forwarded(ModemCall *, RingCallChannel *);
static void on_modem_call_waiting(ModemCall *, RingCallChannel *);
static void on_modem_call_multiparty(ModemCall *, RingCallChannel *);

/* ====================================================================== */
/* GObject interface */

static void
ring_call_channel_init(RingCallChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, RING_TYPE_CALL_CHANNEL, RingCallChannelPrivate);
}

static void
ring_call_channel_constructed(GObject *object)
{
  RingCallChannel *self = RING_CALL_CHANNEL(object);
  RingCallChannelPrivate *priv = self->priv;
  TpBaseConnection *connection;
  TpHandleRepoIface *repo;
  TpHandle self_handle;

  if (G_OBJECT_CLASS(ring_call_channel_parent_class)->constructed)
    G_OBJECT_CLASS(ring_call_channel_parent_class)->constructed(object);

  connection = TP_BASE_CONNECTION(self->base.connection);
  repo = tp_base_connection_get_handles(connection, TP_HANDLE_TYPE_CONTACT);
  if (priv->target)
    tp_handle_ref(repo, priv->target);
  tp_handle_ref(repo, priv->initiator);

  if (priv->peer_handle != 0)
    g_assert(priv->peer_handle == priv->target);

  self_handle = tp_base_connection_get_self_handle(connection);

  tp_group_mixin_init(
    object, G_STRUCT_OFFSET(RingCallChannel, group),
    tp_base_connection_get_handles(connection, TP_HANDLE_TYPE_CONTACT),
    self_handle);

  priv->constructed = 1;
}

static void
ring_call_channel_emit_initial(RingMediaChannel *_self)
{
  TpGroupMixin *mixin = TP_GROUP_MIXIN(_self);
  RingCallChannel *self = RING_CALL_CHANNEL(_self);
  RingCallChannelPrivate *priv = self->priv;
  TpHandle self_handle = mixin->self_handle;
  TpIntSet *member_set = tp_intset_new();
  TpIntSet *local_pending_set = NULL, *remote_pending_set = NULL;
  char const *message = "";
  TpChannelGroupChangeReason reason = 0;
  TpChannelGroupFlags add = 0, del = 0;

  tp_intset_add(member_set, priv->initiator);

  if (priv->initiator == priv->target) {
    /* Incoming call */
    message = "Channel created for incoming call";
    reason = TP_CHANNEL_GROUP_CHANGE_REASON_INVITED;
    local_pending_set = tp_intset_new();
    tp_intset_add(local_pending_set, self_handle);
  }
  else if (priv->initial_remote) {
    /* Outgoing call */
    message = "Channel created for outgoing call";
    reason = TP_CHANNEL_GROUP_CHANGE_REASON_INVITED;
    remote_pending_set = tp_intset_new();
    tp_intset_add(remote_pending_set, priv->initial_remote);
    add |= TP_CHANNEL_GROUP_FLAG_CAN_RESCIND;
  }
  else {
    /* Outgoing call, but without handle */
    message = "Channel created";
    reason = TP_CHANNEL_GROUP_CHANGE_REASON_NONE;
    if (priv->target == 0)
      add |= TP_CHANNEL_GROUP_FLAG_CAN_ADD;
  }

  add |= TP_CHANNEL_GROUP_FLAG_PROPERTIES;
  add |= TP_CHANNEL_GROUP_FLAG_MEMBERS_CHANGED_DETAILED;
  add |= TP_CHANNEL_GROUP_FLAG_CAN_REMOVE;

  tp_group_mixin_change_flags(G_OBJECT(self), add, del);

  tp_group_mixin_change_members(G_OBJECT(self), message,
    member_set, NULL,
    local_pending_set, remote_pending_set,
    priv->initiator, reason);

  tp_intset_destroy(member_set);
  if (local_pending_set) tp_intset_destroy(local_pending_set);
  if (remote_pending_set) tp_intset_destroy(remote_pending_set);
}

static void
ring_call_channel_get_property(GObject *obj,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  RingCallChannel *self = RING_CALL_CHANNEL(obj);
  RingCallChannelPrivate *priv = self->priv;
  char const *id;

  switch (property_id) {
    case PROP_CHANNEL_PROPERTIES:
      g_value_take_boxed(value, ring_call_channel_properties(self));
      break;
    case PROP_INTERFACES:
      g_value_set_boxed(value, ring_call_channel_interfaces);
      break;
    case PROP_TARGET:
      g_value_set_uint(value, priv->target);
      break;
    case PROP_TARGET_TYPE:
      g_value_set_uint(value, priv->target
        ? TP_HANDLE_TYPE_CONTACT
        : TP_HANDLE_TYPE_NONE);
      break;
    case PROP_TARGET_ID:
      id = ring_connection_inspect_contact(self->base.connection, priv->target);
      g_value_set_string(value, id);
      break;
    case PROP_INITIATOR:
      g_value_set_uint (value, priv->initiator);
      break;
    case PROP_INITIATOR_ID:
      id = ring_connection_inspect_contact(self->base.connection, priv->initiator);
      g_value_set_static_string(value, id);
      break;
    case PROP_ANON_MODES:
      g_value_set_uint(value, priv->anon_modes);
      break;
    case PROP_ORIGINATING:
      g_value_set_boolean(value, priv->originating);
      break;
    case PROP_TERMINATING:
      g_value_set_boolean(value, priv->terminating);
      break;
    case PROP_MEMBER:
      g_value_set_uint(value, ring_call_channel_get_member_handle(self));
      break;
    case PROP_MEMBER_MAP:
      g_value_take_boxed(
        value, ring_member_channel_get_handlemap(RING_MEMBER_CHANNEL(self)));
      break;
    case PROP_CONFERENCE:
      {
        char *object_path = NULL;
        if (priv->member.conference) {
          g_object_get(priv->member.conference, "object-path", &object_path, NULL);
        }
        else {
          object_path = strdup("/");
        }
        g_value_take_boxed(value, object_path);
      }
      break;
    case PROP_INITIAL_SERVICE_POINT:
      g_value_take_boxed(value,
        ring_emergency_service_new(priv->initial_emergency_service));
      break;
    case PROP_CURRENT_SERVICE_POINT:
      g_value_take_boxed(value,
        ring_emergency_service_new(priv->emergency_service));
      break;
    case PROP_EMERGENCY_SERVICE:
      g_value_set_string(value, priv->emergency_service);
      break;
    case PROP_INITIAL_EMERGENCY_SERVICE:
      g_value_set_string(value, priv->initial_emergency_service);
      break;
    case PROP_PEER:
      g_value_set_uint(value, priv->peer_handle);
      break;
    case PROP_INITIAL_REMOTE:
      g_value_set_uint(value, priv->initial_remote);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_call_channel_set_property(GObject *obj,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  RingCallChannel *self = RING_CALL_CHANNEL(obj);
  RingCallChannelPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_ANON_MODES:
      priv->anon_modes = g_value_get_uint(value);
      break;
    case PROP_TARGET_TYPE:
      /* this property is writable in the interface, but not actually
       * meaningfully changable on this channel, so we do nothing */
      break;
    case PROP_TARGET:
      priv->target = g_value_get_uint(value);
      break;
    case PROP_INITIATOR:
      priv->initiator = g_value_get_uint(value);
      break;
    case PROP_ORIGINATING:
      priv->originating = g_value_get_boolean(value);
      break;
    case PROP_TERMINATING:
      priv->terminating = g_value_get_boolean(value);
      break;
    case PROP_MEMBER:
      priv->member.handle = g_value_get_uint(value);
      break;
    case PROP_INITIAL_EMERGENCY_SERVICE:
      priv->initial_emergency_service = g_value_dup_string(value);
      break;
    case PROP_PEER:
      if (priv->peer_handle == 0) {
        if (priv->target == 0 || priv->target == g_value_get_uint(value))
          priv->peer_handle = g_value_get_uint(value);
      }
      break;
    case PROP_INITIAL_REMOTE:
      priv->initial_remote = g_value_get_uint(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_call_channel_dispose(GObject *object)
{
  RingCallChannel *self = RING_CALL_CHANNEL(object);
  RingCallChannelPrivate *priv = self->priv;

  if (priv->member.handle) {
    TpHandleRepoIface *repo = tp_base_connection_get_handles(
      (TpBaseConnection *)(self->base.connection), TP_HANDLE_TYPE_CONTACT);
    tp_handle_unref(repo, priv->member.handle);
    priv->member.handle = 0;
  }

  ((GObjectClass *)ring_call_channel_parent_class)->dispose(object);
}


static void
ring_call_channel_finalize(GObject *object)
{
  RingCallChannel *self = RING_CALL_CHANNEL(object);
  RingCallChannelPrivate *priv = self->priv;

  tp_group_mixin_finalize(object);

  g_free(priv->emergency_service);
  g_free(priv->initial_emergency_service);
  g_free(priv->dial2nd);
  g_free(priv->accepted);
  g_free(priv->release.message);

  G_OBJECT_CLASS(ring_call_channel_parent_class)->finalize(object);

  DEBUG("exit");
}

/* ====================================================================== */
/* GObjectClass */

static void
ring_call_channel_class_init(RingCallChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  g_type_class_add_private(klass, sizeof (RingCallChannelPrivate));

  object_class->constructed = ring_call_channel_constructed;
  object_class->get_property = ring_call_channel_get_property;
  object_class->set_property = ring_call_channel_set_property;
  object_class->dispose = ring_call_channel_dispose;
  object_class->finalize = ring_call_channel_finalize;

  ring_call_channel_implement_media_channel(RING_MEDIA_CHANNEL_CLASS(klass));

  klass->dbus_properties_class.interfaces =
    ring_call_channel_dbus_property_interfaces;
  tp_dbus_properties_mixin_class_init(object_class,
    G_STRUCT_OFFSET(RingCallChannelClass, dbus_properties_class));
  tp_group_mixin_init_dbus_properties(object_class);

  tp_group_mixin_class_init(
    object_class,
    G_STRUCT_OFFSET(RingCallChannelClass, group_class),
    ring_call_channel_add_member,
    NULL);
  tp_group_mixin_class_set_remove_with_reason_func(
    object_class, ring_call_channel_remove_member_with_reason);

  g_object_class_override_property(
    object_class, PROP_CHANNEL_PROPERTIES, "channel-properties");

  g_object_class_override_property(
    object_class, PROP_INTERFACES, "interfaces");

  g_object_class_override_property(
    object_class, PROP_TARGET_TYPE, "handle-type");

  g_object_class_override_property(
    object_class, PROP_TARGET, "handle");

  g_object_class_override_property(
    object_class, PROP_TARGET_ID, "handle-id");

  g_object_class_override_property(
    object_class, PROP_INITIATOR, "initiator");

  g_object_class_override_property(
    object_class, PROP_INITIATOR_ID, "initiator-id");

  g_object_class_install_property(
    object_class, PROP_ANON_MODES, ring_param_spec_anon_modes());

  g_object_class_install_property(
    object_class, PROP_INITIAL_SERVICE_POINT,
    g_param_spec_boxed("initial-service-point",
      "Initial Service Point",
      "The service point initially associated with this channel",
      TP_STRUCT_TYPE_SERVICE_POINT,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_CURRENT_SERVICE_POINT,
    g_param_spec_boxed("current-service-point",
      "Current Service Point",
      "The service point currently associated with this channel",
      TP_STRUCT_TYPE_SERVICE_POINT,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_EMERGENCY_SERVICE,
    g_param_spec_string("emergency-service",
      "Emergency Service",
      "Emergency service associated with this channel",
      "",
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_INITIAL_EMERGENCY_SERVICE,
    g_param_spec_string("initial-emergency-service",
      "Initial Emergency Service",
      "Emergency service initially associated with this channel",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_ORIGINATING,
    g_param_spec_boolean("originating",
      "Mobile-Originated Call",
      "Call associated with this channel is mobile-originated",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_TERMINATING,
    g_param_spec_boolean("terminating",
      "Mobile-Terminating Call",
      "Call associated with this channel is mobile-terminating",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS));

  g_object_class_override_property(
    object_class, PROP_MEMBER, "member-handle");

  g_object_class_override_property(
    object_class, PROP_MEMBER_MAP, "member-map");

  g_object_class_override_property(
    object_class, PROP_CONFERENCE, "member-conference");

  g_object_class_override_property(
    object_class, PROP_PEER, "peer");

  g_object_class_install_property(
    object_class, PROP_INITIAL_REMOTE,
    g_param_spec_uint("initial-remote",
      "Initial Remote Handle",
      "Handle added to the remote pending set initially",
      0, G_MAXUINT32, 0,    /* min, max, default */
      G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS));
}

/* ====================================================================== */
/**
 * org.freedesktop.DBus properties
 */

/* Properties for o.f.T.Channel.Interface.ServicePoint */
static TpDBusPropertiesMixinPropImpl service_point_properties[] = {
  { "InitialServicePoint", "initial-service-point" },
  { "CurrentServicePoint", "current-service-point" },
  { NULL }
};

static TpDBusPropertiesMixinIfaceImpl
ring_call_channel_dbus_property_interfaces[] = {
  {
    TP_IFACE_CHANNEL_INTERFACE_SERVICE_POINT,
    tp_dbus_properties_mixin_getter_gobject_properties,
    NULL,
    service_point_properties,
  },
  { NULL }
};

/** Return a hash describing channel properties
 *
 * A channel's properties are constant for its lifetime on the bus, so
 * this property should only change when the closed signal is emitted (so
 * that respawned channels can reappear on the bus with different
 * properties).
 */
static GHashTable *
ring_call_channel_properties(RingCallChannel *self)
{
  GHashTable *properties;

  properties = ring_media_channel_properties(RING_MEDIA_CHANNEL(self));

  ring_channel_add_properties(self, properties,
    TP_IFACE_CHANNEL_INTERFACE_SERVICE_POINT, "CurrentServicePoint",
    NULL);

  if (!RING_STR_EMPTY(self->priv->initial_emergency_service))
    ring_channel_add_properties(self, properties,
      TP_IFACE_CHANNEL_INTERFACE_SERVICE_POINT, "InitialServicePoint",
      NULL);

  return properties;
}

/* ====================================================================== */
/* RingMediaChannel implementation */

static ModemRequest *ring_call_channel_create(RingCallChannel *, GError **error);

/** Close channel */
static gboolean
ring_call_channel_close(RingMediaChannel *_self, gboolean immediately)
{
  RingCallChannel *self = RING_CALL_CHANNEL(_self);
  RingCallChannelPrivate *priv = RING_CALL_CHANNEL(self)->priv;

  priv->closing = 1;

  if (priv->member.conference) {
    TpHandle actor = priv->release.actor;
    TpChannelGroupChangeReason reason = priv->release.reason;

    ring_conference_channel_emit_channel_removed(
      priv->member.conference, RING_MEMBER_CHANNEL(self),
      "Member channel closed",
      actor, reason);

    g_assert(priv->member.conference == NULL);
    priv->member.conference = NULL;
  }

  if (self->base.call_instance) {
    if (!priv->release.message)
      priv->release.message = g_strdup("Channel closed");
    modem_call_request_release(self->base.call_instance, NULL, NULL);
    return immediately;
  }
  else if (priv->creating_call) {
    if (immediately) {
      modem_request_cancel(priv->creating_call);
      priv->creating_call = NULL;
      g_object_unref(self);
    }
    else if (!priv->release.message)
      priv->release.message = g_strdup("Channel closed");
    return immediately;
  }

  return TRUE;
}

static void
ring_call_channel_update_state(RingMediaChannel *_self,
  guint state, guint causetype, guint cause)
{
  RingCallChannel *self = RING_CALL_CHANNEL(_self);
#if nomore
  RingCallChannelPrivate *priv = self->priv;
#endif

  switch (state) {
    case MODEM_CALL_STATE_DIALING:
    case MODEM_CALL_STATE_INCOMING:
    case MODEM_CALL_STATE_WAITING:
    case MODEM_CALL_STATE_ACTIVE:
      ring_media_channel_stop_playing(RING_MEDIA_CHANNEL(self), TRUE);
      break;
    case MODEM_CALL_STATE_ALERTING:
      ring_media_channel_play_tone(RING_MEDIA_CHANNEL(self),
        TONES_EVENT_RINGING, 0, 0);
      break;
    case MODEM_CALL_STATE_DISCONNECTED:
      ring_call_channel_play_error_tone(self, state, causetype, cause);
      break;

#if nomore
    case MODEM_CALL_STATE_TERMINATED:
      if (!priv->released) {
        ring_call_channel_play_error_tone(self, state, causetype, cause);
        break;
      }
      /* FALLTHROUGH */
#endif
    case MODEM_CALL_STATE_INVALID:
      ring_media_channel_idle_playing(RING_MEDIA_CHANNEL(self));
      break;

    default:
      break;
  }

  switch (state) {
    case MODEM_CALL_STATE_DIALING: on_modem_call_state_dialing(self); break;
    case MODEM_CALL_STATE_INCOMING: on_modem_call_state_incoming(self); break;
    case MODEM_CALL_STATE_ALERTING: on_modem_call_state_mo_alerting(self); break;
    case MODEM_CALL_STATE_WAITING: on_modem_call_state_waiting(self); break;
    case MODEM_CALL_STATE_ACTIVE: on_modem_call_state_active(self); break;
    case MODEM_CALL_STATE_DISCONNECTED: on_modem_call_state_mo_release(self, causetype, cause); break;
      /*case MODEM_CALL_STATE_TERMINATED: on_modem_call_state_terminated(self, causetype, cause); break;*/
    default:
      break;
  }
}

static void
ring_call_channel_play_error_tone(RingCallChannel *self,
  guint state, guint causetype, guint cause)
{
  int event_tone;
  guint duration = 5000;
  int volume = 0;

  guint hold;

  if (!self->base.call_instance)
    return;

  event_tone = modem_call_event_tone(state, causetype, cause);

#if nomore
  if (state == MODEM_CALL_STATE_MT_RELEASE &&
    event_tone != TONES_STOP && event_tone != TONES_NONE) {
    TpGroupMixin *mixin = TP_GROUP_MIXIN(self);
    if (tp_handle_set_size(mixin->local_pending))
      /* Case remote end drops call before it is accepted */
      event_tone = TONES_NONE;
  }
#endif

  hold = TP_LOCAL_HOLD_STATE_UNHELD;
  g_object_get(self, "hold-state", &hold, NULL);

  if (hold != TP_LOCAL_HOLD_STATE_UNHELD &&
    hold != TP_LOCAL_HOLD_STATE_PENDING_UNHOLD) {
    /* XXX - dropped tone damped 3dB if call was on hold */
    event_tone = TONES_EVENT_DROPPED, duration = 1200, volume = -3;
  }

  ring_media_channel_play_tone(RING_MEDIA_CHANNEL(self),
    event_tone, volume, duration);
}

static void
ring_call_channel_set_call_instance(RingMediaChannel *_self,
  ModemCall *ci)
{
  RingCallChannel *self = RING_CALL_CHANNEL(_self);
  RingCallChannelPrivate *priv = self->priv;

  if (ci) {
    priv->call_instance_seen = 1;

#define CONNECT(n, f)                                                   \
    g_signal_connect(ci, n, G_CALLBACK(on_modem_call_ ## f), self)

    priv->signals.waiting = CONNECT("waiting", waiting);
    priv->signals.multiparty = CONNECT("multiparty", multiparty);
    priv->signals.emergency = CONNECT("emergency", emergency);
    priv->signals.on_hold = CONNECT("on-hold", on_hold);
    priv->signals.forwarded = CONNECT("forwarded", forwarded);
#undef CONNECT
  }
  else {
    ci = self->base.call_instance;

#define DISCONNECT(n)                                           \
    if (priv->signals.n &&                                      \
      g_signal_handler_is_connected(ci, priv->signals.n)) {     \
      g_signal_handler_disconnect(ci, priv->signals.n);         \
    } (priv->signals.n = 0)

    DISCONNECT(waiting);
    DISCONNECT(multiparty);
    DISCONNECT(emergency);
    DISCONNECT(on_hold);
    DISCONNECT(forwarded);
#undef DISCONNECT
  }
}

static gboolean
ring_call_channel_validate_media_handle(RingMediaChannel *_self,
  guint *handlep,
  GError **error)
{
  DEBUG("enter");

  RingCallChannel *self = RING_CALL_CHANNEL(_self);
  TpGroupMixin *mixin = TP_GROUP_MIXIN(self);
  TpHandleRepoIface *repo;
  RingCallChannelPrivate *priv = self->priv;

  guint handle = *handlep;

  /* generic checks */
  if (handle == 0)
    handle = priv->peer_handle;
  if (handle == 0)
    handle = priv->target;

  repo = tp_base_connection_get_handles(
    (TpBaseConnection *)(self->base.connection), TP_HANDLE_TYPE_CONTACT);

  if (!tp_handle_is_valid(repo, handle, error))
    return FALSE;

  if (handle == mixin->self_handle) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "cannot establish call streams with yourself");
    return FALSE;
  }

  if (!modem_call_is_valid_address(tp_handle_inspect(repo, handle))) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Invalid handle %u=\"%s\" for media streams",
      handle, tp_handle_inspect(repo, handle));
    return FALSE;
  }

  if (!tp_handle_set_is_member(self->group.members, handle) &&
    !tp_handle_set_is_member(self->group.remote_pending, handle) &&
    handle != priv->target) {
    g_set_error(error,
      TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "given handle %u is not a member of the channel",
      handle);
    return FALSE;
  }

  *handlep = handle;
  return TRUE;
}

static void reply_to_modem_call_request_dial(ModemCallService *_service,
  ModemRequest *request,
  ModemCall *instance,
  GError *error,
  gpointer _channel);

static gboolean
ring_call_channel_create_streams(RingMediaChannel *_self,
  guint handle,
  gboolean audio,
  gboolean video,
  GError **error)
{
  RingCallChannel *self = RING_CALL_CHANNEL(_self);
  RingCallChannelPrivate *priv = self->priv;

  (void)audio; (void)video;

  if (priv->peer_handle == 0)
    g_object_set(self, "peer", handle, NULL);

  if (handle != priv->peer_handle) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT, "Invalid handle");
    return FALSE;
  }

  if (!modem_call_service_is_connected(self->base.call_service)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_DISCONNECTED,
      "not connected to call service");
    return FALSE;
  }

  if (priv->call_instance_seen) {
    DEBUG("Already associated with a call");
    return TRUE;
  }

  priv->call_instance_seen = 1;

  return ring_call_channel_create(self, error) != NULL;
}

void
ring_call_channel_initial_audio(RingCallChannel *self,
  RingMediaManager *manager,
  gpointer channelrequest)
{
  ModemRequest *request;
  GError *error = NULL;

  DEBUG("%s(%p, %p, %p) called", __func__, self, manager, channelrequest);

  self->priv->call_instance_seen = 1;

  request = ring_call_channel_create(self, &error);

  if (request) {
    modem_request_add_qdatas(
      request,
      g_quark_from_static_string("RingChannelRequest"), channelrequest, NULL,
      g_type_qname(RING_TYPE_MEDIA_MANAGER), g_object_ref(manager), g_object_unref,
      NULL);
  }
  else {
    ring_media_manager_emit_new_channel(manager, channelrequest, self, error);
    g_error_free(error);
  }
}

static ModemRequest *
ring_call_channel_create(RingCallChannel *self, GError **error)
{
  RingCallChannelPrivate *priv = self->priv;
  TpHandle handle = priv->peer_handle;
  char const *destination;
  ModemClirOverride clir;
  char *number = NULL;
  ModemRequest *request;

  destination = ring_connection_inspect_contact(self->base.connection, handle);

  if (RING_STR_EMPTY(destination)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT, "Invalid handle");
    return NULL;
  }

  if (priv->anon_modes & TP_ANONYMITY_MODE_CLIENT_INFO)
    clir = MODEM_CLIR_OVERRIDE_ENABLED;
  else if (priv->anon_modes & TP_ANONYMITY_MODE_SHOW_CLIENT_INFO)
    clir = MODEM_CLIR_OVERRIDE_DISABLED;
  else
    clir = MODEM_CLIR_OVERRIDE_DEFAULT;

  if (priv->dial2nd)
    g_free(priv->dial2nd), priv->dial2nd = NULL;

  modem_call_split_address(destination, &number, &priv->dial2nd, &clir);
  if (priv->dial2nd)
    DEBUG("2nd stage dialing: \"%s\"", priv->dial2nd);

  request = modem_call_request_dial(self->base.call_service, number, clir,
            reply_to_modem_call_request_dial, self);

  g_free(number);

  if (request) {
    priv->creating_call = request;
    g_object_ref(self);
  }

  return request;
}

static void
reply_to_modem_call_request_dial(ModemCallService *_service,
  ModemRequest *request,
  ModemCall *ci,
  GError *error,
  gpointer _channel)
{
  RingCallChannel *self = RING_CALL_CHANNEL(_channel);
  RingCallChannelPrivate *priv = self->priv;
  GError *error0 = NULL;
  TpChannelGroupChangeReason reason;
  char *debug;
  gpointer channelrequest;

  if (request == priv->creating_call) {
    priv->creating_call = NULL;
    g_object_unref(self);
  }

  channelrequest = modem_request_steal_data(request, "RingChannelRequest");

  if (channelrequest) {
    RingMediaManager *manager;

    manager = modem_request_get_qdata(request, g_type_qname(RING_TYPE_MEDIA_MANAGER));

    if (ci)
      g_object_set(self, "initial-remote", priv->target, NULL);

    ring_media_manager_emit_new_channel(manager, channelrequest, self, NULL);
  }

  if (ci) {
    g_assert(self->base.call_instance == NULL);
    g_object_set(self, "call-instance", ci, NULL);
    if (priv->release.message == NULL)
      ring_media_channel_set_state(RING_MEDIA_CHANNEL(self),
        MODEM_CALL_STATE_DIALING, 0, 0);
    else
      modem_call_request_release(ci, NULL, NULL);
    return;
  }

  ring_media_channel_play_tone(RING_MEDIA_CHANNEL(self),
    modem_call_error_tone(error), 0, 4000);

  reason = ring_channel_group_error_reason(error);

  ring_warning("Call.Dial: message=\"%s\" reason=%s (%u) cause=%s.%s",
    error->message, ring_util_reason_name(reason), reason,
    modem_error_domain_prefix(error->domain),
    modem_error_name(error, NULL, 0));
  debug = g_strdup_printf("Dial() failed: reason=%s (%u) cause=%s.%s",
          ring_util_reason_name(reason), reason,
          modem_error_domain_prefix(error->domain),
          modem_error_name(error, NULL, 0));

  ring_call_channel_released(self,
    priv->peer_handle, reason, error->message, error, debug);

  if (error0)
    g_error_free(error0);
  g_free(debug);

  if (!ring_media_channel_is_playing(RING_MEDIA_CHANNEL(self)))
    ring_media_channel_close(RING_MEDIA_CHANNEL(self));
}

static void
ring_call_channel_implement_media_channel(RingMediaChannelClass *media_class)
{
  media_class->emit_initial = ring_call_channel_emit_initial;
  media_class->close = ring_call_channel_close;
  media_class->update_state = ring_call_channel_update_state;
  media_class->set_call_instance = ring_call_channel_set_call_instance;
  media_class->validate_media_handle = ring_call_channel_validate_media_handle;
  media_class->create_streams = ring_call_channel_create_streams;
}

/* ---------------------------------------------------------------------- */
/* Implement org.freedesktop.Telepathy.Channel.Interface.CallState */

static
void get_call_states(TpSvcChannelInterfaceCallState *iface,
  DBusGMethodInvocation *context)
{
  RingCallChannel *self = RING_CALL_CHANNEL(iface);
  RingCallChannelPrivate *priv = self->priv;
  GHashTable *call_states;

  call_states = g_hash_table_new (NULL, NULL);

  if (priv->peer_handle) {
    g_hash_table_replace (call_states,
      GUINT_TO_POINTER (priv->peer_handle),
      GUINT_TO_POINTER ((guint)priv->call_state));
  }

  tp_svc_channel_interface_call_state_return_from_get_call_states
    (context, call_states);

  g_hash_table_destroy (call_states);
}

static void
ring_channel_call_state_iface_init(gpointer g_iface, gpointer iface_data)
{
  TpSvcChannelInterfaceCallStateClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_call_state_implement_##x( \
    klass, x)
  IMPLEMENT(get_call_states);
#undef IMPLEMENT
}

static void ring_update_call_state(RingCallChannel *self,
  unsigned set_states,
  unsigned zap_states)
{
  RingCallChannelPrivate *priv = RING_CALL_CHANNEL(self)->priv;
  unsigned call_state, old_state = priv->call_state;

  call_state = priv->call_state =
    (old_state & ~zap_states) | set_states;

  if (priv->peer_handle && call_state != old_state) {
    DEBUG("emitting %s(%u, %u)",
      "CallStateChanged", priv->peer_handle, call_state);
    tp_svc_channel_interface_call_state_emit_call_state_changed(
      TP_SVC_CHANNEL_INTERFACE_CALL_STATE(self),
      priv->peer_handle, call_state);
  }
}

/** Remote end has put us on hold */
static void
on_modem_call_on_hold(ModemCall *ci,
  gboolean onhold,
  RingCallChannel *self)
{
  if (onhold)
    ring_update_call_state(self, TP_CHANNEL_CALL_STATE_HELD, 0);
  else
    ring_update_call_state(self, 0, TP_CHANNEL_CALL_STATE_HELD);
}

/** This call has been forwarded */
static void
on_modem_call_forwarded(ModemCall *ci,
  RingCallChannel *self)
{
  ring_update_call_state(self, TP_CHANNEL_CALL_STATE_FORWARDED, 0);
}

/* MO call is waiting */
static void
on_modem_call_waiting(ModemCall *ci,
  RingCallChannel *self)
{
  ring_update_call_state(self, TP_CHANNEL_CALL_STATE_QUEUED, 0);
}

static void
on_modem_call_multiparty(ModemCall *ci,
  RingCallChannel *self)
{
  RingCallChannelPrivate *priv = self->priv;

  /* If the conference host state is set, we need to first zap it */
  if (priv->call_state & TP_CHANNEL_CALL_STATE_CONFERENCE_HOST)
    ring_update_call_state(self, 0, TP_CHANNEL_CALL_STATE_CONFERENCE_HOST);

  ring_update_call_state(self, TP_CHANNEL_CALL_STATE_CONFERENCE_HOST, 0);
}

/* Invoked when MO call targets an emergency service */
static void
on_modem_call_emergency(ModemCall *ci,
  char const *emergency_service,
  RingCallChannel *self)
{
  RingCallChannelPrivate *priv = self->priv;

  DEBUG("%s", emergency_service);

  if (g_strcmp0 (emergency_service, priv->emergency_service) != 0) {
    RingEmergencyService *esp;

    g_free(priv->emergency_service);
    priv->emergency_service = g_strdup(emergency_service);
    g_object_notify(G_OBJECT(self), "emergency-service");

    DEBUG("emitting ServicePointChanged");

    esp = ring_emergency_service_new(emergency_service);
    tp_svc_channel_interface_service_point_emit_service_point_changed(
      (TpSvcChannelInterfaceServicePoint *)self, esp);
    ring_emergency_service_free(esp);
  }
}

/* ---------------------------------------------------------------------- */

/**
 * Interface Telepathy.Channel.Interface.Group (version 0.15)
 * using TpGroupMixin from telepathy-glib
 *
 * Methods:
 *   AddMembers ( au: contacts, s: message ) -> nothing
 *   GetAllMembers ( ) -> au, au, au
 *   GetGroupFlags ( ) -> u
 *   GetHandleOwners ( au: handles ) -> au
 *   GetLocalPendingMembers ( ) -> au
 *   GetLocalPendingMembersWithInfo ( ) -> a(uuus)
 *   GetMembers ( ) -> au
 *   GetRemotePendingMembers ( ) -> au
 *   GetSelfHandle ( ) -> u
 *   RemoveMembers ( au: contacts, s: message ) -> nothing
 * Signals:
 *    -> GroupFlagsChanged ( u: added, u: removed )
 *    -> MembersChanged ( s: message, au: added, au: removed,
 *                        au: local_pending, au: remote_pending,
 *                        u: actor, u: reason )
 *
 * TPGroupMixin interface:
 * ring_call_channel_add_member()
 * ring_call_channel_remove_member_with_reason()
 */


static gboolean
ring_call_channel_add_member(GObject *iface,
  TpHandle handle,
  const char *message,
  GError **error)
{
  TpGroupMixin *mixin = TP_GROUP_MIXIN(iface);

  DEBUG("enter");

  if (tp_handle_set_is_member(mixin->members, handle))
    return TRUE;
  else if (tp_handle_set_is_member(mixin->local_pending, handle))
    /* Incoming call */
    return ring_call_channel_accept_pending(iface, handle, message, error);
  else
    /* Outgoing call */
    return ring_call_channel_request_remote(iface, handle, message, error);
}


static gboolean
ring_call_channel_remove_member_with_reason(GObject *iface,
  TpHandle handle,
  const char *message,
  guint reason,
  GError **error)
{
  RingCallChannel *self = RING_CALL_CHANNEL(iface);
  RingCallChannelPrivate *priv = self->priv;
  TpGroupMixin *mixin = TP_GROUP_MIXIN(iface);
  TpIntSet *set;

  DEBUG("enter");

  if (handle != mixin->self_handle && handle != priv->peer_handle) {
    g_set_error(error, TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
      "handle %u cannot be removed", handle);
    return FALSE;
  }

  if (priv->release.message)    /* Already releasing */
    return FALSE;

  tp_group_mixin_change_flags(iface,
    0,
    TP_CHANNEL_GROUP_FLAG_CAN_REMOVE |
    TP_CHANNEL_GROUP_FLAG_CAN_RESCIND);

  if (self->base.call_instance) {
    priv->release.message = g_strdup(message ? message : "Call released");
    priv->release.actor = mixin->self_handle;
    priv->release.reason = reason;
    modem_call_request_release(self->base.call_instance, NULL, NULL);
  }
  else {
    /* Remove handle from set */
    set = tp_intset_new ();
    tp_intset_add(set, handle);
    tp_group_mixin_change_members(iface, message, NULL, set, NULL, NULL,
      mixin->self_handle, reason);
    tp_intset_destroy(set);
    ring_media_channel_close(RING_MEDIA_CHANNEL(self));
  }

  return TRUE;
}

/* Add handle to 'remote pending' set */
gboolean
ring_call_channel_request_remote(GObject *iface,
  TpHandle handle,
  const char *message,
  GError **error)
{
  RingCallChannel *self = RING_CALL_CHANNEL(iface);
  char const *destination;

  destination = ring_connection_inspect_contact(self->base.connection, handle);

  DEBUG("Trying to add %u=\"%s\" to remote pending", handle, destination);

  g_assert(handle != 0);

  if (!modem_call_is_valid_address(destination)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Invalid handle for media channel");
    return FALSE;
  }

  g_object_set(self, "peer", handle, NULL);

  if (handle == self->priv->peer_handle) {
    ring_call_channel_remote_pending(self, handle, message);
    return TRUE;
  }
  else {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Only one target allowed for media channel");
    return FALSE;
  }
}

gboolean
ring_call_channel_remote_pending(RingCallChannel *self,
  TpHandle handle,
  const char *message)
{
  RingCallChannelPrivate *priv = self->priv;
  GObject *object = G_OBJECT(self);
  TpGroupMixin *mixin = TP_GROUP_MIXIN(self);
  TpIntSet *remote_pending_set;
  gboolean done;

  g_assert(handle == priv->peer_handle);

  if (handle == 0)
    return FALSE;
  if (tp_handle_set_is_member(mixin->remote_pending, handle))
    return TRUE;

  remote_pending_set = tp_intset_new();
  tp_intset_add(remote_pending_set, handle);

  done = tp_group_mixin_change_members(object, message,
         NULL, NULL, NULL, remote_pending_set,
         mixin->self_handle,
         TP_CHANNEL_GROUP_CHANGE_REASON_INVITED);

  tp_intset_destroy(remote_pending_set);

  if (done) {
    tp_group_mixin_change_flags(object,
      TP_CHANNEL_GROUP_FLAG_CAN_REMOVE |
      TP_CHANNEL_GROUP_FLAG_CAN_RESCIND,
      TP_CHANNEL_GROUP_FLAG_CAN_ADD);
  }
  return done;
}

gboolean
ring_call_channel_accept_pending(GObject *iface,
  TpHandle handle,
  const char *message,
  GError **error)
{
  RingCallChannel *self = RING_CALL_CHANNEL(iface);
  guint state = 0;

  DEBUG("accepting an incoming call");

  if (self->base.call_instance == NULL) {
    g_set_error(error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
      "Missing call instance");
    return FALSE;
  }

  g_object_get(self->base.call_instance, "state", &state, NULL);

  if (state == MODEM_CALL_STATE_DISCONNECTED) {
    g_set_error(error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
      "Invalid call state");
    return FALSE;
  }

  if (!self->priv->accepted)
    self->priv->accepted = g_strdup(message ? message : "Call accepted");

    modem_call_request_answer(self->base.call_instance, NULL, NULL);

  return TRUE;
}

static void on_modem_call_state_incoming(RingCallChannel *self)
{
  RingCallChannelPrivate *priv = self->priv;
  if (!priv->terminating)
    g_object_set(self, "terminating", TRUE, NULL);
}

static void on_modem_call_state_dialing(RingCallChannel *self)
{
  RingCallChannelPrivate *priv = self->priv;
  if (!priv->originating)
    g_object_set(self, "originating", TRUE, NULL);
  if (!priv->closing)
    ring_call_channel_remote_pending(self, priv->peer_handle, "Call created");
}

static void on_modem_call_state_mo_alerting(RingCallChannel *self)
{
  RingCallChannelPrivate *priv = self->priv;

  ring_call_channel_remote_pending(self, priv->peer_handle, "Call alerting");

  ring_update_call_state(self,
    TP_CHANNEL_CALL_STATE_RINGING,
    TP_CHANNEL_CALL_STATE_QUEUED);
}

static void on_modem_call_state_mt_alerting(RingCallChannel *self)
{
  /* We can answer the call now */
  if (self->priv->accepted)
    modem_call_request_answer(self->base.call_instance, NULL, NULL);
}

static void on_modem_call_state_waiting(RingCallChannel *self)
{
  /* We can answer the call now */
  if (self->priv->accepted)
    modem_call_request_answer(self->base.call_instance, NULL, NULL);
}

static void on_modem_call_state_answered(RingCallChannel *self)
{
}

static void on_modem_call_state_active(RingCallChannel *self)
{
  RingCallChannelPrivate *priv = self->priv;
  TpGroupMixin *mixin = TP_GROUP_MIXIN(self);

  if (tp_handle_set_size(mixin->local_pending) ||
    tp_handle_set_size(mixin->remote_pending)) {
    char const *message = "Call answered";
    TpHandle who = 0;
    TpIntSet *add = tp_intset_new();

    if (tp_handle_set_size(mixin->local_pending))
      who = mixin->self_handle;
    else if (tp_handle_set_size(mixin->remote_pending))
      who = priv->peer_handle;

    if (self->priv->accepted)
      message = self->priv->accepted;

    tp_intset_add(add, mixin->self_handle);
    if (priv->peer_handle)
      tp_intset_add(add, priv->peer_handle);

    tp_group_mixin_change_members((GObject *)self,
      message,
      add, NULL, NULL, NULL,
      who, 0);
    tp_intset_destroy(add);

    /* Allow removal, deny rescind and adding  */
    tp_group_mixin_change_flags((GObject *)self,
      TP_CHANNEL_GROUP_FLAG_CAN_REMOVE,
      TP_CHANNEL_GROUP_FLAG_CAN_RESCIND |
      TP_CHANNEL_GROUP_FLAG_CAN_ADD);
  }

  ring_update_call_state(self, 0,
    TP_CHANNEL_CALL_STATE_RINGING |
    TP_CHANNEL_CALL_STATE_QUEUED);

  if (priv->dial2nd) {
    /* p equals 0b1100, 0xC, or "DTMF Control Digits Separator" in the 3GPP
       TS 11.11 section 10.5.1 "Extended BCD coding" table,

       According to 3GPP TS 02.07 appendix B.3.4, 'p', or DTMF Control
       Digits Separator is used "to distinguish between the addressing
       digits (i.e. the phone number) and the DTMF digits." According to the
       B.3.4, "upon the called party answering the ME shall send the DTMF
       digits automatically to the network after a delay of 3 seconds. Upon
       subsequent occurrences of the separator, the ME shall pause again for
       3 seconds (± 20 %) before sending any further DTMF digits."

       According to 3GPP TS 11.11 section 10.5.1 note 6, "A second or
       subsequent 'C'" will be interpreted as a 3 second pause.
    */

    if (!ring_media_channel_send_dialstring(RING_MEDIA_CHANNEL(self),
        1, priv->dial2nd, 0, 0, NULL)) {
      DEBUG("Ignoring dialstring \"%s\"", priv->dial2nd);
    }

    g_free(priv->dial2nd), priv->dial2nd = NULL;
  }
}

static void
on_modem_call_state_mo_release(RingCallChannel *self,
  guint causetype,
  guint cause)
{
  RingCallChannelPrivate *priv = self->priv;
  char const *message = priv->release.message;
  TpHandle actor = priv->release.actor;
  TpChannelGroupChangeReason reason = priv->release.reason;
  GError *error = NULL;
  char *debug;
  int details = 0;

  error = modem_call_new_error(causetype, cause, NULL);

  if (!actor) {
    if (/*MODEM_CALL_CAUSE_FROM_GSM(causetype)*/1) {
      /* Cancelled by modem for unknown reasons? */
      message = error->message;
      reason = ring_channel_group_release_reason(causetype, cause);
      details = causetype && cause &&
        reason != TP_CHANNEL_GROUP_CHANGE_REASON_BUSY &&
        reason != TP_CHANNEL_GROUP_CHANGE_REASON_NONE;
    }
    else {
      /* Call cancelled by MO but did not come from GSM; likely intentional */
      message = "";
      actor = TP_GROUP_MIXIN(self)->self_handle;
      reason = 0;
      details = 0;
    }
  }

  DEBUG("MO_RELEASE: message=\"%s\" reason=%s (%u) cause=%s.%s (%u.%u)",
    message, ring_util_reason_name(reason), reason,
    modem_error_domain_prefix(error->domain),
    modem_error_name(error, NULL, 0),
    causetype, cause);
  debug = g_strdup_printf("mo-release: reason=%s (%u) cause=%s.%s (%u.%u)",
          ring_util_reason_name(reason), reason,
          modem_error_domain_prefix(error->domain),
          modem_error_name(error, NULL, 0),
          causetype, cause);

  ring_call_channel_released(self, actor, reason, message,
    details ? error : NULL, debug);

  g_error_free(error);
  g_free(debug);
}

static void
on_modem_call_state_mt_release(RingCallChannel *self,
  guint causetype,
  guint cause)
{
  char const *message;
  TpHandle actor;
  TpChannelGroupChangeReason reason;
  GError *error;
  char *debug;
  int details;

  message = "Call released";
  actor = self->priv->peer_handle;

  reason = ring_channel_group_release_reason(causetype, cause);
  error = modem_call_new_error(causetype, cause, NULL);

  details = causetype && cause &&
    reason != TP_CHANNEL_GROUP_CHANGE_REASON_BUSY &&
    reason != TP_CHANNEL_GROUP_CHANGE_REASON_NONE;

  message = error->message;

  DEBUG("MT_RELEASE: message=\"%s\" reason=%s (%u) cause=%s.%s (%u.%u)",
    message, ring_util_reason_name(reason), reason,
    modem_error_domain_prefix(error->domain),
    modem_error_name(error, NULL, 0),
    causetype, cause);
  debug = g_strdup_printf("mt-release: reason=%s (%u) cause=%s.%s (%u.%u)",
          ring_util_reason_name(reason), reason,
          modem_error_domain_prefix(error->domain),
          modem_error_name(error, NULL, 0),
          causetype, cause);

  ring_call_channel_released(self, actor, reason, message,
    details ? error : NULL, debug);

  g_error_free(error);
  g_free(debug);
}

static void
on_modem_call_state_terminated(RingCallChannel *self,
  guint causetype,
  guint cause)
{
  RingCallChannelPrivate *priv = self->priv;
  char const *message;
  TpHandle actor;
  TpChannelGroupChangeReason reason;
  GError *error;
  char *debug;
  int details;

  if (priv->released)
    return;

  message = "Call terminated";
  actor = priv->peer_handle;

  reason = ring_channel_group_release_reason(causetype, cause);
  error = modem_call_new_error(causetype, cause, NULL);

  details = causetype && cause &&
    reason != TP_CHANNEL_GROUP_CHANGE_REASON_BUSY &&
    reason != TP_CHANNEL_GROUP_CHANGE_REASON_NONE;

  message = error->message;

  DEBUG("TERMINATED: message=\"%s\" reason=%s (%u) cause=%s.%s (%u.%u)",
    message, ring_util_reason_name(reason), reason,
    modem_error_domain_prefix(error->domain),
    modem_error_name(error, NULL, 0),
    causetype, cause);
  debug = g_strdup_printf("terminated: reason=%s (%u) cause=%s.%s (%u.%u)",
          ring_util_reason_name(reason), reason,
          modem_error_domain_prefix(error->domain),
          modem_error_name(error, NULL, 0),
          causetype, cause);

  ring_call_channel_released(self, actor, reason, message,
    details ? error : NULL, debug);

  g_error_free(error);
  g_free(debug);
}

static void
ring_call_channel_released(RingCallChannel *self,
  TpHandle actor,
  TpChannelGroupChangeReason reason,
  char const *message,
  GError *error,
  char const *debug)
{
  TpIntSet *removed;
  char *dbus_error = NULL;

  if (self->priv->released)
    return;
  self->priv->released = 1;

  ring_update_call_state(self, 0,
    TP_CHANNEL_CALL_STATE_RINGING |
    TP_CHANNEL_CALL_STATE_QUEUED |
    TP_CHANNEL_CALL_STATE_HELD);

  /* update flags accordingly -- deny adding/removal/rescind */
  tp_group_mixin_change_flags((GObject *)self, 0,
    TP_CHANNEL_GROUP_FLAG_CAN_ADD |
    TP_CHANNEL_GROUP_FLAG_CAN_REMOVE |
    TP_CHANNEL_GROUP_FLAG_CAN_RESCIND);

  if (error)
    dbus_error = modem_error_fqn(error);

  removed = tp_intset_new();
  tp_intset_add(removed, TP_GROUP_MIXIN(self)->self_handle);
  tp_intset_add(removed, self->priv->peer_handle);

  ring_util_group_change_members(self,
    NULL, removed, NULL, NULL,
    actor ? "actor" : "", G_TYPE_UINT, actor,
    reason ? "change-reason" : "", G_TYPE_UINT, reason,
    message ? "message" : "", G_TYPE_STRING, message,
    dbus_error ? "error" : "", G_TYPE_STRING, dbus_error,
    "debug-message", G_TYPE_STRING, debug,
    NULL);

  tp_intset_destroy(removed);

  if (self->priv->member.conference) {
    ring_conference_channel_emit_channel_removed(
      self->priv->member.conference, RING_MEMBER_CHANNEL(self),
      message, actor, reason);
  }

  g_free(dbus_error);
}

/* ---------------------------------------------------------------------- */
/* Conference member */

gboolean
ring_member_channel_is_in_conference(RingMemberChannel const *iface)
{
  return iface && RING_CALL_CHANNEL(iface)->priv->member.conference != NULL;
}


RingConferenceChannel *
ring_member_channel_get_conference(RingMemberChannel const *iface)
{
  if (!iface || !RING_IS_CALL_CHANNEL(iface))
    return FALSE;

  RingCallChannel const *self = RING_CALL_CHANNEL(iface);
  RingCallChannelPrivate const *priv = self->priv;

  return priv->member.conference;
}


gboolean
ring_member_channel_can_become_member(RingMemberChannel const *iface,
  GError **error)
{
  if (!iface || !RING_IS_CALL_CHANNEL(iface)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Member is not valid channel object");
    return FALSE;
  }

  RingCallChannel const *self = RING_CALL_CHANNEL(iface);
  RingCallChannelPrivate const *priv = self->priv;

  if (!priv->peer_handle) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Member channel has no target");
    return FALSE;
  }
  if (priv->member.conference) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Member channel is already in conference");
    return FALSE;
  }

  if (!self->base.call_instance) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Member channel has no ongoing call");
    return FALSE;
  }

  if (!modem_call_can_join(self->base.call_instance)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
      "Member channel in state %s",
      modem_call_get_state_name(
        modem_call_get_state(self->base.call_instance)));
    return FALSE;
  }

  return TRUE;
}

static guint
ring_call_channel_get_member_handle(RingCallChannel *self)
{
  RingCallChannelPrivate *priv = self->priv;
  TpHandle handle = priv->member.handle;
  TpHandle owner = priv->peer_handle;

  if (handle == 0) {
    TpHandleRepoIface *repo = tp_base_connection_get_handles(
      TP_BASE_CONNECTION((self->base.connection)), TP_HANDLE_TYPE_CONTACT);
    gpointer context = ring_network_normalization_context();
    char *object_path, *unique, *membername;

    g_object_get(self, "object-path", &object_path, NULL);
    unique = strrchr(object_path, '/');

    membername = g_strdup_printf(
      "%s/%s", tp_handle_inspect(repo, owner),
      unique + strcspn(unique, "0123456789"));

    handle = tp_handle_ensure(repo, membername, context, NULL);

    priv->member.handle = handle;

    g_free(object_path);
    g_free(membername);
  }

  return handle;
}


GHashTable *
ring_member_channel_get_handlemap(RingMemberChannel *iface)
{
  RingCallChannel *self = RING_CALL_CHANNEL(iface);
  RingCallChannelPrivate const *priv = self->priv;
  TpHandle handle = ring_call_channel_get_member_handle(self);
  TpHandle owner = priv->peer_handle;

  GHashTable *handlemap = g_hash_table_new(NULL, NULL);

  g_hash_table_replace(handlemap,
    GUINT_TO_POINTER(handle),
    GUINT_TO_POINTER(owner));

  return handlemap;
}

gboolean
ring_member_channel_release(RingMemberChannel *iface,
  const char *message,
  TpChannelGroupChangeReason reason,
  GError **error)
{
  RingCallChannel *self = RING_CALL_CHANNEL(iface);
  RingCallChannelPrivate *priv = self->priv;

  if (priv->release.message) {
    g_set_error(error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "already releasing");
    return FALSE;
  }

  if (self->base.call_instance == NULL) {
    g_set_error(error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "no call instance");
    return FALSE;
  }

  g_assert(message);

  priv->release.message = g_strdup(message ? message : "Call Released");
  priv->release.actor = TP_GROUP_MIXIN(iface)->self_handle;
  priv->release.reason = reason;

  modem_call_request_release(self->base.call_instance, NULL, NULL);

  return TRUE;
}

void
ring_member_channel_joined(RingMemberChannel *iface,
  RingConferenceChannel *conference)
{
  RingCallChannel *self = RING_CALL_CHANNEL(iface);
  RingCallChannelPrivate *priv = self->priv;

  if (priv->member.conference) {
    if (priv->member.conference == conference)
      return;
    ring_conference_channel_emit_channel_removed(
      priv->member.conference, iface,
      "Joined new conference",
      self->group.self_handle,
      TP_CHANNEL_GROUP_CHANGE_REASON_INVITED);
  }

  g_assert(priv->member.conference == NULL);

  priv->member.conference = g_object_ref(conference);

  DEBUG("%s joined conference %s",
    RING_MEDIA_CHANNEL(self)->nick,
    RING_MEDIA_CHANNEL(conference)->nick);
}

void
ring_member_channel_left(RingMemberChannel *iface)
{
  RingCallChannel *self = RING_CALL_CHANNEL(iface);
  RingCallChannelPrivate *priv = self->priv;

  if (priv->member.conference) {
    g_object_unref(priv->member.conference);
    priv->member.conference = NULL;

    DEBUG("Leaving conference");
  }
  else {
    DEBUG("got Left but not in conference");
  }
}

/* ---------------------------------------------------------------------- */
/* org.freedesktop.Telepathy.Channel.Interface.Splittable */

static ModemCallReply ring_call_channel_request_split_reply;

static void
ring_member_channel_method_split(
  RingSvcChannelInterfaceSplittable *iface,
  DBusGMethodInvocation *context)
{
  RingCallChannel *self = RING_CALL_CHANNEL(iface);
  GError *error = NULL;

  DEBUG("enter");

  if (ring_member_channel_is_in_conference(RING_MEMBER_CHANNEL(self))) {
#if 0
    RingConferenceChannel *conference = self->priv->member.conference;

    if (ring_conference_channel_has_members(conference) <= 1) {
      g_set_error(&error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
        "Last member cannot leave");

      /* ConferenceChannel gets now closed if there is only one member
       * channel */
      ring_svc_channel_interface_splittable_return_from_split
        (context);
      ring_conference_channel_emit_channel_removed(
        conference, (RingMemberChannel *)self,
        tp_base_connection_get_self_handle(
          TP_BASE_CONNECTION(self->base.connection)),
        TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

      /* If conference was on hold, unhold it */
      ModemCallConference *cc;

      cc = modem_call_service_get_conference(self->base.call_service);

      if (modem_call_is_hold(MODEM_CALL(cc))) {
        modem_call_request_hold(MODEM_CALL(cc), 0);
      }
      return;
    }
#endif

    ModemRequest *request;
    request = modem_call_request_split(self->base.call_instance,
              ring_call_channel_request_split_reply,
              context);
    modem_request_add_data_full(request,
      "tp-request",
      context,
      ring_method_return_internal_error);
    return;
  }

  g_set_error(&error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
    "Not a member channel");
  dbus_g_method_return_error(context, error);
  g_error_free(error);
}

static void
ring_call_channel_request_split_reply(ModemCall *instance,
  ModemRequest *request,
  GError *error,
  gpointer _context)
{
  DBusGMethodInvocation *context;

  context = modem_request_steal_data(request, "tp-request");

  g_assert(context);
  g_assert(context == _context);

  if (error) {
    DEBUG("split failed: " GERROR_MSG_FMT, GERROR_MSG_CODE(error));

    GError tperror[1] = {{
        TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
        "Cannot LeaveConference"
      }};

    dbus_g_method_return_error(context, tperror);
  }
  else {
    DEBUG("enter");
    ring_svc_channel_interface_splittable_return_from_split
      (context);
  }
}

static void
ring_channel_splittable_iface_init(gpointer g_iface, gpointer iface_data)
{
  RingSvcChannelInterfaceSplittableClass *klass = g_iface;

#define IMPLEMENT(x)                                    \
  ring_svc_channel_interface_splittable_implement_##x   \
    (klass, ring_member_channel_method_ ## x)
  IMPLEMENT(split);
#undef IMPLEMENT
}

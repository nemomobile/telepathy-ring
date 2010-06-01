/*
 * ring-conference-channel.c - Source for RingConferenceChannel
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

#include "ring-conference-channel.h"
#include "ring-member-channel.h"
#include "ring-util.h"
#include "ring-extensions/gtypes.h"

#include "modem/call.h"
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

#include <rtcom-telepathy-glib/extensions.h>
#include <ring-extensions/ring-extensions.h>

#include "ring-connection.h"

#include "ring-param-spec.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MODEM_MAX_CALLS (7)

struct _RingConferenceChannelPrivate
{
  ModemCallConference *cc;

  RingInitialMembers *initial_members;

  RingMemberChannel *members[MODEM_MAX_CALLS];
  int is_current[MODEM_MAX_CALLS];

  TpIntSet *current, *pending;

  struct {
    gulong left, joined;
  } signals;

  unsigned streams_created:1, closing:1, closed:1, disposed:1, :0;
};

/* properties */
enum
{
  PROP_NONE,
  /* telepathy-glib properties */
  PROP_CHANNEL_PROPERTIES,

  /* o.f.T.Channel.Interfaces */
  PROP_INTERFACES,
  PROP_INITIAL_CHANNELS,
  PROP_CHANNELS,
  PROP_SUPPORTS_NON_MERGES,
  LAST_PROPERTY
};

static void ring_conference_channel_implement_media_channel(
  RingMediaChannelClass *media_class);

static TpDBusPropertiesMixinIfaceImpl
ring_conference_channel_dbus_property_interfaces[];

static void ring_channel_mergeable_conference_iface_init(gpointer, gpointer);

static gboolean ring_conference_channel_add_member(
  GObject *obj, TpHandle handle, const char *message, GError **error);
static gboolean ring_conference_channel_remove_member_with_reason(
  GObject *obj, TpHandle handle, const char *message,
  guint reason,
  GError **error);

static GPtrArray *ring_conference_get_channels(
  RingConferenceChannel const *self);

static GHashTable *ring_conference_channel_properties(RingConferenceChannel *self);

static void ring_conference_channel_emit_channel_merged(
  RingConferenceChannel *channel,
  RingMemberChannel *member,
  gboolean current);

static void ring_conference_channel_release(RingConferenceChannel *self,
  unsigned causetype,
  unsigned cause,
  GError const *error);

/* ====================================================================== */
/* GObject interface */

G_DEFINE_TYPE_WITH_CODE(
  RingConferenceChannel, ring_conference_channel, RING_TYPE_MEDIA_CHANNEL,
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_DBUS_PROPERTIES,
    tp_dbus_properties_mixin_iface_init)
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_GROUP,
    tp_group_mixin_iface_init)
  G_IMPLEMENT_INTERFACE(RING_TYPE_SVC_CHANNEL_INTERFACE_CONFERENCE,
    NULL)
  G_IMPLEMENT_INTERFACE(RING_TYPE_SVC_CHANNEL_INTERFACE_MERGEABLE_CONFERENCE,
    ring_channel_mergeable_conference_iface_init));

const char *ring_conference_channel_interfaces[] = {
  RING_MEDIA_CHANNEL_INTERFACES,
  TP_IFACE_CHANNEL_INTERFACE_GROUP,
  RING_IFACE_CHANNEL_INTERFACE_CONFERENCE,
  RING_IFACE_CHANNEL_INTERFACE_MERGEABLE_CONFERENCE,
  NULL
};

static void
modem_call_service_join_reply(ModemCallService *service,
  ModemRequest *request,
  GError *error,
  gpointer _self)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingMemberChannel *member;
  char const *member_path;
  DBusGMethodInvocation *context;
  GError *clearerror = NULL;

  ring_media_channel_dequeue_request(RING_MEDIA_CHANNEL(self), request);

  context = modem_request_steal_data(request, "tp-request");

  if (!error) {
    member_path = modem_request_get_data(request, "member-object-path");
    member = ring_connection_lookup_channel(self->base.connection,
             member_path);
    if (ring_conference_channel_join(self, member, &error)) {
      ring_svc_channel_interface_mergeable_conference_return_from_merge
        (context);
      DEBUG("ok");
      return;
    }
    clearerror = error;
  }

  DEBUG("return error \"%s\"", error->message);
  dbus_g_method_return_error(context, error);
  g_clear_error(&clearerror);
}

static void
ring_mergeable_conference_merge(RingSvcChannelInterfaceMergeableConference *iface,
  const char *channel_path,
  DBusGMethodInvocation *context)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(iface);
  RingMemberChannel *member;
  GError *error = NULL;

  DEBUG("enter");

  member = ring_connection_lookup_channel(self->base.connection,
           channel_path);

  if (!member || !RING_IS_MEMBER_CHANNEL(member)) {
    error = g_error_new(TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
            "Invalid channel path");
  }
  else if (!ring_member_channel_can_become_member(member, &error)) {
    ;
  }
  else {
    ModemRequest *request;

    request = modem_call_request_conference(
      self->base.call_service, modem_call_service_join_reply, self);

    ring_media_channel_queue_request(RING_MEDIA_CHANNEL(self), request);

    modem_request_add_data_full(request,
      "member-object-path",
      g_strdup(channel_path),
      g_free);
    modem_request_add_data_full(request,
      "tp-request",
      context,
      ring_method_return_internal_error);
    return;
  }
  DEBUG("return error \"%s\"", error->message);
  dbus_g_method_return_error(context, error);
  g_clear_error(&error);
}


static void
ring_channel_mergeable_conference_iface_init(gpointer g_iface,
  gpointer iface_data)
{
  RingSvcChannelInterfaceMergeableConferenceClass *klass =
    (RingSvcChannelInterfaceMergeableConferenceClass *)g_iface;

#define IMPLEMENT(x)                                            \
  ring_svc_channel_interface_mergeable_conference_implement_##x \
    (klass, ring_mergeable_conference_##x)

  IMPLEMENT(merge);
#undef IMPLEMENT
}

static void
ring_conference_channel_init(RingConferenceChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, RING_TYPE_CONFERENCE_CHANNEL, RingConferenceChannelPrivate);
}

static void
ring_conference_channel_constructed(GObject *object)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(object);
  TpBaseConnection *connection = TP_BASE_CONNECTION(self->base.connection);

  if (G_OBJECT_CLASS(ring_conference_channel_parent_class)->constructed)
    G_OBJECT_CLASS(ring_conference_channel_parent_class)->constructed(object);

  tp_group_mixin_init(
    object,
    G_STRUCT_OFFSET(RingConferenceChannel, group),
    tp_base_connection_get_handles(connection, TP_HANDLE_TYPE_CONTACT),
    connection->self_handle);
}

static void
ring_conference_channel_emit_initial(RingMediaChannel *_self)
{
  DEBUG("enter");

  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = self->priv;
  RingConnection *connection = self->base.connection;
  TpGroupMixin *group = TP_GROUP_MIXIN(self);
  char const *message;
  TpChannelGroupChangeReason reason;
  TpChannelGroupFlags add = 0, del = 0;

  if (priv->cc) {
    message = "Conference created";
    reason = TP_CHANNEL_GROUP_CHANGE_REASON_INVITED;
  }
  else {
    message = "Channel created";
    reason = TP_CHANNEL_GROUP_CHANGE_REASON_NONE;
  }

  priv->current = tp_intset_new();
  tp_intset_add(priv->current, group->self_handle);
  priv->pending = tp_intset_new();

  if (priv->cc) {
    char const *member_path;
    RingMemberChannel *member;
    int i;

    for (i = 0; i < priv->initial_members->len; i++) {
      member_path = priv->initial_members->odata[i];
      member = ring_connection_lookup_channel(connection, member_path);
      if (member) {
        ring_conference_channel_emit_channel_merged(
          self, RING_MEMBER_CHANNEL(member),
          modem_call_is_member(RING_MEDIA_CHANNEL(member)->call_instance));
      }
      else {
        DEBUG("No member channel %s found\n", member_path);
      }
    }
  }

  tp_group_mixin_change_members((GObject *)self, message,
    priv->current, NULL, NULL, priv->pending,
    group->self_handle, reason);
  tp_intset_destroy(priv->current); priv->current = NULL;
  tp_intset_destroy(priv->pending); priv->pending = NULL;

  add |= TP_CHANNEL_GROUP_FLAG_CAN_REMOVE;
  if (tp_handle_set_size(group->remote_pending))
    add |= TP_CHANNEL_GROUP_FLAG_CAN_RESCIND;
  add |= TP_CHANNEL_GROUP_FLAG_PROPERTIES;
  add |= TP_CHANNEL_GROUP_FLAG_CHANNEL_SPECIFIC_HANDLES;
  add |= TP_CHANNEL_GROUP_FLAG_MEMBERS_CHANGED_DETAILED;

  tp_group_mixin_change_flags((GObject *)self, add, del);

  if (priv->cc)
    ring_media_channel_set_state(RING_MEDIA_CHANNEL(self),
      modem_call_get_state(self->base.call_instance), 0, 0);
}

static void
ring_conference_channel_get_property(GObject *obj,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(obj);
  RingConferenceChannelPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CHANNEL_PROPERTIES:
      g_value_take_boxed(value, ring_conference_channel_properties(self));
      break;
    case PROP_INTERFACES:
      g_value_set_boxed(value, ring_conference_channel_interfaces);
      break;
    case PROP_INITIAL_CHANNELS:
      g_value_set_boxed(value, priv->initial_members);
      break;
    case PROP_CHANNELS:
      g_value_take_boxed(value, ring_conference_get_channels(self));
      break;
    case PROP_SUPPORTS_NON_MERGES:
      g_value_set_boolean (value, FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_conference_channel_set_property(GObject *obj,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(obj);
  RingConferenceChannelPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_INITIAL_CHANNELS:
      priv->initial_members = g_value_dup_boxed(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_conference_channel_dispose(GObject *obj)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(obj);

  if (self->priv->disposed)
    return;
  self->priv->disposed = TRUE;

  guint i;

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (self->priv->members[i]) {
      g_object_unref(self->priv->members[i]), self->priv->members[i] = NULL;
    }
  }

  ((GObjectClass *)ring_conference_channel_parent_class)->dispose(obj);
}


static void
ring_conference_channel_finalize(GObject *obj)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(obj);
  RingConferenceChannelPrivate *priv = self->priv;

  tp_group_mixin_finalize(obj);

  g_boxed_free(TP_ARRAY_TYPE_OBJECT_PATH_LIST, priv->initial_members);
  priv->initial_members = NULL;

  G_OBJECT_CLASS(ring_conference_channel_parent_class)->finalize(obj);

  DEBUG("exit");
}

/* ====================================================================== */
/* GObjectClass */

static void
ring_conference_channel_class_init(RingConferenceChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  g_type_class_add_private(klass, sizeof (RingConferenceChannelPrivate));

  object_class->constructed = ring_conference_channel_constructed;
  object_class->get_property = ring_conference_channel_get_property;
  object_class->set_property = ring_conference_channel_set_property;
  object_class->dispose = ring_conference_channel_dispose;
  object_class->finalize = ring_conference_channel_finalize;

  ring_conference_channel_implement_media_channel(
    RING_MEDIA_CHANNEL_CLASS(klass));

  klass->dbus_properties_class.interfaces =
    ring_conference_channel_dbus_property_interfaces;
  tp_dbus_properties_mixin_class_init(
    object_class,
    G_STRUCT_OFFSET(RingConferenceChannelClass, dbus_properties_class));

  tp_group_mixin_class_init(
    object_class,
    G_STRUCT_OFFSET(RingConferenceChannelClass, group_class),
    ring_conference_channel_add_member,
    NULL);
  tp_group_mixin_class_set_remove_with_reason_func(
    object_class, ring_conference_channel_remove_member_with_reason);
  tp_group_mixin_init_dbus_properties(object_class);

  g_object_class_override_property(
    object_class, PROP_CHANNEL_PROPERTIES, "channel-properties");

  g_object_class_override_property(
    object_class, PROP_INTERFACES, "interfaces");

  g_object_class_install_property(
    object_class, PROP_INITIAL_CHANNELS,
    g_param_spec_boxed("initial-channels",
      "Initial member channels",
      "The two initial member channels used "
      "when the conference channel is constructed.",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_CHANNELS,
    g_param_spec_boxed("channels",
      "Current member channels",
      "The current member channels participating in "
      "the conference.",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_SUPPORTS_NON_MERGES,
    g_param_spec_boolean("supports-non-merges",
      "SupportsNonMerges",
      "Whether this channel supports non-merges",
      FALSE,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));
}

/* ====================================================================== */
/**
 * org.freedesktop.DBus properties
 */

/* Properties for com.nokia.Telepathy.Channel.Interface.Conference */
static TpDBusPropertiesMixinPropImpl conference_properties[] = {
  { "InitialChannels", "initial-channels" },
  { "Channels", "channels" },
  { "SupportsNonMerges", "supports-non-merges" },
  { NULL }
};

static TpDBusPropertiesMixinIfaceImpl
ring_conference_channel_dbus_property_interfaces[] = {
  {
    RING_IFACE_CHANNEL_INTERFACE_CONFERENCE,
    tp_dbus_properties_mixin_getter_gobject_properties,
    NULL,
    conference_properties,
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
ring_conference_channel_properties(RingConferenceChannel *self)
{
  return ring_channel_add_properties(
    self, ring_media_channel_properties(RING_MEDIA_CHANNEL(self)),
    RING_IFACE_CHANNEL_INTERFACE_CONFERENCE, "InitialChannels",
    NULL);
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
 * ring_conference_channel_add_member()
 * ring_conference_channel_remove_member_with_reason()
 */

static gboolean
ring_conference_channel_add_member(GObject *iface,
  TpHandle handle,
  const char *message,
  GError **error)
{
  TpGroupMixin *mixin = TP_GROUP_MIXIN(iface);

  DEBUG("enter");

  if (tp_handle_set_is_member(mixin->members, handle))
    return TRUE;
  else
    return FALSE;
}


static gboolean
ring_conference_channel_remove_member_with_reason(GObject *iface,
  TpHandle handle,
  const char *message,
  guint reason,
  GError **error)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(iface);
  RingConferenceChannelPrivate *priv = self->priv;
  RingMemberChannel *member;
  TpGroupMixin *mixin = TP_GROUP_MIXIN(iface);
  TpIntSet *removing;
  TpHandle selfhandle, memberhandle;
  int i;

  DEBUG("enter");

  selfhandle = tp_base_connection_get_self_handle(
    TP_BASE_CONNECTION(self->base.connection));

  removing = tp_intset_new();

  if (handle == selfhandle)
    tp_intset_add(removing, handle);

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    member = priv->members[i];
    if (!member)
      continue;

    g_object_get(member, "member-handle", &memberhandle, NULL);

    if (handle == selfhandle) {
      tp_intset_add(removing, memberhandle);
      ring_member_channel_release(member, message, reason, NULL);
    }
    else if (handle == memberhandle) {
      tp_intset_add(removing, memberhandle);
      if (!ring_member_channel_release(member, message, reason, error))
        goto error;
      break;
    }
  }

  if (tp_intset_size(removing) == 0) {
    g_set_error(error, TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
      "handle %u cannot be removed", handle);
    goto error;
  }

  /* Remove handles from member set */
  tp_group_mixin_change_members(iface, message,
    NULL, removing, NULL, NULL,
    selfhandle, reason);
  tp_intset_destroy(removing), removing = NULL;

  guint del = 0;

  if (tp_handle_set_size(mixin->members) < 1)
    del |= TP_CHANNEL_GROUP_FLAG_CAN_REMOVE;
  if (tp_handle_set_size(mixin->remote_pending) < 1)
    del |= TP_CHANNEL_GROUP_FLAG_CAN_RESCIND;

  if (del)
    tp_group_mixin_change_flags(iface, 0, del);

  return TRUE;

error:
  tp_intset_destroy(removing);
  return FALSE;
}


/* ---------------------------------------------------------------------- */
/* com.nokia.Telepathy.Channel.Interface.Conference */

guint
ring_conference_channel_has_members(RingConferenceChannel const *self)
{
  int i, n = 0;

  if (self) {
    for (i = 0; i < MODEM_MAX_CALLS; i++) {
      if (self->priv->members[i] != NULL)
        n++;
    }
  }

  return n;
}

static void
ring_conference_channel_emit_channel_merged(RingConferenceChannel *self,
  RingMemberChannel *member,
  gboolean current)
{
  RingConferenceChannelPrivate *priv = self->priv;
  int i;
  char *member_object_path;
  GHashTable *member_map;
  TpHandle member_handle;

  g_object_get(member,
    "object-path", &member_object_path,
    "member-map", &member_map,
    "member-handle", &member_handle,
    NULL);

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (member == priv->members[i]) {
      priv->is_current[i] = current;
      goto emit_members_changed;
    }
  }

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (priv->members[i] == NULL)
      break;
  }

  g_assert(i < MODEM_MAX_CALLS);
  if (i >= MODEM_MAX_CALLS)
    goto error;

  priv->members[i] = g_object_ref(member);
  priv->is_current[i] = current;

  DEBUG("emitting MemberChannelAdded(%s)",
    strrchr(member_object_path, '/') + 1);

  /* XXX: This used to take member_map, which could be useful */
  ring_svc_channel_interface_conference_emit_channel_merged(
    self, member_object_path);

emit_members_changed:
  DEBUG("%s member handle %u for %s",
    current ? "current" : "pending",
    member_handle, strrchr(member_object_path, '/') + 1);

  if (current)
    ring_member_channel_joined(member, self);

  gpointer owner, owned;
  GHashTableIter j[1];

  for (g_hash_table_iter_init(j, member_map);
       g_hash_table_iter_next(j, &owned, &owner);) {
    tp_group_mixin_add_handle_owner((GObject *)self,
      GPOINTER_TO_UINT(owned),
      GPOINTER_TO_UINT(owner));
  }

  if (!priv->pending) {
    TpGroupMixin *mixin = TP_GROUP_MIXIN(self);
    TpIntSet *members = tp_intset_new();

    tp_intset_add(members, member_handle);
    tp_group_mixin_change_members((GObject *)self,
      "New conference members",
      current ? members : NULL, NULL,
      NULL, current ? NULL : members,
      current ? 0 : mixin->self_handle,
      TP_CHANNEL_GROUP_CHANGE_REASON_INVITED);
    tp_intset_destroy(members);

    TpChannelGroupFlags add = 0, del = 0;
    /* Allow removal and rescind */
    add = TP_CHANNEL_GROUP_FLAG_CAN_REMOVE;
    if (tp_handle_set_size(mixin->remote_pending) == 0)
      /* Deny rescind */
      del = TP_CHANNEL_GROUP_FLAG_CAN_RESCIND;
    else if (!current)
      add |= TP_CHANNEL_GROUP_FLAG_CAN_RESCIND;
    tp_group_mixin_change_flags((GObject *)self, add, del);
  }
  else {
    /* Report all handles at once */
    if (current)
      tp_intset_add(priv->current, member_handle);
    else
      tp_intset_add(priv->pending, member_handle);
  }

error:
  g_hash_table_destroy(member_map);
  g_free(member_object_path);
}


void
ring_conference_channel_emit_channel_removed(
  RingConferenceChannel *self,
  RingMemberChannel *member,
  char const *message,
  TpHandle actor,
  TpChannelGroupChangeReason reason)
{
  RingConferenceChannelPrivate *priv = self->priv;
  guint i, n;
  char *object_path;
  TpHandle member_handle;

  if (!member)
    return;

  if (priv->closing)
    return;

  n = ring_conference_channel_has_members(self);

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (member == priv->members[i]) {
      priv->members[i] = NULL;
      priv->is_current[i] = 0;
      break;
    }
  }

  if (i >= MODEM_MAX_CALLS)
    return;

  while (member) {
    TpIntSet *remove = tp_intset_new();

    g_object_get(member,
      "object-path", &object_path,
      "member-handle", &member_handle,
      NULL);

    tp_intset_add(remove, member_handle);

    DEBUG("emitting MemberChannelRemoved(%s, %u, %u)",
      strrchr(object_path, '/') + 1,
      actor, reason);

    /* XXX: this used to take actor and reason which could be
       useful */
    ring_svc_channel_interface_conference_emit_channel_removed(
      self, object_path);

    g_free(object_path);

    ring_member_channel_left(member);

    g_object_unref((GObject *)member), member = NULL;

    tp_group_mixin_change_members((GObject *)self,
      message,
      NULL, remove, NULL, NULL,
      actor, reason);
    tp_intset_destroy(remove);

    if (n > 2)
      break;

    /* The conference channel goes away */
    message = "Deactivating conference" ;
    actor = self->group.self_handle;
    reason = TP_CHANNEL_GROUP_CHANGE_REASON_SEPARATED;

    for (i = 0; i < MODEM_MAX_CALLS; i++) {
      if (priv->members[i]) {
        member = priv->members[i];
        priv->members[i] = NULL;
        priv->is_current[i] = 0;
        break;
      }
    }
  }

  if (n > 2)
    return;

  DEBUG("Too few members, close channel %p", self);

  /* Last member channel removed, close channel */
  ring_media_channel_close(RING_MEDIA_CHANNEL(self));
}


static GPtrArray *
ring_conference_get_channels(RingConferenceChannel const *self)
{
  RingConferenceChannelPrivate const *priv = self->priv;
  GPtrArray *list;
  int i;
  char *object_path;

  list = g_ptr_array_sized_new(MODEM_MAX_CALLS);

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (priv->members[i] && priv->is_current[i]) {
      g_object_get(priv->members[i], "object-path", &object_path, NULL);
      g_ptr_array_add(list, object_path);
    }
  }

  return list;
}

gboolean
ring_conference_channel_check_initial_members(RingConferenceChannel const *channel,
  RingInitialMembers const *maybe)
{
  if (channel == NULL || maybe == NULL)
    return FALSE;

  if (!RING_IS_CONFERENCE_CHANNEL(channel))
    return FALSE;

  RingInitialMembers const *initial = channel->priv->initial_members;

  if (initial->len != maybe->len)
    return FALSE;

  guint i, j;

  for (i = 0; i < maybe->len; i++) {
    for (j = 0; j < initial->len; j++) {
      if (strcmp(initial->odata[j], maybe->odata[i]) == 0)
        break;
    }
    if (j == initial->len)
      return FALSE;
  }

  return TRUE;
}

gboolean
ring_conference_channel_join(RingConferenceChannel *self,
  RingMemberChannel *member,
  GError **error)
{
  if (!RING_IS_CONFERENCE_CHANNEL(self)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Not a Conference Channel");
    return FALSE;
  }
#if 0
  if (!ring_member_channel_can_become_member(member, error)) {
    return FALSE;
  }
#endif
  ring_conference_channel_emit_channel_merged(self, member, FALSE);

  return TRUE;
}

static void
on_modem_conference_joined(ModemCallConference *cc,
  ModemCall *ci,
  RingConferenceChannel *self)
{
  RingMemberChannel *member =
    (RingMemberChannel *)modem_call_get_handler(ci);

  DEBUG("called with ModemCall(%p) and RingMemberChannel(%p)", ci, member);

  if (RING_IS_MEMBER_CHANNEL(member))
    ring_conference_channel_emit_channel_merged(
      RING_CONFERENCE_CHANNEL(self), member, TRUE);
}

void
on_modem_conference_left(ModemCallConference *cc,
  ModemCall *ci,
  RingConferenceChannel *self)
{
  RingMemberChannel *member = modem_call_get_handler(ci);

  DEBUG("called with ModemCall(%p) and RingMemberChannel(%p)", ci, member);

  if (RING_IS_MEMBER_CHANNEL(member)) {
    TpHandle actor = 0;
    TpChannelGroupChangeReason reason = 0;
    char const *message;

    if (modem_call_get_state(ci) == MODEM_CALL_STATE_ACTIVE) {
      message = "Split to private call";
      actor = self->group.self_handle;
      reason = TP_CHANNEL_GROUP_CHANGE_REASON_INVITED;
    }
    else {
      message = "Member channel removed";
      actor = self->group.self_handle;
      reason = TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;
      DEBUG("got Left but member channel is not split (call in %s)",
        modem_call_get_state_name(modem_call_get_state(ci)));
    }

    ring_conference_channel_emit_channel_removed(
      RING_CONFERENCE_CHANNEL(self), member,
      message, actor, reason);
  }
}

/* ====================================================================== */
/* RingMediaChannel interface */

/** Close channel */
static gboolean
ring_conference_channel_close(RingMediaChannel *_self,
  gboolean immediately)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = RING_CONFERENCE_CHANNEL(self)->priv;
  int i;

  priv->closing = TRUE;

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    RingMemberChannel *member = priv->members[i];
    if (member) {
      ring_member_channel_left(member);
      g_object_unref(member);
    }
    priv->members[i] = NULL;
  }

  return priv->closed = TRUE;
}

static void
ring_conference_channel_update_state(RingMediaChannel *_self,
  unsigned state,
  unsigned causetype,
  unsigned cause)
{
  if (state != MODEM_CALL_STATE_DISCONNECTED) /* was: TERMINATED */
    return;

  /* Emit MemberChanged when request to build conference fails */
  if (tp_handle_set_size(TP_GROUP_MIXIN(_self)->remote_pending) != 0)
    ring_conference_channel_release(RING_CONFERENCE_CHANNEL(_self),
      causetype, cause, NULL);

  ring_media_channel_close(_self);
}

static void
ring_conference_channel_release(RingConferenceChannel *self,
  unsigned causetype,
  unsigned cause,
  GError const *error0)
{
  RingConferenceChannelPrivate *priv = self->priv;
  char const *message;
  TpIntSet *removed;
  TpChannelGroupChangeReason reason;
  GError *error = NULL;
  char *debug = NULL;
  char *dbus_error = NULL;
  int i, details;

  if (error0) {
    reason = TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;
    error = (GError *)error0;
  }
  else {
    reason = ring_channel_group_release_reason(causetype, cause);
    error = modem_call_new_error(causetype, cause, NULL);
  }
  message = error->message;
  dbus_error = modem_error_fqn(error);

  details = causetype && cause &&
    reason != TP_CHANNEL_GROUP_CHANGE_REASON_BUSY &&
    reason != TP_CHANNEL_GROUP_CHANGE_REASON_NONE;

  DEBUG("TERMINATED: message=\"%s\" reason=%s (%u) cause=%s.%s (%u.%u)",
    message, ring_util_reason_name(reason), reason,
    modem_error_domain_prefix(error->domain),
    modem_error_name(error, NULL, 0),
    causetype, cause);
  debug = g_strdup_printf("conference terminated: "
          "reason=%s (%u) cause=%s.%s (%u.%u)",
          ring_util_reason_name(reason), reason,
          modem_error_domain_prefix(error->domain),
          modem_error_name(error, NULL, 0),
          causetype, cause);

  /* update flags accordingly -- deny adding/removal/rescind */
  tp_group_mixin_change_flags((GObject *)self, 0,
    TP_CHANNEL_GROUP_FLAG_CAN_ADD |
    TP_CHANNEL_GROUP_FLAG_CAN_REMOVE |
    TP_CHANNEL_GROUP_FLAG_CAN_RESCIND);

  removed = tp_intset_new();
  tp_intset_add(removed, TP_GROUP_MIXIN(self)->self_handle);

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    RingMemberChannel *member = priv->members[i];
    if (member) {
      uint member_handle = 0;
      g_object_get(member, "member-handle", &member_handle, NULL);
      tp_intset_add(removed, member_handle);
    }
  }

  ring_util_group_change_members(self,
    NULL, removed, NULL, NULL,
    reason ? "change-reason" : "", G_TYPE_UINT, reason,
    message ? "message" : "", G_TYPE_STRING, message,
    dbus_error ? "error" : "", G_TYPE_STRING, dbus_error,
    "debug-message", G_TYPE_STRING, debug,
    NULL);

  tp_intset_destroy(removed);

  g_free(dbus_error);
  if (error0 == NULL)
    g_error_free(error);
  g_free(debug);
}

static void
ring_conference_channel_set_call_instance(RingMediaChannel *_self,
  ModemCall *ci)
{
  DEBUG("(%p, %p): enter", _self, ci);

  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = self->priv;

  if (ci) {
    g_assert(MODEM_IS_CALL_CONFERENCE(ci));

    priv->cc = MODEM_CALL_CONFERENCE(ci);

    priv->streams_created = 1;

#define CONNECT(n, f)                                                   \
    g_signal_connect(priv->cc, n, G_CALLBACK(on_modem_conference_ ## f), self)

    priv->signals.joined = CONNECT("joined", joined);
    priv->signals.left = CONNECT("left", left);
#undef CONNECT
  }
  else {
#define DISCONNECT(n)                                                   \
    if (priv->signals.n &&                                              \
      g_signal_handler_is_connected(priv->cc, priv->signals.n)) {       \
      g_signal_handler_disconnect(priv->cc, priv->signals.n);           \
    } (priv->signals.n = 0)

    DISCONNECT(joined);
    DISCONNECT(left);

#undef DISCONNECT

    priv->cc = NULL;
  }
}

static gboolean
ring_conference_channel_validate_media_handle(RingMediaChannel *_self,
  guint *handlep,
  GError **error)
{
  if (*handlep != 0) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Conference channel carries a multiparty stream only");
    return FALSE;
  }

  return TRUE;
}

static void reply_to_modem_call_request_conference(ModemCallService *_service,
  ModemRequest *request, GError *error, gpointer _channel);

static gboolean
ring_conference_channel_create_streams(RingMediaChannel *_self,
  guint handle,
  gboolean audio,
  gboolean video,
  GError **error)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = self->priv;

  (void)audio; (void)video;

  if (priv->streams_created) {
    DEBUG("Already associated with a conference");
    return TRUE;
  }
  priv->streams_created = 1;

  if (!ring_connection_validate_initial_members(self->base.connection,
      priv->initial_members,
      error))
    return FALSE;

  ring_media_channel_queue_request(RING_MEDIA_CHANNEL(self),
    modem_call_request_conference(self->base.call_service,
      reply_to_modem_call_request_conference, self));

  return TRUE;
}

void
ring_conference_channel_initial_audio(RingConferenceChannel *self,
  RingMediaManager *manager,
  gpointer channelrequest)
{
  RingConferenceChannelPrivate *priv = self->priv;
  ModemRequest *request;

  DEBUG("%s(%p, %p, %p) called", __func__, self, manager, channelrequest);

  priv->streams_created = 1;

  request = modem_call_request_conference(self->base.call_service,
            reply_to_modem_call_request_conference,
            self);

  ring_media_channel_queue_request(RING_MEDIA_CHANNEL(self), request);

  modem_request_add_qdatas(
    request,
    g_type_qname(RING_TYPE_MEDIA_MANAGER), g_object_ref(manager), g_object_unref,
    /* XXX - GDestroyNotify for channelrequest  */
    g_quark_from_static_string("RingChannelRequest"), channelrequest, NULL,
    NULL);
}

static void
reply_to_modem_call_request_conference(ModemCallService *_service,
  ModemRequest *request,
  GError *error,
  gpointer _self)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = self->priv;
  gpointer channelrequest;
  ModemCallConference *cc = priv->cc;
  GError error0[1] = {{
      TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "Conference channel already exists"
    }};

  ring_media_channel_dequeue_request(RING_MEDIA_CHANNEL(self), request);

  if (error)
    DEBUG("Call.CreateMultiparty with channel %s (%p) failed: " GERROR_MSG_FMT,
      self->base.nick, self, GERROR_MSG_CODE(error));
  else
    DEBUG("Call.CreateMultiparty with channel %s (%p) returned",
      self->base.nick, self);

  if (error == NULL && cc == NULL) {
    RingConferenceChannel *already;

    cc = modem_call_service_get_conference(self->base.call_service);
    already = modem_call_get_handler(MODEM_CALL(cc));
    if (already == NULL) {
      g_object_set(self, "call-instance", cc, NULL);
    }
    else {
      DEBUG("RingConferenceChannel already exists (%p)", already);
      cc = NULL;
      error = error0;
    }
  }

  channelrequest = modem_request_steal_data(request, "RingChannelRequest");
  if (channelrequest) {
    RingMediaManager *manager = modem_request_get_data(request, "RingMediaManager");

    ring_media_manager_emit_new_channel(manager,
      channelrequest, self, error ? error0 : NULL);
  }
  else if (error) {
    ring_conference_channel_release(_self, 0, 0, error);
    ring_media_channel_close(_self);
  }
  else {
    ring_conference_channel_emit_initial(_self);
  }
}

static void
ring_conference_channel_implement_media_channel(RingMediaChannelClass *media_class)
{
  media_class->emit_initial = ring_conference_channel_emit_initial;
  media_class->close = ring_conference_channel_close;
  media_class->update_state = ring_conference_channel_update_state;
  media_class->set_call_instance = ring_conference_channel_set_call_instance;
  media_class->validate_media_handle = ring_conference_channel_validate_media_handle;
  media_class->create_streams = ring_conference_channel_create_streams;
}

/* ---------------------------------------------------------------------- */

GType ring_object_path_list_get_type(void)
{
  static GType type;

  if (G_UNLIKELY(!type))
    type = dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH);

  return type;
}

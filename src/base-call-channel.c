/*
 * base-call-channel.c - Source for GabbleBaseCallChannel
 * Copyright © 2009–2010 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <stdlib.h>

#include <gio/gio.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/exportable-channel.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-properties-interface.h>
#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/gtypes.h>

#include "base-call-channel.h"
#include "base-call-content.h"

#include "ring-connection.h"

#define DEBUG_FLAG RING_DEBUG_MEDIA
#include "ring-debug.h"

static void call_iface_init (gpointer, gpointer);
static void gabble_base_call_channel_close (TpBaseChannel *base);

G_DEFINE_TYPE_WITH_CODE(GabbleBaseCallChannel, gabble_base_call_channel,
  TP_TYPE_BASE_CHANNEL,
  G_IMPLEMENT_INTERFACE (TP_TYPE_BASE_MEDIA_CALL_CHANNEL,
        call_iface_init)
);

static const gchar *gabble_base_call_channel_interfaces[] = {
    NULL
};

/* properties */
enum
{
  PROP_OBJECT_PATH_PREFIX = 1,

  PROP_INITIAL_AUDIO,
  PROP_INITIAL_VIDEO,
  PROP_MUTABLE_CONTENTS,
  PROP_HARDWARE_STREAMING,
  PROP_CONTENTS,

  PROP_CALL_STATE,
  PROP_CALL_FLAGS,
  PROP_CALL_STATE_DETAILS,
  PROP_CALL_STATE_REASON,

  PROP_CALL_MEMBERS,

  LAST_PROPERTY
};

enum
{
  ENDED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };


/* private structure */
struct _GabbleBaseCallChannelPrivate
{
  gchar *object_path_prefix;

  gboolean dispose_has_run;

  GList *contents;

  TpCallState state;
  TpCallFlags flags;
  GHashTable *details;
  GValueArray *reason;

  /* handle -> TpCallMemberFlags hash */
  GHashTable *members;
};

static void
gabble_base_call_channel_constructed (GObject *obj)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (obj);
  TpBaseChannel *base = TP_BASE_CHANNEL (self);

  if (G_OBJECT_CLASS (gabble_base_call_channel_parent_class)->constructed
      != NULL)
    G_OBJECT_CLASS (gabble_base_call_channel_parent_class)->constructed (obj);

  if (tp_base_channel_is_requested (base))
    gabble_base_call_channel_set_state (self,
      TP_CALL_STATE_PENDING_INITIATOR);
  else
    gabble_base_call_channel_set_state (self,
      TP_CALL_STATE_INITIALISING);
}

static void
gabble_base_call_channel_init (GabbleBaseCallChannel *self)
{
  GabbleBaseCallChannelPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GABBLE_TYPE_BASE_CALL_CHANNEL, GabbleBaseCallChannelPrivate);

  self->priv = priv;

  priv->reason = tp_value_array_build (3,
    G_TYPE_UINT, 0,
    G_TYPE_UINT, 0,
    G_TYPE_STRING, "",
    G_TYPE_INVALID);

  priv->details = tp_asv_new (NULL, NULL);

  priv->members = g_hash_table_new (NULL, NULL);
}

static void gabble_base_call_channel_dispose (GObject *object);
static void gabble_base_call_channel_finalize (GObject *object);

static void
gabble_base_call_channel_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (object);
  GabbleBaseCallChannelPrivate *priv = self->priv;
  GabbleBaseCallChannelClass *klass = GABBLE_BASE_CALL_CHANNEL_GET_CLASS (self);

  switch (property_id)
    {
      case PROP_OBJECT_PATH_PREFIX:
        g_value_set_string (value, priv->object_path_prefix);
        break;
      case PROP_INITIAL_AUDIO:
        g_value_set_boolean (value, self->initial_audio);
        break;
      case PROP_INITIAL_VIDEO:
        g_value_set_boolean (value, self->initial_video);
        break;
      case PROP_MUTABLE_CONTENTS:
        g_value_set_boolean (value, klass->mutable_contents);
        break;
      case PROP_CONTENTS:
        {
          GPtrArray *arr = g_ptr_array_sized_new (2);
          GList *l;

          for (l = priv->contents; l != NULL; l = g_list_next (l))
            {
              GabbleBaseCallContent *c = GABBLE_BASE_CALL_CONTENT (l->data);
              g_ptr_array_add (arr,
                (gpointer) gabble_base_call_content_get_object_path (c));
            }

          g_value_set_boxed (value, arr);
          g_ptr_array_free (arr, TRUE);
          break;
        }
      case PROP_HARDWARE_STREAMING:
        g_value_set_boolean (value, klass->hardware_streaming);
        break;
      case PROP_CALL_STATE:
        g_value_set_uint (value, priv->state);
        break;
      case PROP_CALL_FLAGS:
        g_value_set_uint (value, priv->flags);
        break;
      case PROP_CALL_STATE_DETAILS:
        g_value_set_boxed (value, priv->details);
        break;
      case PROP_CALL_STATE_REASON:
        g_value_set_boxed (value, priv->reason);
        break;
      case PROP_CALL_MEMBERS:
        g_value_set_boxed (value, priv->members);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gabble_base_call_channel_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (object);
  GabbleBaseCallChannelPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_OBJECT_PATH_PREFIX:
        priv->object_path_prefix = g_value_dup_string (value);
        break;
      case PROP_INITIAL_AUDIO:
        self->initial_audio = g_value_get_boolean (value);
        break;
      case PROP_INITIAL_VIDEO:
        self->initial_video = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static gchar *
gabble_base_call_channel_get_object_path_suffix (TpBaseChannel *base)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (base);
  GabbleBaseCallChannelPrivate *priv = self->priv;

  g_assert (priv->object_path_prefix != NULL);

  return g_strdup_printf ("%s/CallChannel%p", priv->object_path_prefix, self);
}

static void
gabble_base_call_channel_fill_immutable_properties (
    TpBaseChannel *chan,
    GHashTable *properties)
{
  TP_BASE_CHANNEL_CLASS (gabble_base_call_channel_parent_class)
      ->fill_immutable_properties (chan, properties);

  tp_dbus_properties_mixin_fill_properties_hash (
      G_OBJECT (chan), properties,
      TP_IFACE_CHANNEL_TYPE_CALL, "InitialAudio",
      TP_IFACE_CHANNEL_TYPE_CALL, "InitialVideo",
      TP_IFACE_CHANNEL_TYPE_CALL, "MutableContents",
      TP_IFACE_CHANNEL_TYPE_CALL, "HardwareStreaming",
      NULL);
}

static void
gabble_base_call_channel_class_init (
    GabbleBaseCallChannelClass *gabble_base_call_channel_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (gabble_base_call_channel_class);
  TpBaseChannelClass *base_channel_class =
      TP_BASE_CHANNEL_CLASS (gabble_base_call_channel_class);
  GParamSpec *param_spec;
  static TpDBusPropertiesMixinPropImpl call_props[] = {
      { "CallMembers", "call-members", NULL },
      { "MutableContents", "mutable-contents", NULL },
      { "InitialAudio", "initial-audio", NULL },
      { "InitialVideo", "initial-video", NULL },
      { "Contents", "contents", NULL },
      { "HardwareStreaming", "hardware-streaming", NULL },
      { "CallState", "call-state", NULL },
      { "CallFlags", "call-flags", NULL },
      { "CallStateReason",  "call-state-reason", NULL },
      { "CallStateDetails", "call-state-details", NULL },
      { NULL }
  };

  g_type_class_add_private (gabble_base_call_channel_class,
      sizeof (GabbleBaseCallChannelPrivate));

  object_class->constructed = gabble_base_call_channel_constructed;

  object_class->get_property = gabble_base_call_channel_get_property;
  object_class->set_property = gabble_base_call_channel_set_property;

  object_class->dispose = gabble_base_call_channel_dispose;
  object_class->finalize = gabble_base_call_channel_finalize;

  base_channel_class->channel_type = TP_IFACE_CHANNEL_TYPE_CALL;
  base_channel_class->interfaces = gabble_base_call_channel_interfaces;
  base_channel_class->get_object_path_suffix =
      gabble_base_call_channel_get_object_path_suffix;
  base_channel_class->fill_immutable_properties =
      gabble_base_call_channel_fill_immutable_properties;
  base_channel_class->close = gabble_base_call_channel_close;

  signals[ENDED] = g_signal_new ("ended",
      G_OBJECT_CLASS_TYPE (object_class),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  param_spec = g_param_spec_string ("object-path-prefix", "Object path prefix",
      "prefix of the object path",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH_PREFIX,
      param_spec);

  param_spec = g_param_spec_boolean ("initial-audio", "InitialAudio",
      "Whether the channel initially contained an audio stream",
      FALSE,
      G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIAL_AUDIO,
      param_spec);

  param_spec = g_param_spec_boolean ("initial-video", "InitialVideo",
      "Whether the channel initially contained an video stream",
      FALSE,
      G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIAL_VIDEO,
      param_spec);

  param_spec = g_param_spec_boolean ("mutable-contents", "MutableContents",
      "Whether the set of streams on this channel are mutable once requested",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_MUTABLE_CONTENTS,
      param_spec);

  param_spec = g_param_spec_boxed ("contents", "Contents",
      "The contents of the channel",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTENTS,
      param_spec);

  param_spec = g_param_spec_boolean ("hardware-streaming", "HardwareStreaming",
      "True if all the streaming is done by hardware",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_HARDWARE_STREAMING,
      param_spec);

  param_spec = g_param_spec_uint ("call-state", "CallState",
      "The status of the call",
      TP_CALL_STATE_UNKNOWN,
      NUM_TP_CALL_STATES,
      TP_CALL_STATE_UNKNOWN,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CALL_STATE, param_spec);

  param_spec = g_param_spec_uint ("call-flags", "CallFlags",
      "Flags representing the status of the call",
      0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CALL_FLAGS,
      param_spec);

  param_spec = g_param_spec_boxed ("call-state-reason", "CallStateReason",
      "The reason why the call is in the current state",
      TP_STRUCT_TYPE_CALL_STATE_REASON,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CALL_STATE_REASON,
      param_spec);

  param_spec = g_param_spec_boxed ("call-state-details", "CallStateDetails",
      "The reason why the call is in the current state",
      TP_HASH_TYPE_QUALIFIED_PROPERTY_VALUE_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CALL_STATE_DETAILS,
      param_spec);

  param_spec = g_param_spec_boxed ("call-members", "CallMembers",
      "The members",
      TP_HASH_TYPE_CALL_MEMBER_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CALL_MEMBERS,
      param_spec);

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CHANNEL_TYPE_CALL,
      tp_dbus_properties_mixin_getter_gobject_properties,
      NULL,
      call_props);
}

void
gabble_base_call_channel_dispose (GObject *object)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (object);
  GabbleBaseCallChannelPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  self->priv->dispose_has_run = TRUE;

  g_list_foreach (priv->contents, (GFunc) gabble_base_call_content_deinit, NULL);
  g_list_foreach (priv->contents, (GFunc) g_object_unref, NULL);
  tp_clear_pointer (&priv->contents, g_list_free);

  tp_clear_pointer (&priv->members, g_hash_table_unref);

  if (G_OBJECT_CLASS (gabble_base_call_channel_parent_class)->dispose)
    G_OBJECT_CLASS (gabble_base_call_channel_parent_class)->dispose (object);
}

void
gabble_base_call_channel_finalize (GObject *object)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (object);
  GabbleBaseCallChannelPrivate *priv = self->priv;

  g_hash_table_unref (priv->details);
  g_value_array_free (priv->reason);
  g_free (self->priv->object_path_prefix);

  G_OBJECT_CLASS (gabble_base_call_channel_parent_class)->finalize (object);
}

void
gabble_base_call_channel_set_state (GabbleBaseCallChannel *self,
  TpCallState state)
{
  GabbleBaseCallChannelPrivate *priv = self->priv;

  DEBUG ("changing from %u to %u", priv->state, state);

  if (state == priv->state)
    return;

  /* signal when going to the ended state */
  if (state == TP_CALL_STATE_ENDED)
    g_signal_emit (self, signals[ENDED], 0);

  priv->state = state;

  if (priv->state != TP_CALL_STATE_INITIALISING)
    priv->flags &= ~TP_CALL_FLAG_LOCALLY_RINGING;

  if (tp_base_channel_is_registered (TP_BASE_CHANNEL (self)))
    tp_svc_channel_type_call_emit_call_state_changed (self, priv->state,
      priv->flags, priv->reason, priv->details);
}

TpCallState
gabble_base_call_channel_get_state (GabbleBaseCallChannel *self)
{
  return self->priv->state;
}

void
gabble_base_call_channel_update_flags (GabbleBaseCallChannel *self,
    TpCallFlags set_flags,
    TpCallFlags clear_flags)
{
  GabbleBaseCallChannelPrivate *priv = self->priv;
  TpCallFlags old_flags = priv->flags;

  priv->flags = (old_flags | set_flags) & ~clear_flags;
  DEBUG ("was %u; set %u and cleared %u; now %u", old_flags, set_flags,
      clear_flags, priv->flags);

  if (priv->flags != old_flags)
    tp_svc_channel_type_call_emit_call_state_changed (self, priv->state,
      priv->flags, priv->reason, priv->details);
}

static gboolean
base_call_channel_update_member (GabbleBaseCallChannel *self,
    TpHandle contact,
    TpCallMemberFlags set_flags,
    TpCallMemberFlags clear_flags,
    TpCallMemberFlags *new_flags)
{
  gpointer old_flags_p;

  DEBUG ("updating member #%u, setting %u and clearing %u", contact, set_flags,
      clear_flags);

  if (g_hash_table_lookup_extended (self->priv->members,
          GUINT_TO_POINTER (contact), NULL, &old_flags_p))
    {
      TpCallMemberFlags old_flags = GPOINTER_TO_UINT (old_flags_p);

      *new_flags = (old_flags | set_flags) & ~clear_flags;

      DEBUG ("previous flags: %u; new flags: %u", old_flags, *new_flags);

      if (old_flags == *new_flags)
        return FALSE;
    }
  else
    {
      *new_flags = set_flags & ~clear_flags;
      DEBUG ("not previously a member; new flags: %u", *new_flags);
    }

  g_hash_table_insert (self->priv->members, GUINT_TO_POINTER (contact),
      GUINT_TO_POINTER (*new_flags));
  return TRUE;
}

gboolean
gabble_base_call_channel_update_members (
    GabbleBaseCallChannel *self,
    TpHandle contact,
    TpCallMemberFlags set_flags,
    TpCallMemberFlags clear_flags,
    ...)
{
  GHashTable *updates = g_hash_table_new (NULL, NULL);
  gboolean updated = FALSE;
  va_list args;

  va_start (args, clear_flags);

  do
    {
      TpCallMemberFlags new_flags;

      if (base_call_channel_update_member (self, contact, set_flags,
              clear_flags, &new_flags))
        {
          g_hash_table_insert (updates,
              GUINT_TO_POINTER (contact),
              GUINT_TO_POINTER (new_flags));
          updated = TRUE;
        }

      contact = va_arg (args, TpHandle);

      if (contact != 0)
        {
          set_flags = va_arg (args, TpCallMemberFlags);
          clear_flags = va_arg (args, TpCallMemberFlags);
        }
    }
  while (contact != 0);

  if (updated)
    {
      GArray *empty = g_array_new (FALSE, FALSE, sizeof (TpHandle));

      tp_svc_channel_type_call_emit_call_members_changed (self, updates, NULL,
          empty, empty);
      g_array_unref (empty);
    }

  g_hash_table_unref (updates);
  return updated;
}

gboolean
gabble_base_call_channel_remove_members (
    GabbleBaseCallChannel *self,
    TpHandle contact,
    ...)
{
  GArray *removed = g_array_new (FALSE, FALSE, sizeof (TpHandle));
  gboolean updated = FALSE;
  va_list args;

  va_start (args, contact);

  do
    {
      if (g_hash_table_remove (self->priv->members, GUINT_TO_POINTER (contact)))
        {
          g_array_append_val (removed, contact);
          updated = TRUE;
        }

      contact = va_arg (args, TpHandle);
    }
  while (contact != 0);

  if (updated)
    {
      GHashTable *empty = g_hash_table_new (NULL, NULL);

      tp_svc_channel_type_call_emit_call_members_changed (self, empty,
          removed);
      g_hash_table_unref (empty);
    }

  g_array_unref (removed);
  return updated;
}

void
gabble_base_call_channel_add_content (GabbleBaseCallChannel *self,
    GabbleBaseCallContent *content)
{
  self->priv->contents = g_list_prepend (self->priv->contents,
      g_object_ref (content));

  if (tp_base_channel_is_registered (TP_BASE_CHANNEL (self)))
    tp_svc_channel_type_call_emit_content_added (self,
        gabble_base_call_content_get_object_path (content),
        gabble_base_call_content_get_media_type (content));
}

static void
gabble_base_call_channel_close (TpBaseChannel *base)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (base);
  GabbleBaseCallChannelPrivate *priv = self->priv;

  DEBUG ("Closing media channel %s", tp_base_channel_get_object_path (base));

  /* shutdown all our contents */
  g_list_foreach (priv->contents, (GFunc) gabble_base_call_content_deinit,
      NULL);
  g_list_foreach (priv->contents, (GFunc) g_object_unref, NULL);
  tp_clear_pointer (&priv->contents, g_list_free);

  tp_base_channel_destroyed (base);
}

static void
gabble_base_call_channel_ringing (TpSvcChannelTypeCall *iface,
    DBusGMethodInvocation *context)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (iface);
  GabbleBaseCallChannelPrivate *priv = self->priv;
  TpBaseChannel *tp_base = TP_BASE_CHANNEL (self);

  if (tp_base_channel_is_requested (tp_base))
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Call was requested. Ringing doesn't make sense." };
      dbus_g_method_return_error (context, &e);
    }
  else if (priv->state != TP_CALL_STATE_INITIALISING)
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Call is not in the right state for Ringing." };
      dbus_g_method_return_error (context, &e);
    }
  else
    {
      if ((priv->flags & TP_CALL_FLAG_LOCALLY_RINGING) == 0)
        {
          DEBUG ("Client is ringing");
          priv->flags |= TP_CALL_FLAG_LOCALLY_RINGING;
          gabble_base_call_channel_set_state (self, priv->state);
        }

      tp_svc_channel_type_call_return_from_ringing (context);
    }
}

static void
gabble_base_call_channel_accept (TpSvcChannelTypeCall *iface,
        DBusGMethodInvocation *context)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (iface);
  GabbleBaseCallChannelPrivate *priv = self->priv;
  GabbleBaseCallChannelClass *base_class =
      GABBLE_BASE_CALL_CHANNEL_GET_CLASS (self);
  TpBaseChannel *tp_base = TP_BASE_CHANNEL (self);

  DEBUG ("Client accepted the call");

  if (tp_base_channel_is_requested (tp_base))
    {
      if (priv->state == TP_CALL_STATE_PENDING_INITIATOR)
        {
          gabble_base_call_channel_set_state (self,
              TP_CALL_STATE_INITIALISING);
        }
      else
        {
          DEBUG ("Invalid state for Accept: Channel requested and "
              "state == %d", priv->state);
          goto err;
        }
    }
  else if (priv->state < TP_CALL_STATE_ACCEPTED)
    {
      gabble_base_call_channel_set_state (self,
        TP_CALL_STATE_ACCEPTED);
    }
  else
    {
      DEBUG ("Invalid state for Accept: state == %d", priv->state);
      goto err;
    }

  if (base_class->accept != NULL)
    base_class->accept (self);

  tp_svc_channel_type_call_return_from_accept (context);
  return;

err:
  {
    GError e = { TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
        "Invalid state for Accept" };
    dbus_g_method_return_error (context, &e);
  }
}

static void
gabble_base_call_channel_hangup (TpSvcChannelTypeCall *iface,
  guint reason,
  const gchar *detailed_reason,
  const gchar *message,
  DBusGMethodInvocation *context)
{
  GabbleBaseCallChannel *self = GABBLE_BASE_CALL_CHANNEL (iface);
  GabbleBaseCallChannelClass *base_class =
      GABBLE_BASE_CALL_CHANNEL_GET_CLASS (self);

  if (base_class->hangup)
    base_class->hangup (self, reason, detailed_reason, message);

  gabble_base_call_channel_set_state ( GABBLE_BASE_CALL_CHANNEL (self),
          TP_CALL_STATE_ENDED);

  tp_svc_channel_type_call_return_from_hangup (context);
}

static void
gabble_base_call_channel_add_content_dbus (RingSvcChannelTypeCall *iface,
  const gchar *name,
  TpMediaStreamType mtype,
  DBusGMethodInvocation *context)
{
  GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "Contents cannot be added; only a single audio content is supported" };

  dbus_g_method_return_error (context, &e);
}


static void
call_iface_init (gpointer g_iface, gpointer iface_data)
{
  RingSvcChannelTypeCallClass *klass = g_iface;

#define IMPLEMENT(x, suffix) ring_svc_channel_type_call_implement_##x (\
    klass, gabble_base_call_channel_##x##suffix)
  IMPLEMENT(ringing,);
  IMPLEMENT(accept,);
  IMPLEMENT(hangup,);
  IMPLEMENT(add_content, _dbus);
#undef IMPLEMENT
}

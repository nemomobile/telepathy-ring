/*
 * base-call-channel.c - Source for RingBaseCallChannel
 * Copyright © 2009–2010 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 * @author Tom Swindell <t.swindell@rubyx.co.uk>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <gio/gio.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#include "util.h"
#include "ring-call-content.h"
#include "ring-call-member.h"

#include "base-call-channel.h"
#include "ring-connection.h"

/*
#define DEBUG_FLAG RING_DEBUG_MEDIA
#include "debug.h"
*/

G_DEFINE_TYPE(RingBaseCallChannel, ring_base_call_channel,
  TP_TYPE_BASE_MEDIA_CALL_CHANNEL);

static void ring_base_call_channel_hangup (
    TpBaseCallChannel *base,
    guint reason,
    const gchar *detailed_reason,
    const gchar *message);

static void ring_base_call_channel_close (TpBaseChannel *base);

/* properties */
enum
{
  PROP_OBJECT_PATH_PREFIX = 1,
  LAST_PROPERTY
};

/* private structure */
struct _RingBaseCallChannelPrivate
{
  gchar *object_path_prefix;

  gboolean dispose_has_run;

  /* handle -> CallMember object hash */
  GHashTable *members;
};

static void
ring_base_call_channel_init (RingBaseCallChannel *self)
{
  RingBaseCallChannelPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      RING_TYPE_BASE_CALL_CHANNEL, RingBaseCallChannelPrivate);

  self->priv = priv;

  priv->members = g_hash_table_new_full (g_direct_hash, g_direct_equal,
    NULL, g_object_unref);
}

static void ring_base_call_channel_dispose (GObject *object);
static void ring_base_call_channel_finalize (GObject *object);

static void
ring_base_call_channel_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  RingBaseCallChannel *self = RING_BASE_CALL_CHANNEL (object);
  RingBaseCallChannelPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_OBJECT_PATH_PREFIX:
        g_value_set_string (value, priv->object_path_prefix);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
ring_base_call_channel_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  RingBaseCallChannel *self = RING_BASE_CALL_CHANNEL (object);
  RingBaseCallChannelPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_OBJECT_PATH_PREFIX:
        priv->object_path_prefix = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static gchar *
ring_base_call_channel_get_object_path_suffix (TpBaseChannel *base)
{
  RingBaseCallChannel *self = RING_BASE_CALL_CHANNEL (base);
  RingBaseCallChannelPrivate *priv = self->priv;

  g_assert (priv->object_path_prefix != NULL);

  return g_strdup_printf ("%s/CallChannel%p", priv->object_path_prefix, self);
}

static void
ring_base_call_channel_class_init (
    RingBaseCallChannelClass *ring_base_call_channel_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (ring_base_call_channel_class);
  TpBaseChannelClass *base_channel_class =
      TP_BASE_CHANNEL_CLASS (ring_base_call_channel_class);
  TpBaseCallChannelClass *tp_base_call_channel_class =
      TP_BASE_CALL_CHANNEL_CLASS (ring_base_call_channel_class);
  GParamSpec *param_spec;

  g_type_class_add_private (ring_base_call_channel_class,
      sizeof (RingBaseCallChannelPrivate));

  object_class->get_property = ring_base_call_channel_get_property;
  object_class->set_property = ring_base_call_channel_set_property;

  object_class->dispose = ring_base_call_channel_dispose;
  object_class->finalize = ring_base_call_channel_finalize;

  base_channel_class->get_object_path_suffix =
      ring_base_call_channel_get_object_path_suffix;
  base_channel_class->close = ring_base_call_channel_close;

  tp_base_call_channel_class->hangup = ring_base_call_channel_hangup;

  param_spec = g_param_spec_string ("object-path-prefix", "Object path prefix",
      "prefix of the object path",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH_PREFIX,
      param_spec);
}

void
ring_base_call_channel_dispose (GObject *object)
{
  RingBaseCallChannel *self = RING_BASE_CALL_CHANNEL (object);
  RingBaseCallChannelPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  self->priv->dispose_has_run = TRUE;

  tp_clear_pointer (&priv->members, g_hash_table_unref);

  if (G_OBJECT_CLASS (ring_base_call_channel_parent_class)->dispose)
    G_OBJECT_CLASS (ring_base_call_channel_parent_class)->dispose (object);
}

void
ring_base_call_channel_finalize (GObject *object)
{
  RingBaseCallChannel *self = RING_BASE_CALL_CHANNEL (object);
  RingBaseCallChannelPrivate *priv = self->priv;

  g_free (priv->object_path_prefix);

  G_OBJECT_CLASS (ring_base_call_channel_parent_class)->finalize (object);
}

RingCallContent *
ring_base_call_channel_add_content (RingBaseCallChannel *self,
    const gchar *name,
    TpCallContentDisposition disposition)
{
  TpBaseChannel *base = TP_BASE_CHANNEL (self);
  gchar *object_path;
  TpBaseCallContent *content;
  gchar *escaped;

  /* FIXME could clash when other party in a one-to-one call creates a stream
   * with the same media type and name */
  escaped = tp_escape_as_identifier (name);
  object_path = g_strdup_printf ("%s/Content_%s",
      tp_base_channel_get_object_path (base),
      escaped);
  g_free (escaped);

  content = g_object_new (RING_TYPE_CALL_CONTENT,
    "connection", tp_base_channel_get_connection (base),
    "object-path", object_path,
    "disposition", disposition,
    //"media-type", wocky_jingle_media_type_to_tp (mtype),
    "name", name,
    NULL);

  g_free (object_path);

  tp_base_call_channel_add_content (TP_BASE_CALL_CHANNEL (self),
    content);

  return RING_CALL_CONTENT (content);
}

static void
call_member_flags_changed_cb (RingCallMember *member,
  TpCallMemberFlags flags,
  gpointer user_data)
{
  TpBaseCallChannel *base = TP_BASE_CALL_CHANNEL (user_data);

  tp_base_call_channel_update_member_flags (base,
    ring_call_member_get_handle (member),
    flags,
    0, TP_CALL_STATE_CHANGE_REASON_PROGRESS_MADE, "", "");
}

RingCallMember *
ring_base_call_channel_get_member_from_handle (
    RingBaseCallChannel *self,
    TpHandle handle)
{
  return g_hash_table_lookup (self->priv->members, GUINT_TO_POINTER (handle));
}

RingCallMember *
ring_base_call_channel_ensure_member_from_handle (
    RingBaseCallChannel *self,
    TpHandle handle)
{
  RingBaseCallChannelPrivate *priv = self->priv;
  RingCallMember *m;

  m = g_hash_table_lookup (priv->members, GUINT_TO_POINTER (handle));
  if (m == NULL)
    {
      m = RING_CALL_MEMBER (g_object_new (RING_TYPE_CALL_MEMBER,
        "target", handle,
        "call", self,
        NULL));
      g_hash_table_insert (priv->members, GUINT_TO_POINTER (handle), m);

      tp_base_call_channel_update_member_flags (TP_BASE_CALL_CHANNEL (self),
        ring_call_member_get_handle (m),
        ring_call_member_get_flags (m),
        0, TP_CALL_STATE_CHANGE_REASON_PROGRESS_MADE, "", "");

      ring_signal_connect_weak (m, "flags-changed",
        G_CALLBACK (call_member_flags_changed_cb), G_OBJECT (self));
    }

  return m;
}

void
ring_base_call_channel_remove_member (RingBaseCallChannel *self,
    RingCallMember *member)
{
  TpHandle h = ring_call_member_get_handle (member);

  g_assert (g_hash_table_lookup (self->priv->members,
    GUINT_TO_POINTER (h))== member);

  ring_call_member_shutdown (member);
  tp_base_call_channel_remove_member (TP_BASE_CALL_CHANNEL (self),
    ring_call_member_get_handle (member),
    0, TP_CALL_STATE_CHANGE_REASON_PROGRESS_MADE, "", "");
  g_hash_table_remove (self->priv->members, GUINT_TO_POINTER (h));
}

static void
ring_base_call_channel_shutdown_all_members (RingBaseCallChannel *self)
{
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, self->priv->members);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    ring_call_member_shutdown (RING_CALL_MEMBER (value));
}

static void
ring_base_call_channel_hangup (TpBaseCallChannel *base,
    guint reason,
    const gchar *detailed_reason,
    const gchar *message)
{
  ring_base_call_channel_shutdown_all_members (
    RING_BASE_CALL_CHANNEL (base));
}

static void
ring_base_call_channel_close (TpBaseChannel *base)
{
  ring_base_call_channel_shutdown_all_members (
    RING_BASE_CALL_CHANNEL (base));

  TP_BASE_CHANNEL_CLASS (ring_base_call_channel_parent_class)->close (base);
}

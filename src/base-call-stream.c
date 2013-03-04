/*
 * base-call-stream.c - Source for GabbleBaseCallStream
 * Copyright © 2009–2010 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 * @author Will Thompson <will.thompson@collabora.co.uk>
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

#include "base-call-stream.h"

#define DEBUG_FLAG RING_DEBUG_MEDIA
#include "ring-debug.h"

#include "ring-connection.h"

G_DEFINE_TYPE_WITH_CODE(GabbleBaseCallStream, gabble_base_call_stream,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
        tp_dbus_properties_mixin_iface_init);
    /* The base class doesn't implement SetSending or RequestReceiving, because
     * they're pretty protocol-specific. It just implements the properties. */
    G_IMPLEMENT_INTERFACE (RING_TYPE_SVC_CALL_STREAM, NULL);
    )

enum
{
  PROP_OBJECT_PATH = 1,
  PROP_CONNECTION,

  /* Call interface properties */
  PROP_INTERFACES,
  PROP_SENDERS,
};

struct _GabbleBaseCallStreamPrivate
{
  gboolean dispose_has_run;

  gchar *object_path;
  RingConnection *conn;

  GHashTable *senders;
};

static void
gabble_base_call_stream_init (GabbleBaseCallStream *self)
{
  GabbleBaseCallStreamPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GABBLE_TYPE_BASE_CALL_STREAM, GabbleBaseCallStreamPrivate);

  self->priv = priv;
  priv->senders = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
gabble_base_call_stream_constructed (GObject *obj)
{
  GabbleBaseCallStream *self = GABBLE_BASE_CALL_STREAM (obj);
  GabbleBaseCallStreamPrivate *priv = self->priv;
  TpDBusDaemon *bus = tp_base_connection_get_dbus_daemon (
      (TpBaseConnection *) priv->conn);

  if (G_OBJECT_CLASS (gabble_base_call_stream_parent_class)->constructed
      != NULL)
    G_OBJECT_CLASS (gabble_base_call_stream_parent_class)->constructed (obj);

  DEBUG ("Registering %s", priv->object_path);
  tp_dbus_daemon_register_object (bus, priv->object_path, obj);
}

static void
gabble_base_call_stream_dispose (GObject *object)
{
  GabbleBaseCallStream *self = GABBLE_BASE_CALL_STREAM (object);
  GabbleBaseCallStreamPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  tp_clear_object (&priv->conn);

  if (G_OBJECT_CLASS (gabble_base_call_stream_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (gabble_base_call_stream_parent_class)->dispose (object);
}

static void
gabble_base_call_stream_finalize (GObject *object)
{
  GabbleBaseCallStream *self = GABBLE_BASE_CALL_STREAM (object);
  GabbleBaseCallStreamPrivate *priv = self->priv;

  /* free any data held directly by the object here */
  g_free (priv->object_path);
  g_hash_table_destroy (priv->senders);

  if (G_OBJECT_CLASS (gabble_base_call_stream_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (gabble_base_call_stream_parent_class)->finalize (object);
}

static void
gabble_base_call_stream_get_property (
    GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  GabbleBaseCallStream *self = GABBLE_BASE_CALL_STREAM (object);
  GabbleBaseCallStreamPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_value_set_object (value, priv->conn);
        break;
      case PROP_OBJECT_PATH:
        g_value_set_string (value, priv->object_path);
        break;
      case PROP_INTERFACES:
        {
          GabbleBaseCallStreamClass *klass =
              GABBLE_BASE_CALL_STREAM_GET_CLASS (self);

          if (klass->extra_interfaces != NULL)
            {
              g_value_set_boxed (value, klass->extra_interfaces);
            }
          else
            {
              gchar *empty[] = { NULL };

              g_value_set_boxed (value, empty);
            }
          break;
        }
      case PROP_SENDERS:
        g_value_set_boxed (value, priv->senders);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gabble_base_call_stream_set_property (
    GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  GabbleBaseCallStream *self = GABBLE_BASE_CALL_STREAM (object);
  GabbleBaseCallStreamPrivate *priv = self->priv;

  switch (property_id)
    {
      case PROP_CONNECTION:
        priv->conn = g_value_dup_object (value);
        g_assert (priv->conn != NULL);
        break;
      case PROP_OBJECT_PATH:
        g_free (priv->object_path);
        priv->object_path = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gabble_base_call_stream_class_init (GabbleBaseCallStreamClass *bsc_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (bsc_class);
  GParamSpec *param_spec;
  static TpDBusPropertiesMixinPropImpl stream_props[] = {
    { "Interfaces", "interfaces", NULL },
    { "Senders", "senders", NULL },
    { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
      { RING_IFACE_CALL_STREAM,
        tp_dbus_properties_mixin_getter_gobject_properties,
        NULL,
        stream_props,
      },
      { NULL }
  };

  g_type_class_add_private (bsc_class, sizeof (GabbleBaseCallStreamPrivate));

  object_class->constructed = gabble_base_call_stream_constructed;
  object_class->dispose = gabble_base_call_stream_dispose;
  object_class->finalize = gabble_base_call_stream_finalize;
  object_class->set_property = gabble_base_call_stream_set_property;
  object_class->get_property = gabble_base_call_stream_get_property;

  param_spec = g_param_spec_string ("object-path", "D-Bus object path",
      "The D-Bus object path used for this object on the bus.",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, param_spec);

  param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
      "Additional interfaces implemented by this stream",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

  param_spec = g_param_spec_boxed ("senders", "Senders",
      "Sender map",
      RING_HASH_TYPE_CONTACT_SENDING_STATE_MAP,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SENDERS, param_spec);

  param_spec = g_param_spec_object ("connection", "RingConnection object",
      "Gabble connection object that owns this call stream",
      RING_TYPE_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  bsc_class->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (GabbleBaseCallStreamClass, dbus_props_class));
}

RingConnection *
gabble_base_call_stream_get_connection (GabbleBaseCallStream *self)
{
  g_return_val_if_fail (GABBLE_IS_BASE_CALL_STREAM (self), NULL);

  return self->priv->conn;
}

const gchar *
gabble_base_call_stream_get_object_path (GabbleBaseCallStream *self)
{
  g_return_val_if_fail (GABBLE_IS_BASE_CALL_STREAM (self), NULL);

  return self->priv->object_path;
}

static gboolean
base_call_stream_sender_update_state (GabbleBaseCallStream *self,
    TpHandle contact,
    TpSendingState state)
{
  GabbleBaseCallStreamPrivate *priv = self->priv;
  gpointer state_p = 0;
  gboolean exists;

  exists = g_hash_table_lookup_extended (priv->senders,
    GUINT_TO_POINTER (contact),
    NULL,
    &state_p);

  if (exists && GPOINTER_TO_UINT (state_p) == state)
    return FALSE;

  DEBUG ("Updating sender %d state: %d => %d", contact,
    GPOINTER_TO_UINT (state_p), state);

  g_hash_table_insert (priv->senders,
    GUINT_TO_POINTER (contact),
    GUINT_TO_POINTER (state));

  return TRUE;
}

gboolean
gabble_base_call_stream_update_senders (
    GabbleBaseCallStream *self,
    TpHandle contact,
    TpSendingState state,
    ...)
{
  GHashTable *updates = g_hash_table_new (g_direct_hash, g_direct_equal);
  gboolean updated = FALSE;
  va_list args;

  va_start (args, state);

  do
    {
      if (base_call_stream_sender_update_state (self, contact, state))
        {
          g_hash_table_insert (updates,
              GUINT_TO_POINTER (contact),
              GUINT_TO_POINTER (state));
          updated = TRUE;
        }

      contact = va_arg (args, TpHandle);
      if (contact != 0)
        state = va_arg (args, TpSendingState);
    }
  while (contact != 0);

  if (updated)
    {
      GArray *empty = g_array_new (FALSE, TRUE, sizeof (TpHandle));

      ring_svc_call_stream_emit_senders_changed (self, updates, empty);
      g_array_unref (empty);
    }

  g_hash_table_unref (updates);
  return updated;
}

TpSendingState
gabble_base_call_stream_get_sender_state (
    GabbleBaseCallStream *self,
    TpHandle sender,
    gboolean *existed)
{
  gpointer state;

  g_return_val_if_fail (GABBLE_IS_BASE_CALL_STREAM (self),
      TP_SENDING_STATE_NONE);

  if (g_hash_table_lookup_extended (self->priv->senders,
          GUINT_TO_POINTER (sender), NULL, &state))
    {
      if (existed != NULL)
        *existed = TRUE;

      return GPOINTER_TO_UINT (state);
    }
  else
    {
      if (existed != NULL)
        *existed = FALSE;

      return TP_SENDING_STATE_NONE;
    }
}

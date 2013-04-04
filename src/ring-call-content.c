/*
 * ring-call-content.c - a Content object owned by a Call channel
 * Copyright ©2010 Collabora Ltd.
 * Copyright ©2010 Nokia Corporation
 *   @author Will Thompson <will.thompson@collabora.co.uk>
 *   @author Tom Swindell <t.swindell@rubyx.co.uk>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ring-call-content.h"

#include <telepathy-glib/telepathy-glib.h>

#include <ring-extensions/ring-extensions.h>

#define DEBUG_FLAG RING_DEBUG_MEDIA
#include "ring-debug.h"

struct _RingCallContentPrivate {
    RingCallStream *stream;
};

static void implement_call_content (gpointer klass,
    gpointer unused G_GNUC_UNUSED);

G_DEFINE_TYPE (RingCallContent, ring_call_content,
    TP_TYPE_BASE_MEDIA_CALL_CONTENT);

static void
ring_call_content_init (RingCallContent *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, RING_TYPE_CALL_CONTENT,
      RingCallContentPrivate);
}

static void
ring_call_content_constructed (GObject *object)
{
  RingCallContent *self = RING_CALL_CONTENT (object);
  RingCallContentPrivate *priv = self->priv;
  TpBaseCallContent *base = TP_BASE_CALL_CONTENT (self);
  gchar *stream_path;

  if (G_OBJECT_CLASS (ring_call_content_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (ring_call_content_parent_class)->constructed (object);

  stream_path = g_strdup_printf ("%s/%s",
      tp_base_call_content_get_object_path (base), "stream");
  priv->stream = ring_call_stream_new (
      RING_CONNECTION(tp_base_call_content_get_connection (base)), stream_path);
  tp_base_call_content_add_stream (base,
      TP_BASE_CALL_STREAM (priv->stream));
  g_free (stream_path);
}

static void
ring_call_content_dispose (GObject *object)
{
  RingCallContent *self = RING_CALL_CONTENT (object);
  RingCallContentPrivate *priv = self->priv;

  tp_clear_object (&priv->stream);

  if (G_OBJECT_CLASS (ring_call_content_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (ring_call_content_parent_class)->dispose (object);
}

static void
ring_call_content_class_init (RingCallContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TpBaseCallContentClass *base_class = TP_BASE_CALL_CONTENT_CLASS(klass);

  object_class->constructed = ring_call_content_constructed;
  object_class->dispose = ring_call_content_dispose;

  g_type_class_add_private (klass, sizeof (RingCallContentPrivate));
}

RingCallContent *
ring_call_content_new (RingConnection *connection,
    const gchar *object_path,
    TpHandle creator)
{
  return g_object_new (RING_TYPE_CALL_CONTENT,
      "connection", connection,
      "object-path", object_path,
      "name", "audio",
      "media-type", TP_MEDIA_STREAM_TYPE_AUDIO,
      "creator", creator,
      "disposition", TP_CALL_CONTENT_DISPOSITION_INITIAL,
      NULL);
}

RingCallStream *
ring_call_content_get_stream (RingCallContent *self)
{
  g_return_val_if_fail (RING_IS_CALL_CONTENT (self), NULL);

  return self->priv->stream;
}

static void
ring_call_content_remove (
    RingCallContent *self,
    DBusGMethodInvocation *context)
{
  /* We could just leave all this out — the base class leaves Remove()
   * unimplemented, so TP_ERROR_NOT_IMPLEMENTED would be returned just like
   * this. But I think having a less generic error message is worth thirty(!)
   * lines of boilerplate.
   */
  GError error = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "Removing contents is not supported for cellular calls." };

  dbus_g_method_return_error (context, &error);
}

static void
implement_call_content (gpointer klass,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) ring_svc_call_content_implement_##x (\
  klass, ring_call_content_##x)
  IMPLEMENT (remove);
#undef IMPLEMENT
}

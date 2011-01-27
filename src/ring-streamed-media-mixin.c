/*
 * ring-streamed-media-mixin.c - Source for RingStreamedMediaMixin
 * Copyright © 2011 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#include "config.h"

#define DEBUG_FLAG RING_DEBUG_MEDIA
#include "ring-debug.h"

#include <ring-streamed-media-mixin.h>
#include "ring-util.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/interfaces.h>

#include <string.h>

struct _RingStreamedMediaMixinPrivate
{
  struct stream_state {
    guint id;                   /* nonzero when active, 0 otherwise */
    TpHandle handle;
    TpMediaStreamType type;
    TpMediaStreamState state;
    TpMediaStreamDirection direction;
    TpMediaStreamPendingSend pending;
  } audio[1], video[1];
};

enum {
  RING_MEDIA_STREAM_ID_AUDIO = 1,
  RING_MEDIA_STREAM_ID_VIDEO = 2
};

static TpDBusPropertiesMixinPropImpl streamed_media_properties[] = {
  { "InitialAudio", "initial-audio", NULL },
  { "InitialVideo", "initial-video", NULL },
  { "ImmutableStreams", "immutable-streams", NULL },
  { NULL }
};

void
ring_streamed_media_mixin_fill_immutable_properties (gpointer iface,
                                                     GHashTable *properties)
{
  tp_dbus_properties_mixin_fill_properties_hash (G_OBJECT (iface),
      properties,
      TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, "InitialAudio",
      TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, "InitialVideo",
      TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, "ImmutableStreams",
      NULL);
}

GQuark
ring_streamed_media_mixin_class_get_offset_quark (void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("RingStreamedMediaMixinClassOffset");

  return quark;
}

GQuark
ring_streamed_media_mixin_get_offset_quark (void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("RingStreamedMediaMixinOffset");

  return quark;
}

RingStreamedMediaMixinClass *
ring_streamed_media_mixin_class (GObjectClass *object_class)
{
  return RING_STREAMED_MEDIA_MIXIN_CLASS (object_class);
}

void
ring_streamed_media_mixin_class_init (GObjectClass *object_class, glong offset,
    RingStreamedMediaMixinValidateMediaHandle validate_handle,
    RingStreamedMediaMixinCreateStreams create_streams)
{
  RingStreamedMediaMixinClass *mixin_class;

  g_assert (G_IS_OBJECT_CLASS (object_class));
  g_assert (create_streams != NULL);
  g_assert (validate_handle != NULL);

  g_type_set_qdata (G_OBJECT_CLASS_TYPE (object_class),
      RING_STREAMED_MEDIA_MIXIN_CLASS_OFFSET_QUARK,
      GINT_TO_POINTER (offset));

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CHANNEL_TYPE_STREAMED_MEDIA,
      tp_dbus_properties_mixin_getter_gobject_properties,
      NULL,
      streamed_media_properties);

  mixin_class = RING_STREAMED_MEDIA_MIXIN_CLASS (object_class);

  mixin_class->validate_handle = validate_handle;
  mixin_class->create_streams = create_streams;
}

void
ring_streamed_media_mixin_init (GObject *object, gsize offset)
{
  RingStreamedMediaMixin *mixin;

  g_type_set_qdata (G_OBJECT_TYPE (object),
                    RING_STREAMED_MEDIA_MIXIN_OFFSET_QUARK,
                    GSIZE_TO_POINTER (offset));

  mixin = RING_STREAMED_MEDIA_MIXIN (object);

  mixin->priv = g_slice_new0 (RingStreamedMediaMixinPrivate);
}

/**
 * ring_streamed_media_mixin_finalize: (skip)
 * @obj: An object with this mixin.
 *
 * Free resources held by the contacts mixin.
 *
 * Since: 0.7.14
 *
 */
void
ring_streamed_media_mixin_finalize (GObject *object)
{
  RingStreamedMediaMixin *mixin = RING_STREAMED_MEDIA_MIXIN (object);
  RingStreamedMediaMixinPrivate *priv = mixin->priv;

  DEBUG ("%p", object);

  /* free any data held directly by the object here */

  memset (priv->audio, 0, sizeof *priv->audio);
  memset (priv->video, 0, sizeof *priv->video);

  g_slice_free (RingStreamedMediaMixinPrivate, priv);
}

/**
 * Telepathy.Channel.Type.StreamedMedia DBus interface - version 0.15
 *
 * Methods:
 * ListStreams ( ) -> a(uuuuuu)
 *
 * Returns an array of structs representing the streams currently active
 * within this channel. Each stream is identified by an unsigned integer
 * which is unique for each stream within the channel.
 *
 * Returns
 *
 * a(uuuuuu)
 *     An array of structs containing:
 *
 *         * the stream identifier
 *         * the contact handle who the stream is with
 *           (or 0 if the stream represents more than a single member)
 *         * the type of the stream
 *         * the current stream state
 *         * the current direction of the stream
 *         * the current pending send flags
 *
 *
 * RemoveStreams ( au: streams ) -> nothing
 *
 * Request that the given streams are removed.
 *
 * Parameters
 *
 * streams - au
 *     An array of stream identifiers (as defined in ListStreams)
 *
 * Possible errors
 *
 * org.freedesktop.Telepathy.Error.InvalidArgument
 *     Raised when one of the provided arguments is invalid.
 *
 * RequestStreamDirection ( u: stream_id, u: stream_direction ) -> nothing
 *
 * Request a change in the direction of an existing stream. In particular,
 * this might be useful to stop sending media of a particular type, or
 * inform the peer that you are no longer using media that is being sent to
 * you.
 *
 * Depending on the protocol, streams which are no longer sending in either
 * direction should be removed and a StreamRemoved signal emitted. Some
 * direction changes can be enforced locally (for example, BIDIRECTIONAL ->
 * RECEIVE can be achieved by merely stopping sending), others may not be
 * possible on some protocols, and some need agreement from the remote end.
 * In this case, the MEDIA_STREAM_PENDING_REMOTE_SEND flag will be set in
 * the StreamDirectionChanged signal, and the signal emitted again without
 * the flag to indicate the resulting direction when the remote end has
 * accepted or rejected the change. Parameters
 *
 * stream_id - u
 *     The stream identifier (as defined in ListStreams)
 * stream_direction - u
 *     The desired stream direction (a value of MediaStreamDirection)
 *
 * Possible errors
 *
 * org.freedesktop.Telepathy.Error.InvalidArgument
 *     Raised when one of the provided arguments is invalid.
 * org.freedesktop.Telepathy.Error.NotAvailable
 *     Raised when the requested functionality is temporarily
 *     unavailable. (generic description)
 *
 * RequestStreams ( u: contact_handle, au: types ) -> a(uuuuuu)
 *
 * Request that streams be established to exchange the given types of media
 * with the given member. In general this will try and establish a
 * bidirectional stream, but on some protocols it may not be possible to
 * indicate to the peer that you would like to receive media, so a send-only
 * stream will be created initially. In the cases where the stream requires
 * remote agreement (eg you wish to receive media from them), the
 * StreamDirectionChanged signal will be emitted with the
 * MEDIA_STREAM_PENDING_REMOTE_SEND flag set, and the signal emitted again
 * with the flag cleared when the remote end has replied. Parameters
 *
 * contact_handle - u
 *     A contact handle with whom to establish the streams
 * types - au
 *     An array of stream types (values of MediaStreamType)
 *
 * Returns
 *
 * a(uuuuuu)
 *     An array of structs (in the same order as the given stream types)
 *     containing:
 *
 *         * the stream identifier
 *         * the contact handle who the stream is with
 *           (or 0 if the stream represents more than a single member)
 *         * the type of the stream
 *         * the current stream state
 *         * the current direction of the stream
 *         * the current pending send flags
 *
 * Possible errors
 *
 * org.freedesktop.Telepathy.Error.InvalidHandle
 *     The contact name specified is unknown on this channel or
 *     connection. (generic description)
 * org.freedesktop.Telepathy.Error.InvalidArgument
 *     Raised when one of the provided arguments is invalid. (generic
 *     description)
 * org.freedesktop.Telepathy.Error.NotAvailable
 *     Raised when the requested functionality is temporarily
 *     unavailable. (generic description)
 *
 * Signals:
 * -> StreamAdded ( u: stream_id, u: contact_handle, u: stream_type )
 *
 * Emitted when a new stream has been added to this channel.
 *
 * Parameters
 *
 * stream_id - u
 *     The stream identifier (as defined in ListStreams)
 * contact_handle - u
 *     The contact handle who the stream is with (or 0 if it represents more than a single member)
 * stream_type - u
 *     The stream type (a value from MediaStreamType)
 *
 * -> StreamDirectionChanged ( u: stream_id, u: stream_direction, u: pending_flags )
 *
 * Emitted when the direction or pending flags of a stream are changed. If
 * the MEDIA_STREAM_PENDING_LOCAL_SEND flag is set, the remote user has
 * requested that we begin sending on this stream. RequestStreamDirection
 * should be called to indicate whether or not this change is acceptable.
 * Parameters
 *
 * stream_id - u
 *     The stream identifier (as defined in ListStreams)
 * stream_direction - u
 *     The new stream direction (as defined in ListStreams)
 * pending_flags - u
 *     The new pending send flags (as defined in ListStreams)
 *
 * StreamError ( u: stream_id, u: errno, s: message )
 *
 * Emitted when a stream encounters an error.
 *
 * Parameters
 *
 * stream_id - u
 *     The stream identifier (as defined in ListStreams)
 * errno - u
 *     A stream error number, one of the values of MediaStreamError
 * message - s
 *     A string describing the error (for debugging purposes only)
 *
 * StreamRemoved ( u: stream_id )
 *
 * Emitted when a stream has been removed from this channel.
 *
 * Parameters
 *
 * stream_id - u
 *     stream_id - the stream identifier (as defined in ListStreams)
 *
 * StreamStateChanged ( u: stream_id, u: stream_state )
 *
 * Emitted when a member's stream's state changes.
 *
 * Parameters
 *
 * stream_id - u
 *     The stream identifier (as defined in ListStreams)
 * stream_state - u
 *     The new stream state (as defined in ListStreams)
 */

#define TP_CHANNEL_STREAM_TYPE                  \
  (dbus_g_type_get_struct("GValueArray",        \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_INVALID))

/* Return a pointer to GValue with boxed stream struct */
static gpointer
describe_stream (struct stream_state *ss)
{
  const GType ElementType = TP_CHANNEL_STREAM_TYPE;
  GValue element[1] = {{ 0 }};

  g_value_init (element, ElementType);
  g_value_take_boxed (element, dbus_g_type_specialized_construct (ElementType));

  dbus_g_type_struct_set (element,
      0, ss->id,
      1, ss->handle,
      2, ss->type,
      3, ss->state,
      4, ss->direction,
      5, ss->pending,
      G_MAXUINT);

  return g_value_get_boxed (element);
}

static gpointer
describe_null_media (TpMediaStreamType tptype)
{
  struct stream_state ss[1] = {{ 0 }};

  ss->type = tptype;

  return describe_stream (ss);
}

static GPtrArray *
list_media_streams (TpSvcChannelTypeStreamedMedia *iface)
{
  RingStreamedMediaMixin *mixin = RING_STREAMED_MEDIA_MIXIN (iface);
  RingStreamedMediaMixinPrivate *priv = mixin->priv;
  GPtrArray *list;
  size_t size;

  size = (priv->audio->id != 0) + (priv->video->id != 0);

  list = g_ptr_array_sized_new(size);

  if (priv->audio->id)
    g_ptr_array_add(list, describe_stream(priv->audio));
  if (priv->video->id)
    g_ptr_array_add(list, describe_stream(priv->video));

  return list;
}

static void
free_media_stream_list (GPtrArray *list)
{
  const GType ElementType = TP_CHANNEL_STREAM_TYPE;
  guint i;

  if (list)
    {
      for (i = list->len; i-- > 0;)
        g_boxed_free(ElementType, g_ptr_array_index(list, i));

      g_ptr_array_free(list, TRUE);
    }
}

/** DBus method ListStreams ( ) -> a(uuuuuu)
 *
 * Returns an array of structs representing the streams currently active
 * within this channel. Each stream is identified by an unsigned integer
 * which is unique for each stream within the channel.
 *
 * Returns
 *
 * a(uuuuuu)
 *     An array of structs containing:
 *
 *         * the stream identifier
 *         * the contact handle who the stream is with
 *           (or 0 if the stream represents more than a single member)
 *         * the type of the stream
 *         * the current stream state
 *         * the current direction of the stream
 *         * the current pending send flags
 */
static void
ring_streamed_media_list_streams (TpSvcChannelTypeStreamedMedia *iface,
                                  DBusGMethodInvocation *context)
{
  GPtrArray *list = list_media_streams (iface);

  tp_svc_channel_type_streamed_media_return_from_list_streams (context, list);

  free_media_stream_list (list);
}

/** Update media stream state.
 *
 * @retval 0 if nothing changed
 * @retval 1 (or nonzero) if state changed
 */
static int
update_media_stream (TpSvcChannelTypeStreamedMedia *iface,
                     TpHandle handle,
                     guint id,
                     TpMediaStreamType type,
                     TpMediaStreamState state,
                     TpMediaStreamDirection direction,
                     TpMediaStreamPendingSend pending)
{
  RingStreamedMediaMixin *mixin = RING_STREAMED_MEDIA_MIXIN (iface);
  RingStreamedMediaMixinPrivate *priv = mixin->priv;
  struct stream_state *ss;
  int changed = 0;

  if (id == TP_MEDIA_STREAM_TYPE_AUDIO)
    ss = priv->audio;
  else if (id == TP_MEDIA_STREAM_TYPE_VIDEO)
    ss = priv->video;
  else
    return 0;

  if (state == TP_MEDIA_STREAM_STATE_DISCONNECTED)
    {
      if (ss->id != id)
        return 0;

      /* emit StreamRemoved */
      tp_svc_channel_type_streamed_media_emit_stream_removed (iface, id);
      memset (ss, 0, sizeof ss);

      return 1;
    }

  /* emit StreamAdded */
  if (ss->id != id)
    {
      changed = 1;

      if (handle == 0)
        {
          g_object_get (iface, "peer", &handle, NULL);
        }

      ss->id = id;
      ss->handle = handle;
      ss->type = type;

      DEBUG ("emitting StreamAdded(%u, %d, %s)",
          id, handle,
          type == TP_MEDIA_STREAM_TYPE_AUDIO ? "AUDIO" :
          type == TP_MEDIA_STREAM_TYPE_VIDEO ? "VIDEO" : "???");

      tp_svc_channel_type_streamed_media_emit_stream_added (iface,
          id, handle, type);
    }

  /* emit StreamStateChanged */
  if (ss->state != state)
    {
      changed = 1;
      ss->state = state;

      DEBUG("emitting StreamStateChanged(%u, %s)",
          ss->id,
          state == TP_MEDIA_STREAM_STATE_DISCONNECTED ? "DISCONNECTED" :
          state == TP_MEDIA_STREAM_STATE_CONNECTING ? "CONNECTING" :
          state == TP_MEDIA_STREAM_STATE_CONNECTED ? "CONNECTED" : "???");

      tp_svc_channel_type_streamed_media_emit_stream_state_changed (iface,
          ss->id, state);
    }

  /* emit StreamDirectionChanged */
  if (ss->direction != direction || ss->pending != pending)
    {
      changed = 1;
      ss->direction = direction;
      ss->pending = pending;

      DEBUG("emitting StreamDirectionChanged(%u, %s,%s%s%s)",
          ss->id,
          direction == TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL
          ? "BIDIRECTIONAL" :
          direction == TP_MEDIA_STREAM_DIRECTION_SEND ? "SEND" :
          direction == TP_MEDIA_STREAM_DIRECTION_RECEIVE ? "RECV" : "NONE",
          pending & TP_MEDIA_STREAM_PENDING_REMOTE_SEND ? " remote" : "",
          pending & TP_MEDIA_STREAM_PENDING_LOCAL_SEND ? " local" : "",
          pending == 0 ? " 0" : "");

      tp_svc_channel_type_streamed_media_emit_stream_direction_changed (iface,
          ss->id, ss->direction, ss->pending);
    }

  return changed;
}

/** DBus method RequestStreams ( u: contact_handle, au: types ) -> a(uuuuuu)
 *
 * Request that streams be established to exchange the given types of media
 * with the given member. In general this will try and establish a
 * bidirectional stream, but on some protocols it may not be possible to
 * indicate to the peer that you would like to receive media, so a send-only
 * stream will be created initially. In the cases where the stream requires
 * remote agreement (eg you wish to receive media from them), the
 * StreamDirectionChanged signal will be emitted with the
 * MEDIA_STREAM_PENDING_REMOTE_SEND flag set, and the signal emitted again
 * with the flag cleared when the remote end has replied.
 *
 */
static void
ring_streamed_media_request_streams (TpSvcChannelTypeStreamedMedia *iface,
                                     guint handle,
                                     const GArray *media_types,
                                     DBusGMethodInvocation *context)
{
  RingStreamedMediaMixin *mixin = RING_STREAMED_MEDIA_MIXIN (iface);
  RingStreamedMediaMixinPrivate *priv = mixin->priv;
  RingStreamedMediaMixinClass *klass;
  GError *error = NULL;
  GPtrArray *list;
  guint i, create_audio_stream = 0, create_video_stream = 0;

  DEBUG("()");

  klass = RING_STREAMED_MEDIA_MIXIN_GET_CLASS (iface);
  g_assert (klass->validate_handle != NULL);
  g_assert (klass->create_streams != NULL);

  if (!klass->validate_handle (iface, &handle, &error))
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
      return;
    }

  /* We can create media only when call has not been initiated */
  for (i = 0; i < media_types->len; i++)
    {
      guint media_type = g_array_index(media_types, guint, i);

      if (media_type == TP_MEDIA_STREAM_TYPE_AUDIO)
        {
          if (priv->audio->id == 0)
            continue;

          if (!klass->create_streams (iface, handle, TRUE, FALSE, &error))
            {
              dbus_g_method_return_error(context, error);
              g_clear_error(&error);
              return;
            }

          create_audio_stream = TRUE;

          update_media_stream (iface, handle,
              RING_MEDIA_STREAM_ID_AUDIO, TP_MEDIA_STREAM_TYPE_AUDIO,
              TP_MEDIA_STREAM_STATE_CONNECTING,
              TP_MEDIA_STREAM_DIRECTION_NONE,
              TP_MEDIA_STREAM_PENDING_LOCAL_SEND |
              TP_MEDIA_STREAM_PENDING_REMOTE_SEND);
        }
    }

  list = g_ptr_array_sized_new (media_types->len);

  for (i = 0; i < media_types->len; i++) {
    guint media_type = g_array_index(media_types, guint, i);
    gpointer element;

    if (media_type == TP_MEDIA_STREAM_TYPE_AUDIO && create_audio_stream)
      element = describe_stream(priv->audio), create_audio_stream = 0;
    else if (media_type == TP_MEDIA_STREAM_TYPE_VIDEO && create_video_stream)
      element = describe_stream(priv->video), create_video_stream = 0;
    else
      element = describe_null_media(media_type);

    g_ptr_array_add(list, element);
  }

  tp_svc_channel_type_streamed_media_return_from_request_streams (context,
      list);

  free_media_stream_list (list);

  DEBUG("exit");
}

void
ring_streamed_media_mixin_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcChannelTypeStreamedMediaClass *klass =
    (TpSvcChannelTypeStreamedMediaClass *)g_iface;

#define IMPLEMENT(x) tp_svc_channel_type_streamed_media_implement_##x (klass, \
      ring_streamed_media_##x)
  IMPLEMENT (list_streams);
  IMPLEMENT (request_streams);
#undef IMPLEMENT
}

/** Update audio state.
 *
 * @retval 0 if nothing changed
 * @retval 1 (or nonzero) if state changed
 */
int
ring_streamed_media_mixin_update_audio (gpointer _self,
                                        TpHandle handle,
                                        TpMediaStreamState state,
                                        TpMediaStreamDirection direction,
                                        TpMediaStreamPendingSend pending)
{
  return update_media_stream (TP_SVC_CHANNEL_TYPE_STREAMED_MEDIA (_self),
      handle, RING_MEDIA_STREAM_ID_AUDIO,
      TP_MEDIA_STREAM_TYPE_AUDIO, state, direction, pending);
}

gboolean
ring_streamed_media_mixin_is_audio_stream (gpointer iface,
                                           guint stream_id)
{
  RingStreamedMediaMixin *mixin = RING_STREAMED_MEDIA_MIXIN (iface);

  return stream_id != 0 && stream_id == mixin->priv->audio->id;
}

gboolean
ring_streamed_media_mixin_is_stream_connected (gpointer iface,
                                               guint stream_id)
{
  RingStreamedMediaMixin *mixin = RING_STREAMED_MEDIA_MIXIN (iface);

  if (stream_id == 0)
    return FALSE;

  if (stream_id == mixin->priv->audio->id)
    return mixin->priv->audio->state == TP_MEDIA_STREAM_STATE_CONNECTED;

  if (stream_id == mixin->priv->video->id)
    return mixin->priv->video->state == TP_MEDIA_STREAM_STATE_CONNECTED;

  return FALSE;
}

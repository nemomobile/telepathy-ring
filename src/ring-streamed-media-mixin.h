/*
 * ring-streamed-media-mixin.h - Header for RingStreamedMediaMixin
 *
 * Copyright (C) 2011 Nokia Corporation
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

#ifndef RING_STREAMED_MEDIA_MIXIN_H
#define RING_STREAMED_MEDIA_MIXIN_H

#include <telepathy-glib/svc-connection.h>
#include <telepathy-glib/handle-repo.h>

G_BEGIN_DECLS

typedef struct _RingStreamedMediaMixinClass RingStreamedMediaMixinClass;
typedef struct _RingStreamedMediaMixin RingStreamedMediaMixin;
typedef struct _RingStreamedMediaMixinPrivate RingStreamedMediaMixinPrivate;

typedef gboolean (*RingStreamedMediaMixinValidateMediaHandle) (gpointer,
    guint *handle, GError **);
typedef gboolean (*RingStreamedMediaMixinCreateStreams) (gpointer,
    guint handle, gboolean audio, gboolean video, GError **);

struct _RingStreamedMediaMixinClass {
  RingStreamedMediaMixinValidateMediaHandle validate_handle;
  RingStreamedMediaMixinCreateStreams create_streams;
};

struct _RingStreamedMediaMixin {
  RingStreamedMediaMixinPrivate *priv;
};

/* TYPE MACROS */

#define RING_STREAMED_MEDIA_MIXIN_CLASS_OFFSET_QUARK \
  (ring_streamed_media_mixin_class_get_offset_quark ())

#define RING_STREAMED_MEDIA_MIXIN_CLASS_OFFSET(klass) \
  (tp_mixin_class_get_offset (klass,                  \
      RING_STREAMED_MEDIA_MIXIN_CLASS_OFFSET_QUARK))

#define RING_STREAMED_MEDIA_MIXIN_CLASS(klass) \
  ((RingStreamedMediaMixinClass *) \
      tp_mixin_offset_cast (klass, \
          RING_STREAMED_MEDIA_MIXIN_CLASS_OFFSET (klass)))

#define RING_STREAMED_MEDIA_MIXIN_GET_CLASS(object) \
  ring_streamed_media_mixin_class (G_OBJECT_GET_CLASS (object))

GQuark ring_streamed_media_mixin_class_get_offset_quark (void);
void ring_streamed_media_mixin_class_init (GObjectClass *klass, glong offset,
    RingStreamedMediaMixinValidateMediaHandle validate_handle,
    RingStreamedMediaMixinCreateStreams create_streams);

#define RING_STREAMED_MEDIA_MIXIN_OFFSET_QUARK \
  (ring_streamed_media_mixin_get_offset_quark ())

#define RING_STREAMED_MEDIA_MIXIN_OFFSET(object) \
  (tp_mixin_instance_get_offset (object,         \
      RING_STREAMED_MEDIA_MIXIN_OFFSET_QUARK))

#define RING_STREAMED_MEDIA_MIXIN(object) \
  ((RingStreamedMediaMixin *)                           \
      tp_mixin_offset_cast (object,                     \
          RING_STREAMED_MEDIA_MIXIN_OFFSET (object)))

GQuark ring_streamed_media_mixin_get_offset_quark (void);

void ring_streamed_media_mixin_init (GObject *object, gsize offset);
void ring_streamed_media_mixin_finalize (GObject *object);

void ring_streamed_media_mixin_iface_init (gpointer iface, gpointer data);

/* RingStreamedMediaMixin interface */

void ring_streamed_media_mixin_fill_immutable_properties (gpointer iface,
    GHashTable *properties);

int ring_streamed_media_mixin_update_audio (gpointer iface,
    TpHandle handle,
    TpMediaStreamState state,
    TpMediaStreamDirection direction,
    TpMediaStreamPendingSend pending);

gboolean ring_streamed_media_mixin_is_audio_stream (gpointer iface,
    guint stream_id);

gboolean ring_streamed_media_mixin_is_stream_connected (gpointer iface,
    guint stream_id);

G_END_DECLS

#endif /* #ifndef RING_STREAMED_MEDIA_MIXIN_H */

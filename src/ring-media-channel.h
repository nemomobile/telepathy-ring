/*
 * ring-media-channel.h - Header for RingMediaChannel
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

#ifndef RING_MEDIA_CHANNEL_H
#define RING_MEDIA_CHANNEL_H

#include <glib-object.h>
#include <telepathy-glib/group-mixin.h>
#include <telepathy-glib/dbus-properties-mixin.h>

G_BEGIN_DECLS

typedef struct _RingMediaChannel RingMediaChannel;
typedef struct _RingMediaChannelClass RingMediaChannelClass;
typedef struct _RingMediaChannelPrivate RingMediaChannelPrivate;

G_END_DECLS

#include "ring-connection.h"

#include <modem/call.h>

G_BEGIN_DECLS

struct _RingMediaChannelClass {
  GObjectClass parent_class;
  TpDBusPropertiesMixinClass dbus_properties_class;

  void (*emit_initial)(RingMediaChannel *self);
  void (*update_state)(RingMediaChannel *self, guint status, guint causetype, guint cause);
  gboolean (*close)(RingMediaChannel *, gboolean immediately);
  void (*set_call_instance)(RingMediaChannel *self, ModemCall *ci);
  gboolean (*validate_media_handle)(RingMediaChannel *, guint *, GError **);
  gboolean (*create_streams)(RingMediaChannel *,
    guint handle, gboolean audio, gboolean video,
    GError **);
};

struct _RingMediaChannel {
  GObject parent;
  /* Read-only */
  RingConnection *connection;
  ModemCallService *call_service;
  ModemCall *call_instance;
  char const *nick;

  RingMediaChannelPrivate *priv;
};

GType ring_media_channel_get_type(void);

/* TYPE MACROS */
#define RING_TYPE_MEDIA_CHANNEL                 \
  (ring_media_channel_get_type())
#define RING_MEDIA_CHANNEL(obj)                                         \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), RING_TYPE_MEDIA_CHANNEL, RingMediaChannel))
#define RING_MEDIA_CHANNEL_CLASS(klass)                                 \
  (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_MEDIA_CHANNEL, RingMediaChannelClass))
#define RING_IS_MEDIA_CHANNEL(obj)                              \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_MEDIA_CHANNEL))
#define RING_IS_MEDIA_CHANNEL_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_MEDIA_CHANNEL))
#define RING_MEDIA_CHANNEL_GET_CLASS(obj)                               \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), RING_TYPE_MEDIA_CHANNEL, RingMediaChannelClass))

/***********************************************************************
 * Additional declarations (not based on generated templates)
 ***********************************************************************/

#define RING_MEDIA_CHANNEL_INTERFACES                   \
  TP_IFACE_CHANNEL_INTERFACE_DTMF,                      \
    TP_IFACE_CHANNEL_INTERFACE_HOLD
#if nomore
,                    \
    RTCOM_TP_IFACE_CHANNEL_INTERFACE_DIAL_STRINGS
#endif

#define RING_MEDIA_CHANNEL_CAPABILITY_FLAGS     \
  (TP_CHANNEL_MEDIA_CAPABILITY_AUDIO)

void ring_media_channel_emit_initial(RingMediaChannel *self);

void ring_media_channel_close(RingMediaChannel *self);

ModemRequest *ring_media_channel_queue_request(RingMediaChannel *self,
  ModemRequest *request);

ModemRequest *ring_media_channel_dequeue_request(RingMediaChannel *self,
  ModemRequest *request);

gboolean ring_media_channel_send_dialstring(RingMediaChannel *self,
  guint id,
  char const *dialstring,
  guint duration,
  guint pause,
  GError **error);

void ring_media_channel_play_tone(RingMediaChannel *self,
  int tone, int volume, unsigned duration);

gboolean ring_media_channel_is_playing(RingMediaChannel const *self);

void ring_media_channel_idle_playing(RingMediaChannel *self);
void ring_media_channel_stop_playing(RingMediaChannel *self,
  gboolean always);

void ring_media_channel_set_state(RingMediaChannel *self,
  guint state,
  guint causetype,
  guint cause);

GHashTable *ring_media_channel_properties(RingMediaChannel *self);

G_END_DECLS

#endif /* #ifndef RING_MEDIA_CHANNEL_H*/

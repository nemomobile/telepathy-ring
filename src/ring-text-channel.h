/*
 * ring-text-channel.h - Header for RingTextChannel
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

#ifndef __RING_TEXT_CHANNEL_H__
#define __RING_TEXT_CHANNEL_H__

#include <glib-object.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/message-mixin.h>
#include <telepathy-glib/base-channel.h>

G_BEGIN_DECLS

typedef struct _RingTextChannel RingTextChannel;
typedef struct _RingTextChannelClass RingTextChannelClass;
typedef struct _RingTextChannelPrivate RingTextChannelPrivate;

struct _RingTextChannelClass {
  TpBaseChannelClass parent_class;
  TpDBusPropertiesMixinClass dbus_properties_class;
};

struct _RingTextChannel {
  TpBaseChannel parent;

  TpMessageMixin message;

  RingTextChannelPrivate *priv;
};

GType ring_text_channel_get_type (void);

/* TYPE MACROS */
#define RING_TYPE_TEXT_CHANNEL                  \
  (ring_text_channel_get_type())
#define RING_TEXT_CHANNEL(obj)                  \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), RING_TYPE_TEXT_CHANNEL, RingTextChannel))
#define RING_TEXT_CHANNEL_CLASS(klass)          \
  (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_TEXT_CHANNEL, RingTextChannelClass))
#define RING_IS_TEXT_CHANNEL(obj)               \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_TEXT_CHANNEL))
#define RING_IS_TEXT_CHANNEL_CLASS(klass)       \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_TEXT_CHANNEL))
#define RING_TEXT_CHANNEL_GET_CLASS(obj)        \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), RING_TYPE_TEXT_CHANNEL, RingTextChannelClass))

#define RING_TEXT_CHANNEL_CAPABILITY_FLAGS (0)

char *ring_text_channel_destination(char const *inspection);

#if nomore

/* FIXME: the gpointers are temporary hacks */
gboolean ring_text_channel_can_handle(gpointer);
void ring_text_channel_receive_deliver(RingTextChannel *, gpointer);

void ring_text_channel_receive_status_report(RingTextChannel *, gpointer);

#endif

void ring_text_channel_receive_text (RingTextChannel *self,
    gchar const *message_token,
    gchar const *message,
    gint64 message_sent,
    gint64 message_received,
    guint32 sms_class);

void ring_text_channel_outgoing_sms_complete(RingTextChannel *,
  char const *token);

void ring_text_channel_outgoing_sms_error(RingTextChannel *,
  char const *token,
  GError const *error);

char const * const *ring_text_get_content_types(void);

G_END_DECLS

#endif

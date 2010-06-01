/*
 * ring-call-channel.h - Header for RingCallChannel
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

#ifndef RING_CALL_CHANNEL_H
#define RING_CALL_CHANNEL_H

#include <glib-object.h>
#include <telepathy-glib/group-mixin.h>
#include <telepathy-glib/dbus-properties-mixin.h>

G_BEGIN_DECLS

typedef struct _RingCallChannel RingCallChannel;
typedef struct _RingCallChannelClass RingCallChannelClass;
typedef struct _RingCallChannelPrivate RingCallChannelPrivate;

G_END_DECLS

#include "ring-conference-channel.h"

G_BEGIN_DECLS

struct _RingCallChannelClass {
  RingMediaChannelClass base_class;
  TpGroupMixinClass group_class;
  TpDBusPropertiesMixinClass dbus_properties_class;
};

struct _RingCallChannel {
  RingMediaChannel base;
  TpGroupMixin group;
  RingCallChannelPrivate *priv;
};

GType ring_call_channel_get_type(void);

/* TYPE MACROS */
#define RING_TYPE_CALL_CHANNEL                                 \
  (ring_call_channel_get_type())
#define RING_CALL_CHANNEL(obj)                                 \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), RING_TYPE_CALL_CHANNEL, RingCallChannel))
#define RING_CALL_CHANNEL_CLASS(klass)                          \
  (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_CALL_CHANNEL, RingCallChannelClass))
#define RING_IS_CALL_CHANNEL(obj)                               \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_CALL_CHANNEL))
#define RING_IS_CALL_CHANNEL_CLASS(klass)                       \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_CALL_CHANNEL))
#define RING_CALL_CHANNEL_GET_CLASS(obj)                        \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), RING_TYPE_CALL_CHANNEL, RingCallChannelClass))

/***********************************************************************
 * Additional declarations (not based on generated templates)
 ***********************************************************************/

void ring_call_channel_initial_audio(RingCallChannel *self,
  RingMediaManager *manager,
  gpointer channelrequest);

G_END_DECLS

#endif /* #ifndef RING_CALL_CHANNEL_H*/

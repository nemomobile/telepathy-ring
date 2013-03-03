/*
 * ring-conference-channel.h - Header for RingConferenceChannel
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

#ifndef RING_CONFERENCE_CHANNEL_H
#define RING_CONFERENCE_CHANNEL_H

#include <glib-object.h>
#include <telepathy-glib/base-channel.h>

G_BEGIN_DECLS

typedef struct _RingConferenceChannel RingConferenceChannel;
typedef struct _RingConferenceChannelClass RingConferenceChannelClass;
typedef struct _RingConferenceChannelPrivate RingConferenceChannelPrivate;

G_END_DECLS

#include "ring-media-manager.h"
#include "ring-member-channel.h"

G_BEGIN_DECLS

struct _RingConferenceChannelClass {
  RingMediaChannelClass parent_class;
  TpGroupMixinClass group_class;
  TpDBusPropertiesMixinClass dbus_properties_class;
};

struct _RingConferenceChannel {
  RingMediaChannel base;
  TpGroupMixin group;
  RingConferenceChannelPrivate *priv;
  gchar *nick;
};

GType ring_conference_channel_get_type(void);

/* TYPE MACROS */
#define RING_TYPE_CONFERENCE_CHANNEL \
  (ring_conference_channel_get_type())
#define RING_CONFERENCE_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), RING_TYPE_CONFERENCE_CHANNEL, RingConferenceChannel))
#define RING_CONFERENCE_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_CONFERENCE_CHANNEL, RingConferenceChannelClass))
#define RING_IS_CONFERENCE_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_CONFERENCE_CHANNEL))
#define RING_IS_CONFERENCE_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_CONFERENCE_CHANNEL))
#define RING_CONFERENCE_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), RING_TYPE_CONFERENCE_CHANNEL, RingConferenceChannelClass))


/***********************************************************************
 * Additional declarations (not based on generated templates)
 ***********************************************************************/

guint ring_conference_channel_has_members(RingConferenceChannel const *);

gboolean ring_conference_channel_join(RingConferenceChannel *channel,
  RingMemberChannel *member,
  GError **error);

void ring_conference_channel_emit_channel_removed(
  RingConferenceChannel *channel,
  RingMemberChannel *member,
  char const *message,
  guint actor,
  TpChannelGroupChangeReason reason);

void ring_conference_channel_emit_initial(RingConferenceChannel *channel);

gboolean ring_conference_channel_check_initial_members (
    RingConferenceChannel const *,
    RingInitialMembers const *);

void ring_conference_channel_initial_audio (RingConferenceChannel *self);

G_END_DECLS

#endif /* #ifndef RING_CONFERENCE_CHANNEL_H*/

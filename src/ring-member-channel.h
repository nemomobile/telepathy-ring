/*
 * ring-member-channel.h - Interface for mergeable channels
 *
 * Copyright (C) 2008-2010 Nokia Corporation
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

#ifndef RING_MEMBER_CHANNEL_H
#define RING_MEMBER_CHANNEL_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _RingMemberChannel RingMemberChannel;
typedef struct _RingMemberChannelIface RingMemberChannelIface;

G_END_DECLS

#include <ring-conference-channel.h>

G_BEGIN_DECLS

#define RING_TYPE_MEMBER_CHANNEL (ring_member_channel_get_type ())

#define RING_MEMBER_CHANNEL(obj)                        \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                   \
    RING_TYPE_MEMBER_CHANNEL, RingMemberChannel))

#define RING_IS_MEMBER_CHANNEL(obj)             \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
    RING_TYPE_MEMBER_CHANNEL))

#define RING_MEMBER_CHANNEL_GET_INTERFACE(obj)          \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj),                \
    RING_TYPE_MEMBER_CHANNEL, RingMemberChannelIface))

struct _RingMemberChannelIface {
  GTypeInterface parent;
};

GType ring_member_channel_get_type (void);

gboolean ring_member_channel_is_in_conference(RingMemberChannel const *iface);

gboolean ring_member_channel_can_become_member(RingMemberChannel const *iface,
  GError **error);

RingConferenceChannel *ring_member_channel_get_conference(RingMemberChannel const *iface);

GHashTable *ring_member_channel_get_handlemap(RingMemberChannel *);

gboolean ring_member_channel_release(RingMemberChannel *iface,
  const char *message,
  guint reason,
  GError **error);

void ring_member_channel_joined(RingMemberChannel *iface,
  RingConferenceChannel *conference);

void ring_member_channel_left(RingMemberChannel *iface);

G_END_DECLS

#endif

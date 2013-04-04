/*
 * base-call-channel.h - Header for RingBaseCallChannel
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

#ifndef __RING_BASE_CALL_CHANNEL_H__
#define __RING_BASE_CALL_CHANNEL_H__

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#include "ring-call-content.h"
#include "ring-call-member.h"

G_BEGIN_DECLS

typedef struct _RingBaseCallChannel RingBaseCallChannel;
typedef struct _RingBaseCallChannelPrivate RingBaseCallChannelPrivate;
typedef struct _RingBaseCallChannelClass RingBaseCallChannelClass;

struct _RingBaseCallChannelClass {
    TpBaseMediaCallChannelClass parent_class;
};

struct _RingBaseCallChannel {
    TpBaseMediaCallChannel parent;

    RingBaseCallChannelPrivate *priv;
};

GType ring_base_call_channel_get_type (void);

/* TYPE MACROS */
#define RING_TYPE_BASE_CALL_CHANNEL \
  (ring_base_call_channel_get_type ())
#define RING_BASE_CALL_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   RING_TYPE_BASE_CALL_CHANNEL, RingBaseCallChannel))
#define RING_BASE_CALL_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
   RING_TYPE_BASE_CALL_CHANNEL, RingBaseCallChannelClass))
#define RING_IS_BASE_CALL_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_BASE_CALL_CHANNEL))
#define RING_IS_BASE_CALL_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_BASE_CALL_CHANNEL))
#define RING_BASE_CALL_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   RING_TYPE_BASE_CALL_CHANNEL, RingBaseCallChannelClass))

RingCallMember *ring_base_call_channel_ensure_member (
    RingBaseCallChannel *self,
    const gchar *jid);

void ring_base_call_channel_remove_member (RingBaseCallChannel *self,
    RingCallMember *member);

RingCallMember *ring_base_call_channel_ensure_member_from_handle (
    RingBaseCallChannel *self,
    TpHandle handle);

RingCallMember * ring_base_call_channel_get_member_from_handle (
    RingBaseCallChannel *self,
    TpHandle handle);

RingCallContent * ring_base_call_channel_add_content (
    RingBaseCallChannel *self,
    const gchar *name,
    TpCallContentDisposition disposition);

void ring_base_call_channel_remove_content (RingBaseCallChannel *self,
    RingCallContent *content);

GHashTable *ring_base_call_channel_get_members (RingBaseCallChannel *self);

G_END_DECLS

#endif /* #ifndef __RING_BASE_CALL_CHANNEL_H__*/

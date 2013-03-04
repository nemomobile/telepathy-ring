/*
 * base-call-channel.h - Header for GabbleBaseCallChannel
 * Copyright © 2009–2010 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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

#ifndef __GABBLE_BASE_CALL_CHANNEL_H__
#define __GABBLE_BASE_CALL_CHANNEL_H__

#include <glib-object.h>

#include <ring-extensions/ring-extensions.h>

#include <telepathy-glib/base-channel.h>

#include "base-call-content.h"

G_BEGIN_DECLS

typedef struct _GabbleBaseCallChannel GabbleBaseCallChannel;
typedef struct _GabbleBaseCallChannelPrivate GabbleBaseCallChannelPrivate;
typedef struct _GabbleBaseCallChannelClass GabbleBaseCallChannelClass;

struct _GabbleBaseCallChannelClass {
    TpBaseChannelClass parent_class;

    gboolean hardware_streaming;
    gboolean mutable_contents;

    void (*accept) (GabbleBaseCallChannel *self);
    void (*hangup) (GabbleBaseCallChannel *self,
      guint reason,
      const gchar *detailed_reason,
      const gchar *message);
};

struct _GabbleBaseCallChannel {
    TpBaseChannel parent;

    gboolean initial_audio;
    gboolean initial_video;

    GabbleBaseCallChannelPrivate *priv;
};

GType gabble_base_call_channel_get_type (void);

/* TYPE MACROS */
#define GABBLE_TYPE_BASE_CALL_CHANNEL \
  (gabble_base_call_channel_get_type ())
#define GABBLE_BASE_CALL_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   GABBLE_TYPE_BASE_CALL_CHANNEL, GabbleBaseCallChannel))
#define GABBLE_BASE_CALL_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
   GABBLE_TYPE_BASE_CALL_CHANNEL, GabbleBaseCallChannelClass))
#define GABBLE_IS_BASE_CALL_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GABBLE_TYPE_BASE_CALL_CHANNEL))
#define GABBLE_IS_BASE_CALL_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GABBLE_TYPE_BASE_CALL_CHANNEL))
#define GABBLE_BASE_CALL_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   GABBLE_TYPE_BASE_CALL_CHANNEL, GabbleBaseCallChannelClass))

TpCallState gabble_base_call_channel_get_state (
  GabbleBaseCallChannel *self);
void gabble_base_call_channel_set_state (GabbleBaseCallChannel *self,
  TpCallState state);

void gabble_base_call_channel_update_flags (GabbleBaseCallChannel *self,
    TpCallFlags set_flags,
    TpCallFlags clear_flags);

gboolean gabble_base_call_channel_update_members (
    GabbleBaseCallChannel *self,
    TpHandle contact,
    TpCallMemberFlags set_flags,
    TpCallMemberFlags clear_flags,
    ...) G_GNUC_NULL_TERMINATED;
gboolean gabble_base_call_channel_remove_members (
    GabbleBaseCallChannel *self,
    TpHandle contact,
    ...) G_GNUC_NULL_TERMINATED;

void gabble_base_call_channel_add_content (GabbleBaseCallChannel *self,
    GabbleBaseCallContent *content);

G_END_DECLS

#endif /* #ifndef __GABBLE_BASE_CALL_CHANNEL_H__*/

/*
 * ring-call-stream.h - header for a Stream object owned by a RingCallContent
 * Copyright ©2010 Collabora Ltd.
 * Copyright ©2010 Nokia Corporation
 *   @author Will Thompson <will.thompson@collabora.co.uk>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef RING_CALL_STREAM_H
#define RING_CALL_STREAM_H

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>

#include "ring-connection.h"

G_BEGIN_DECLS

typedef struct _RingCallStream RingCallStream;
typedef struct _RingCallStreamPrivate RingCallStreamPrivate;
typedef struct _RingCallStreamClass RingCallStreamClass;

struct _RingCallStreamClass {
    TpBaseMediaCallStreamClass parent_class;
};

struct _RingCallStream {
    TpBaseMediaCallStream parent;

    RingCallStreamPrivate *priv;
};

GType ring_call_stream_get_type (void);

RingCallStream *ring_call_stream_new (RingConnection *connection,
    const gchar *object_path);

/* TYPE MACROS */
#define RING_TYPE_CALL_STREAM \
  (ring_call_stream_get_type ())
#define RING_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), RING_TYPE_CALL_STREAM, RingCallStream))
#define RING_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_CALL_STREAM,\
                           RingCallStreamClass))
#define RING_IS_CALL_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_CALL_STREAM))
#define RING_IS_CALL_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_CALL_STREAM))
#define RING_CALL_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), RING_TYPE_CALL_STREAM, \
                              RingCallStreamClass))

void ring_call_stream_update_member_states(RingCallStream *self);

G_END_DECLS

#endif /* RING_CALL_STREAM_H */

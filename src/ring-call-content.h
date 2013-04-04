/*
 * ring-call-content.h - header for a Content object owned by a Call channel
 * Copyright ©2010 Collabora Ltd.
 * Copyright ©2010 Nokia Corporation
 *   @author Will Thompson <will.thompson@collabora.co.uk>
 *   @author Tom Swindell <t.swindell@rubyx.co.uk>
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

#ifndef RING_CALL_CONTENT_H
#define RING_CALL_CONTENT_H

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>

#include "ring-connection.h"
#include "ring-call-stream.h"

typedef struct _RingCallContent RingCallContent;
typedef struct _RingCallContentClass RingCallContentClass;
typedef struct _RingCallContentPrivate RingCallContentPrivate;

struct _RingCallContentClass {
    TpBaseMediaCallContentClass parent_class;
};

struct _RingCallContent {
    TpBaseMediaCallContent parent;

    RingCallContentPrivate *priv;
};

GType ring_call_content_get_type (void);

RingCallContent *ring_call_content_new (RingConnection *connection,
    const gchar *object_path,
    TpHandle creator);

RingCallStream *ring_call_content_get_stream (RingCallContent *self);

static void ring_call_content_remove(RingCallContent *self, DBusGMethodInvocation *context);

/* TYPE MACROS */
#define RING_TYPE_CALL_CONTENT \
  (ring_call_content_get_type ())
#define RING_CALL_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), RING_TYPE_CALL_CONTENT, RingCallContent))
#define RING_CALL_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_CALL_CONTENT,\
                           RingCallContentClass))
#define RING_IS_CALL_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_CALL_CONTENT))
#define RING_IS_CALL_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_CALL_CONTENT))
#define RING_CALL_CONTENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), RING_TYPE_CALL_CONTENT, \
                              RingCallContentClass))

#endif /* RING_CALL_CONTENT_H */

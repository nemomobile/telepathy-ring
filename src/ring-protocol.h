/*
 * protocol.h - header for RingProtocol
 * Copyright (C) 2007-2010 Collabora Ltd.
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

#ifndef RING_PROTOCOL_H
#define RING_PROTOCOL_H

#include <glib-object.h>
#include <telepathy-glib/base-protocol.h>

G_BEGIN_DECLS

typedef struct _RingProtocol
    RingProtocol;
typedef struct _RingProtocolPrivate
    RingProtocolPrivate;
typedef struct _RingProtocolClass
    RingProtocolClass;
typedef struct _RingProtocolClassPrivate
    RingProtocolClassPrivate;

struct _RingProtocolClass {
    TpBaseProtocolClass parent_class;

    RingProtocolClassPrivate *priv;
};

struct _RingProtocol {
    TpBaseProtocol parent;

    RingProtocolPrivate *priv;
};

GType ring_protocol_get_type (void);

#define RING_TYPE_PROTOCOL \
    (ring_protocol_get_type ())
#define RING_PROTOCOL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        RING_TYPE_PROTOCOL, \
        RingProtocol))
#define RING_PROTOCOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
        RING_TYPE_PROTOCOL, \
        RingProtocolClass))
#define IS_RING_PROTOCOL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), \
        RING_TYPE_PROTOCOL))
#define RING_PROTOCOL_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        RING_TYPE_PROTOCOL, \
        RingProtocolClass))

RingProtocol *ring_protocol_new (void);

G_END_DECLS

#endif

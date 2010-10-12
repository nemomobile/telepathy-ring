/*
 * ring-connection-manager.h - Header for RingConnectionManager
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

#ifndef RING_CONNECTION_MANAGER_H
#define RING_CONNECTION_MANAGER_H

#include <glib-object.h>

#include <telepathy-glib/base-connection-manager.h>

G_BEGIN_DECLS

#define TEL_PROTOCOL_STRING "tel"

typedef struct _RingConnectionManager RingConnectionManager;
typedef struct _RingConnectionManagerClass RingConnectionManagerClass;
typedef struct _RingConnectionManagerPrivate RingConnectionManagerPrivate;

struct _RingConnectionManagerClass {
  TpBaseConnectionManagerClass parent_class;
};

struct _RingConnectionManager {
  TpBaseConnectionManager parent;
  RingConnectionManagerPrivate *priv;
};

GType ring_connection_manager_get_type(void);

/* Type Macros */
#define RING_TYPE_CONNECTION_MANAGER                                    \
  (ring_connection_manager_get_type())
#define RING_CONNECTION_MANAGER(obj)                                    \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), RING_TYPE_CONNECTION_MANAGER, RingConnectionManager))
#define RING_CONNECTION_MANAGER_CLASS(klass)                            \
  (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_CONNECTION_MANAGER, RingConnectionManagerClass))
#define RING_IS_CONNECTION_MANAGER(obj)                                 \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_CONNECTION_MANAGER))
#define RING_IS_CONNECTION_MANAGER_CLASS(klass)                         \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_CONNECTION_MANAGER))
#define RING_CONNECTION_MANAGER_GET_CLASS(obj)                          \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), RING_TYPE_CONNECTION_MANAGER, RingConnectionManagerClass))

G_END_DECLS

#endif /* #ifndef RING_CONNECTION_MANAGER_H*/

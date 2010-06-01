/*
 * ring-connection.h - Header for RingConnection
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

#ifndef RING_CONNECTION_H
#define RING_CONNECTION_H

#include <glib-object.h>
#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/base-connection-manager.h>
#include <telepathy-glib/contacts-mixin.h>
#include <ring-util.h>

G_BEGIN_DECLS

typedef struct _RingConnection RingConnection;
typedef struct _RingConnectionClass RingConnectionClass;
typedef struct _RingConnectionPrivate RingConnectionPrivate;

struct _RingConnectionClass {
  TpBaseConnectionClass parent_class;
  TpDBusPropertiesMixinClass dbus_properties_class;
  TpContactsMixinClass contacts_mixin_class;
};

struct _RingConnection {
  TpBaseConnection parent;
  TpContactsMixin contacts_mixin;
  TpHandle anon_handle, sos_handle;
  RingConnectionPrivate *priv;
};

GType ring_connection_get_type(void);

/* Type macros */
#define RING_TYPE_CONNECTION \
  (ring_connection_get_type())
#define RING_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), RING_TYPE_CONNECTION, RingConnection))
#define RING_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_CONNECTION, RingConnectionClass))
#define RING_IS_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_CONNECTION))
#define RING_IS_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_CONNECTION))
#define RING_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), RING_TYPE_CONNECTION, RingConnectionClass))

/* Extensions to TpBaseConnection */

extern TpCMParamSpec const ring_connection_params[];
gpointer ring_connection_params_alloc(void);
void ring_connection_params_free(gpointer p);

RingConnection *ring_connection_new(
  TpIntSet *params_present, gpointer ring_connection_params);

gboolean ring_connection_check_status(RingConnection *self);

char const *ring_connection_inspect_contact(RingConnection const *, TpHandle);

gboolean ring_connection_contact_is_special(RingConnection const *, TpHandle);

gpointer ring_connection_lookup_channel(RingConnection const *, char const *);

gboolean ring_connection_validate_initial_members(RingConnection *self,
  RingInitialMembers *initial,
  GError **error);

G_END_DECLS

#endif /* #ifndef RING_CONNECTION_H */

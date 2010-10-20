/*
 * ring-connection-manager.c - Source for RingConnectionManager
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

/* Based on telepathy-glib/examples/cm/extended/manager.c with copyright notice
 *
 * """
 * manager.c - an example connection manager
 *
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 * """
 */

#define DEBUG_FLAG RING_DEBUG_CONNECTION
#include "ring-debug.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus-protocol.h>

#include "ring-connection-manager.h"
#include "ring-connection.h"
#include "ring-protocol.h"

#include "modem/service.h"

#include <telepathy-glib/errors.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct _RingConnectionManagerPrivate
{
  int dummy;
};

G_DEFINE_TYPE (RingConnectionManager,
    ring_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

static void
ring_connection_manager_init(RingConnectionManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
      RING_TYPE_CONNECTION_MANAGER, RingConnectionManagerPrivate);

  modem_service_refresh (modem_service ());
}

static void
ring_connection_manager_constructed (GObject *obj)
{
  RingConnectionManager *self = RING_CONNECTION_MANAGER (obj);
  TpBaseConnectionManager *base = (TpBaseConnectionManager *) self;
  GObjectClass *base_class = (GObjectClass *)
    ring_connection_manager_parent_class;
  RingProtocol *protocol;

  if (base_class->constructed)
    base_class->constructed (obj);

  protocol = ring_protocol_new ();
  tp_base_connection_manager_add_protocol (base, TP_BASE_PROTOCOL (protocol));
  g_object_unref (protocol);
}

static void
ring_connection_manager_class_init(RingConnectionManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TpBaseConnectionManagerClass *parent_class = &klass->parent_class;

  g_type_class_add_private (klass, sizeof (RingConnectionManagerPrivate));

  object_class->constructed = ring_connection_manager_constructed;

  parent_class->new_connection = NULL;
  parent_class->cm_dbus_name = "ring";
  parent_class->protocol_params = NULL;
}

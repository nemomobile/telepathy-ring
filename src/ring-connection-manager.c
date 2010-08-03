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

#include <telepathy-glib/errors.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

G_DEFINE_TYPE (RingConnectionManager,
    ring_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

static void
ring_connection_manager_init(RingConnectionManager *self)
{
  /* Xyzzy */
}

static TpCMProtocolSpec ring_protocols[] = {
  {
    "tel",
    NULL, /* filled in in ring_connection_manager_class_init() */
    ring_connection_params_alloc,
    ring_connection_params_free
  },
  { NULL, NULL }
};

static TpBaseConnection *
new_connection(TpBaseConnectionManager *self,
               const char *proto,
               TpIntSet *params_present,
               gpointer parsed_params,
               GError **error)
{
  if (strcmp(proto, "tel")) {
    g_set_error(error, TP_ERRORS,
        TP_ERROR_INVALID_ARGUMENT, "Protocol is not supported");
    return NULL;
  }

  if (dbus_g_bus_get(DBUS_BUS_SYSTEM, error) == NULL)
    return NULL;

  return (TpBaseConnection *)ring_connection_new(params_present, parsed_params);
}


static void
ring_connection_manager_class_init(RingConnectionManagerClass *klass)
{
  TpBaseConnectionManagerClass *parent_class = &klass->parent_class;

  ring_protocols[0].parameters = ring_connection_get_param_specs();

  parent_class->new_connection = new_connection;
  parent_class->cm_dbus_name = "ring";
  parent_class->protocol_params = ring_protocols;
}

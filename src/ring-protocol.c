/*
 * ring-protocol.c - source for RingProtocol
 * Copyright (C) 2007-2010 Collabora Ltd.
 * Copyright (C) 2007-2010 Nokia Corporation
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

#include "ring-protocol.h"

#include <telepathy-glib/base-connection-manager.h>
#include <telepathy-glib/dbus.h>
#include <dbus/dbus-protocol.h>
#include <dbus/dbus-glib.h>

#include "ring-connection.h"
#include "ring-media-manager.h"
#include "ring-text-manager.h"

#define NAME              "tel"
#define ICON_NAME         "im-tel"
#define VCARD_FIELD_NAME  "TEL"
#define ENGLISH_NAME      "Mobile Telephony"

G_DEFINE_TYPE (RingProtocol, ring_protocol, TP_TYPE_BASE_PROTOCOL)

static void
ring_protocol_init (RingProtocol *self)
{
}

static const TpCMParamSpec *
get_parameters (TpBaseProtocol *self G_GNUC_UNUSED)
{
  return ring_connection_get_param_specs ();
}

static TpBaseConnection *
new_connection (TpBaseProtocol *protocol G_GNUC_UNUSED,
                GHashTable *params,
                GError **error)
{
  if (dbus_g_bus_get (DBUS_BUS_SYSTEM, error) == NULL)
    return NULL;

  return TP_BASE_CONNECTION (ring_connection_new (params));
}

static gchar *
normalize_contact (TpBaseProtocol *self G_GNUC_UNUSED,
                   const gchar *contact,
                   GError **error)
{
  return ring_normalize_contact (contact, error);
}

static gchar *
identify_account (TpBaseProtocol *self G_GNUC_UNUSED,
                  GHashTable *asv,
                  GError **error)
{
  const gchar *account = tp_asv_get_string (asv, "account");

  g_assert (account != NULL);

  return g_strdup (account);
}

static GStrv
get_interfaces (TpBaseProtocol *self)
{
  return g_new0 (gchar *, 1);
}

static void
get_connection_details (TpBaseProtocol *self,
                        GStrv *connection_interfaces,
                        GType **channel_managers,
                        gchar **icon_name,
                        gchar **english_name,
                        gchar **vcard_field)
{
  if (connection_interfaces)
    *connection_interfaces = ring_connection_dup_implemented_interfaces ();

  if (channel_managers)
    {
      GType types[] = {
        RING_TYPE_TEXT_MANAGER,
        RING_TYPE_MEDIA_MANAGER,
        G_TYPE_INVALID
      };

      *channel_managers = g_memdup (types, sizeof(types));
    }

  if (icon_name)
    *icon_name = g_strdup (ICON_NAME);

  if (english_name)
    *english_name = g_strdup (ENGLISH_NAME);

  if (vcard_field)
    *vcard_field = g_strdup (VCARD_FIELD_NAME);
}

static void
ring_protocol_class_init (RingProtocolClass *klass)
{
  TpBaseProtocolClass *base_class = (TpBaseProtocolClass *) klass;

  base_class->get_parameters = get_parameters;
  base_class->new_connection = new_connection;
  base_class->normalize_contact = normalize_contact;
  base_class->identify_account = identify_account;
  base_class->get_interfaces = get_interfaces;
  base_class->get_connection_details = get_connection_details;
}

RingProtocol *
ring_protocol_new (void)
{
  return g_object_new (RING_TYPE_PROTOCOL, "name", NAME, NULL);
}


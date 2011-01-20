/*
 * ring-member-channel.c - Interface for mergeable channels
 *
 * Copyright (C) 2008-2010 Nokia Corporation
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

#include "config.h"

#include "ring-member-channel.h"
#include "ring-media-channel.h"

#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/util.h>

static void
ring_member_channel_base_init(gpointer klass)
{
  static gboolean initialized;

  if (initialized)
    return;

  initialized = TRUE;

  g_object_interface_install_property(
    klass,
    g_param_spec_uint("member-handle",
      "Member Handle",
      "Handle representing the channel target in conference",
      0, G_MAXUINT, 0,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property(
    klass,
    g_param_spec_boxed("member-map",
      "Mapping from peer to member handle",
      "Mapping from peer to member handle",
      TP_HASH_TYPE_HANDLE_OWNER_MAP,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property(
    klass,
    g_param_spec_boxed("member-conference",
      "Conference Channel",
      "Conference Channel this object is associated with",
      DBUS_TYPE_G_OBJECT_PATH,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));
}

GType
ring_member_channel_get_type(void)
{
  static GType type;

  if (G_UNLIKELY(!type)) {
    static const GTypeInfo info = {
      sizeof (RingMemberChannelIface),
      ring_member_channel_base_init,
    };

    type = g_type_register_static(
      G_TYPE_INTERFACE, "RingMemberChannel", &info, 0);

    g_type_interface_add_prerequisite(type, RING_TYPE_MEDIA_CHANNEL);
  }

  return type;
}

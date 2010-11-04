/*
 * modem/ofono.h - Ofono
 *
 * Copyright (C) 2010 Nokia Corporation
 *   @author Lassi Syrjala <first.surname@nokia.com>
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

#ifndef _MODEM_OFONO_H_
#define _MODEM_OFONO_H_

#include "modem/request.h"

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

/* ---------------------------------------------------------------------- */

/* D-Bus name of the Ofono service */
#define OFONO_BUS_NAME           "org.ofono"

/* Interfaces */
#define OFONO_IFACE_MANAGER      "org.ofono.Manager"
#define OFONO_IFACE_MODEM        "org.ofono.Modem"

/* Interfaces per modem */
#define OFONO_IFACE_SIM          "org.ofono.SimManager"
#define OFONO_IFACE_CALL_MANAGER "org.ofono.VoiceCallManager"
#define OFONO_IFACE_CALL         "org.ofono.VoiceCall"
#define OFONO_IFACE_SMS          "org.ofono.MessageManager"

/* Quarks for mandatory modem interfaces */
#define OFONO_IFACE_QUARK_SIM modem_ofono_iface_quark_sim ()
#define OFONO_IFACE_QUARK_CALL_MANAGER modem_ofono_iface_quark_call_manager ()
#define OFONO_IFACE_QUARK_SMS modem_ofono_iface_quark_sms ()

/* D-Bus type a{sv} for Ofono properties */
#define MODEM_TYPE_DBUS_DICT modem_type_dbus_dict ()
#define MODEM_TYPE_ARRAY_OF_PATHS modem_type_dbus_ao ()

/* D-Bus type a{oa{sv}} for oFono managed object list */
#define MODEM_TYPE_DBUS_MANAGED_ARRAY modem_type_dbus_managed_array ()

/* ---------------------------------------------------------------------- */

GType modem_type_dbus_dict (void);
GType modem_type_dbus_ao (void);
GType modem_type_dbus_managed_array (void);
GQuark modem_ofono_iface_quark_sim (void);
GQuark modem_ofono_iface_quark_call_manager (void);
GQuark modem_ofono_iface_quark_sms (void);
void modem_ofono_init_quarks (void);

DBusGProxy *modem_ofono_proxy (char const *object_path, char const *interface);

void modem_ofono_debug_managed (char const *name,
    char const *object_path,
    GHashTable *properties);

G_END_DECLS

#endif /* #ifndef _MODEM_OFONO_H_ */

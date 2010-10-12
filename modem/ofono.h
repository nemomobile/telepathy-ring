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
#define OFONO_IFACE_SMS          "org.ofono.SmsManager"

/* Quarks for mandatory modem interfaces */
#define OFONO_IFACE_QUARK_SIM modem_ofono_iface_quark_sim()
#define OFONO_IFACE_QUARK_CALL_MANAGER modem_ofono_iface_quark_call_manager()
#define OFONO_IFACE_QUARK_SMS modem_ofono_iface_quark_sms()

/* D-Bus type a{sv} for Ofono properties */
#define MODEM_TYPE_DBUS_DICT modem_type_dbus_dict()
#define MODEM_TYPE_ARRAY_OF_PATHS modem_type_dbus_ao()

/* D-Bus type a{oa{sv}} for oFono path-property list */
#define MODEM_TYPE_DBUS_DESC_ARRAY modem_type_dbus_desc_array()

/* ---------------------------------------------------------------------- */

typedef void ModemOfonoPropChangedCb(
  DBusGProxy *proxy, char const *property,
  GValue const *value, gpointer user_data);

typedef void ModemOfonoPropsReply(
  gpointer object,
  ModemRequest *request, GHashTable *properties,
  GError const *error, gpointer user_data);

typedef void ModemOfonoVoidReply(
  gpointer object,
  ModemRequest *request,
  GError const *error, gpointer user_data);

typedef void ModemOfonoGetDescsReply(gpointer object,
    ModemRequest *request, GPtrArray *descs,
    GError const *error, gpointer user_data);

/* ---------------------------------------------------------------------- */

GType modem_type_dbus_dict(void);
GType modem_type_dbus_ao(void);
GType modem_type_dbus_desc_array(void);
GQuark modem_ofono_iface_quark_sim(void);
GQuark modem_ofono_iface_quark_call_manager(void);
GQuark modem_ofono_iface_quark_sms(void);
void modem_ofono_init_quarks(void);

DBusGProxy *modem_ofono_proxy(
  char const *object_path, char const *interface);

ModemRequest *modem_ofono_proxy_set_property(
  DBusGProxy *proxy,
  char const *property, GValue *value,
  ModemOfonoVoidReply *callback,
  gpointer object, gpointer user_data);

ModemRequest *modem_ofono_proxy_request_properties(
  DBusGProxy *proxy, ModemOfonoPropsReply *callback,
  gpointer object, gpointer user_data);

void modem_ofono_proxy_connect_to_property_changed(
  DBusGProxy *proxy, ModemOfonoPropChangedCb callback,
  gpointer user_data);

void modem_ofono_proxy_disconnect_from_property_changed(
  DBusGProxy *proxy, ModemOfonoPropChangedCb callback,
  gpointer user_data);

ModemRequest *modem_ofono_request_descs(gpointer object,
    DBusGProxy *proxy, char const *method,
    ModemOfonoGetDescsReply *callback, gpointer user_dat);

void modem_ofono_debug_desc(char const *name,
    char const *object_path,
    GHashTable *properties);

G_END_DECLS

#endif /* #ifndef _MODEM_OFONO_H_ */

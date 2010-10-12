/*
 * modem/ofono.c - Ofono
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

#include "config.h"

#define MODEM_DEBUG_FLAG MODEM_SERVICE_MODEM
#include "modem/debug.h"

#include "modem/ofono.h"
#include "modem/request-private.h"
#include "modem/errors.h"

/* ---------------------------------------------------------------------- */

GType
modem_type_dbus_dict(void)
{
  static gsize type = 0;

  if (g_once_init_enter(&type)) {
    GType t = dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE);
    g_once_init_leave(&type, t);
  }

  return type;
}

GType
modem_type_dbus_ao(void)
{
  static gsize type = 0;

  if (g_once_init_enter(&type)) {
    GType t = dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH);
    g_once_init_leave(&type, t);
  }

  return type;
}

GType
modem_type_dbus_desc_array(void)
{
  static gsize type = 0;

  if (g_once_init_enter(&type)) {
    GType stype = dbus_g_type_get_struct("GValueArray",
	DBUS_TYPE_G_OBJECT_PATH,
	MODEM_TYPE_DBUS_DICT,
	G_TYPE_INVALID);
    GType t = dbus_g_type_get_collection("GPtrArray", stype);
    g_once_init_leave(&type, t);
  }

  return type;
}

GQuark
modem_ofono_iface_quark_sim(void)
{
  static gsize quark = 0;

  if (g_once_init_enter(&quark)) {
    GQuark q = g_quark_from_static_string(OFONO_IFACE_SIM);
    g_once_init_leave(&quark, q);
  }

  return quark;
}

GQuark
modem_ofono_iface_quark_call_manager(void)
{
  static gsize quark = 0;

  if (g_once_init_enter(&quark)) {
    GQuark q = g_quark_from_static_string(OFONO_IFACE_CALL_MANAGER);
    g_once_init_leave(&quark, q);
  }

  return quark;
}

GQuark
modem_ofono_iface_quark_sms(void)
{
  static gsize quark = 0;

  if (g_once_init_enter(&quark)) {
    GQuark q = g_quark_from_static_string(OFONO_IFACE_SMS);
    g_once_init_leave(&quark, q);
  }

  return quark;
}

void
modem_ofono_init_quarks(void)
{
  modem_ofono_iface_quark_sim();
  modem_ofono_iface_quark_call_manager();
  modem_ofono_iface_quark_sms();
}

static DBusGConnection *
modem_ofono_get_bus(void)
{
  static DBusGConnection *bus = NULL;

  if (G_UNLIKELY (bus == NULL))
    bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);

  return bus;
}

static void
reply_to_set_property(DBusGProxy *proxy,
  DBusGProxyCall *call,
  gpointer _request)
{
  ModemRequest *request = _request;
  gpointer object = modem_request_object(request);
  ModemOfonoVoidReply *callback = modem_request_callback(request);
  gpointer user_data = modem_request_user_data(request);
  GError *error = NULL;

  if (!dbus_g_proxy_end_call(proxy, call, &error,
      G_TYPE_INVALID)) {
    modem_error_fix(&error);
  }

  if (callback)
    callback(object, request, error, user_data);

  if (error)
    g_error_free (error);
}

static void
reply_to_get_properties(DBusGProxy *proxy,
  DBusGProxyCall *call,
  gpointer _request)
{
  GHashTable *properties = NULL;
  ModemRequest *request = _request;
  gpointer object = modem_request_object(request);
  ModemOfonoPropsReply *callback = modem_request_callback(request);
  gpointer user_data = modem_request_user_data(request);
  GError *error = NULL;

  if (!dbus_g_proxy_end_call(proxy, call, &error,
      MODEM_TYPE_DBUS_DICT, &properties,
      G_TYPE_INVALID)) {
    modem_error_fix(&error);
  }

  if (callback)
    callback(object, request, properties, error, user_data);

  if (error)
    g_error_free (error);
  if (properties)
    g_hash_table_unref (properties);
}

static void
reply_to_get_descs(DBusGProxy *proxy,
		   DBusGProxyCall *call,
		   gpointer _request)
{
  GPtrArray *calls = NULL;
  ModemRequest *request = _request;
  gpointer object = modem_request_object(request);
  ModemOfonoGetDescsReply *callback = modem_request_callback(request);
  gpointer user_data = modem_request_user_data(request);
  GError *error = NULL;

  if (!dbus_g_proxy_end_call(proxy, call, &error,
          MODEM_TYPE_DBUS_DESC_ARRAY, &calls,
          G_TYPE_INVALID))
    modem_error_fix(&error);

  if (callback)
    callback(object, request, calls, error, user_data);

  if (error)
    g_error_free (error);
  if (calls)
    g_ptr_array_free (calls, TRUE);
}

ModemRequest *modem_ofono_request_descs(gpointer object,
    DBusGProxy *proxy, char const *method,
    ModemOfonoGetDescsReply *callback, gpointer userdata)
{
  return modem_request_begin(object,
      proxy, method, reply_to_get_descs,
      G_CALLBACK(callback), userdata,
      G_TYPE_INVALID);
}

void
modem_ofono_debug_desc(char const *name,
                       char const *object_path,
                       GHashTable *properties)
{
  char *key;
  GValue *value;
  GHashTableIter iter[1];

  DEBUG("%s path %s", name, object_path);

  for (g_hash_table_iter_init(iter, properties);
       g_hash_table_iter_next(iter, (gpointer)&key, (gpointer)&value);) {
    char *s = g_strdup_value_contents(value);
    DEBUG("%s = %s", key, s);
    g_free(s);
  }
}

/* ---------------------------------------------------------------------- */

DBusGProxy *
modem_ofono_proxy(char const *object_path, char const *interface)
{
  return dbus_g_proxy_new_for_name(modem_ofono_get_bus(),
    OFONO_BUS_NAME,
    object_path,
    interface);
}

ModemRequest *
modem_ofono_proxy_set_property(DBusGProxy *proxy,
  char const *property, GValue *value,
  ModemOfonoVoidReply *callback,
  gpointer object, gpointer user_data)
{
  return modem_request_begin(object, proxy,
    "SetProperty",
    reply_to_set_property,
    G_CALLBACK(callback), user_data,
    G_TYPE_STRING, property,
    G_TYPE_VALUE, value,
    G_TYPE_INVALID);
}

ModemRequest *
modem_ofono_proxy_request_properties(DBusGProxy *proxy,
  ModemOfonoPropsReply *callback,
  gpointer object,
  gpointer user_data)
{
  return modem_request_begin(object, proxy,
    "GetProperties",
    reply_to_get_properties,
    G_CALLBACK(callback), user_data,
    G_TYPE_INVALID);
}

void
modem_ofono_proxy_connect_to_property_changed(DBusGProxy *proxy,
  ModemOfonoPropChangedCb callback,
  gpointer user_data)
{
  if (callback) {
    dbus_g_proxy_add_signal(proxy, "PropertyChanged",
      G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(proxy, "PropertyChanged",
      G_CALLBACK(callback), user_data, NULL);
  }
}


void
modem_ofono_proxy_disconnect_from_property_changed(
  DBusGProxy *proxy, ModemOfonoPropChangedCb callback,
  gpointer user_data)
{
  if (callback) {
    dbus_g_proxy_disconnect_signal(proxy, "PropertyChanged",
      G_CALLBACK(callback), user_data);
  }
}

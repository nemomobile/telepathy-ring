/*
 * modem/request-private.h - Private ModemRequest
 *
 * Copyright (C) 2008 Nokia Corporation
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

#ifndef _MODEM_REQUEST_PRIVATE_H_
#define _MODEM_REQUEST_PRIVATE_H_

#include <modem/request.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

typedef struct _ModemRequestPrivate ModemRequestPrivate;

typedef void ModemRequestCallNotify (DBusGProxy *, DBusGProxyCall *, gpointer);

ModemRequest *_modem_request_new(gpointer object,
  DBusGProxy *proxy,
  GCallback callback,
  gpointer user_data);

void _modem_request_add_proxy_call(ModemRequest *request,
  DBusGProxyCall *call);

void _modem_request_destroy_notify(gpointer _request);

/** Make a DBus method call with reply */
#define modem_request_begin(object, proxy, method, method_callback,     \
  callback, user, gtype, ...)                                           \
  ({ ModemRequest *_temp = NULL; DBusGProxyCall *_pcall;                \
    _temp = _modem_request_new((object), (proxy), (callback), (user));  \
    _pcall = dbus_g_proxy_begin_call((proxy), (method), (method_callback), \
             _temp, _modem_request_destroy_notify,                      \
             (gtype), ## __VA_ARGS__);                                  \
    _modem_request_add_proxy_call(_temp, _pcall);                       \
    _temp; })

/** Make a DBus method call with reply and timeout */
#define modem_request_with_timeout(object, proxy, method,               \
  method_callback, callback, user_data, timeout, gtype, ...)            \
  ({ ModemRequest *_temp = NULL; DBusGProxyCall *_pcall;                \
    _temp = _modem_request_new((object), (proxy),                       \
            (callback), (user_data));                                   \
    _pcall = dbus_g_proxy_begin_call_with_timeout((proxy), (method),    \
             (method_callback), _temp, _modem_request_destroy_notify,   \
             (timeout), (gtype), ## __VA_ARGS__);                       \
    _modem_request_add_proxy_call(_temp, _pcall);                       \
    _temp; })


/** Make a DBus-request.
 *
 * If @a callback is NULL, do not wait for reply.
 */
#define modem_request(object, proxy, method, method_callback,           \
  callback, user, gtype, ...)                                           \
  (callback) ?                                                          \
             modem_request_begin((object), (proxy), (method), (method_callback), \
               (callback), (user), (gtype), ## __VA_ARGS__)             \
             : (dbus_g_proxy_call_no_reply((proxy), (method),           \
                 (gtype), ## __VA_ARGS__),                              \
               NULL)

#define modem_request_object(request)           \
  (g_ptr_array_index((GPtrArray*)(request), 0))

#define modem_request_callback(request)         \
  (g_ptr_array_index((GPtrArray*)(request), 1))

#define modem_request_user_data(request)        \
  (g_ptr_array_index((GPtrArray*)(request), 2))


G_END_DECLS

#endif /* #ifndef _MODEM_REQUEST_PRIVATE_H_ */

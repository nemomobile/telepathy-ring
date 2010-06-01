/*
 * modem/request.h - Extensible closure for asyncronous DBus calls
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

#ifndef _MODEM_REQUEST_H_
#define _MODEM_REQUEST_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ModemRequest ModemRequest;

void modem_request_cancel(ModemRequest *request);

void modem_request_add_cancel_notify(ModemRequest *request,
  GDestroyNotify notify);

void modem_request_add_notifys(ModemRequest *request,
  GDestroyNotify notify,
  gpointer data,
  ...) G_GNUC_NULL_TERMINATED;

void modem_request_add_qdata(ModemRequest *, GQuark, gpointer data);
void modem_request_add_qdata_full(ModemRequest *request,
  GQuark quark,
  gpointer data,
  GDestroyNotify destroy);
void modem_request_add_qdatas(ModemRequest *request,
  GQuark quark,
  gpointer data,
  GDestroyNotify notify,
  ...) G_GNUC_NULL_TERMINATED;

gpointer modem_request_get_qdata(ModemRequest *request, GQuark quark);
gpointer modem_request_steal_qdata(ModemRequest *request, GQuark quark);

void modem_request_add_data(ModemRequest *, char const *key, gpointer data);
void modem_request_add_data_full(ModemRequest *request,
  char const *key,
  gpointer data,
  GDestroyNotify destroy);

gpointer modem_request_get_data(ModemRequest *request, char const *key);
gpointer modem_request_steal_data(ModemRequest *request, char const *key);

G_END_DECLS

#endif /* #ifndef _MODEM_REQUEST_H_ */

/*
 * ring-emergency-service.c - RingEmergencyService and RingEmergencyServiceList
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

#ifndef RING_EMERGENCY_SERVICE_H
#define RING_EMERGENCY_SERVICE_H

#include <glib-object.h>
#include <rtcom-telepathy-glib/extensions.h>
#include <modem/call.h>

G_BEGIN_DECLS

/* Boxed struct describing RingEmergencyService */
typedef GValueArray RingEmergencyService;
typedef GPtrArray RingEmergencyServiceList;

#define RING_TYPE_EMERGENCY_SERVICE RTCOM_TP_STRUCT_TYPE_EMERGENCY_SERVICE

RingEmergencyService *ring_emergency_service_new(char const *service,
  guint handle,
  char const * const *aliases);

void ring_emergency_service_free(RingEmergencyService *service);

RingEmergencyServiceList *ring_emergency_service_list_new(RingEmergencyService *, ...)
G_GNUC_NULL_TERMINATED;

void ring_emergency_service_list_free(RingEmergencyServiceList *);

RingEmergencyServiceList *ring_emergency_service_list_default(
  guint sos_handle, char const * const *numbers);

#define RING_EMERGENCY_SERVICE_URN "urn:service:sos"

G_END_DECLS

#endif /* #ifndef RING_EMERGENCY_SERVICE_H*/

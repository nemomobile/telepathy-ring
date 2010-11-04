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
#include <ring-extensions/ring-extensions.h>
#include <ring-extensions/gtypes.h>

G_BEGIN_DECLS

#define RING_EMERGENCY_SERVICE_URN "urn:service:sos"

/* Boxed struct describing RingEmergencyService */
typedef GValueArray RingEmergencyService;
typedef GValueArray RingEmergencyServiceInfo;
typedef GPtrArray RingEmergencyServiceInfoList;

RingEmergencyService *ring_emergency_service_new(char const *service);
void ring_emergency_service_free(RingEmergencyService *service);

RingEmergencyServiceInfo *ring_emergency_service_info_new(char const *service,
  char * const *aliases);

void ring_emergency_service_info_free(RingEmergencyServiceInfo *service);

RingEmergencyServiceInfoList *ring_emergency_service_info_list_new(
  RingEmergencyServiceInfo *, ...) G_GNUC_NULL_TERMINATED;

void ring_emergency_service_info_list_free(RingEmergencyServiceInfoList *);

RingEmergencyServiceInfoList *ring_emergency_service_info_list_default(
  char * const *numbers);

G_END_DECLS

#endif /* #ifndef RING_EMERGENCY_SERVICE_H*/

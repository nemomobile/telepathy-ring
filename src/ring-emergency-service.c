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

#define DEBUG_FLAG RING_DEBUG_CONNECTION
#include "ring-debug.h"

#include "ring-emergency-service.h"
#include "ring-util.h"
#include "modem/call.h"

#include <dbus/dbus-glib.h>
#include <telepathy-glib/gtypes.h>

#include <string.h>

RingEmergencyService *
ring_emergency_service_new(char const *service)
{
  GValue value[1] = {{ 0 }};
  GType gtype = TP_STRUCT_TYPE_SERVICE_POINT;
  TpServicePointType service_type;

  if (service == NULL)
    service = "";

  if (service[0] == '\0')
    service_type = TP_SERVICE_POINT_TYPE_NONE;
  else
    service_type = TP_SERVICE_POINT_TYPE_EMERGENCY;

  g_value_init(value, gtype);
  g_value_take_boxed(value, dbus_g_type_specialized_construct(gtype));
  dbus_g_type_struct_set(value,
    0, service_type,
    1, service,
    G_MAXUINT);

  return (RingEmergencyService *)g_value_get_boxed(value);
}

void
ring_emergency_service_free(RingEmergencyService *service)
{
  g_boxed_free(TP_STRUCT_TYPE_SERVICE_POINT, (gpointer)service);
}

/* Return a pointer to a boxed service struct */
RingEmergencyServiceInfo *
ring_emergency_service_info_new(char const *service,
  char const * const *aliases)
{
  RingEmergencyService *es;
  GValue value[1] = {{ 0 }};
  GType gtype = TP_STRUCT_TYPE_SERVICE_POINT_INFO;

  g_value_init(value, gtype);
  g_value_take_boxed(value, dbus_g_type_specialized_construct(gtype));

  es = ring_emergency_service_new(service);

  dbus_g_type_struct_set(value,
    0, es,
    1, aliases,
    G_MAXUINT);

  ring_emergency_service_free(es);

  return (RingEmergencyServiceInfo *)g_value_get_boxed(value);
}

void
ring_emergency_service_info_free(RingEmergencyServiceInfo *info)
{
  g_boxed_free(TP_STRUCT_TYPE_SERVICE_POINT_INFO, info);
}

RingEmergencyServiceInfoList *
ring_emergency_service_info_list_new(RingEmergencyServiceInfo *info,
  ...)
{
  RingEmergencyServiceInfoList *self = g_ptr_array_sized_new(1);
  va_list ap;

  for (va_start(ap, info); info; info = va_arg(ap, gpointer)) {
    g_ptr_array_add(self, info);
  }
  va_end(ap);

  return self;
}

void
ring_emergency_service_info_list_free(RingEmergencyServiceInfoList *list)
{
  guint i;
  for (i = 0; i < list->len; i++)
    ring_emergency_service_free(g_ptr_array_index(list, i));
  g_ptr_array_free(list, TRUE);
}

RingEmergencyServiceInfoList *
ring_emergency_service_info_list_default(char const * const * numbers)
{
  return ring_emergency_service_info_list_new(
    ring_emergency_service_info_new(RING_EMERGENCY_SERVICE_URN,
      numbers),
    NULL);
}

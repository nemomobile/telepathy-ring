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

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#include <string.h>

/* Return a pointer to a boxed service struct */
RingEmergencyService *
ring_emergency_service_new(char const *service,
  guint handle,
  char const * const *aliases)
{
  GValue value[1] = {{ 0 }};
  GType gtype = RING_TYPE_EMERGENCY_SERVICE;

  g_value_init(value, gtype);
  g_value_take_boxed(value, dbus_g_type_specialized_construct(gtype));
  dbus_g_type_struct_set(value,
    0, service,
    1, handle,
    2, aliases,
    G_MAXUINT);

  return (RingEmergencyService *)g_value_get_boxed(value);
}

void
ring_emergency_service_free(RingEmergencyService *service)
{
  g_boxed_free(RING_TYPE_EMERGENCY_SERVICE, (gpointer)service);
}

RingEmergencyServiceList *
ring_emergency_service_list_new(RingEmergencyService *service,
  ...)
{
  RingEmergencyServiceList *self = g_ptr_array_sized_new(1);
  va_list ap;

  for (va_start(ap, service); service; service = va_arg(ap, gpointer)) {
    g_ptr_array_add(self, service);
  }
  va_end(ap);

  return self;
}

void
ring_emergency_service_list_free(RingEmergencyServiceList *list)
{
  guint i;
  for (i = 0; i < list->len; i++)
    ring_emergency_service_free(g_ptr_array_index(list, i));
  g_ptr_array_free(list, TRUE);
}

RingEmergencyServiceList *
ring_emergency_service_list_default(guint sos_handle,
  char const * const * numbers)
{
  return ring_emergency_service_list_new(
    ring_emergency_service_new(RING_EMERGENCY_SERVICE_URN,
      sos_handle, numbers),
    NULL);
}

/*
 * modem/service.h - Interface towards Ofono
 *
 * Copyright (C) 2009 Nokia Corporation
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

#ifndef _MODEM_SERVICE_H_
#define _MODEM_SERVICE_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ModemService ModemService;
typedef struct _ModemServiceClass ModemServiceClass;
typedef struct _ModemServicePrivate ModemServicePrivate;

struct _ModemServiceClass {
  GObjectClass parent_class;
};

struct _ModemService {
  GObject parent;
  ModemServicePrivate *priv;
};

GType modem_service_get_type(void);

/* TYPE MACROS */
#define MODEM_TYPE_SERVICE                      \
  (modem_service_get_type())
#define MODEM_SERVICE(obj)                                              \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MODEM_TYPE_SERVICE, ModemService))
#define MODEM_SERVICE_CLASS(klass)                                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), MODEM_TYPE_SERVICE, ModemServiceClass))
#define MODEM_IS_SERVICE(obj)                                   \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MODEM_TYPE_SERVICE))
#define MODEM_IS_SERVICE_CLASS(klass)                           \
  (G_TYPE_CHECK_CLASS_TYPE((klass), MODEM_TYPE_SERVICE))
#define MODEM_SERVICE_GET_CLASS(obj)                                    \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MODEM_TYPE_SERVICE, ModemServiceClass))

/* ---------------------------------------------------------------------- */

gboolean modem_service_connect(ModemService *self);
gboolean modem_service_is_connected(ModemService *self);
gboolean modem_service_is_connecting(ModemService *self);
void modem_service_disconnect(ModemService *self);

char const *modem_service_get_modem_path(ModemService *self);

gboolean modem_service_supports_call(ModemService *self);

G_END_DECLS

#endif /* #ifndef _MODEM_SERVICE_H_*/

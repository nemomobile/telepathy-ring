/*
 * modem/modem.h - Interface towards oFono modem instance
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

#ifndef _MODEM_MODEM_H_
#define _MODEM_MODEM_H_

#include <glib-object.h>
#include <modem/request.h>

G_BEGIN_DECLS

typedef struct _Modem Modem;
typedef struct _ModemClass ModemClass;
typedef struct _ModemPrivate ModemPrivate;

struct _ModemClass {
  GObjectClass parent_class;
};

struct _Modem {
  GObject parent;
  ModemPrivate *priv;
};

GType modem_get_type (void);

/* TYPE MACROS */
#define MODEM_TYPE_MODEM (modem_get_type ())
#define MODEM_MODEM(obj)                                               \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MODEM_TYPE_MODEM, Modem))
#define MODEM_CLASS_MODEM(klass)                                       \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MODEM_TYPE_MODEM, ModemClass))
#define MODEM_IS_MODEM(obj)                                            \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MODEM_TYPE_MODEM))
#define MODEM_IS_MODEM_CLASS(klass)                                    \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MODEM_TYPE_MODEM))
#define MODEM_GET_MODEM_CLASS(obj)                                     \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MODEM_TYPE_MODEM, ModemClass))

/* ---------------------------------------------------------------------- */

char const *modem_property_name_by_ofono_name (char const *name);

char const *modem_get_modem_path (Modem const *self);

gboolean modem_is_powered (Modem const *self);
gboolean modem_is_online (Modem const *self);
gboolean modem_has_interface (Modem const *self, char const *interface);
gboolean modem_supports_sim (Modem const *self);
gboolean modem_supports_call (Modem const *self);
gboolean modem_supports_sms (Modem const *self);

G_END_DECLS

#endif /* #ifndef _MODEM_MODEM_H_*/

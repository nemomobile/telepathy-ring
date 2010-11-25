/*
 * modem/oface.h - Parent class for modem services
 *
 * Copyright (C) 2008,2010 Nokia Corporation
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

#ifndef _MODEM_OFACE_H_
#define _MODEM_OFACE_H_

#include <glib-object.h>
#include <modem/request.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

typedef struct _ModemOface ModemOface;
typedef struct _ModemOfaceClass ModemOfaceClass;
typedef struct _ModemOfacePrivate ModemOfacePrivate;

struct _ModemOfaceClass
{
  GObjectClass parent_class;

  char const *ofono_interface;
  /** Called when start connecting */
  void (*connect)(ModemOface *);
  /** Called when got connected */
  void (*connected)(ModemOface *);
  /** Called when disconnecting */
  void (*disconnect)(ModemOface *);

  char const *(*property_mapper)(char const *ofono_property);
};

struct _ModemOface
{
  GObject parent;
  ModemOfacePrivate *priv;
};

GType modem_oface_get_type (void);

/* TYPE MACROS */
#define MODEM_TYPE_OFACE (modem_oface_get_type ())
#define MODEM_OFACE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MODEM_TYPE_OFACE, ModemOface))
#define MODEM_OFACE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MODEM_TYPE_OFACE, ModemOfaceClass))
#define MODEM_IS_OFACE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MODEM_TYPE_OFACE))
#define MODEM_IS_OFACE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MODEM_TYPE_OFACE))
#define MODEM_OFACE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MODEM_TYPE_OFACE, ModemOfaceClass))

/* ---------------------------------------------------------------------- */

typedef void ModemOfacePropertiesReply (ModemOface *, ModemRequest *,
    GHashTable *properties, GError const *error,
    gpointer user_data);

typedef void ModemOfaceManagedReply (ModemOface *, ModemRequest *,
    GPtrArray *managed, GError const *error,
    gpointer user_data);

typedef void ModemOfaceVoidReply (ModemOface *, ModemRequest *request,
    GError const *error,
    gpointer user_data);

/* ---------------------------------------------------------------------- */

void modem_oface_register_type (GType type);

char const *modem_oface_get_interface_name_by_type (GType type);

GType modem_oface_get_type_by_interface_name (char const *interface);

ModemOface *modem_oface_new (char const *interface, char const *object_path);

gboolean modem_oface_connect (ModemOface *);
void modem_oface_add_connect_request (ModemOface *, ModemRequest *);
void modem_oface_check_connected (ModemOface *, ModemRequest *, GError const *);
void modem_oface_set_connecting_error (ModemOface *, GError const *);

void modem_oface_connect_properties (ModemOface *, gboolean get_all);
void modem_oface_disconnect_properties (ModemOface *);

gboolean modem_oface_is_connecting (ModemOface const *self);
gboolean modem_oface_is_connected (ModemOface const *self);
void modem_oface_disconnect (ModemOface *self);

ModemRequest *modem_oface_set_property_req (ModemOface *,
  char const *property, GValue *value,
  ModemOfaceVoidReply *callback, gpointer user_data);

ModemRequest *modem_oface_request_properties (ModemOface *,
    ModemOfacePropertiesReply *callback,
    gpointer user_data);

void modem_oface_update_properties (ModemOface *,
    GHashTable *properties);

ModemRequest *modem_oface_request_managed (ModemOface *oface,
    char const *method,
    ModemOfaceManagedReply *callback,
    gpointer userdata);

DBusGProxy *modem_oface_dbus_proxy (ModemOface *);
char const *modem_oface_object_path (ModemOface *self);
char const *modem_oface_interface (ModemOface *self);

G_END_DECLS

#endif /* #ifndef _MODEM_OFACE_H_*/

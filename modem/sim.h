/*
 * modem/sim.h - Client for Modem SIM service
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

#ifndef _MODEM_SIM_SERVICE_H_
#define _MODEM_SIM_SERVICE_H_

#include <glib-object.h>
#include <modem/request.h>

G_BEGIN_DECLS

typedef struct _ModemSIMService ModemSIMService;
typedef struct _ModemSIMServiceClass ModemSIMServiceClass;
typedef struct _ModemSIMServicePrivate ModemSIMServicePrivate;
typedef enum _ModemSIMState ModemSIMState;

struct _ModemSIMServiceClass
{
  GObjectClass parent_class;
};

struct _ModemSIMService
{
  GObject parent;
  ModemSIMServicePrivate *priv;
};

GType modem_sim_service_get_type (void);

/* TYPE MACROS */
#define MODEM_TYPE_SIM_SERVICE                  \
  (modem_sim_service_get_type ())
#define MODEM_SIM_SERVICE(obj)                  \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      MODEM_TYPE_SIM_SERVICE, ModemSIMService))
#define MODEM_SIM_SERVICE_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                    \
      MODEM_TYPE_SIM_SERVICE, ModemSIMServiceClass))
#define MODEM_IS_SIM_SERVICE(obj)                               \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MODEM_TYPE_SIM_SERVICE))
#define MODEM_IS_SIM_SERVICE_CLASS(klass)                       \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MODEM_TYPE_SIM_SERVICE))
#define MODEM_SIM_SERVICE_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                    \
      MODEM_TYPE_SIM_SERVICE, ModemSIMServiceClass))

enum _ModemSIMState
{
  MODEM_SIM_STATE_UNKNOWN     = 0,
  MODEM_SIM_STATE_OK,
  MODEM_SIM_STATE_NO_SIM,
  MODEM_SIM_STATE_REMOVED,
  MODEM_SIM_STATE_PERMANENTLY_BLOCKED,
  MODEM_SIM_STATE_NOT_READY,
  MODEM_SIM_STATE_PIN_REQUIRED,
  MODEM_SIM_STATE_PUK_REQUIRED,
  MODEM_SIM_STATE_SIMLOCK_REJECTED,
  MODEM_SIM_STATE_REJECTED,
  LAST_MODEM_SIM_STATE
};

/* ---------------------------------------------------------------------- */

gboolean modem_sim_service_connect (ModemSIMService *self);
gboolean modem_sim_service_is_connected (ModemSIMService *self);
gboolean modem_sim_service_is_connecting (ModemSIMService *self);
void modem_sim_service_disconnect (ModemSIMService *self);

typedef void ModemSIMStringReply (ModemSIMService *self,
    ModemRequest *request,
    char *reply,
    GError *error,
    gpointer user_data);

typedef void ModemSIMUnsignedReply (ModemSIMService *self,
    ModemRequest *request,
    guint reply,
    GError *error,
    gpointer user_data);

char const *modem_sim_get_imsi (ModemSIMService const *self);

ModemSIMState modem_sim_get_state (ModemSIMService const *self);

G_END_DECLS

#endif /* #ifndef _MODEM_SIM_SERVICE_H_*/

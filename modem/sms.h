/*
 * modem/sms.h - Client for Modem SMS Service
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

#ifndef _MODEM_SMS_SERVICE_H_
#define _MODEM_SMS_SERVICE_H_

#include <glib-object.h>

#include <sms-glib/deliver.h>
#include <sms-glib/status-report.h>
#include <sms-glib/submit.h>
#include <modem/request.h>
#include <modem/oface.h>

G_BEGIN_DECLS

typedef struct _ModemSMSService ModemSMSService;
typedef struct _ModemSMSServiceClass ModemSMSServiceClass;
typedef struct _ModemSMSServicePrivate ModemSMSServicePrivate;

struct _ModemSMSServiceClass
{
  ModemOfaceClass parent_class;
};

struct _ModemSMSService
{
  ModemOface parent;
  ModemSMSServicePrivate *priv;
};

GType modem_sms_service_get_type (void);

/* TYPE MACROS */
#define MODEM_TYPE_SMS_SERVICE \
  (modem_sms_service_get_type ())
#define MODEM_SMS_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MODEM_TYPE_SMS_SERVICE, ModemSMSService))
#define MODEM_SMS_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MODEM_TYPE_SMS_SERVICE, ModemSMSServiceClass))
#define MODEM_IS_SMS_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MODEM_TYPE_SMS_SERVICE))
#define MODEM_IS_SMS_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MODEM_TYPE_SMS_SERVICE))
#define MODEM_SMS_SERVICE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MODEM_TYPE_SMS_SERVICE, ModemSMSServiceClass))

/* ---------------------------------------------------------------------- */

#define MODEM_OFACE_SMS "org.ofono.MessageManager"

typedef void ModemSMSConnectedHandler (ModemSMSService *, gpointer);
typedef void ModemSMSDeliverHandler (ModemSMSService *,
    SMSGDeliver *, gpointer);

typedef void ModemSMSServiceReply (ModemSMSService *self,
  ModemRequest *request,
  GError const *error,
  gpointer user_data);

typedef void ModemSMSServiceSendReply (ModemSMSService *self,
  ModemRequest *request,
  char const *message_id,
  GError const *error,
  gpointer user_data);

/* ---------------------------------------------------------------------- */

char const *modem_sms_service_property_name_by_ofono_name (char const *);

gulong modem_sms_connect_to_connected (ModemSMSService *self,
  ModemSMSConnectedHandler *user_function,
  gpointer user_data);

gulong modem_sms_connect_to_deliver (ModemSMSService *self,
  ModemSMSDeliverHandler *user_function,
  gpointer user_data);

guint64 modem_sms_service_time_connected (ModemSMSService const *self);

/* ---------------------------------------------------------------------- */

ModemRequest *modem_sms_set_sc_address (ModemSMSService *self,
    char const *address,
    ModemSMSServiceReply *reply,
    gpointer user_data);

ModemRequest *modem_sms_set_srr (ModemSMSService *self,
    gboolean srr,
    ModemSMSServiceReply *reply,
    gpointer user_data);

ModemRequest *modem_sms_request_send (ModemSMSService *self,
  char const *to, char const *message,
  ModemSMSServiceSendReply *reply,
  gpointer user_data);

/* ---------------------------------------------------------------------- */

gboolean modem_sms_is_valid_address (gchar const *address);
gboolean modem_sms_validate_address (gchar const *address, GError **error);

G_END_DECLS

#endif /* #ifndef _MODEM_SMS_SERVICE_H_*/

/*
 * modem/sms-message.h - ModemSMSMessage class
 *
 * Copyright (C) 2013 Jolla Ltd
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

#ifndef _MODEM_SMS_MESSAGE_H_
#define _MODEM_SMS_MESSAGE_H_

#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <modem/sms.h>

G_BEGIN_DECLS

typedef struct _ModemSMSMessage ModemSMSMessage;
typedef struct _ModemSMSMessageClass ModemSMSMessageClass;

struct _ModemSMSMessageClass
{
  GObjectClass parent_class;
};

struct _ModemSMSMessage
{
  GObject parent;

  /* Properties */
  char *destination;
  char *message_token;
  ModemSMSService *message_service;
  gboolean status_report_requested;

  /* Internal */
  DBusGProxy *message_proxy;

};


GType modem_sms_message_get_type (void);

/* TYPE MACROS */
#define MODEM_TYPE_SMS_MESSAGE (modem_sms_message_get_type ())
#define MODEM_SMS_MESSAGE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
		MODEM_TYPE_SMS_MESSAGE, ModemSMSMessage))


G_END_DECLS

#endif /* #ifndef _MODEM_SMS_MESSAGE_H_*/

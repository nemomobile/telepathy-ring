/*
 * sms-glib/message.h - SMS message interface
 *
 * Copyright (C) 2008-2010 Nokia Corporation
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

#ifndef __SMS_G_MESSAGE_H__
#define __SMS_G_MESSAGE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _SMSGMessage SMSGMessage;
typedef struct _SMSGMessageIface SMSGMessageIface;

GType sms_g_message_get_type(void);

/* TYPE MACROS */
#define SMS_G_TYPE_MESSAGE                      \
  (sms_g_message_get_type())
#define SMS_G_MESSAGE(obj)                                              \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), SMS_G_TYPE_MESSAGE, SMSGMessage))
#define SMS_G_IS_MESSAGE(obj)                                   \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), SMS_G_TYPE_MESSAGE))
#define SMS_G_MESSAGE_GET_INTERFACE(obj)                                \
  (G_TYPE_INSTANCE_GET_INTERFACE((obj), SMS_G_TYPE_MESSAGE, SMSGMessageIface))

struct _SMSGMessageIface {
  GTypeInterface parent;
};

enum {
  /* Mobile-terminated */
  SMS_MESSAGE_TYPE_DELIVER = 0,
  SMS_MESSAGE_TYPE_SUBMIT_REPORT = 1,
  SMS_MESSAGE_TYPE_STATUS_REPORT = 2,

  /* Mobile-originated */
  SMS_MESSAGE_TYPE_DELIVER_REPORT = 0,
  SMS_MESSAGE_TYPE_SUBMIT = 1,
  SMS_MESSAGE_TYPE_COMMAND = 2
};

/* GByteArray */
#define SMS_G_TYPE_BYTE_ARRAY sms_g_byte_array_get_type()

GType sms_g_byte_array_get_type(void);

G_END_DECLS

#endif /* #ifndef __SMS_G_MESSAGE_H__*/

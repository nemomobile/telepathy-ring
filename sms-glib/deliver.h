/*
 * sms-deliver.h - wrapper class for SMS DELIVER messages
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

#ifndef __SMS_G_DELIVER_H__
#define __SMS_G_DELIVER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _SMSGDeliver SMSGDeliver;
typedef struct _SMSGDeliverClass SMSGDeliverClass;
typedef struct _SMSGDeliverPrivate SMSGDeliverPrivate;

struct _SMSGDeliverClass {
  GObjectClass parent_class;
};

struct _SMSGDeliver {
  GObject parent;
  SMSGDeliverPrivate *priv;
};

GType sms_g_deliver_get_type(void);

/* TYPE MACROS */
#define SMS_G_TYPE_DELIVER                      \
  (sms_g_deliver_get_type())
#define SMS_G_DELIVER(obj)                                              \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), SMS_G_TYPE_DELIVER, SMSGDeliver))
#define SMS_G_DELIVER_CLASS(klass)                                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), SMS_G_TYPE_DELIVER, SMSGDeliverClass))
#define SMS_G_IS_DELIVER(obj)                                   \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), SMS_G_TYPE_DELIVER))
#define SMS_G_IS_DELIVER_CLASS(klass)                           \
  (G_TYPE_CHECK_CLASS_TYPE((klass), SMS_G_TYPE_DELIVER))
#define SMS_G_DELIVER_GET_CLASS(obj)                                    \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SMS_G_TYPE_DELIVER, SMSGDeliverClass))

/* SMSGDeliver interface */

gchar const *sms_g_deliver_get_content_type(SMSGDeliver const *self);

gboolean sms_g_deliver_is_type(SMSGDeliver const *self, gchar const *type);

gboolean sms_g_deliver_is_text(SMSGDeliver const *self); /* text/plain */
gboolean sms_g_deliver_is_vcard(SMSGDeliver const *self); /* text/x-vcard */
gboolean sms_g_deliver_is_vcalendar(SMSGDeliver const *self); /* text/x-vcalendar */

char const *sms_g_deliver_get_smsc(SMSGDeliver const *self);
char const *sms_g_deliver_get_text(SMSGDeliver const *self);
int sms_g_deliver_get_sms_class(SMSGDeliver const *self);
time_t sms_g_deliver_get_timestamp(SMSGDeliver const *self);
time_t sms_g_deliver_get_received(SMSGDeliver const *self);
time_t sms_g_deliver_get_delivered(SMSGDeliver const *self);
char const *sms_g_deliver_get_originator(SMSGDeliver const *self);
GArray const *sms_g_deliver_get_binary(SMSGDeliver const *self);
char const *sms_g_deliver_get_message_token(SMSGDeliver const *self);

SMSGDeliver *sms_g_deliver_incoming(gchar const *message,
  gchar const *message_token,
  gchar const *originator,
  gchar const *smsc,
  gchar const *content_type,
  gchar const *mwi_type,
  GError **return_error);

void sms_g_deliver_report(SMSGDeliver *self);

G_END_DECLS

#endif /* #ifndef __SMS_G_DELIVER_H__*/

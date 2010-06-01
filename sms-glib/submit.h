/*
 * sms-glib/submit.h - SMS Submit
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

#ifndef __SMS_G_SUBMIT_H__
#define __SMS_G_SUBMIT_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _SMSGSubmit SMSGSubmit;
typedef struct _SMSGSubmitClass SMSGSubmitClass;
typedef struct _SMSGSubmitPrivate SMSGSubmitPrivate;

struct _SMSGSubmitClass {
  GObjectClass parent_class;
};

struct _SMSGSubmit {
  GObject parent;
  SMSGSubmitPrivate *priv;
};

GType sms_g_submit_get_type(void);

/* TYPE MACROS */
#define SMS_G_TYPE_SUBMIT                       \
  (sms_g_submit_get_type())
#define SMS_G_SUBMIT(obj)                                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), SMS_G_TYPE_SUBMIT, SMSGSubmit))
#define SMS_G_SUBMIT_CLASS(klass)                                       \
  (G_TYPE_CHECK_CLASS_CAST((klass), SMS_G_TYPE_SUBMIT, SMSGSubmitClass))
#define SMS_G_IS_SUBMIT(obj)                                    \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), SMS_G_TYPE_SUBMIT))
#define SMS_G_IS_SUBMIT_CLASS(klass)                    \
  (G_TYPE_CHECK_CLASS_TYPE((klass), SMS_G_TYPE_SUBMIT))
#define SMS_G_SUBMIT_GET_CLASS(obj)                                     \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SMS_G_TYPE_SUBMIT, SMSGSubmitClass))

/* SMSGSubmit interface */

void sms_g_submit_set_status_report_request(SMSGSubmit *self, gboolean);
void sms_g_submit_set_destination(SMSGSubmit *self, gchar const *);
void sms_g_submit_set_smsc(SMSGSubmit *self, gchar const *);
void sms_g_submit_set_sms_class(SMSGSubmit *self, gint);
void sms_g_submit_set_validity_period(SMSGSubmit *self, guint);
void sms_g_submit_set_reduced_charset(SMSGSubmit *self, gboolean);

gboolean sms_g_submit_get_status_report_request(SMSGSubmit *self);
gchar const *sms_g_submit_get_destination(SMSGSubmit const *self);
gchar const *sms_g_submit_get_smsc(SMSGSubmit const *self);
gint sms_g_submit_get_sms_class(SMSGSubmit const *self);
guint sms_g_submit_get_validity_period(SMSGSubmit const *self);
gboolean sms_g_submit_get_reduced_charset(SMSGSubmit const *self);

SMSGSubmit *sms_g_submit_new(void);

SMSGSubmit *sms_g_submit_new_type(char const *content_type);

GPtrArray const *sms_g_submit_text(SMSGSubmit *self,
  gchar const *text,
  GError **gerror);

GPtrArray const *sms_g_submit_binary(SMSGSubmit *self,
  GArray const *binary,
  GError **gerror);

GPtrArray const *sms_g_submit_bytes(SMSGSubmit *self,
  gconstpointer data,
  guint size,
  GError **gerror);

GPtrArray const *sms_g_submit_get_data(SMSGSubmit const *self);

gpointer const *sms_g_submit_get_pdata(SMSGSubmit const *self);
guint sms_g_submit_get_len(SMSGSubmit const *self);

G_END_DECLS

#endif /* #ifndef __SMS_G_SUBMIT_H__*/

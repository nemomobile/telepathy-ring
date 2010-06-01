/*
 * sms-status-report.h - wrapper class for SMS-STATUS-REPORT
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

#ifndef __SMS_G_STATUS_REPORT_H__
#define __SMS_G_STATUS_REPORT_H__

#include <glib-object.h>
#include <time.h>

G_BEGIN_DECLS

typedef struct _SMSGStatusReport SMSGStatusReport;
typedef struct _SMSGStatusReportClass SMSGStatusReportClass;
typedef struct _SMSGStatusReportPrivate SMSGStatusReportPrivate;

struct _SMSGStatusReportClass {
  GObjectClass parent_class;
};

struct _SMSGStatusReport {
  GObject parent;
  SMSGStatusReportPrivate *priv;
};

GType sms_g_status_report_get_type(void);

/* TYPE MACROS */
#define SMS_G_TYPE_STATUS_REPORT                \
  (sms_g_status_report_get_type())
#define SMS_G_STATUS_REPORT(obj)                                        \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), SMS_G_TYPE_STATUS_REPORT, SMSGStatusReport))
#define SMS_G_STATUS_REPORT_CLASS(klass)                                \
  (G_TYPE_CHECK_CLASS_CAST((klass), SMS_G_TYPE_STATUS_REPORT, SMSGStatusReportClass))
#define SMS_G_IS_STATUS_REPORT(obj)                             \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), SMS_G_TYPE_STATUS_REPORT))
#define SMS_G_IS_STATUS_REPORT_CLASS(klass)                     \
  (G_TYPE_CHECK_CLASS_TYPE((klass), SMS_G_TYPE_STATUS_REPORT))
#define SMS_G_STATUS_REPORT_GET_CLASS(obj)                              \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SMS_G_TYPE_STATUS_REPORT, SMSGStatusReportClass))

/* SMSGStatusReport interface */

SMSGStatusReport *sms_g_status_report_incoming(GPtrArray const *tpdus,
  gchar const *smsc,
  gchar const *message_token,
  GError **return_error);

gchar const *sms_g_status_report_get_smsc(SMSGStatusReport const *self);

gboolean sms_g_status_report_get_srq(SMSGStatusReport const *self);
guint8 sms_g_status_report_get_reference(SMSGStatusReport const *self);
gchar const *sms_g_status_report_get_recipient(SMSGStatusReport const *self);
time_t sms_g_status_report_get_timestamp(SMSGStatusReport const *self);
time_t sms_g_status_report_get_discharge(SMSGStatusReport const *self);
time_t sms_g_status_report_get_received(SMSGStatusReport const *self);
time_t sms_g_status_report_get_delivered(SMSGStatusReport const *self);
guint8 sms_g_status_report_get_status(SMSGStatusReport const *self);
char const *sms_g_status_report_get_message_token(SMSGStatusReport const *self);
char const *sms_g_status_report_get_delivery_token(SMSGStatusReport const *self);

gboolean sms_g_status_report_is_status_completed(SMSGStatusReport const *self);
gboolean sms_g_status_report_is_status_still_trying(SMSGStatusReport const *self);
gboolean sms_g_status_report_is_status_permanent(SMSGStatusReport const *self);
gboolean sms_g_status_report_is_status_temporary(SMSGStatusReport const *self);

G_END_DECLS

#endif /* #ifndef __SMS_G_STATUS_REPORT_H__*/

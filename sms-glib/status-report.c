/*
 * sms-glib/status-report.c - SMSGStatusReport class implementation
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

#include "config.h"

#define SMS_G_DEBUG_FLAG SMS_G_DEBUG_STATUS_REPORT

#include "debug.h"

#undef SMS_G_STATUS_REPORT

#include "sms-glib/errors.h"
#include "sms-glib/enums.h"
#include "sms-glib/message.h"
#include "sms-glib/status-report.h"
#include "sms-glib/utils.h"
#include "sms-glib/param-spec.h"

#include <limits.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static void sms_g_status_report_init_message_iface(gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE(
  SMSGStatusReport, sms_g_status_report, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(SMS_G_TYPE_MESSAGE,
    sms_g_status_report_init_message_iface);
  );

/* Properties */
enum
{
  PROP_NONE,
  PROP_MESSAGE_TYPE,
  PROP_MO,
  PROP_CONTENT_TYPE,
  PROP_SRQ,
  PROP_REFERENCE,
  PROP_RECIPIENT,
  PROP_TIME_ORIGINAL,
  PROP_TIME_SENT,
  PROP_TIME_RECEIVED,
  PROP_TIME_DELIVERED,
  PROP_STATUS,
  PROP_SMSC,
  PROP_MESSAGE_TOKEN,
  PROP_DELIVERY_TOKEN,
  LAST_PROPERTY
};

/* private data */
struct _SMSGStatusReportPrivate
{
  gchar *message_token;		/* Message ID */
  gchar *delivery_token;		/* Path of message that report concerns */
  gchar *smsc;
  gchar *recipient;

  gint64 original, discharge, received, delivered;

  guint8 reference, status;

  unsigned srq:1, dispose_has_run:1, :0;
};

/* ---------------------------------------------------------------------- */

/* ---------------------------------------------------------------------- */
/* GObject interface */

static void
sms_g_status_report_init(SMSGStatusReport *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, SMS_G_TYPE_STATUS_REPORT, SMSGStatusReportPrivate);
}

static void
sms_g_status_report_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  SMSGStatusReport *self = SMS_G_STATUS_REPORT(object);
  SMSGStatusReportPrivate *priv = self->priv;

  switch(property_id) {
    case PROP_MESSAGE_TYPE:
      g_value_set_uint(value, SMS_G_TP_MTI_STATUS_REPORT);
      break;

    case PROP_MO:
      g_value_set_boolean(value, FALSE);
      break;

    case PROP_CONTENT_TYPE:
      g_value_set_static_string(value, "sms-status-report");
      break;

    case PROP_SRQ:
      g_value_set_boolean(value, priv->srq);
      break;

    case PROP_REFERENCE:
      g_value_set_uchar(value, priv->reference);
      break;

    case PROP_RECIPIENT:
      g_value_set_string(value, priv->recipient);
      break;

    case PROP_TIME_ORIGINAL:
      g_value_set_int64(value, priv->original);
      break;

    case PROP_TIME_SENT:
      g_value_set_int64(value, priv->discharge);
      break;

    case PROP_TIME_RECEIVED:
      g_value_set_int64(value, priv->received);
      break;

    case PROP_TIME_DELIVERED:
      g_value_set_int64(value, priv->delivered);
      break;

    case PROP_STATUS:
      g_value_set_uchar(value, priv->status);
      break;

    case PROP_SMSC:
      g_value_set_string(value, priv->smsc ? priv->smsc : "");
      break;

    case PROP_MESSAGE_TOKEN:
      g_value_set_string(value, priv->message_token);
      break;

    case PROP_DELIVERY_TOKEN:
      g_value_set_string(value, priv->delivery_token);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
sms_g_status_report_set_property(GObject *object,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  SMSGStatusReport *self = SMS_G_STATUS_REPORT(object);
  SMSGStatusReportPrivate *priv = self->priv;
  gpointer old;

  switch(property_id) {
    case PROP_CONTENT_TYPE:
      /* Writeable in interface but not meaningfully writeable here */
      break;

    case PROP_SMSC:
      priv->smsc = g_value_dup_string(value);
      break;

    case PROP_SRQ:
      priv->srq = g_value_get_boolean(value);
      break;

    case PROP_REFERENCE:
      priv->reference = g_value_get_uchar(value);
      break;

    case PROP_RECIPIENT:
      priv->recipient = g_value_dup_string(value);
      break;

    case PROP_TIME_ORIGINAL:
      priv->original = g_value_get_int64(value);
      break;

    case PROP_TIME_SENT:
      priv->discharge = g_value_get_int64(value);
      break;

    case PROP_TIME_RECEIVED:
      priv->received = g_value_get_int64(value);
      break;

    case PROP_TIME_DELIVERED:
      priv->delivered = g_value_get_int64(value);
      break;

    case PROP_STATUS:
      priv->status = g_value_get_uchar(value);
      break;

    case PROP_MESSAGE_TOKEN:
      priv->message_token = g_value_dup_string(value);
      break;

    case PROP_DELIVERY_TOKEN:
      old = priv->delivery_token;
      priv->delivery_token = g_value_dup_string(value);
      if (old) g_free(old);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}


static void
sms_g_status_report_dispose(GObject *object)
{
  SMSGStatusReport *self = SMS_G_STATUS_REPORT(object);
  SMSGStatusReportPrivate *priv = self->priv;

  DEBUG("SMSGStatusReport: enter: %s", priv->dispose_has_run ? "already" : "disposing");

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;

  if (G_OBJECT_CLASS(sms_g_status_report_parent_class)->dispose)
    G_OBJECT_CLASS(sms_g_status_report_parent_class)->dispose(object);
}


static void
sms_g_status_report_finalize(GObject *object)
{
  SMSGStatusReport *self = SMS_G_STATUS_REPORT(object);
  SMSGStatusReportPrivate *priv = self->priv;

  DEBUG("SMSGStatusReport: enter");

  g_free(priv->message_token), priv->message_token = NULL;
  g_free(priv->delivery_token), priv->delivery_token = NULL;
  g_free(priv->smsc), priv->smsc = NULL;
  g_free(priv->recipient), priv->recipient = NULL;

  G_OBJECT_CLASS(sms_g_status_report_parent_class)->finalize(object);
}


static void
sms_g_status_report_class_init(SMSGStatusReportClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  DEBUG("enter");

  g_type_class_add_private(klass, sizeof (SMSGStatusReportPrivate));

  object_class->get_property = sms_g_status_report_get_property;
  object_class->set_property = sms_g_status_report_set_property;
  object_class->dispose = sms_g_status_report_dispose;
  object_class->finalize = sms_g_status_report_finalize;

  /* Properties */
  g_object_class_override_property(
    object_class, PROP_MESSAGE_TYPE, "message-type");
  g_object_class_override_property(
    object_class, PROP_MO, "mobile-originated");
  g_object_class_override_property(
    object_class, PROP_CONTENT_TYPE, "content-type");

  g_object_class_install_property(
    object_class, PROP_SRQ,
    g_param_spec_boolean("srq",
      "Status Report Qualifier",
      "True if this STATUS-REPORT is sent in reponse to SMS-COMMAND",
      FALSE, /* default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_REFERENCE,
    g_param_spec_uchar("reference",
      "Message Reference",
      "Message rerefence identifying original message",
      0, 255, 0, /* min, max, default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_RECIPIENT,
    g_param_spec_string("recipient",
      "SMS Recipient Address",
      "Address for SMS recipient",
      NULL, /* default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_TIME_ORIGINAL,
    sms_g_param_spec_time_original(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_TIME_SENT,
    sms_g_param_spec_time_sent(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_TIME_RECEIVED,
    sms_g_param_spec_time_received(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_TIME_DELIVERED,
    sms_g_param_spec_time_delivered(G_PARAM_READWRITE));

  g_object_class_install_property(
    object_class, PROP_STATUS,
    g_param_spec_uchar("status",
      "Status of previously sent SMS",
      "TP-FailureCauseStatus indicating status of "
      "previously sent Short Message",
      0, 255, 0, /* min, max, default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_SMSC,
    sms_g_param_spec_smsc(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_MESSAGE_TOKEN,
    sms_g_param_spec_message_token(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_DELIVERY_TOKEN,
    g_param_spec_string("delivery-token",
      "Report Path",
      "Unique identifier of message which status is reported",
      "", /* default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS));

  DEBUG("return");
}

/* --------------------------------------------------------------------------------- */
/* sms_g_message interface */

static void
sms_g_status_report_init_message_iface(gpointer ifacep,
  gpointer data)
{
  (void)ifacep, (void)data;
}

/* --------------------------------------------------------------------------------- */
/* sms_g_status_report interface */

SMSGStatusReport *
sms_g_status_report_incoming(GPtrArray const *tpdus,
  gchar const *smsc,
  gchar const *message_token,
  GError **return_error)
{
  SMSGStatusReport *object;
  gint64 t_received;

  if (!sms_g_validate_sms_address(smsc, return_error)) {
    DEBUG("invalid SMSC number (%s)", smsc);
    return NULL;
  }

  if (!sms_g_validate_message_id(message_token, return_error)) {
    DEBUG("invalid message id (%s)", message_token);
    return NULL;
  }

  t_received = sms_g_received_timestamp();

#if nomore
  DEBUG("SMS-STATUS-REPORT from %s\n"
    "\tsrq = %u\n"
    "\treference = %u\n"
    "\trecipient = %s\n"
    "\ttimestamp = %lld (zone=%d)\n"
    "\tdischarge = %lld\n"
    "\treceived = %lld\n"
    "\tstatus = %u\n"
    "\tmessage_id = %s\n",
    smsc,
    tpdu->status_report,
    tpdu->msg_reference,
    tpdu->addr_value.str,
    t_scts, tpdu->smsc_time.offset,
    t_discharge,
    t_received,
    tpdu->status,
    message_token);
#endif

  object = (SMSGStatusReport *)
    g_object_new(SMS_G_TYPE_STATUS_REPORT,
      "service-centre", smsc,
      /*"time-original", t_scts,
      "time-sent", t_discharge,*/
      "time-received", t_received,
      "message-token", message_token,
      NULL);

  return object;
}

char const *sms_g_status_report_get_smsc(SMSGStatusReport const *self)
{
  if (SMS_G_IS_STATUS_REPORT(self) && self->priv->smsc)
    return self->priv->smsc;
  else
    return "";
}

gboolean sms_g_status_report_get_srq(SMSGStatusReport const *self)
{
  if (SMS_G_IS_STATUS_REPORT(self))
    return self->priv->srq;
  else
    return FALSE;
}

guint8 sms_g_status_report_get_reference(SMSGStatusReport const *self)
{
  if (SMS_G_IS_STATUS_REPORT(self))
    return self->priv->reference;
  else
    return FALSE;
}

char const *sms_g_status_report_get_recipient(SMSGStatusReport const *self)
{
  if (SMS_G_IS_STATUS_REPORT(self) && self->priv->recipient)
    return self->priv->recipient;
  else
    return "";
}

guint8 sms_g_status_report_get_status(SMSGStatusReport const *self)
{
  if (SMS_G_IS_STATUS_REPORT(self))
    return self->priv->status;
  else
    return SMS_G_TP_FCS_UNSPECIFIED;
}

/** Return "message-token" property.
 *
 * The "message-token" is the message-id created for incoming SMS-STATUS-REPORT by
 * libsms.
 */
char const *sms_g_status_report_get_message_token(SMSGStatusReport const *self)
{
  if (SMS_G_IS_STATUS_REPORT(self))
    return self->priv->message_token;
  else
    return "";
}

/** Return "delivery-token" property. */
char const *sms_g_status_report_get_delivery_token(SMSGStatusReport const *self)
{
  if (SMS_G_IS_STATUS_REPORT(self))
    return self->priv->delivery_token;
  else
    return "";
}

/** Return true if TP-Status indicates transaction completed. */
gboolean sms_g_status_report_is_status_completed(SMSGStatusReport const *self)
{
  return SMS_G_IS_STATUS_REPORT(self) && (self->priv->status & 0xe0) == 0x00;
}

/** Return true if TP-Status indicates that SMSC still tries to send message. */
gboolean sms_g_status_report_is_status_still_trying(SMSGStatusReport const *self)
{
  return SMS_G_IS_STATUS_REPORT(self) && (self->priv->status & 0xe0) == 0x20;
}

/** Return true if TP-Status is permanent. */
gboolean sms_g_status_report_is_status_permanent(SMSGStatusReport const *self)
{
  return SMS_G_IS_STATUS_REPORT(self) && (self->priv->status & 0xe0) == 0x40;
}

/** Return true if TP-Status indicates temporary error. */
gboolean sms_g_status_report_is_status_temporary(SMSGStatusReport const *self)
{
  return SMS_G_IS_STATUS_REPORT(self) && (self->priv->status & 0xe0) == 0x60;
}

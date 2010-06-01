/*
 * sms-glib/submit.c - SMSGSubmit class implementation
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

#define SMS_G_DEBUG_FLAG SMS_G_DEBUG_SUBMIT

#include "debug.h"

#include "sms-glib/errors.h"
#include "sms-glib/enums.h"
#include "sms-glib/message.h"
#include "sms-glib/submit.h"
#include "sms-glib/param-spec.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

G_DEFINE_TYPE_WITH_CODE(
  SMSGSubmit, sms_g_submit, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(SMS_G_TYPE_MESSAGE, NULL);
  );

/* Properties */
enum {
  PROP_NONE,
  PROP_MESSAGE_TYPE,
  PROP_MO,
  PROP_CONTENT_TYPE,
  PROP_DESTINATION,
  PROP_SMSC,
  PROP_CLASS,
  PROP_STATUS_REPORT_REQUEST,
  PROP_VALIDITY_PERIOD,
  PROP_REDUCED_CHARSET,
  LAST_PROPERTY
};

/* private data */
struct _SMSGSubmitPrivate
{
  GPtrArray aay[1];
  gpointer ptrarray[256];
  GByteArray ay[256];

  gchar *content_type;
  gchar *destination;
  gchar *smsc;
  guint validity_period;
  unsigned sms_class:5;

  unsigned reduced_charset:1;
  unsigned status_report_request:1;
  unsigned constructed:1;
};

/* ---------------------------------------------------------------------- */

/* ---------------------------------------------------------------------- */
/* GObject interface */

static void
sms_g_submit_init(SMSGSubmit *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, SMS_G_TYPE_SUBMIT, SMSGSubmitPrivate);
}

static void
sms_g_submit_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  SMSGSubmit *self = SMS_G_SUBMIT(object);
  SMSGSubmitPrivate *priv = self->priv;

  switch(property_id) {
    case PROP_MESSAGE_TYPE:
      g_value_set_uint(value, SMS_G_TP_MTI_SUBMIT);
      break;

    case PROP_MO:
      g_value_set_boolean(value, TRUE);
      break;

    case PROP_CONTENT_TYPE:
      g_value_set_string(value, priv->content_type);
      break;

    case PROP_DESTINATION:
      g_value_set_string(value, sms_g_submit_get_destination(self));
      break;

    case PROP_STATUS_REPORT_REQUEST:
      g_value_set_boolean(value, priv->status_report_request);
      break;

    case PROP_SMSC:
      g_value_set_string(value, sms_g_submit_get_smsc(self));
      break;

    case PROP_CLASS:
      g_value_set_int(value, sms_g_submit_get_sms_class(self));
      break;

    case PROP_VALIDITY_PERIOD:
      g_value_set_uint(value, priv->validity_period);
      break;

    case PROP_REDUCED_CHARSET:
      g_value_set_boolean(value, priv->reduced_charset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
sms_g_submit_set_property(GObject *object,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  SMSGSubmit *self = SMS_G_SUBMIT(object);
  SMSGSubmitPrivate *priv = self->priv;

  switch(property_id) {
    case PROP_DESTINATION:
      g_free(priv->destination);
      priv->destination = g_value_dup_string(value);
      return;

    case PROP_CONTENT_TYPE:
      priv->content_type = g_value_dup_string(value);
      break;

    case PROP_STATUS_REPORT_REQUEST:
      priv->status_report_request = g_value_get_boolean(value);
      return;

    case PROP_SMSC:
      g_free(priv->smsc);
      priv->smsc = g_value_dup_string(value);
      break;

    case PROP_CLASS:
      if (g_value_get_int(value) >= 0)
        priv->sms_class = g_value_get_int(value);
      else
        priv->sms_class = 0;
      return;

    case PROP_VALIDITY_PERIOD:
      priv->validity_period = g_value_get_uint(value);
      break;

    case PROP_REDUCED_CHARSET:
      priv->reduced_charset = g_value_get_boolean(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }
}

static void
sms_g_submit_constructed(GObject *object)
{
  if (G_OBJECT_CLASS(sms_g_submit_parent_class)->constructed)
    G_OBJECT_CLASS(sms_g_submit_parent_class)->constructed(object);

  SMS_G_SUBMIT(object)->priv->constructed = 1;
}

static void
sms_g_submit_finalize(GObject *object)
{
  SMSGSubmit *self = SMS_G_SUBMIT(object);
  SMSGSubmitPrivate *priv = self->priv;

  DEBUG("enter");

  g_free(priv->destination);
  g_free(priv->content_type);
  g_free(priv->smsc);

  G_OBJECT_CLASS(sms_g_submit_parent_class)->finalize(object);
}


static void
sms_g_submit_class_init(SMSGSubmitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  DEBUG("enter");

  g_type_class_add_private(klass, sizeof (SMSGSubmitPrivate));

  object_class->get_property = sms_g_submit_get_property;
  object_class->set_property = sms_g_submit_set_property;
  object_class->constructed = sms_g_submit_constructed;
  object_class->finalize = sms_g_submit_finalize;

  /* No Signals */

  /* Properties */
  g_object_class_override_property(
    object_class, PROP_MESSAGE_TYPE, "message-type");

  g_object_class_override_property(
    object_class, PROP_MO, "mobile-originated");

  g_object_class_override_property(
    object_class, PROP_CONTENT_TYPE, "content-type");

  g_object_class_install_property(
    object_class, PROP_DESTINATION,
    g_param_spec_string("destination",
      "SMS Destination Address",
      "Address for SMS destination",
      NULL, /* default value */
      G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_STATUS_REPORT_REQUEST,
    g_param_spec_boolean("status-report-request",
      "Request status report",
      "Request for SMS-STATUS-REPORT in SMS-SUBMIT.",
      FALSE, /* default value */
      G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_SMSC,
    sms_g_param_spec_smsc(G_PARAM_READWRITE));

  g_object_class_install_property(
    object_class, PROP_CLASS,
    sms_g_param_spec_sms_class(G_PARAM_READWRITE));

  g_object_class_install_property(
    object_class, PROP_VALIDITY_PERIOD,
    sms_g_param_spec_validity_period(G_PARAM_READWRITE));

  g_object_class_install_property(
    object_class, PROP_REDUCED_CHARSET,
    sms_g_param_spec_reduced_charset(G_PARAM_READWRITE));

  DEBUG("return");
}

/* --------------------------------------------------------------------------------- */
/* sms_g_submit interface */

gchar const *sms_g_submit_get_destination(SMSGSubmit const *self)
{
  if (SMS_G_IS_SUBMIT(self) && self->priv->destination)
    return  self->priv->destination;
  else
    return "";
}

void sms_g_submit_set_destination(SMSGSubmit *self, gchar const *destination)
{
  g_object_set((GObject *)self, "destination", destination, NULL);
}

char const *sms_g_submit_get_smsc(SMSGSubmit const *self)
{
  if (SMS_G_IS_SUBMIT(self) && self->priv->smsc)
    return self->priv->smsc;
  else
    return "";
}

void sms_g_submit_set_smsc(SMSGSubmit *self, gchar const *smsc)
{
  g_object_set((GObject *)self, "service-centre", smsc, NULL);
}

gint sms_g_submit_get_sms_class(SMSGSubmit const *self)
{
  if (SMS_G_IS_SUBMIT(self) && self->priv->sms_class)
    return self->priv->sms_class & 3;
  else
    return -1;
}

void sms_g_submit_set_sms_class(SMSGSubmit *self, gint sms_class)
{
  g_object_set((GObject *)self, "class", sms_class, NULL);
}

gboolean sms_g_submit_get_status_report_request(SMSGSubmit *self)
{
  if (SMS_G_IS_SUBMIT(self))
    return self->priv->status_report_request;
  else
    return FALSE;
}

void sms_g_submit_set_status_report_request(SMSGSubmit *self, gboolean srr)
{
  g_object_set((GObject *)self, "status-report-request", srr, NULL);
}

guint sms_g_submit_get_validity_period(SMSGSubmit const *self)
{
  if (SMS_G_IS_SUBMIT(self))
    return self->priv->validity_period;
  else
    return 0;
}

void sms_g_submit_set_validity_period(SMSGSubmit *self, guint validity_period)
{
  return g_object_set((GObject *)self, "validity-period", validity_period, NULL);
}

gboolean sms_g_submit_get_reduced_charset(SMSGSubmit const *self)
{
  if (SMS_G_IS_SUBMIT(self))
    return self->priv->reduced_charset;
  else
    return FALSE;
}

void sms_g_submit_set_reduced_charset(SMSGSubmit *self, gboolean reduced_charset)
{
  return g_object_set((GObject *)self, "reduced-charset", reduced_charset, NULL);
}

SMSGSubmit *
sms_g_submit_new(void)
{
  return sms_g_submit_new_type("*/*");
}

SMSGSubmit *
sms_g_submit_new_type(char const *content_type)
{
  return (SMSGSubmit *) g_object_new(SMS_G_TYPE_SUBMIT,
    "content-type", content_type,
    NULL);
}

/* ---------------------------------------------------------------------- */

static
GPtrArray const *
sms_g_submit_any(SMSGSubmit *self,
  gchar const *text,
  GArray const *binary,
  GError **gerror)
{
  SMSGSubmitPrivate *priv;

  if (text == NULL && binary == NULL) {
    g_set_error(gerror, SMS_G_ERRORS, SMS_G_ERROR_INVALID_PARAM,
      "No data to encode");
    return NULL;
  }

  priv = self->priv;

  if (priv->destination == NULL) {
    g_set_error(gerror, SMS_G_ERRORS, SMS_G_ERROR_INVALID_PARAM,
      "No destination address");
    return NULL;
  }

  return priv->aay;
}

GPtrArray const *
sms_g_submit_text(SMSGSubmit *self,
  gchar const *text,
  GError **gerror)
{
  return sms_g_submit_any(self, text, NULL, gerror);
}

GPtrArray const *
sms_g_submit_binary(SMSGSubmit *self,
  GArray const *binary,
  GError **gerror)
{
  return sms_g_submit_any(self, NULL, binary, gerror);
}

GPtrArray const *
sms_g_submit_bytes(SMSGSubmit *self,
  gconstpointer data,
  guint size,
  GError **gerror)

{
  GArray array = { (gpointer)data, size };
  return sms_g_submit_any(self, NULL, &array, gerror);
}

GPtrArray const *
sms_g_submit_get_data(SMSGSubmit const *self)
{
  if (SMS_G_IS_SUBMIT(self) && self->priv->aay->pdata)
    return self->priv->aay;	/* Encoded */
  else
    return NULL;		/* Not encoded */
}

gpointer const *
sms_g_submit_get_pdata(SMSGSubmit const *self)
{
  if (SMS_G_IS_SUBMIT(self) && self->priv->aay->pdata)
    return self->priv->aay->pdata;	/* Encoded */
  else
    return NULL;		/* Not encoded */
}

guint sms_g_submit_get_len(SMSGSubmit const *self)
{
  if (SMS_G_IS_SUBMIT(self) && self->priv->aay->pdata)
    return self->priv->aay->len;
  else
    return 0;
}

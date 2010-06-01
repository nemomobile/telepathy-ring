/*
 * sms-glib/deliver.c - SMSGDeliver class implementation
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

#define SMS_G_DEBUG_FLAG SMS_G_DEBUG_DELIVER

#include "debug.h"

#include "sms-glib/errors.h"
#include "sms-glib/enums.h"
#include "sms-glib/utils.h"
#include "sms-glib/message.h"
#include "sms-glib/deliver.h"
#include "sms-glib/param-spec.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

G_DEFINE_TYPE_WITH_CODE(
  SMSGDeliver, sms_g_deliver, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(SMS_G_TYPE_MESSAGE, NULL));

/* Properties */
enum
{
  PROP_NONE,
  PROP_MESSAGE_TYPE,
  PROP_MO,
  PROP_CONTENT_TYPE,
  PROP_SMSC,
  PROP_MESSAGE_TOKEN,
  PROP_ORIGINATOR,
  PROP_TEXT,
  PROP_BINARY,
  PROP_SMS_CLASS,
  PROP_TIME_SENT,
  PROP_TIME_RECEIVED,
  PROP_TIME_DELIVERED,
  PROP_MWI_TYPE,
  PROP_MWI_ACTIVE,
  PROP_MWI_DISCARD,
  PROP_MWI_LINE,
  PROP_MWI_MESSAGES,
  LAST_PROPERTY
};

/* private data */
struct _SMSGDeliverPrivate
{
  gchar *text;
  gchar *originator;
  gchar *message_token;		/* Message ID */
  gchar *content_type;		/* Content-type */
  gchar *smsc;
  gchar *mwi_type;
  GArray binary[1];

  int sms_class;

  gint64 timestamp, received, delivered;
};

/* ---------------------------------------------------------------------- */
/* GObject interface */

static void
sms_g_deliver_init(SMSGDeliver *self)
{
  DEBUG("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, SMS_G_TYPE_DELIVER, SMSGDeliverPrivate);

  DEBUG("return");
}

static void
sms_g_deliver_set_property(GObject *object,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  SMSGDeliver *self = SMS_G_DELIVER(object);
  SMSGDeliverPrivate *priv = self->priv;

  switch(property_id) {
    case PROP_CONTENT_TYPE:
      priv->content_type = g_value_dup_string(value);
      break;

    case PROP_SMSC:
      priv->smsc = g_value_dup_string(value);
      break;

    case PROP_MESSAGE_TOKEN:
      priv->message_token = g_value_dup_string(value);
      break;

    case PROP_ORIGINATOR:
      priv->originator = g_value_dup_string(value);
      break;

    case PROP_TEXT:
      priv->text = g_value_dup_string(value);
      break;

    case PROP_TIME_RECEIVED:
      priv->received = g_value_get_int64(value);
      break;

    case PROP_TIME_DELIVERED:
      priv->delivered = g_value_get_int64(value);
      break;

    case PROP_MWI_TYPE:
      priv->mwi_type = g_value_dup_string(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
sms_g_deliver_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  SMSGDeliver *self = SMS_G_DELIVER(object);
  SMSGDeliverPrivate *priv = self->priv;

  switch(property_id) {
    case PROP_MESSAGE_TYPE:
      g_value_set_uint(value, SMS_G_TP_MTI_DELIVER);
      break;

    case PROP_MO:
      g_value_set_boolean(value, FALSE);
      break;

    case PROP_CONTENT_TYPE:
      g_value_set_string(value, priv->content_type);
      break;

    case PROP_ORIGINATOR:
      g_value_set_string(value, priv->originator);
      break;

    case PROP_TEXT:
      g_value_set_string(value, priv->text);
      break;

    case PROP_BINARY:
      g_value_set_boxed(value, priv->binary);
      break;

    case PROP_SMSC:
      g_value_set_string(value, priv->smsc ? priv->smsc : "");
      break;

    case PROP_SMS_CLASS:
      g_value_set_int(value, sms_g_deliver_get_sms_class(self));
      break;

    case PROP_MESSAGE_TOKEN:
      g_value_set_string(value, priv->message_token);
      break;

    case PROP_TIME_SENT:
      g_value_set_int64(value, priv->timestamp);
      break;

    case PROP_TIME_RECEIVED:
      g_value_set_int64(value, priv->received);
      break;

    case PROP_TIME_DELIVERED:
      g_value_set_int64(value, priv->delivered);
      break;

    case PROP_MWI_TYPE:
      g_value_set_string(value, priv->mwi_type);
      break;

      /* TODO: */
    case PROP_MWI_ACTIVE:
      g_value_set_boolean(value, FALSE);
      break;

    case PROP_MWI_DISCARD:
      g_value_set_boolean(value, FALSE);
      break;

    case PROP_MWI_LINE:
      g_value_set_uint(value, 0);
      break;

    case PROP_MWI_MESSAGES:
      g_value_set_uint(value, 0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}


static void
sms_g_deliver_finalize(GObject *object)
{
  SMSGDeliver *self = SMS_G_DELIVER(object);
  SMSGDeliverPrivate *priv = self->priv;

  DEBUG("SMSGDeliver: enter");

  g_free(priv->text);
  g_free(priv->originator);
  g_free(priv->message_token);
  g_free(priv->smsc);
  g_free (priv->content_type);
  g_free(priv->mwi_type);

  G_OBJECT_CLASS(sms_g_deliver_parent_class)->finalize(object);
}


static void
sms_g_deliver_class_init(SMSGDeliverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  DEBUG("enter");

  g_type_class_add_private(klass, sizeof (SMSGDeliverPrivate));

  object_class->get_property = sms_g_deliver_get_property;
  object_class->set_property = sms_g_deliver_set_property;
  object_class->finalize = sms_g_deliver_finalize;

  /* Properties */
  g_object_class_override_property(
    object_class, PROP_MESSAGE_TYPE, "message-type");

  g_object_class_override_property(
    object_class, PROP_MO, "mobile-originated");

  g_object_class_override_property(
    object_class, PROP_CONTENT_TYPE, "content-type");

  g_object_class_install_property(
    object_class, PROP_SMSC,
    sms_g_param_spec_smsc(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_MESSAGE_TOKEN,
    sms_g_param_spec_message_token(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_ORIGINATOR,
    g_param_spec_string("originator",
      "SMS Originator Address",
      "ISDN Address for SMS originator",
      NULL, /* default value */
      G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_TEXT,
    g_param_spec_string("text",
      "Text Content",
      "Text content",
      NULL, /* default value */
      G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_BINARY,
    g_param_spec_boxed("binary",
      "Binary content",
      "Binary content",
      SMS_G_TYPE_BYTE_ARRAY, /* GByteArray */
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_SMS_CLASS,
    sms_g_param_spec_sms_class(G_PARAM_READABLE));

  g_object_class_install_property(
    object_class, PROP_TIME_SENT,
    sms_g_param_spec_time_sent(G_PARAM_READABLE));

  g_object_class_install_property(
    object_class, PROP_TIME_RECEIVED,
    sms_g_param_spec_time_received(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property(
    object_class, PROP_TIME_DELIVERED,
    sms_g_param_spec_time_delivered(G_PARAM_READWRITE));

  g_object_class_install_property(
    object_class, PROP_MWI_TYPE,
    g_param_spec_string("mwi-type",
      "Message Waiting Indicator",
      "Type of Message Waiting Indicator",
      NULL, /* default value */
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_MWI_ACTIVE,
    g_param_spec_boolean("mwi-active",
      "Message Waiting Indicator should be Active",
      "When true, activate message waiting indicator for mwi-type, "
      "when false, deactivate message waiting indicator.",
      0, /* default value */
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_MWI_DISCARD,
    g_param_spec_boolean("mwi-discard",
      "Discard Message After Setting MWI",
      "When true, this message can be discarded after mwi has been set. ",
      0, /* default value */
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_MWI_LINE,
    g_param_spec_uint("mwi-line",
      "Message Waiting Indication Line",
      "Line for Message Waiting Indication",
      0, 3, 0, /* default value */
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_MWI_MESSAGES,
    g_param_spec_uint("mwi-messages",
      "Number of Waiting Messages",
      "Number of Waiting Messages",
      0, 255, 0,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));
}

/* --------------------------------------------------------------------------------- */
/* sms_g_deliver interface */

gchar const *
sms_g_deliver_get_content_type(SMSGDeliver const *self)
{
  return SMS_G_IS_DELIVER(self) ? self->priv->content_type : NULL;
}

gboolean
sms_g_deliver_is_type(SMSGDeliver const *self, gchar const *type)
{
  gchar const *self_type = sms_g_deliver_get_content_type(self);

  return self_type && type && g_ascii_strcasecmp(self_type, type) == 0;
}

gboolean sms_g_deliver_is_text(SMSGDeliver const *self)
{
  return sms_g_deliver_is_type(self, "text/plain");
}

gboolean sms_g_deliver_is_vcard(SMSGDeliver const *self)
{
  return sms_g_deliver_is_type(self, "text/x-vcard");
}

gboolean sms_g_deliver_is_vcalendar(SMSGDeliver const *self)
{
  return sms_g_deliver_is_type(self, "text/x-vcalendar");
}

int sms_g_deliver_get_sms_class(SMSGDeliver const *self)
{
  if (SMS_G_IS_DELIVER(self) && self->priv->sms_class)
    return self->priv->sms_class & 3;
  else
    return -1;
}

/**sms_g_deliver_get_timestamp:
 * @self: The SMSGDeliver object
 *
 * Returns: Timestamp from SMS Service Centre.
 */
time_t sms_g_deliver_get_timestamp(SMSGDeliver const *self)
{
  return (time_t)self->priv->timestamp;
}

/**sms_g_deliver_get_received:
 * @self: The SMSGDeliver object
 *
 * Returns: Timestamp when this message was received.
 */
time_t sms_g_deliver_get_received(SMSGDeliver const *self)
{
  return (time_t)self->priv->received;
}

/**sms_g_deliver_get_received:
 * @self: The SMSGDeliver object
 *
 * Returns: Timestamp when this message was delivered.
 */
time_t sms_g_deliver_get_delivered(SMSGDeliver const *self)
{
  return (time_t)self->priv->delivered;
}

/**
 * sms_g_deliver_get_originator:
 * @self: The SMSGDeliver object
 *
 * Returns: Originator address.
 */
char const *sms_g_deliver_get_originator(SMSGDeliver const *self)
{
  return self->priv->originator;
}

char const *sms_g_deliver_get_smsc(SMSGDeliver const *self)
{
  return self->priv->smsc ? self->priv->smsc : "";
}

char const *sms_g_deliver_get_message_token(SMSGDeliver const *self)
{
  return self->priv->message_token;
}

char const *sms_g_deliver_get_text(SMSGDeliver const *self)
{
  return SMS_G_IS_DELIVER(self) ? self->priv->text : NULL;
}

GArray const *sms_g_deliver_get_binary(SMSGDeliver const *self)
{
  return self->priv->binary ? self->priv->binary : NULL;
}

SMSGDeliver *
sms_g_deliver_incoming(gchar const *message,
  gchar const *message_token,
  gchar const *originator,
  gchar const *smsc,
  gchar const *content_type,
  gchar const *mwi_type,
  GError **return_error)
{
  SMSGDeliver *object;

  if (!sms_g_validate_sms_address(smsc, return_error)) {
    DEBUG("invalid SMSC number (%s)", smsc);
    return NULL;
  }

  if (!sms_g_validate_message_id(message_token, return_error)) {
    DEBUG("invalid message id (%s)", message_token);
    return NULL;
  }

  object = g_object_new(SMS_G_TYPE_DELIVER,
           "text", message,
           "originator", originator,
           "service-centre", smsc,
           "message-token", message_token,
           "content-type", content_type,
           "time-received", sms_g_received_timestamp(),
           "mwi-type", mwi_type,
           NULL);

  return object;
}

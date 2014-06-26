/*
 * modem/sms-message.c - ModemSMSMessage class
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

#include "config.h"

#define MODEM_DEBUG_FLAG MODEM_LOG_SMS

#include "debug.h"

#include "modem/sms-message.h"
#include "modem/sms.h"
#include "modem/request-private.h"
#include "modem/errors.h"

#include "modem/ofono.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/errors.h>

#include "signals-marshal.h"

#include <uuid/uuid.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MODEM_OFACE_SMS_MESSAGE "org.ofono.Message"

/* ---------------------------------------------------------------------- */

G_DEFINE_TYPE (ModemSMSMessage, modem_sms_message, G_TYPE_OBJECT);

/* Forward declarations */
static void on_message_property_changed (DBusGProxy *, char const *, GValue *,
  gpointer);

static void
modem_sms_message_init (ModemSMSMessage *self)
{
  self->destination = NULL;
  self->message_token = NULL;
  self->message_proxy = NULL;
  self->message_service = NULL;
  self->status_report_requested = FALSE;
}

/* ------------------------------------------------------------------------- */
enum {
  PROP_0,
  PROP_DESTINATION,
  PROP_TOKEN,
  PROP_PROXY,
  PROP_SERVICE,
  PROP_SRR,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
modem_sms_message_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  ModemSMSMessage *self = MODEM_SMS_MESSAGE (object);

  switch (property_id)
  {
    case PROP_DESTINATION:
      g_value_set_string (value, self->destination);
      break;
    case PROP_TOKEN:
      g_value_set_string (value, self->message_token);
      break;
    case PROP_PROXY:
      g_value_set_pointer (value, self->message_proxy);
      break;
    case PROP_SERVICE:
      g_value_set_pointer (value, self->message_service);
      break;
    case PROP_SRR:
      g_value_set_boolean (value, self->status_report_requested);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_sms_message_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  ModemSMSMessage *self = MODEM_SMS_MESSAGE (object);
  gpointer old;

  switch (property_id)
  {
    case PROP_DESTINATION:
      old = self->destination;
      self->destination = g_value_dup_string (value);
      g_free (old);
      break;
    case PROP_TOKEN:
      old = self->message_token;
      self->message_token = g_value_dup_string (value);
      g_free (old);
      break;
    case PROP_PROXY:
      self->message_proxy = g_value_get_pointer (value);
      break;
    case PROP_SERVICE:
      self->message_service = g_value_get_pointer (value);
      break;
    case PROP_SRR:
      self->status_report_requested = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_sms_message_finalize (GObject *object)
{
  ModemSMSMessage *self = MODEM_SMS_MESSAGE (object);
  g_free (self->destination);
  g_free (self->message_token);
  if (self->message_proxy)
    g_object_unref (self->message_proxy);
  G_OBJECT_CLASS (modem_sms_message_parent_class)->finalize (object);
}

static void
modem_sms_message_constructed (GObject *object)
{
  ModemSMSMessage *self = MODEM_SMS_MESSAGE (object);

  if (!self->destination || !self->message_token
      || !self->message_service)
  {
    DEBUG("This message object needs destination, token and service");
    return;
  }

  /* Listen to this org.ofono.Message object's state changes */
  DBusGConnection *bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
  if (!bus)
    return;
  self->message_proxy = dbus_g_proxy_new_for_name (bus, OFONO_BUS_NAME,
      self->message_token, MODEM_OFACE_SMS_MESSAGE);
  if (!self->message_proxy)
    return;
  dbus_g_proxy_add_signal (self->message_proxy, "PropertyChanged", G_TYPE_STRING,
      G_TYPE_VALUE, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (self->message_proxy, "PropertyChanged",
      G_CALLBACK (on_message_property_changed), self, NULL);

}

static void
modem_sms_message_class_init (ModemSMSMessageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = modem_sms_message_get_property;
  gobject_class->set_property = modem_sms_message_set_property;
  gobject_class->constructed = modem_sms_message_constructed;
  gobject_class->finalize = modem_sms_message_finalize;

  obj_properties[PROP_DESTINATION] =
      g_param_spec_string ("destination",
      "Destination",
      "MO SMS Destination",
      NULL, /* default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT );

  obj_properties[PROP_TOKEN] =
      g_param_spec_string ("message_token",
      "Token",
      "MO SMS Message Token",
      NULL, /* default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT );

  obj_properties[PROP_PROXY] =
      g_param_spec_pointer ("message_proxy",
      "D-Bus proxy",
      "MO SMS D-Bus proxy",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT );

  obj_properties[PROP_SERVICE] =
      g_param_spec_pointer ("message_service",
      "SMS Service",
      "SMS Service",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT );

  obj_properties[PROP_SRR] =
      g_param_spec_boolean ("status_report_requested",
      "SRR",
      "MO SMS Status Report Requested",
      FALSE, /* default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT );

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     obj_properties);

}

static void
on_message_property_changed (DBusGProxy *proxy,
                          char const *property,
                          GValue *value,
                          gpointer user_data)
{
  ModemSMSMessage *self = MODEM_SMS_MESSAGE (user_data);
  if (strcmp (property, "State") == 0){
    const char *state = g_value_get_string (value);
    if (strcmp (state, "sent") == 0){
      modem_sms_emit_outgoing(self->message_service,
          self->destination, dbus_g_proxy_get_path(proxy));
    }
    if (strcmp (state, "failed") == 0){
      GError error = { TP_ERROR, TP_ERROR_NOT_AVAILABLE, "Unspecified SMS error" };
      modem_sms_emit_error(self->message_service,
          self->destination, dbus_g_proxy_get_path(proxy), error);
    }
  }
}


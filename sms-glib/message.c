/*
 * sms-glib/message.c - SMS Message interface
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

#include "sms-glib/message.h"
#include "sms-glib/param-spec.h"
#include "sms-glib/enums.h"

static void
sms_g_message_base_init(gpointer iface)
{
  static gboolean init;

  if (init)
    return;

  init = TRUE;

  g_object_interface_install_property(
    iface, g_param_spec_uint("message-type",
      "SMS Message Type",
      "Message Type for SMS",
      SMS_G_TP_MTI_DELIVER_REPORT, /* min */
      SMS_G_TP_MTI_COMMAND, /* max */
      SMS_G_TP_MTI_SUBMIT, /* default value */
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property(
    iface, g_param_spec_boolean("mobile-originated",
      "SMS Message Originated from Mobile",
      "Message Originated from Mobile",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property(
    iface, g_param_spec_string("content-type",
      "MIME Type",
      "MIME type for message contents",
      "*/*",
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS));
}


GType
sms_g_message_get_type (void)
{
  static GType type;

  if (G_UNLIKELY(type == 0)) {
    static const GTypeInfo info = {
      .class_size = sizeof (SMSGMessageIface),
      .base_init = sms_g_message_base_init,
    };

    type = g_type_register_static (G_TYPE_INTERFACE, "SMSGMessage", &info, 0);
  }

  return type;
}

/* ------------------------------------------------------------------------- */

static gpointer sms_g_byte_array_copy(gpointer boxed_byte_array);
static void sms_g_byte_array_free(gpointer boxed_byte_array);

/** GType for GByteArray */
GType
sms_g_byte_array_get_type (void)
{
  static GType type;

  if (G_UNLIKELY(type == 0)) {
    type = g_boxed_type_register_static("SMSGByteArray",
           sms_g_byte_array_copy,
           sms_g_byte_array_free);
  }

  return type;
}

static gpointer
sms_g_byte_array_copy(gpointer boxed_byte_array)
{
  GByteArray const *s = boxed_byte_array;
  GByteArray *d;

  if (s == NULL)
    return NULL;

  d = g_byte_array_sized_new(s->len);
  g_byte_array_append(d, s->data, s->len);

  return (gpointer)d;
}

static void
sms_g_byte_array_free(gpointer boxed_byte_array)
{
  g_byte_array_free(boxed_byte_array, TRUE);
}

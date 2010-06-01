/*
 * sms-glib/param-spec.c -
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

#include "sms-glib/param-spec.h"
#include "sms-glib/enums.h"

GParamSpec *sms_g_param_spec_smsc(guint flags)
{
  return
    g_param_spec_string("service-centre",
      "SMS Service Centre",
      "ISDN Address for SMS Service Centre",
      "", /* default value */
      flags | G_PARAM_STATIC_STRINGS);
}

GParamSpec *sms_g_param_spec_validity_period(guint flags)
{
  return
    g_param_spec_uint("validity-period",
      "SMS Validity Period",
      "Period while SMS service centre "
      "keep trying to deliver SMS.",
      /* anything above 0 gets rounded up to 5 minutes */
      0,  /* 0 means no validity period */
      63 * 7 * 24 * 60 * 60, /* max - 63 weeks */
      0, /* no validity period - it is up to service centre */
      flags | G_PARAM_STATIC_STRINGS);
}

GParamSpec *sms_g_param_spec_reduced_charset(guint flags)
{
  return
    g_param_spec_boolean("reduced-charset",
      "SMS reduced character set support",
      "Whether SMS should be encoded with "
      "a reduced character set",
      FALSE,
      flags | G_PARAM_STATIC_STRINGS);
}

GParamSpec *sms_g_param_spec_sms_class(guint flags)
{
  return
    g_param_spec_int("class",
      "Short Message Class",
      "Short Message Class (0-3), -1 indicates classless",
      -1 /* min */, 3 /* max */, -1 /* default value */,
      flags | G_PARAM_STATIC_STRINGS);
}

GParamSpec *sms_g_param_spec_message_token(guint flags)
{
  return
    g_param_spec_string("message-token",
      "Message Token",
      "Unique identifier for this object",
      "", /* default value */
      flags | G_PARAM_STATIC_STRINGS);
}

GParamSpec *sms_g_param_spec_time_sent(guint flags)
{
  return
    g_param_spec_int64("time-sent",
      "Time when this message was sent",
      "Timestamp set by Short Message Service Centre",
      G_MININT64, G_MAXINT64, 0, /* min, max, default value */
      flags | G_PARAM_STATIC_STRINGS);
}

GParamSpec *sms_g_param_spec_time_original(guint flags)
{
  return
    g_param_spec_int64("time-original",
      "Timestamp when original sent",
      "Timestamp when the original message was received by server",
      G_MININT64, G_MAXINT64, 0, /* min, max, default value */
      flags | G_PARAM_STATIC_STRINGS);
}

GParamSpec *sms_g_param_spec_time_received(guint flags)
{
  return
    g_param_spec_int64("time-received",
      "Timestamp when received",
      "Timestamp when the message was originally received",
      G_MININT64, G_MAXINT64, 0, /* min, max, default value */
      flags | G_PARAM_STATIC_STRINGS);
}

GParamSpec *sms_g_param_spec_time_delivered(guint flags)
{
  return
    g_param_spec_int64("time-delivered",
      "Timestamp when delivered",
      "Timestamp when the message was originally delivered",
      G_MININT64, G_MAXINT64, 0, /* min, max, default value */
      flags | G_PARAM_STATIC_STRINGS);
}

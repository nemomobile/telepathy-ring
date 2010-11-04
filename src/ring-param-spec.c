/*
 * ring-param-spec.c - Common param specs for object properties
 *
 * Copyright (C) 2007-2010 Nokia Corporation
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
#include "ring-param-spec.h"
#include "ring-connection.h"

GParamSpec *ring_param_spec_imsi(guint flags)
{
  return
    g_param_spec_string("imsi",
      "IMSI",
      "Internation Mobile Subscriber Identifer",
      "", /* default value */
      flags | G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_sms_valid(void)
{
  return
    g_param_spec_uint("sms-validity-period",
      "SMS Validity Period",
      "Period while SMS service centre "
      "keep trying to deliver SMS.",
      /* anything above 0 gets rounded up to 5 minutes */
      0,  /* 0 means no validity period */
      63 * 7 * 24 * 60 * 60, /* max - 63 weeks */
      0,        /* default - set by service centre */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_sms_reduced_charset(void)
{
  return
    g_param_spec_boolean("sms-reduced-charset",
      "SMS reduced character set support",
      "Whether SMSes should be encoded with "
      "a reduced character set",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_smsc(void)
{
  return
    g_param_spec_string("sms-service-centre",
      "SMS Service Centre",
      "ISDN Address for SMS Service Centre",
      "", /* default value */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_connection(void)
{
  return
    g_param_spec_object("connection",
      "Connection object",
      "The connection that owns this object",
      TP_TYPE_BASE_CONNECTION,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_interfaces(void)
{
  return
    g_param_spec_boxed("interfaces",
      "List of extra interfaces",
      "List of extra D-Bus interfaces implemented by this object",
      G_TYPE_STRV,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_handle_id(guint flags)
{
  return g_param_spec_string(
    "handle-id",
    "Target as string",
    "The string that would result from inspecting the TargetHandle property",
    "",
    flags | G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_initiator(guint flags)
{
  return g_param_spec_uint(
    "initiator",
    "Initiator handle",
    "The handle of the contact which"
    "initiated this channel.",
    0, G_MAXUINT32, 0,
    flags | G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_initiator_id(guint flags)
{
  return g_param_spec_string(
    "initiator-id",
    "Initiator as string",
    "The string that would result from inspecting the InitiatorHandle property",
    "",
    flags | G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_requested(guint flags)
{
  g_assert(flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY));
  return g_param_spec_boolean(
    "requested",
    "Requested Channel",
    "True if this channel was created in response to a local request.",
    FALSE,
    flags | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
}

GParamSpec *
ring_param_spec_type_specific_capability_flags(guint flags,
  guint default_value)
{
  g_assert(flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY));
  return g_param_spec_uint(
    "capability-flags",
    "Channel-Type-Specific Capability Flags",
    "Capability flags for the channel type.",
    0, G_MAXUINT32, default_value,
    flags | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
}

GParamSpec *ring_param_spec_anon_modes(void)
{
  return g_param_spec_uint("anon-modes",
    "Anonymity modes",
    "Specifies the active anonymity modes",
    0, G_MAXUINT, 0,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
}

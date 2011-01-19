/*
 * modem/debug.c - Debugging facilities
 *
 * Copyright (C) 2008 Nokia Corporation
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

#include <stdarg.h>
#include <glib.h>

#include "modem/debug.h"

static ModemLogFlags modem_debug_flags = 0;

static const GDebugKey modem_debug_keys[] = {
  { "dbus", MODEM_LOG_DBUS },
  { "modem", MODEM_LOG_MODEM },
  { "call",  MODEM_LOG_CALL },
  { "sms", MODEM_LOG_SMS },
  { "sim", MODEM_LOG_SIM },
  { "audio", MODEM_LOG_AUDIO },
  { "radio", MODEM_LOG_RADIO },
  { "settings", MODEM_LOG_SETTINGS },
  { "gprs", MODEM_LOG_GPRS },
  { "cdma", MODEM_LOG_CDMA },
};

void
modem_debug_set_flags_from_env(void)
{
  const char *flags_string;
  int flags = 0;

  flags_string = g_getenv("MODEM_DEBUG");

  if (flags_string) {
    flags |= g_parse_debug_string(flags_string,
             modem_debug_keys,
             G_N_ELEMENTS(modem_debug_keys));
  }

  flags_string = g_getenv("CALL_DEBUG");

  if (flags_string) {
    flags |= g_parse_debug_string(flags_string,
             modem_debug_keys,
             G_N_ELEMENTS(modem_debug_keys));
  }

  if (flags)
    modem_debug_set_flags(flags);
}

void
modem_debug_set_flags(int new_flags)
{
  modem_debug_flags |= new_flags;
}

gboolean
modem_debug_flag_is_set(int flag)
{
  return (modem_debug_flags & flag) != 0;
}

char const *
modem_debug_domain(int flag)
{
  if (flag & MODEM_LOG_CDMA)
    return "modem-cdma";
  if (flag & MODEM_LOG_CALL)
    return "modem-call";
  if (flag & MODEM_LOG_SMS)
    return "modem-sms";
  if (flag & MODEM_LOG_SIM)
    return "modem-sim";
  if (flag & MODEM_LOG_AUDIO)
    return "modem-audio";
  if (flag & MODEM_LOG_RADIO)
    return "modem-radio";
  if (flag & MODEM_LOG_SETTINGS)
    return "modem-settings";
  if (flag & MODEM_LOG_GPRS)
    return "modem-gprs";

  return "modem";
}


void
modem_debug(int flag, const char *format, ...)
{
  if (flag & modem_debug_flags) {
    va_list args;
    va_start(args, format);
    g_logv(modem_debug_domain(flag), G_LOG_LEVEL_DEBUG, format, args);
    va_end(args);
  }
}

void
modem_message(int flag, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  g_logv(modem_debug_domain(flag), G_LOG_LEVEL_MESSAGE, format, args);
  va_end(args);
}

void
modem_critical(int flag, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  g_logv(modem_debug_domain(flag), G_LOG_LEVEL_WARNING, format, args);
  va_end(args);
}

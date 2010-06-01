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

static ModemDebugFlags modem_debug_flags = 0;

static const GDebugKey modem_debug_keys[] = {
  { "dbus", MODEM_SERVICE_DBUS },
  { "call",  MODEM_SERVICE_CALL },
  { "tones", MODEM_SERVICE_TONES },
  { "sms", MODEM_SERVICE_SMS },
  { "sim", MODEM_SERVICE_SIM },
  { "modem", MODEM_SERVICE_MODEM }
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
  if (flag == MODEM_SERVICE_CALL)
    return "Modem-Call";
  else if (flag == MODEM_SERVICE_TONES)
    return "Modem-Tones";
  else if (flag == MODEM_SERVICE_SMS)
    return "Modem-SMS";
  else if (flag == MODEM_SERVICE_SIM)
    return "Modem-SIM";
  else
    return "Modem";
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

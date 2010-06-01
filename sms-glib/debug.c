/*
 * sms-glib/debug.c - Debugging facilities
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

char const sms_g_log_domain[] = "sms-glib";
#include "debug.h"

#include <stdarg.h>
#include <string.h>

static SMSGDebugFlags sms_g_debug_flags = 0;

static const GDebugKey sms_g_debug_keys[] = {
  { "submit",     SMS_G_DEBUG_SUBMIT },
  { "deliver",    SMS_G_DEBUG_DELIVER },
  { "status-report", SMS_G_DEBUG_STATUS_REPORT },
};

void sms_g_debug_set_flags(int new_flags)
{
  sms_g_debug_flags |= new_flags;
}

gboolean sms_g_debug_flag_is_set(int flag)
{
  return (sms_g_debug_flags & flag) != 0;
}

void sms_g_debug(int flag, const gchar *format, ...)
{
  if (flag & sms_g_debug_flags) {
    va_list args;
    va_start(args, format);
    g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
    va_end(args);
  }
}

void sms_g_debug_set_flags_from_env(void)
{
#if ENABLE_DEBUG
  const gchar *flags_string;

  flags_string = g_getenv("SMS_DEBUG");

  if (flags_string) {
    sms_g_debug_set_flags(g_parse_debug_string (flags_string,
        sms_g_debug_keys,
        G_N_ELEMENTS(sms_g_debug_keys)));
  }
#endif /* ENABLE_DEBUG */
}

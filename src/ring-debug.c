/*
 * ring-debug.c - Debugging utilities
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

#include "ring-debug.h"

#ifdef ENABLE_DEBUG

#include <stdarg.h>

#include <glib.h>

#include <telepathy-glib/debug.h>

void modem_debug_set_flags(int flags);
void modem_debug_set_flags_from_env(void);

static RingDebugFlags ring_debug_flags = 0;

static const GDebugKey ring_debug_keys[] = {
  { "media-channel", RING_DEBUG_MEDIA },
  { "call",          RING_DEBUG_MEDIA },
  { "media",         RING_DEBUG_MEDIA },
  { "connection",    RING_DEBUG_CONNECTION },
  { "text-channel",  RING_DEBUG_SMS },
  { "text",          RING_DEBUG_SMS },
  { "sms",           RING_DEBUG_SMS },
};

void
ring_debug_set_flags (RingDebugFlags new_flags)
{
  ring_debug_flags |= new_flags;
}

gboolean
ring_debug_flag_is_set (RingDebugFlags flag)
{
  return (flag & ring_debug_flags) != 0;
}

void
ring_debug (RingDebugFlags flag,
  const char *format,
  ...)
{
  if (flag & ring_debug_flags)
  {
    va_list args;
    va_start (args, format);
    g_logv ("Ring", G_LOG_LEVEL_DEBUG, format, args);
    va_end (args);
  }
}

#endif /* ENABLE_DEBUG */

void
ring_message (const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  g_logv ("Ring", G_LOG_LEVEL_MESSAGE, format, ap);
  va_end (ap);
}

void
ring_warning (const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  g_logv ("Ring", G_LOG_LEVEL_WARNING, format, ap);
  va_end (ap);
}

void
ring_critical (const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  g_logv ("Ring", G_LOG_LEVEL_CRITICAL, format, ap);
  va_end (ap);
}

void
ring_debug_set_flags_from_env (void)
{
  tp_debug_divert_messages(g_getenv("RING_LOGFILE"));

  const char *flags_string;

  flags_string = g_getenv("RING_DEBUG");

#if ENABLE_DEBUG
  if (flags_string)
  {
    tp_debug_set_flags (flags_string);

    ring_debug_set_flags (g_parse_debug_string (flags_string,
        ring_debug_keys,
        G_N_ELEMENTS(ring_debug_keys)));
  }
#endif

  if (flags_string && g_str_equal(flags_string, "all"))
    modem_debug_set_flags(~0);
  else
    modem_debug_set_flags_from_env();

  tp_debug_set_persistent(g_getenv("RING_PERSIST") != NULL);
}

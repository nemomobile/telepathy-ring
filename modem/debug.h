/*
 * modem/debug.h - Debugging facilities
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

#ifndef _MODEM_DEBUG_
#define _MODEM_DEBUG_

#include "config.h"

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  MODEM_SERVICE_DBUS          = 1 << 0,
  MODEM_SERVICE_CALL          = 1 << 1,
  MODEM_SERVICE_TONES         = 1 << 2,
  MODEM_SERVICE_SMS           = 1 << 3,
  MODEM_SERVICE_SIM           = 1 << 4,
  MODEM_SERVICE_MODEM         = 1 << 5,
} ModemDebugFlags;

gboolean modem_debug_flag_is_set(int flag);
void modem_debug_set_flags(int flag);
void modem_debug_set_flags_from_env(void);

void modem_debug(int flag, const char *format, ...)
  G_GNUC_PRINTF (2, 3);

void modem_message(int flag, const char *format, ...)
  G_GNUC_PRINTF (2, 3);

void modem_critical(int flag, const char *format, ...)
  G_GNUC_PRINTF (2, 3);

void modem_dump_properties(GHashTable *properties);

G_END_DECLS

#ifdef ENABLE_DEBUG

#define DEBUG(format, ...) \
  modem_debug(MODEM_DEBUG_FLAG, "%s: " format, G_STRFUNC, ##__VA_ARGS__)

#define DEBUG_DBUS(format, ...) \
  modem_debug(MODEM_SERVICE_DBUS, "%s: " format, G_STRFUNC, ##__VA_ARGS__)

#define DEBUGGING modem_debug_flag_is_set(MODEM_DEBUG_FLAG)

#else /* ENABLE_DEBUG */

#define DEBUG(format, ...)
#define DEBUGGING (0)

#endif /* ENABLE_DEBUG */

#define GERROR_MSG_FMT "%s (%d@%s)"

#define GERROR_MSG_CODE(e) \
  ((e) ? (e)->message : "no error"),            \
    ((e) ? (e)->code : 0),                      \
    ((e) ? g_quark_to_string((e)->domain) : "")

#endif /* _MODEM_DEBUG_ */

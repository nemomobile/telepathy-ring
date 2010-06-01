/*
 * ring-debug.h - Debugging utilities
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

#ifndef __RING_DEBUG_H__
#define __RING_DEBUG_H_

#include "config.h"

#include <glib.h>

G_BEGIN_DECLS

void ring_debug_set_flags_from_env(void);

typedef enum
{
  RING_DEBUG_CONNECTION    = 1 << 0,
  RING_DEBUG_MEDIA         = 1 << 1,
  RING_DEBUG_SMS           = 1 << 2,
} RingDebugFlags;

void ring_debug_set_flags(RingDebugFlags flags);
gboolean ring_debug_flag_is_set(RingDebugFlags flag);
void ring_debug(RingDebugFlags flag, const char *format, ...)
  G_GNUC_PRINTF (2, 3);

G_END_DECLS

#ifdef ENABLE_DEBUG

#ifdef DEBUG_FLAG

#define DEBUG(format, ...)                                              \
  ring_debug(DEBUG_FLAG, "%s: " format, G_STRFUNC, ##__VA_ARGS__)

#define DEBUGGING ring_debug_flag_is_set(DEBUG_FLAG)

#endif /* DEBUG_FLAG */

#else /* ENABLE_DEBUG */

#ifdef DEBUG_FLAG
#define DEBUG(format, ...)
#define DEBUGGING 0
#endif /* DEBUG_FLAG */

#endif /* ENABLE_DEBUG */

void ring_message(const char *format, ...)
  G_GNUC_PRINTF (1, 2);
void ring_warning(const char *format, ...)
  G_GNUC_PRINTF (1, 2);
void ring_critical(const char *format, ...)
  G_GNUC_PRINTF (1, 2);

#define GERROR_MSG_FMT "%s (%d@%s)"

#define GERROR_MSG_CODE(e)                      \
  (e ? e->message : "no error"),                \
    (e ? e->code : 0),                          \
    (e ? g_quark_to_string(e->domain) : "")

#endif /* __RING_DEBUG_H__ */

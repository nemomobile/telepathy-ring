/*
 * sms-glib/debug.h - Debugging facilities
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

#ifndef _SMS_GLIB_DEBUG_H_
#define _SMS_GLIB_DEBUG_H_

/* Include this file first */
#define G_LOG_DOMAIN sms_g_log_domain

extern char const sms_g_log_domain[];

#include <glib.h>
#include <sms-glib/utils.h>

G_BEGIN_DECLS

typedef enum
{
  SMS_G_DEBUG_DELIVER       = 1 << 0,
  SMS_G_DEBUG_STATUS_REPORT = 1 << 1,
  SMS_G_DEBUG_SUBMIT        = 1 << 2,
} SMSGDebugFlags;

gboolean sms_g_debug_flag_is_set(int flag);
void sms_g_debug_set_flags(int flag);

void sms_g_debug(int flag, const gchar *format, ...)
  G_GNUC_PRINTF (2, 3);

G_END_DECLS

#ifdef ENABLE_DEBUG

#define DEBUG(format, ...)                                              \
  sms_g_debug(SMS_G_DEBUG_FLAG, "%s: " format, G_STRFUNC, ##__VA_ARGS__)

#define DEBUGGING sms_g_debug_flag_is_set(SMS_G_DEBUG_FLAG)

#else /* ENABLE_DEBUG */

#define DEBUG(format, ...)
#define DEBUGGING (0)

#endif /* ENABLE_DEBUG */

#define GERROR_MSG_FMT "%s (%d@%s)"

#define GERROR_MSG_CODE(e)                      \
  (e ? e->message : "no error"),                \
    (e ? e->code : 0),                          \
    (e ? g_quark_to_string(e->domain) : "")

#endif /* _SMS_GLIB_DEBUG_H_ */

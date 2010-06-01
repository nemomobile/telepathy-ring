/*
 * sms-utils.h - utilities for SMS glibrary
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

#ifndef __SMS_UTILS_H__
#define __SMS_UTILS_H__

#include <glib-object.h>

G_BEGIN_DECLS

void sms_g_debug_set_flags_from_env(void);

gboolean sms_g_is_valid_sms_address(gchar const *smsc);
gboolean sms_g_validate_sms_address(gchar const *smsc, GError **error);

gboolean sms_g_is_valid_message_id(gchar const *message_id);
gboolean sms_g_validate_message_id(gchar const *message_id, GError **error);

gint64 sms_g_received_timestamp(void);

G_END_DECLS

#endif /* #ifndef __SMS_UTILS_H__*/

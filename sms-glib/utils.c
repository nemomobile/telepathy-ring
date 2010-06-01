/*
 * sms-glib/utils.c - SMS-related utilites
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

#include "sms-glib/errors.h"
#include "sms-glib/enums.h"
#include "sms-glib/utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static gchar const *
_sms_g_is_valid_sms_address(gchar const *address)
{
  size_t len;

  if (address == NULL)
    return "NULL";

  if (address[0] == '+') {
    address++;
  }

  len = strspn(address, "0123456789");

  if (address[len])
    return "invalid character";

  if (len == 0)
    return "too short";

  if (len > 20)
    return "too long";

  return NULL;
}

/** Return TRUE if @a address is a valid SMS address.
 *
 * A valid SMS address is a phone number with at most 20 digits either in
 * national or in international format (starting with +).
 *
 * @param address - ISDN address of address
 *
 * @retval TRUE - address is a valid Short Message Service Centre address
 * @retval FALSE - address is NULL, does not contain valid phone number, or it
 * is too long.
 */
gboolean
sms_g_is_valid_sms_address(gchar const *address)
{
  return !_sms_g_is_valid_sms_address(address);
}

/** Validate a SMS address @a address.
 *
 * A valid SMS address is a phone number with at most 20 digits either in
 * national or in international format (starting with +).
 *
 * @param address - ISDN address of address
 * @param error - return value for GError describing the ADDRESS validation error
 *
 * @retval TRUE - address is a valid Short Message Service Centre address
 * @retval FALSE - address is NULL, does not contain valid phone number, or it
 * is too long.
 */
gboolean
sms_g_validate_sms_address(gchar const *address, GError **error)
{
  gchar const *reason = _sms_g_is_valid_sms_address(address);

  if (reason)
    g_set_error(error, SMS_G_ERRORS, SMS_G_ERROR_INVALID_PARAM,
      "Invalid SMS address \"%s\": %s", address, reason);

  return !reason;
}

static gchar const *
_sms_g_is_valid_message_id(gchar const *message_id)
{
  if (message_id == NULL) return "NULL";
  if (strlen(message_id) == 0) return "empty";
  return NULL;
}

gboolean
sms_g_is_valid_message_id(gchar const *message_id)
{
  return !_sms_g_is_valid_message_id(message_id);
}

gboolean
sms_g_validate_message_id(gchar const *message_id, GError **error)
{
  gchar const *reason = _sms_g_is_valid_message_id(message_id);

  if (reason)
    g_set_error(error, SMS_G_ERRORS, SMS_G_ERROR_INVALID_PARAM,
      "Invalid message_id %s: %s", message_id, reason);

  return !reason;
}

gint64
sms_g_received_timestamp(void)
{
  return (gint64)time(NULL);
}

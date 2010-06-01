/*
 * sms-glib/errors.h -
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

#ifndef __SMS_GLIB_ERRORS_H__
#define __SMS_GLIB_ERRORS_H__

#include <glib-object.h>

G_BEGIN_DECLS

/** The error domain for sms-glib */
#define SMS_G_ERRORS sms_g_errors_quark ()

GQuark sms_g_errors_quark(void);

typedef enum {
  SMS_G_ERROR_INVALID_PARAM = 0
} SMSGErrors;

G_END_DECLS

#endif

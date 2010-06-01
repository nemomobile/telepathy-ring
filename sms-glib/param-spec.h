/*
 * sms-glib/param-spec.h - common parameters for SMS Glib objects
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

#ifndef _SMS_G_PARAM_SPEC_H_
#define _SMS_G_PARAM_SPEC_H_

#include <glib-object.h>

G_BEGIN_DECLS

GParamSpec *sms_g_param_spec_message_token(guint flags);
GParamSpec *sms_g_param_spec_smsc(guint flags);
GParamSpec *sms_g_param_spec_validity_period(guint flags);
GParamSpec *sms_g_param_spec_reduced_charset(guint flags);
GParamSpec *sms_g_param_spec_sms_class(guint flags);
GParamSpec *sms_g_param_spec_time_sent(guint flags);
GParamSpec *sms_g_param_spec_time_original(guint flags);
GParamSpec *sms_g_param_spec_time_received(guint flags);
GParamSpec *sms_g_param_spec_time_delivered(guint flags);

G_END_DECLS

#endif /* #ifndef _SMS_G_PARAM_SPEC_H_ */

/*
 * modem/sms-history.h - oFono SMS History interface
 *
 * Copyright (C) 2013 Jolla Ltd
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

#ifndef _MODEM_SMS_HISTORY_H_
#define _MODEM_SMS_HISTORY_H_

#include <glib-object.h>
#include <modem/oface.h>

G_BEGIN_DECLS

#define MODEM_OFACE_SMS_HISTORY "org.ofono.SmsHistory"

typedef struct _ModemSmsHistory ModemSmsHistory;
typedef struct _ModemSmsHistoryClass ModemSmsHistoryClass;

struct _ModemSmsHistoryClass
{
  ModemOfaceClass parent_class;
};

struct _ModemSmsHistory
{
  ModemOface parent;
};

GType modem_sms_history_get_type (void);

/* TYPE MACROS */
#define MODEM_TYPE_SMS_HISTORY                  \
  (modem_sms_history_get_type ())
#define MODEM_SMS_HISTORY(obj)                  \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      MODEM_TYPE_SMS_HISTORY, ModemSmsHistory))
#define MODEM_SMS_HISTORY_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                    \
      MODEM_TYPE_SMS_HISTORY, ModemSmsHistoryClass))
#define MODEM_IS_SMS_HISTORY(obj)                               \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MODEM_TYPE_SMS_HISTORY))
#define MODEM_IS_SMS_HISTORY_CLASS(klass)                       \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MODEM_TYPE_SMS_HISTORY))
#define MODEM_SMS_HISTORY_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                    \
      MODEM_TYPE_SMS_HISTORY, ModemSmsHistoryClass))

/* ---------------------------------------------------------------------- */

G_END_DECLS

#endif /* #ifndef _MODEM_SMS_HISTORY_H_*/

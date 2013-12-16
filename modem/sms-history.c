/*
 * modem/sms-history.c - oFono SMS History interface
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

#include "config.h"

#define MODEM_DEBUG_FLAG MODEM_LOG_SMS

#include "debug.h"

#include "modem/sms-history.h"
#include "modem/sms.h"
#include "modem/ofono.h"
#include "modem/modem.h"
#include "modem/service.h"

#include <dbus/dbus-glib.h>
#include "signals-marshal.h"

/* ------------------------------------------------------------------------ */

G_DEFINE_TYPE (ModemSmsHistory, modem_sms_history, MODEM_TYPE_OFACE);

/* ------------------------------------------------------------------------ */

static void
modem_sms_history_init (ModemSmsHistory *self)
{
  DEBUG ("enter");
}

static void
modem_sms_history_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (modem_sms_history_parent_class)->constructed)
    G_OBJECT_CLASS (modem_sms_history_parent_class)->constructed (object);
}

static void
modem_sms_history_dispose (GObject *object)
{
  if (G_OBJECT_CLASS (modem_sms_history_parent_class)->dispose)
    G_OBJECT_CLASS (modem_sms_history_parent_class)->dispose (object);
}

static void
modem_sms_history_finalize (GObject *object)
{
  DEBUG ("enter");
}

static void on_sms_history_status_report (DBusGProxy *proxy,
        char const *token,
        GHashTable *dict,
        gpointer user_data)
{
  ModemSmsHistory *self = MODEM_SMS_HISTORY(user_data);
  ModemService *modemService = modem_service();
  if (!modemService)
    return;

  Modem *modem = modem_service_find_by_path (modemService, dbus_g_proxy_get_path(proxy));
  if (!modem)
    return;

  ModemSMSService *smsService = MODEM_SMS_SERVICE(modem_get_interface (modem, MODEM_OFACE_SMS));
  if (!smsService)
    return;

  on_manager_message_status_report(proxy, token, dict, smsService);
}

/* ------------------------------------------------------------------------- */
/* ModemOface interface */

static void
modem_sms_history_connect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  ModemSmsHistory *self = MODEM_SMS_HISTORY (_self);
  DBusGProxy *proxy = modem_oface_dbus_proxy (_self);
  dbus_g_proxy_add_signal (proxy, "StatusReport",
      G_TYPE_STRING, MODEM_TYPE_DBUS_DICT, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (proxy, "StatusReport",
      G_CALLBACK (on_sms_history_status_report), self, NULL);
}

static void
modem_sms_history_connected (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);
}

static void
modem_sms_history_disconnect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);
  DBusGProxy *proxy = modem_oface_dbus_proxy (_self);
  ModemSmsHistory *self = MODEM_SMS_HISTORY (_self);
  dbus_g_proxy_disconnect_signal (proxy, "StatusReport",
      G_CALLBACK (on_sms_history_status_report), self);
}

static void
modem_sms_history_class_init (ModemSmsHistoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ModemOfaceClass *oface_class = MODEM_OFACE_CLASS (klass);

  DEBUG ("enter");

  object_class->constructed = modem_sms_history_constructed;
  object_class->dispose = modem_sms_history_dispose;
  object_class->finalize = modem_sms_history_finalize;

  oface_class->ofono_interface = MODEM_OFACE_SMS_HISTORY;
  oface_class->connect = modem_sms_history_connect;
  oface_class->connected = modem_sms_history_connected;
  oface_class->disconnect = modem_sms_history_disconnect;

  modem_error_domain_prefix (0); /* Init errors */
}



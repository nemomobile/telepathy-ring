/*
 * modem/radio-settings.h - oFono RadiSettings interface
 *
 * Copyright (C) 2011 Nokia Corporation
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

#ifndef _MODEM_RADIO_SETTINGS_H_
#define _MODEM_RADIO_SETTINGS_H_

#include <glib-object.h>
#include <modem/request.h>
#include <modem/oface.h>

G_BEGIN_DECLS

#define MODEM_OFACE_RADIO_SETTINGS "org.ofono.RadioSettings"

typedef struct _ModemRadioSettings ModemRadioSettings;
typedef struct _ModemRadioSettingsClass ModemRadioSettingsClass;
typedef struct _ModemRadioSettingsPrivate ModemRadioSettingsPrivate;

struct _ModemRadioSettingsClass
{
  ModemOfaceClass parent_class;
};

struct _ModemRadioSettings
{
  ModemOface parent;
  ModemRadioSettingsPrivate *priv;
};

GType modem_radio_settings_get_type (void);

/* TYPE MACROS */
#define MODEM_TYPE_RADIO_SETTINGS                  \
  (modem_radio_settings_get_type ())
#define MODEM_RADIO_SETTINGS(obj)                  \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      MODEM_TYPE_RADIO_SETTINGS, ModemRadioSettings))
#define MODEM_RADIO_SETTINGS_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                    \
      MODEM_TYPE_RADIO_SETTINGS, ModemRadioSettingsClass))
#define MODEM_IS_RADIO_SETTINGS(obj)                               \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MODEM_TYPE_RADIO_SETTINGS))
#define MODEM_IS_RADIO_SETTINGS_CLASS(klass)                       \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MODEM_TYPE_RADIO_SETTINGS))
#define MODEM_RADIO_SETTINGS_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                    \
      MODEM_TYPE_RADIO_SETTINGS, ModemRadioSettingsClass))

/* ---------------------------------------------------------------------- */

G_END_DECLS

#endif /* #ifndef _MODEM_RADIO_SETTINGS_H_*/

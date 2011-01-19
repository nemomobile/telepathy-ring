/*
 * modem/radio-settings.c - ModemRadioSettings class
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

#include "config.h"

#define MODEM_DEBUG_FLAG MODEM_LOG_RADIO

#include "debug.h"

#include "modem/radio-settings.h"
#include "modem/request-private.h"
#include "modem/errors.h"
#include "modem/ofono.h"

#include <dbus/dbus-glib.h>

#include "signals-marshal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

/* ------------------------------------------------------------------------ */

G_DEFINE_TYPE (ModemRadioSettings, modem_radio_settings, MODEM_TYPE_OFACE);

/* Properties */
enum
{
  PROP_NONE,
  PROP_TECH_PREF,
  PROP_GSM_BAND,
  PROP_UMTS_BAND,
  PROP_FAST_DORMANCY,
  LAST_PROPERTY
};

/* private data */
struct _ModemRadioSettingsPrivate
{
  char *tech_pref;
  char *gsm_band;
  char *umts_band;
  gboolean fast_dormancy;

  unsigned dispose_has_run:1, connected:1, signals:1, disconnected:1;
  unsigned connection_error:1;
  unsigned :0;
};

/* ------------------------------------------------------------------------ */

static void
modem_radio_settings_init (ModemRadioSettings *self)
{
  DEBUG ("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      MODEM_TYPE_RADIO_SETTINGS, ModemRadioSettingsPrivate);
}

static void
modem_radio_settings_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  ModemRadioSettings *self = MODEM_RADIO_SETTINGS (object);
  ModemRadioSettingsPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_TECH_PREF:
      g_value_set_string (value, priv->tech_pref);
      break;

    case PROP_GSM_BAND:
      g_value_set_string (value, priv->gsm_band);
      break;

    case PROP_UMTS_BAND:
      g_value_set_string (value, priv->umts_band);
      break;

    case PROP_FAST_DORMANCY:
      g_value_set_boolean (value, priv->fast_dormancy);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_radio_settings_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
  ModemRadioSettings *self = MODEM_RADIO_SETTINGS (object);
  ModemRadioSettingsPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_TECH_PREF:
      g_free (priv->tech_pref);
      priv->tech_pref = g_value_dup_string (value);
      break;

    case PROP_GSM_BAND:
      g_free (priv->gsm_band);
      priv->gsm_band = g_value_dup_string (value);
      break;

    case PROP_UMTS_BAND:
      g_free (priv->umts_band);
      priv->umts_band = g_value_dup_string (value);
      break;

    case PROP_FAST_DORMANCY:
      priv->fast_dormancy = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_radio_settings_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (modem_radio_settings_parent_class)->constructed)
    G_OBJECT_CLASS (modem_radio_settings_parent_class)->constructed (object);
}

static void
modem_radio_settings_dispose (GObject *object)
{
  if (G_OBJECT_CLASS (modem_radio_settings_parent_class)->dispose)
    G_OBJECT_CLASS (modem_radio_settings_parent_class)->dispose (object);
}

static void
modem_radio_settings_finalize (GObject *object)
{
  ModemRadioSettings *self = MODEM_RADIO_SETTINGS (object);
  ModemRadioSettingsPrivate *priv = self->priv;

  DEBUG ("enter");

  /* Free any data held directly by the object here */
  g_free (priv->tech_pref);
  g_free (priv->gsm_band);
  g_free (priv->umts_band);

  G_OBJECT_CLASS (modem_radio_settings_parent_class)->finalize (object);
}

/* ------------------------------------------------------------------------- */
/* ModemOface interface */

static char const *
modem_radio_settings_property_mapper (char const *name)
{
  if (!strcmp (name, "TechnologyPreference"))
    return "technology-preference";
  if (!strcmp(name, "GsmBand"))
      return "gsm-band";
  if (!strcmp (name, "UmtsBand"))
    return "umts-band";
  if (!strcmp (name, "FastDormancy"))
    return "fast-dormancy";
  return NULL;
}

static void
modem_radio_settings_connect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  modem_oface_connect_properties (_self, TRUE);
}

static void
modem_radio_settings_connected (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);
}

static void
modem_radio_settings_disconnect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  modem_oface_disconnect_properties (_self);
}

static void
modem_radio_settings_class_init (ModemRadioSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ModemOfaceClass *oface_class = MODEM_OFACE_CLASS (klass);

  DEBUG ("enter");

  object_class->get_property = modem_radio_settings_get_property;
  object_class->set_property = modem_radio_settings_set_property;
  object_class->constructed = modem_radio_settings_constructed;
  object_class->dispose = modem_radio_settings_dispose;
  object_class->finalize = modem_radio_settings_finalize;

  oface_class->ofono_interface = MODEM_OFACE_RADIO_SETTINGS;
  oface_class->property_mapper = modem_radio_settings_property_mapper;
  oface_class->connect = modem_radio_settings_connect;
  oface_class->connected = modem_radio_settings_connected;
  oface_class->disconnect = modem_radio_settings_disconnect;

  /* Properties */
  g_object_class_install_property (object_class, PROP_TECH_PREF,
      g_param_spec_string ("technology-preference",
          "TechnologyPreference",
          "The current radio access selection mode, also known "
          "as network preference.",
          "", /* default value */
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_GSM_BAND,
      g_param_spec_string ("gsm-band",
          "GsmBand",
          "Frequency band in which the modem is allowed to "
          "operate when using \"gsm\" mode. Setting this property "
          "has an imediate effect on modem only if "
          "TechnologyPreference is set to \"gsm\" or \"any\". "
          "Otherwise the value is kept and applied whenever modem "
          "uses this mode.",
          "", /* default value */
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_UMTS_BAND,
      g_param_spec_string ("umts-band",
          "UmtsBand",
          "Frequency band in which the modem is allowed to "
          "operate when using \"umts\" mode. Setting this property "
          "has an imediate effect on modem only if "
          "TechnologyPreference is set to \"umts\" or \"any\". "
          "Otherwise the value is kept and applied whenever modem "
          "uses this mode.",
          "", /* default value */
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FAST_DORMANCY,
      g_param_spec_boolean ("fast-dormancy",
          "FastDormancy",
          "This property will enable or disable the fast "
          "dormancy feature in the modem. Fast dormancy "
          "refers to a modem feature that allows the "
          "modem to quickly release radio resources after "
          "a burst of data transfer has ended. Normally, "
          "radio resources are released by the network "
          "after a timeout configured by the network. "
          "Fast dormancy allows the modem to release the "
          "radio resources more quickly.",
          FALSE, /* default value */
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (ModemRadioSettingsPrivate));

  modem_error_domain_prefix (0); /* Init errors */
}

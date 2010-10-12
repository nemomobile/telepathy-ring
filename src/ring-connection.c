/*
 * ring-connection.c - Source for RingConnection
 *
 * Copyright (C) 2007-2010 Nokia Corporation
 *   @author Pekka Pessi <first.surname@nokia.com>
 *   @author Lassi Syrjala <first.surname@nokia.com>
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

#define DEBUG_FLAG RING_DEBUG_CONNECTION
#include "ring-debug.h"

#include "ring-connection.h"
#include "ring-emergency-service.h"
#include "ring-media-manager.h"
#include "ring-text-manager.h"

#include "ring-param-spec.h"
#include "ring-util.h"

#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/handle-repo-dynamic.h>
#include <telepathy-glib/handle-repo-static.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-connection.h>

#include "ring-extensions/ring-extensions.h"

#include "modem/service.h"
#include "modem/modem.h"
#include "modem/sim.h"
#include "modem/call.h"
#include "modem/sms.h"

#include <sms-glib/utils.h>

#include <dbus/dbus-glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

struct _RingConnectionPrivate
{
  /* Properties */
  char *smsc;
  guint sms_valid;
  guint anon_modes;
  guint anon_supported_modes;

  RingMediaManager *media;
  RingTextManager *text;

  gchar *modem_path;
  Modem *modem;
  ModemSIMService *sim;

  struct {
    gulong modem_added;
    gulong modem_removed;
    gulong modem_interfaces;
    gulong sim_connected;
    gulong imsi_notify;
  } signals;

  guint connecting_source;

  unsigned anon_mandatory:1;
  unsigned sms_reduced_charset:1;
  unsigned dispose_has_run:1;
};

/* properties */
enum {
  PROP_NONE,
  PROP_IMSI,                    /**< IMSI */
  PROP_SMSC,                    /**< SMSC address */
  PROP_SMS_VALID,               /**< SMS validity period in seconds */
  PROP_SMS_REDUCED_CHARSET,     /**< SMS reduced charset support */
  PROP_MODEM_PATH,              /**< Object path of the modem */

  PROP_STORED_MESSAGES,         /**< List of stored messages */
  PROP_KNOWN_SERVICE_POINTS,    /**< List of emergency service points */

  PROP_ANON_SUPPORTED_MODES,
  PROP_ANON_MANDATORY,
  PROP_ANON_MODES,

  N_PROPS
};

static void ring_connection_class_init_base_connection(TpBaseConnectionClass *);
static void ring_connection_capabilities_iface_init(gpointer, gpointer);
static void ring_connection_add_contact_capabilities(GObject *object,
  GArray const *handles, GHashTable *returns);
/*static void ring_connection_stored_messages_iface_init(gpointer, gpointer);*/

static TpDBusPropertiesMixinPropImpl ring_connection_service_point_properties[],
  ring_connection_cellular_properties[],
/*ring_connection_stored_messages_properties[],*/
  ring_connection_anon_properties[];

static gboolean ring_connection_cellular_properties_setter(GObject *object,
  GQuark interface, GQuark name, const GValue *value, gpointer setter_data,
  GError **error);

static TpDBusPropertiesMixinIfaceImpl
ring_connection_dbus_property_interfaces[];

/** Inspection result from self handle. */
static char const ring_self_handle_name[] = "<SelfHandle>";

/* ---------------------------------------------------------------------- */
/* GObject interface */

G_DEFINE_TYPE_WITH_CODE(
  RingConnection, ring_connection, TP_TYPE_BASE_CONNECTION,
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_DBUS_PROPERTIES,
    tp_dbus_properties_mixin_iface_init);
  G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_CONTACTS,
    tp_contacts_mixin_iface_init);
  G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_CAPABILITIES,
    ring_connection_capabilities_iface_init);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CONNECTION_INTERFACE_SERVICE_POINT,
    NULL);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CONNECTION_INTERFACE_CELLULAR,
    NULL);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CONNECTION_INTERFACE_ANONYMITY,
    NULL);
#if nomore
  /* XXX: waiting for upstream tp-glib to get this */
  G_IMPLEMENT_INTERFACE(RTCOM_TYPE_TP_SVC_CONNECTION_INTERFACE_STORED_MESSAGES,
    ring_connection_stored_messages_iface_init);
#endif
  );

static char const * const ring_connection_interfaces_always_present[] = {
  TP_IFACE_CONNECTION_INTERFACE_REQUESTS,
  TP_IFACE_CONNECTION_INTERFACE_CONTACTS,
  TP_IFACE_CONNECTION_INTERFACE_CAPABILITIES,
  TP_IFACE_CONNECTION_INTERFACE_SERVICE_POINT,
  TP_IFACE_CONNECTION_INTERFACE_CELLULAR,
  TP_IFACE_CONNECTION_INTERFACE_ANONYMITY,
#if nomore
  RTCOM_TP_IFACE_CONNECTION_INTERFACE_STORED_MESSAGES,
#endif
  NULL
};

static void
ring_connection_init(RingConnection *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    (self), RING_TYPE_CONNECTION, RingConnectionPrivate);

  /* Initialize Contacts mixin */
  tp_contacts_mixin_init(
    (GObject *)self,
    G_STRUCT_OFFSET(RingConnection, contacts_mixin));

  /* org.freedesktop.Telepathy.Connection attributes */
  tp_base_connection_register_with_contacts_mixin(
    TP_BASE_CONNECTION(self));

  /* org.freedesktop.Telepathy.Connection.Interface.Capabilities attributes */
  tp_contacts_mixin_add_contact_attributes_iface((GObject *)self,
    TP_IFACE_CONNECTION_INTERFACE_CAPABILITIES,
    ring_connection_add_contact_capabilities);
}

static void
on_imsi_changed(GObject *object, GParamSpec *pspec,
  gpointer user_data)
{
  RingConnection *self = RING_CONNECTION(user_data);
  TpBaseConnection *base = TP_BASE_CONNECTION(self);

  if (base->status == TP_CONNECTION_STATUS_CONNECTED) {
    char const *imsi;

    imsi = modem_sim_get_imsi(MODEM_SIM_SERVICE(object));
    tp_svc_connection_interface_cellular_emit_imsi_changed(self, imsi);
  }
}

static void
ring_connection_constructed(GObject *object)
{
  RingConnection *self = RING_CONNECTION(object);
  TpBaseConnection *base = TP_BASE_CONNECTION(object);
  TpHandleRepoIface *repo;
  TpHandle self_handle;

  if (G_OBJECT_CLASS(ring_connection_parent_class)->constructed)
    G_OBJECT_CLASS(ring_connection_parent_class)->constructed(object);

  self->priv->anon_supported_modes =
    TP_ANONYMITY_MODE_CLIENT_INFO |
    TP_ANONYMITY_MODE_SHOW_CLIENT_INFO,

  repo = tp_base_connection_get_handles(base, TP_HANDLE_TYPE_CONTACT);
  self_handle = tp_handle_ensure(repo, ring_self_handle_name, NULL, NULL);
  tp_base_connection_set_self_handle(base, self_handle);
  tp_handle_unref(repo, self_handle);
  g_assert(base->self_handle != 0);

  self->anon_handle = tp_handle_ensure(repo, "", NULL, NULL);
  g_assert(self->anon_handle != 0);

  self->sos_handle = tp_handle_ensure(repo, RING_EMERGENCY_SERVICE_URN, NULL, NULL);
  g_assert(self->sos_handle != 0);
}

static void
ring_connection_dispose(GObject *object)
{
  RingConnection *self = RING_CONNECTION(object);
  TpHandleRepoIface *repo = tp_base_connection_get_handles(
    TP_BASE_CONNECTION(self), TP_HANDLE_TYPE_CONTACT);

  if (self->priv->dispose_has_run)
    return;
  self->priv->dispose_has_run = 1;

  tp_handle_unref(repo, self->anon_handle), self->anon_handle = 0;
  tp_handle_unref(repo, self->sos_handle), self->sos_handle = 0;

  if (self->priv->modem)
    g_object_unref (self->priv->modem);
  if (self->priv->sim)
    g_object_unref(self->priv->sim);

  G_OBJECT_CLASS(ring_connection_parent_class)->dispose(object);
  g_assert(self->parent.self_handle == 0);  /* unref'd by base class */
}

void
ring_connection_finalize(GObject *object)
{
  RingConnection *self = RING_CONNECTION(object);
  RingConnectionPrivate *priv = self->priv;

  DEBUG("enter %p", object);

  /* Free any data held directly by the object here */
  g_free(priv->smsc);
  g_free(priv->modem_path);

  G_OBJECT_CLASS(ring_connection_parent_class)->finalize(object);
}

static void
ring_connection_set_property(GObject *obj,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  TpBaseConnection *base;
  RingConnection *self = RING_CONNECTION(obj);
  RingConnectionPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_SMSC:
      priv->smsc = ring_normalize_isdn(g_value_get_string(value));
      if (priv->text)
        g_object_set(priv->text, "sms-service-centre", priv->smsc, NULL);
      break;
    case PROP_SMS_VALID:
      priv->sms_valid = g_value_get_uint(value);
      if (priv->text)
        g_object_set(priv->text, "sms-validity-period", priv->sms_valid, NULL);
      break;
    case PROP_SMS_REDUCED_CHARSET:
      priv->sms_reduced_charset = g_value_get_boolean(value);
      if (priv->text)
        g_object_set(priv->text, "sms-reduced-charset",
          priv->sms_reduced_charset, NULL);
      break;

    case PROP_MODEM_PATH:
      priv->modem_path = g_value_dup_boxed(value);
      break;

    case PROP_ANON_MANDATORY:
      priv->anon_mandatory = g_value_get_boolean(value);
      break;

    case PROP_ANON_MODES:
      base = TP_BASE_CONNECTION(self);

      priv->anon_modes = g_value_get_uint(value);
      if (priv->media)
        g_object_set(priv->media, "anon-modes", priv->anon_modes, NULL);

      if (base->status == TP_CONNECTION_STATUS_CONNECTED)
        tp_svc_connection_interface_anonymity_emit_anonymity_modes_changed
          (self, priv->anon_modes);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_connection_get_property(GObject *obj,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  RingConnection *self = RING_CONNECTION(obj);
  RingConnectionPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_IMSI:
      if (priv->sim && modem_sim_service_is_connected(priv->sim))
        g_object_get_property(G_OBJECT(priv->sim), "imsi", value);
      else
        g_value_set_string(value, "");
      break;
    case PROP_SMSC:
      g_value_set_string(value, priv->smsc ? priv->smsc : "");
      break;
    case PROP_SMS_VALID:
      g_value_set_uint(value, priv->sms_valid);
      break;
    case PROP_SMS_REDUCED_CHARSET:
      g_value_set_boolean(value, priv->sms_reduced_charset);
      break;
    case PROP_MODEM_PATH:
      g_value_set_boxed(value, priv->modem_path);
      break;
    case PROP_STORED_MESSAGES:
#if nomore
      g_value_take_boxed(value, ring_text_manager_list_stored_messages(priv->text));
#endif
      break;

    case PROP_KNOWN_SERVICE_POINTS:
      g_value_take_boxed(value, ring_media_manager_emergency_services(priv->media));
      break;

    case PROP_ANON_MANDATORY:
      g_value_set_boolean(value, priv->anon_mandatory);
      break;
    case PROP_ANON_SUPPORTED_MODES:
      g_value_set_uint(value, priv->anon_supported_modes);
      break;
    case PROP_ANON_MODES:
      g_value_set_uint(value, priv->anon_modes);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_connection_class_init(RingConnectionClass *ring_connection_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS(ring_connection_class);

  g_type_class_add_private(ring_connection_class, sizeof (RingConnectionPrivate));

  object_class->constructed = ring_connection_constructed;
  object_class->dispose = ring_connection_dispose;
  object_class->finalize = ring_connection_finalize;
  object_class->set_property = ring_connection_set_property;
  object_class->get_property = ring_connection_get_property;

  g_object_class_install_property(
    object_class, PROP_IMSI, ring_param_spec_imsi());

  g_object_class_install_property(
    object_class, PROP_SMSC, ring_param_spec_smsc());

  g_object_class_install_property(
    object_class, PROP_SMS_VALID, ring_param_spec_sms_valid());

  g_object_class_install_property(
    object_class, PROP_SMS_REDUCED_CHARSET,
    ring_param_spec_sms_reduced_charset());

  g_object_class_install_property(
    object_class, PROP_MODEM_PATH,
    g_param_spec_boxed("modem-path",
      "Modem path",
      "oFono object path of the modem to use",
      DBUS_TYPE_G_OBJECT_PATH,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_STORED_MESSAGES,
    g_param_spec_boxed("stored-messages",
      "Stored messages",
      "List of messages "
      "in permanent storage.",
      G_TYPE_STRV,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_KNOWN_SERVICE_POINTS,
    g_param_spec_boxed("known-service-points",
      "Known service points",
      "List of known emergency service points",
      TP_ARRAY_TYPE_SERVICE_POINT_INFO_LIST,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_ANON_MANDATORY,
    g_param_spec_boolean("anon-mandatory",
      "Anonymity mandatory",
      "Specifies whether or not the anonymity "
      "settings should be respected",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_ANON_SUPPORTED_MODES,
    g_param_spec_uint("anon-supported-modes",
      "Supported anonymity modes",
      "Specifies the supported anonymity modes",
      0, G_MAXUINT,
      TP_ANONYMITY_MODE_CLIENT_INFO |
      TP_ANONYMITY_MODE_SHOW_CLIENT_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_ANON_MODES,
    ring_param_spec_anon_modes());

  ring_connection_class_init_base_connection(
    TP_BASE_CONNECTION_CLASS(ring_connection_class));

  ring_connection_class->dbus_properties_class.interfaces =
    ring_connection_dbus_property_interfaces;
  tp_dbus_properties_mixin_class_init(
    object_class,
    G_STRUCT_OFFSET(RingConnectionClass, dbus_properties_class));

  tp_contacts_mixin_class_init(
    object_class,
    G_STRUCT_OFFSET(RingConnectionClass, contacts_mixin_class));
}

/* ------------------------------------------------------------------------- */

static TpDBusPropertiesMixinIfaceImpl
ring_connection_dbus_property_interfaces[] = {
  {
    TP_IFACE_CONNECTION_INTERFACE_SERVICE_POINT,
    tp_dbus_properties_mixin_getter_gobject_properties,
    NULL,
    ring_connection_service_point_properties,
  },
  {
    TP_IFACE_CONNECTION_INTERFACE_CELLULAR,
    tp_dbus_properties_mixin_getter_gobject_properties,
    ring_connection_cellular_properties_setter,
    ring_connection_cellular_properties,
  },
  {
    TP_IFACE_CONNECTION_INTERFACE_ANONYMITY,
    tp_dbus_properties_mixin_getter_gobject_properties,
    tp_dbus_properties_mixin_setter_gobject_properties,
    ring_connection_anon_properties,
  },
#if nomore
  {
    RTCOM_TP_IFACE_CONNECTION_INTERFACE_STORED_MESSAGES,
    tp_dbus_properties_mixin_getter_gobject_properties,
    NULL,
    ring_connection_stored_messages_properties,
  },
#endif
  { NULL }
};

/* ------------------------------------------------------------------------- */

typedef struct {
  char *imsi;                  /* Internation Mobile Subscriber Identifier */
  char *sms_service_centre;    /* SMS Service Center address */
  guint  sms_validity_period;   /* SMS validity period, 0 if default */
  gboolean sms_reduced_charset; /* SMS reduced character set support */

  gboolean anon_mandatory;      /* Whether anonymity modes are mandatory */
  guint    anon_modes;          /* Required anonymity mode */

  gchar *modem;                /* Object path of the modem to use; NULL to
                                * pick one arbitrarily.
                                */

  /* Deprecated */
  char *account;               /* Ignored */
  char *password;              /* Ignored */
} RingConnectionParams;

#if nomore
/**
 * param_filter_tokens:
 * @paramspec: The parameter specification for a string parameter
 * @value: A GValue containing a string, which will not be altered
 * @error: Used to return an error if the string has non-empty value
 *         that does not match filter_data
 *
 * A #TpCMParamFilter which rejects empty strings.
 *
 * Returns: %TRUE to accept, %FALSE (with @error set) to reject
 */
static gboolean
param_filter_tokens(TpCMParamSpec const *paramspec,
  GValue *value,
  GError **error)
{
  const char * const *values;
  const char *str = g_value_get_string(value);

  if (str == NULL || str[0] == '\0')
    return TRUE;

  for (values = paramspec->filter_data; *values; values++)
    if (strcmp(str, *values) == 0)
      return TRUE;

  g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
    "Account parameter '%s' with invalid value",
    paramspec->name);
  return FALSE;
}
#endif

/* Validate ISDN number */
static gboolean
param_filter_isdn(TpCMParamSpec const *paramspec,
  GValue *value,
  GError **error)
{
  const char *str = g_value_get_string(value);
  unsigned len = 0;

  if (str == NULL || str[0] == '\0')
    return TRUE;

  if (str[0] == '+')
    str++;

  for (len = 0; *str; str++) {
    if (strchr(" .-()", *str))  /* Skip fillers */
      continue;

    if (!strchr("0123456789", *str)) {
      if (error)
        g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Account parameter '%s' with invalid ISDN number",
          paramspec->name);
      return FALSE;
    }
    else if (++len > 20) {
      if (error)
        g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Account parameter '%s' with ISDN number too long",
          paramspec->name);
      return FALSE;
    }
  }

  return TRUE;
}

/* Validate SMS validity period  */
static gboolean
param_filter_validity(TpCMParamSpec const *paramspec,
  GValue *value,
  GError **error)
{
  guint validity = g_value_get_uint(value);

  if (validity == 0)
    return TRUE;

  /* from 5 minutes to 63 weeks */
  if (5 * 60 <= validity && validity <= 63 * 7 * 24 * 60 * 60)
    return TRUE;

  g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
    "Account parameter '%s' with invalid validity period - "
    "default (0), minimum 5 minutes (%u), maximum 63 weeks (%u)",
    paramspec->name, 5 * 60, 63 * 7 * 24 * 60 * 60);
  return FALSE;
}

static gboolean
param_filter_anon_modes(TpCMParamSpec const *paramspec,
  GValue *value,
  GError **error)
{
  guint modes = g_value_get_uint(value);

  /* supported modes: 0, 1, 2, and 3 */
  modes &= ~(TP_ANONYMITY_MODE_CLIENT_INFO |
           TP_ANONYMITY_MODE_SHOW_CLIENT_INFO);

  if (modes == 0)
    return TRUE;

  g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
    "Account parameter '%s' with invalid value",
    paramspec->name);

  return FALSE;
}

static gboolean
param_filter_valid_object_path (TpCMParamSpec const *paramspec,
  GValue *value,
  GError **error)
{
  return tp_dbus_check_valid_object_path (g_value_get_boxed (value), error);
}

TpCMParamSpec ring_connection_params[] = {
  { TP_IFACE_CONNECTION_INTERFACE_CELLULAR ".IMSI",
    DBUS_TYPE_STRING_AS_STRING, G_TYPE_STRING,
    TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY,
    "",
    G_STRUCT_OFFSET(RingConnectionParams, imsi),
    NULL,
  },

#define CELLULAR_SMS_VALIDITY_PERIOD_PARAM_SPEC (ring_connection_params + 1)
  { TP_IFACE_CONNECTION_INTERFACE_CELLULAR ".MessageValidityPeriod",
    DBUS_TYPE_UINT32_AS_STRING, G_TYPE_UINT,
    TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY,
    GUINT_TO_POINTER(0),
    G_STRUCT_OFFSET(RingConnectionParams, sms_validity_period),
    param_filter_validity,
  },

#define CELLULAR_SMS_SERVICE_CENTRE_PARAM_SPEC (ring_connection_params + 2)
  { TP_IFACE_CONNECTION_INTERFACE_CELLULAR ".MessageServiceCentre",
    DBUS_TYPE_STRING_AS_STRING, G_TYPE_STRING,
    TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY,
    "",
    G_STRUCT_OFFSET(RingConnectionParams, sms_service_centre),
    param_filter_isdn,
  },

#define CELLULAR_SMS_REDUCED_CHARSET_PARAM_SPEC (ring_connection_params + 3)
  { TP_IFACE_CONNECTION_INTERFACE_CELLULAR ".MessageReducedCharacterSet",
    DBUS_TYPE_BOOLEAN_AS_STRING, G_TYPE_BOOLEAN,
    TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY,
    GUINT_TO_POINTER(0),
    G_STRUCT_OFFSET(RingConnectionParams, sms_reduced_charset),
    NULL,
  },

  { TP_IFACE_CONNECTION_INTERFACE_ANONYMITY ".AnonymityMandatory",
    DBUS_TYPE_BOOLEAN_AS_STRING, G_TYPE_BOOLEAN,
    TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY,
    GUINT_TO_POINTER(FALSE),
    G_STRUCT_OFFSET(RingConnectionParams, anon_mandatory),
    NULL,
  },

  { TP_IFACE_CONNECTION_INTERFACE_ANONYMITY ".AnonymityModes",
    DBUS_TYPE_UINT32_AS_STRING, G_TYPE_UINT,
    TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY,
    GUINT_TO_POINTER(0),
    G_STRUCT_OFFSET(RingConnectionParams, anon_modes),
    param_filter_anon_modes,
  },

#define MODEM_PARAM_SPEC (ring_connection_params + 6)
  { "modem",
    DBUS_TYPE_OBJECT_PATH_AS_STRING,
    /* DBUS_TYPE_G_OBJECT_PATH expands to a function call so we have to fill
     * this in in ring_connection_get_param_specs().
     */
    (GType) 0,
    0,
    NULL,
    G_STRUCT_OFFSET(RingConnectionParams, modem),
    param_filter_valid_object_path,
  },

  /* Deprecated... */
  { "account", DBUS_TYPE_STRING_AS_STRING, G_TYPE_STRING,
    0, NULL,
    G_STRUCT_OFFSET (RingConnectionParams, account),
  },

  { "password", DBUS_TYPE_STRING_AS_STRING, G_TYPE_STRING,
    TP_CONN_MGR_PARAM_FLAG_SECRET, NULL,
    G_STRUCT_OFFSET (RingConnectionParams, password),
  },

  { NULL }
};

TpCMParamSpec *
ring_connection_get_param_specs (void)
{
  TpCMParamSpec *modem = MODEM_PARAM_SPEC;

  modem->gtype = DBUS_TYPE_G_OBJECT_PATH;

  return ring_connection_params;
}

gpointer
ring_connection_params_alloc(void)
{
  return g_slice_new0(RingConnectionParams);
}

void
ring_connection_params_free(gpointer p)
{
  RingConnectionParams *params = p;

  g_free(params->modem);
  g_free(params->account);
  g_free(params->password);

  g_slice_free(RingConnectionParams, params);
}

RingConnection *
ring_connection_new(TpIntSet *params_present,
  gpointer parsed_params)
{
  RingConnectionParams *params = parsed_params;
  char *sms_service_centre = params->sms_service_centre;

  return (RingConnection *) g_object_new(RING_TYPE_CONNECTION,
      "protocol", "tel",
      "modem-path", params->modem,
      "sms-service-centre", sms_service_centre ? sms_service_centre : "",
      "sms-validity-period", params->sms_validity_period,
      "sms-reduced-charset", params->sms_reduced_charset,
      "anon-modes", params->anon_modes,
      "anon-mandatory", params->anon_mandatory,
      NULL);
}


/* ---------------------------------------------------------------------- */
/* TpBaseConnection interface */
/* 1 - repo */

/** Return a context used to normalize contacts from network  */
gpointer ring_network_normalization_context(void)
{
  return (gpointer)ring_network_normalization_context;
}

/** Normalize a GSM address.
 *
 * A contact handle can be either a phone number (up to 20 digits),
 * international phone number ("'+' and up to 20 digits), SOS URN or
 * an alphanumeric address with up to 11 GSM characters.
 *
 * Normalize a telephone number can contain an an optional service prefix
 * and dial string.
 */
static char *
ring_normalize_name(TpHandleRepoIface *repo,
  char const *input,
  gpointer context,
  GError **return_error)
{
  char const *sos;
  char *s;
  int i, j;

  if (g_strcasecmp(input, ring_self_handle_name) == 0)
    return g_strdup(ring_self_handle_name);

  if (strlen(input) == strspn(input, "()-. "))
    return g_strdup("");        /* Anonymous */

  sos = modem_call_get_valid_emergency_urn(input);
  if (sos)
    return g_ascii_strdown(g_strdup(sos), -1);

  if ((s = ring_str_starts_with_case(input, "tel:")))
    input = s;

  s = g_strdup(input);

  if (context == ring_network_normalization_context())
    return s;

  /* Remove usual extra chars like (-. ) */
  for (i = j = 0; s[i]; i++) {
    switch (s[i]) {
      case '(': case ')': case '-': case '.': case ' ':
        continue;
      case 'P': case 'p':
        s[j++] = 'p';
        break;
      case 'W': case 'w':
        s[j++] = 'w';
        break;
      default:
        s[j++] = s[i];
        break;
    }
  }

  s[j] = s[i];

  if (modem_call_is_valid_address(s) || sms_g_is_valid_sms_address(s))
    return s;

  if (g_utf8_strlen(input, -1) <= 11)
    return strcpy(s, input);

  *return_error = g_error_new(TP_ERRORS,
                  TP_ERROR_INVALID_ARGUMENT, "invalid phone number");
  g_free(s);
  return NULL;
}

static void
ring_connection_create_handle_repos(TpBaseConnection *base,
  TpHandleRepoIface *repos[NUM_TP_HANDLE_TYPES])
{
  (void)base;

  repos[TP_HANDLE_TYPE_CONTACT] = (TpHandleRepoIface *)
    g_object_new(TP_TYPE_DYNAMIC_HANDLE_REPO,
      "handle-type", TP_HANDLE_TYPE_CONTACT,
      "normalize-function", ring_normalize_name,
      NULL);
}

/* ---------------------------------------------------------------------- */

static char *
ring_connection_get_unique_connection_name(TpBaseConnection *base)
{
  (void)base;

  return g_strdup("ring");      /* There can be only one */
}

/* ---------------------------------------------------------------------- */
/* TpBaseConnection interface */

static GPtrArray *
ring_connection_create_channel_managers(TpBaseConnection *base)
{
  RingConnection *self = RING_CONNECTION(base);
  RingConnectionPrivate *priv = self->priv;
  GPtrArray *channel_managers = g_ptr_array_sized_new(2);

  DEBUG("enter");

  priv->media =
    g_object_new(RING_TYPE_MEDIA_MANAGER,
      "connection", self,
      "anon-modes", priv->anon_modes,
      NULL);
  g_ptr_array_add(channel_managers, priv->media);

  priv->text =
    g_object_new(RING_TYPE_TEXT_MANAGER,
      "connection", self,
      "sms-service-centre", priv->smsc,
      "sms-validity-period", priv->sms_valid,
      "sms-reduced-charset", priv->sms_reduced_charset,
      NULL);
  g_ptr_array_add(channel_managers, priv->text);

  return channel_managers;
}

static gboolean ring_connection_connecting_timeout (gpointer);
static void ring_connection_sim_connected(ModemSIMService *, gpointer);
static void ring_connection_modem_added (ModemService *, Modem *, gpointer);
static void ring_connection_modem_removed (ModemService *, Modem *, gpointer);
static void on_modem_interfaces_changed (Modem *, GParamSpec *, gpointer);

static gboolean
ring_connection_start_connecting(TpBaseConnection *base,
  GError **return_error)
{
  RingConnection *self = RING_CONNECTION(base);
  RingConnectionPrivate *priv = self->priv;
  ModemService *modems;
  Modem *modem;
  GError *error = NULL;

  DEBUG("called");

  g_assert(base->status == TP_INTERNAL_CONNECTION_STATUS_NEW);

  error = NULL;

  priv->connecting_source = g_timeout_add_seconds (30,
      ring_connection_connecting_timeout, self);

  modems = modem_service ();
  modem = modem_service_find_modem (modems, priv->modem_path);

  if (modem)
    {
      ring_connection_modem_added (modems, modem, self);
    }
  else
    {
      priv->signals.modem_added = g_signal_connect (modems,
          "modem-added", G_CALLBACK (ring_connection_modem_added), self);
    }

  priv->signals.modem_removed = g_signal_connect (modems,
      "modem-removed", G_CALLBACK (ring_connection_modem_removed), self);

  return TRUE;
}

/**
 * ring_connection_connected
 *
 * Called after a connection becomes connected.
 */
static void
ring_connection_connected (TpBaseConnection *base)
{
  RingConnection *self = RING_CONNECTION (base);
  RingConnectionPrivate *priv = self->priv;

  DEBUG ("called");

  if (priv->connecting_source)
    {
      g_source_remove (priv->connecting_source);
      priv->connecting_source = 0;
    }
}

/**
 * ring_connection_disconnected
 *
 * Called after a connection becomes disconnected.
 *
 * Not called unless start_connecting has been called.
 */
static void
ring_connection_disconnected(TpBaseConnection *base)
{
  RingConnection *self = RING_CONNECTION(base);
  RingConnectionPrivate *priv = self->priv;

  DEBUG("called");

  if (priv->connecting_source)
    {
      g_source_remove (priv->connecting_source);
      priv->connecting_source = 0;
    }

  if (priv->sim)
    modem_sim_service_disconnect(priv->sim);
}

/** Called after connection has been disconnected.
 *
 * Shuts down connection and eventually calls
 * tp_base_connection_finish_shutdown().
 */
static void
ring_connection_shut_down(TpBaseConnection *base)
{
  RingConnection *self = RING_CONNECTION(base);
  RingConnectionPrivate *priv = self->priv;

  DEBUG("called");

  if (priv->signals.modem_added)
    {
      g_signal_handler_disconnect (modem_service (),
          priv->signals.modem_added);
      priv->signals.modem_added = 0;
    }

  if (priv->signals.modem_removed)
    {
      g_signal_handler_disconnect (modem_service (),
          priv->signals.modem_removed);
      priv->signals.modem_removed = 0;
    }

  if (priv->signals.modem_interfaces)
    {
      g_signal_handler_disconnect(priv->modem,
          priv->signals.modem_interfaces);
      priv->signals.modem_interfaces = 0;
    }

  if (priv->signals.sim_connected) {
    g_signal_handler_disconnect(priv->sim, priv->signals.sim_connected);
    priv->signals.sim_connected = 0;
  }

  if (priv->signals.imsi_notify) {
    g_signal_handler_disconnect(priv->sim, priv->signals.imsi_notify);
    priv->signals.imsi_notify = 0;
  }

  tp_base_connection_finish_shutdown(base);
}

static void
ring_connection_class_init_base_connection(TpBaseConnectionClass *klass)
{
  /* Implement pure-virtual methods */
#define IMPLEMENT(x) klass->x = ring_connection_##x
  IMPLEMENT(create_handle_repos);
  /* IMPLEMENT(create_channel_factories); */
  IMPLEMENT(get_unique_connection_name);

  /* IMPLEMENT(connecting) */
  IMPLEMENT (connected);
  IMPLEMENT(disconnected);
  IMPLEMENT(shut_down);
  IMPLEMENT(start_connecting);

  klass->interfaces_always_present =
    (char const **)ring_connection_interfaces_always_present;
  IMPLEMENT(create_channel_managers);
#undef IMPLEMENT
}

/* ---------------------------------------------------------------------- */
/* RingConnection interface */

static gboolean
ring_connection_connecting_timeout (gpointer _self)
{
  RingConnection *self = RING_CONNECTION (_self);
  RingConnectionPrivate *priv = self->priv;

  DEBUG ("enter");

  priv->connecting_source = 0;

  ring_connection_check_status (self);

  return FALSE;
}

static void
ring_connection_modem_added (ModemService *modems,
                             Modem *modem,
                             gpointer _self)
{
  RingConnection *self = RING_CONNECTION (_self);
  RingConnectionPrivate *priv = self->priv;
  char const *path;

  DEBUG ("enter");

  if (priv->modem)
    return;

  path = modem_get_modem_path (modem);
  g_assert (path != NULL);
  g_assert (priv->sim == NULL);

  if (priv->modem_path && strcmp (priv->modem_path, path))
    return;

  priv->modem = g_object_ref (modem);

  priv->signals.modem_interfaces = g_signal_connect (modem,
      "notify::interfaces",
      G_CALLBACK (on_modem_interfaces_changed), self);

  on_modem_interfaces_changed (modem, NULL, self);
}

static void
ring_connection_modem_removed (ModemService *modems,
                               Modem *modem,
                               gpointer _self)
{
  TpBaseConnection *base = TP_BASE_CONNECTION (_self);
  RingConnection *self = RING_CONNECTION (_self);
  RingConnectionPrivate *priv = self->priv;

  DEBUG ("enter");

  if (priv->modem != modem)
    return;

  if (base->status != TP_CONNECTION_STATUS_DISCONNECTED)
    {
      tp_base_connection_change_status (base,
          TP_CONNECTION_STATUS_DISCONNECTED,
          TP_CONNECTION_STATUS_REASON_NETWORK_ERROR);
    }
}

static void
on_modem_interfaces_changed (Modem *modem,
                             GParamSpec *spec,
                             gpointer _self)
{
  RingConnection *self = RING_CONNECTION (_self);
  RingConnectionPrivate *priv = self->priv;
  char const *path = modem_get_modem_path (modem);
  GError *error = NULL;

  if (modem_supports_sim (modem))
    {
      if (!priv->sim)
        {
          priv->sim = g_object_new (MODEM_TYPE_SIM_SERVICE,
              "object-path", path, NULL);

          priv->signals.sim_connected =
            g_signal_connect (priv->sim, "connected",
                G_CALLBACK (ring_connection_sim_connected), self);

          priv->signals.imsi_notify =
            g_signal_connect (self->priv->sim, "notify::imsi",
                G_CALLBACK (on_imsi_changed), self);

          if (!modem_sim_service_connect (priv->sim))
            DEBUG ("modem_sim_service_connect failed");
        }
    }

  if (modem_supports_call (modem))
    {
      if (!ring_media_manager_start_connecting (priv->media, path, &error))
        {
          DEBUG ("ring_media_manager_start_connecting: " GERROR_MSG_FMT,
              GERROR_MSG_CODE (error));
          g_clear_error (&error);
        }
    }

  if (modem_supports_sms (modem))
    {
      if (!ring_text_manager_start_connecting (priv->text, path, &error))
        {
          DEBUG ("ring_text_manager_start_connecting: " GERROR_MSG_FMT,
              GERROR_MSG_CODE (error));
          g_clear_error (&error);
        }
    }

  ring_connection_check_status (self);
}

static void
ring_connection_sim_connected(ModemSIMService *sim,
  gpointer _self)
{
  RingConnection *self = RING_CONNECTION(_self);

  DEBUG("enter");

  ring_connection_check_status(self);
}

/* Check for status transition from connecting status */
gboolean
ring_connection_check_status(RingConnection *self)
{
  TpBaseConnection *base = TP_BASE_CONNECTION (self);
  RingConnectionPrivate *priv = self->priv;
  TpConnectionStatus conn, modem, sim, text, media;

  conn = base->status;

  modem = !priv->modem
    ? TP_INTERNAL_CONNECTION_STATUS_NEW
    : modem_is_powered (priv->modem)
    ? TP_CONNECTION_STATUS_CONNECTED
    : TP_CONNECTION_STATUS_CONNECTING;

  sim = !priv->sim
    ? TP_INTERNAL_CONNECTION_STATUS_NEW
    : modem_sim_service_is_connected (priv->sim)
    ? TP_CONNECTION_STATUS_CONNECTED
    : modem_sim_service_is_connecting (priv->sim)
    ? TP_CONNECTION_STATUS_CONNECTING
    : TP_CONNECTION_STATUS_DISCONNECTED;

  media = ring_media_manager_get_status(priv->media);
  text = ring_text_manager_get_status(priv->text);

  DEBUG ("%s - MODEM %s, SIM %s, CALL %s, SMS %s%s",
      ring_connection_status_as_string (conn),
      ring_connection_status_as_string (modem),
      ring_connection_status_as_string (sim),
      ring_connection_status_as_string (media),
      ring_connection_status_as_string (text),
      conn != TP_CONNECTION_STATUS_CONNECTING
      ? ""
      : priv->connecting_source
      ? ", timer running" : ", timer expired");

  if (modem == TP_CONNECTION_STATUS_CONNECTED &&
      sim == TP_CONNECTION_STATUS_CONNECTED &&
      (media == TP_CONNECTION_STATUS_CONNECTED ||
          text == TP_CONNECTION_STATUS_CONNECTED))
    {
      if (base->status != TP_CONNECTION_STATUS_CONNECTED)
        tp_base_connection_change_status (base,
            TP_CONNECTION_STATUS_CONNECTED,
            TP_CONNECTION_STATUS_REASON_REQUESTED);

      return TRUE;
    }

  if (priv->connecting_source)
    return TRUE;

  if (base->status != TP_CONNECTION_STATUS_DISCONNECTED)
    {
      tp_base_connection_change_status (base,
          TP_CONNECTION_STATUS_DISCONNECTED,
          TP_CONNECTION_STATUS_REASON_NETWORK_ERROR);
    }

  return FALSE;
}

char const *
ring_connection_inspect_contact(RingConnection const *self,
  TpHandle contact)
{
  TpHandleRepoIface *repo;

  if (contact == 0)
    return "";

  repo = tp_base_connection_get_handles(TP_BASE_CONNECTION(self),
         TP_HANDLE_TYPE_CONTACT);

  return tp_handle_inspect(repo, contact);
}

gpointer
ring_connection_lookup_channel(RingConnection const *self,
  char const *object_path)
{
  gpointer channel;

  channel = ring_media_manager_lookup(self->priv->media, object_path);
  if (channel == NULL)
    channel = ring_text_manager_lookup(self->priv->text, object_path);

  return channel;
}

gboolean
ring_connection_validate_initial_members(RingConnection *self,
  RingInitialMembers *initial,
  GError **error)
{
  return ring_media_manager_validate_initial_members(
    self->priv->media, initial, error);
}

/* ---------------------------------------------------------------------- */
/* org.freedesktop.Telepathy.Connection.Interface.Anonymity */
static TpDBusPropertiesMixinPropImpl
ring_connection_anon_properties[] = {
  { "SupportedAnonymityModes", "anon-supported-modes", "anon-supported-modes" },
  { "AnonymityMandatory", "anon-mandatory", "anon-mandatory" },
  { "AnonymityModes", "anon-modes", "anon-modes" },
  { NULL }
};

/* ---------------------------------------------------------------------- */
/* org.freedesktop.Telepathy.Connection.Interface.Cellular */
static TpDBusPropertiesMixinPropImpl
ring_connection_cellular_properties[] = {
  { "IMSI", "imsi", "imsi" },
  { "MessageValidityPeriod", "sms-validity-period", "sms-validity-period" },
  { "MessageServiceCentre", "sms-service-centre", "sms-service-centre" },
  { "MessageReducedCharacterSet", "sms-reduced-charset", "sms-reduced-charset" },
  { NULL }
};

static gboolean
ring_connection_cellular_properties_setter(GObject *object,
  GQuark interface,
  GQuark aname,
  const GValue *value,
  gpointer setter_data,
  GError **error)
{
  char const *name = setter_data;
  TpCMParamSpec const *param_spec;

  if (name == NULL) {
    g_set_error(error, TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
      "This property is read-only");
    return FALSE;
  }

  if (strcmp(name, "sms-validity-period") == 0) {
    param_spec = CELLULAR_SMS_VALIDITY_PERIOD_PARAM_SPEC;
  }
  else if (strcmp(name, "sms-service-centre") == 0) {
    param_spec = CELLULAR_SMS_SERVICE_CENTRE_PARAM_SPEC;
  }
  else if (strcmp(name, "sms-reduced-charset") == 0) {
    param_spec = CELLULAR_SMS_REDUCED_CHARSET_PARAM_SPEC;
  }
  else  {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Unknown property");
    return FALSE;
  }

  if (param_spec->filter && !param_spec->filter(
      param_spec, (GValue *)value, error)) {
    return FALSE;
  }

  g_object_set_property(object, name, value);

  return TRUE;
}

/* ---------------------------------------------------------------------- */
/* Connection.Interface.ServicePoint */

static TpDBusPropertiesMixinPropImpl
ring_connection_service_point_properties[] = {
  { "KnownServicePoints", "known-service-points" },
  { NULL}
};

/* ---------------------------------------------------------------------- */
/* Connection.Interface.StoredMessages */

#if nomore
static TpDBusPropertiesMixinPropImpl
ring_connection_stored_messages_properties[] = {
  { "StoredMessages", "stored-messages" },
  { NULL }
};

static void
ring_connection_deliver_stored_messages(
  RTComTpSvcConnectionInterfaceStoredMessages *iface,
  char const **messages,
  DBusGMethodInvocation *context)
{
  ring_text_manager_deliver_stored_messages(
    RING_CONNECTION(iface)->priv->text,
    messages,
    context);
}

static void
ring_connection_expunge_messages(
  RTComTpSvcConnectionInterfaceStoredMessages *iface,
  char const **messages,
  DBusGMethodInvocation *context)
{
#if nomore
  ring_text_manager_expunge_messages(
    RING_CONNECTION(iface)->priv->text,
    messages,
    context);
#endif
}

static void
ring_connection_set_storage_state(
  RTComTpSvcConnectionInterfaceStoredMessages *iface,
  gboolean out_of_storage,
  DBusGMethodInvocation *context)
{
#if nomore
  ring_text_manager_set_storage_state(
    RING_CONNECTION(iface)->priv->text,
    out_of_storage,
    context);
#endif
}

static void
ring_connection_stored_messages_iface_init(gpointer g_iface, gpointer iface_data)
{
  RTComTpSvcConnectionInterfaceStoredMessagesClass *klass = g_iface;

#define IMPLEMENT(x)                                                    \
  rtcom_tp_svc_connection_interface_stored_messages_implement_##x(      \
    klass, ring_connection_ ## x)

  IMPLEMENT(deliver_stored_messages);
  IMPLEMENT(expunge_messages);
  IMPLEMENT(set_storage_state);

#undef IMPLEMENT
}
#endif

/* ---------------------------------------------------------------------- */
/* Connection.Interface.Capabilities */

static GValueArray *
ring_capability_pair_new(char const *channel_type,
  guint type_specific_flags)
{
  GValue value[1];

  memset(value, 0, sizeof value);

  g_value_init(value, TP_STRUCT_TYPE_CAPABILITY_PAIR);
  g_value_take_boxed(value,
    dbus_g_type_specialized_construct(TP_STRUCT_TYPE_CAPABILITY_PAIR));
  dbus_g_type_struct_set(value,
    0, channel_type,
    1, type_specific_flags,
    G_MAXUINT);

  return g_value_get_boxed(value);
}

static void
ring_capability_pair_free(gpointer value)
{
  g_boxed_free(TP_STRUCT_TYPE_CAPABILITY_PAIR, value);
}

static GValueArray *
ring_capability_change_new(guint handle,
  char const *channel_type,
  guint old_generic,
  guint new_generic,
  guint old_specific,
  guint new_specific)
{
  GValue value[1];

  memset(value, 0, sizeof value);

  g_value_init(value, TP_STRUCT_TYPE_CAPABILITY_CHANGE);
  g_value_take_boxed(value,
    dbus_g_type_specialized_construct(TP_STRUCT_TYPE_CAPABILITY_CHANGE));
  dbus_g_type_struct_set(value,
    0, handle,
    1, channel_type,
    2, old_generic,
    3, new_generic,
    4, old_specific,
    5, new_specific,
    G_MAXUINT);

  return g_value_get_boxed(value);
}

static void
ring_capability_change_free(gpointer value)
{
  g_boxed_free(TP_STRUCT_TYPE_CAPABILITY_CHANGE, value);
}

GValueArray *
ring_contact_capability_new(guint handle,
  char const *channel_type,
  guint generic,
  guint specific)
{
  GValue value[1];

  memset(value, 0, sizeof value);

  g_value_init(value, TP_STRUCT_TYPE_CONTACT_CAPABILITY);
  g_value_take_boxed(value,
    dbus_g_type_specialized_construct(TP_STRUCT_TYPE_CONTACT_CAPABILITY));
  dbus_g_type_struct_set(value,
    0, handle,
    1, channel_type,
    2, generic,
    3, specific,
    G_MAXUINT);

  return g_value_get_boxed(value);
}

void
ring_contact_capability_free(gpointer value)
{
  g_boxed_free(TP_STRUCT_TYPE_CONTACT_CAPABILITY, value);
}

static void
ring_connection_advertise_capabilities(
  TpSvcConnectionInterfaceCapabilities *iface,
  const GPtrArray *add,
  const char **remove,
  DBusGMethodInvocation *context)
{
  RingConnection *self = RING_CONNECTION(iface);
  RingConnectionPrivate *priv = self->priv;
  TpBaseConnection *base = TP_BASE_CONNECTION(self);
  guint handle;
  char const media[] = TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA;
  char const text[] = TP_IFACE_CHANNEL_TYPE_TEXT;
  guint add_media = 0, add_text = 0;
  guint remove_media = 0, remove_text = 0;
  guint old_media = 0, old_text = 0;
  guint new_media = 0, new_text = 0;
  GPtrArray *returns, *emits;
  guint i;

  for (i = 0; i < add->len; i++) {
    char *channel_type;
    guint type_specific_flags;
    GValue capability_pair[1];

    memset(capability_pair, 0, sizeof capability_pair);
    g_value_init(capability_pair, TP_STRUCT_TYPE_CAPABILITY_PAIR);
    g_value_set_static_boxed(capability_pair, add->pdata[i]);

    dbus_g_type_struct_get(capability_pair,
      0, &channel_type,
      1, &type_specific_flags,
      G_MAXUINT);

    if (g_str_equal(channel_type, media))
      add_media |= type_specific_flags;
    else if (g_str_equal(channel_type, text))
      add_text |= type_specific_flags;

    g_free(channel_type);
  }

  for (i = 0; remove[i]; i++) {
    char const *channel_type = remove[i];
    if (g_str_equal(channel_type, media))
      remove_media |= ~0;
    else if (g_str_equal(channel_type, text))
      remove_text |= ~0;
  }

  returns = g_ptr_array_sized_new(2);

  g_object_get(priv->media, "capability-flags", &old_media, NULL);
  g_object_set(priv->media,
    "capability-flags", (old_media | add_media) & ~ remove_media,
    NULL);
  g_object_get(priv->media, "capability-flags", &new_media, NULL);

  if (new_media) {
    g_ptr_array_add(returns, ring_capability_pair_new(media, new_media));
  }
  if (old_media != new_media)
    DEBUG("changed %s caps %x (old was %x)",
      "StreamedMedia", new_media, old_media);

  g_object_get(priv->text, "capability-flags", &old_text, NULL);
  g_object_set(priv->text,
    "capability-flags", (old_text | add_text) & ~ remove_text,
    NULL);
  g_object_get(priv->text, "capability-flags", &new_text, NULL);

  if (new_text) {
    g_ptr_array_add(returns, ring_capability_pair_new(text, new_text));
  }
  if (old_text != new_text)
    DEBUG("changed %s caps %x (old was %x)", "Text", new_text, old_text);

  tp_svc_connection_interface_capabilities_return_from_advertise_capabilities(
    context, returns);
  for (i = 0; i < returns->len; i++)
    ring_capability_pair_free(returns->pdata[i]);
  g_ptr_array_free(returns, TRUE);

  if (old_media == new_media && old_text == new_text)
    return;

  /* Emit CapabilitiesChanged */
  handle = tp_base_connection_get_self_handle(base);
  emits = g_ptr_array_sized_new(2);

  if (old_media != new_media) {
    guint generic =
      TP_CONNECTION_CAPABILITY_FLAG_CREATE |
      TP_CONNECTION_CAPABILITY_FLAG_INVITE;

    g_ptr_array_add(emits, ring_capability_change_new(handle, media,
        old_media ? generic : 0, new_media ? generic : 0,
        old_media, new_media));
  }
  if (old_text != new_text) {
    guint generic = TP_CONNECTION_CAPABILITY_FLAG_CREATE;

    g_ptr_array_add(emits, ring_capability_change_new(handle, text,
        old_text ? generic : 0, new_text ? generic : 0,
        old_text, new_text));
  }

  DEBUG("emitting CapabilitiesChanged");
  tp_svc_connection_interface_capabilities_emit_capabilities_changed(self,
    emits);

  for (i = 0; i < emits->len; i++)
    ring_capability_change_free(emits->pdata[i]);
  g_ptr_array_free(emits, TRUE);
}

static void
ring_connection_get_capabilities(
  TpSvcConnectionInterfaceCapabilities *iface,
  GArray const *handles,
  DBusGMethodInvocation *context)
{
  RingConnection *self = RING_CONNECTION(iface);
  RingConnectionPrivate *priv = self->priv;
  TpBaseConnection *base = TP_BASE_CONNECTION(self);
  GError *error = NULL;
  TpHandleRepoIface *repo;
  GPtrArray *returns;
  guint i, n = handles->len;
  guint const *array = (gpointer)handles->data;

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED(base, context);

  repo = tp_base_connection_get_handles(base, TP_HANDLE_TYPE_CONTACT);

  if (!tp_handles_are_valid(repo, handles, TRUE, &error)) {
    dbus_g_method_return_error(context, error);
    g_error_free(error);
    return;
  }

  returns = g_ptr_array_sized_new(2 * n);

  for (i = 0; i < n; i++) {
    guint handle = array[i];

    if (handle != 0) {
      ring_media_manager_add_capabilities(priv->media, handle, returns);
      ring_text_manager_add_capabilities(priv->text, handle, returns);
    }
  }

  tp_svc_connection_interface_capabilities_return_from_get_capabilities(context,
    returns);

  for (i = 0; i < returns->len; i++)
    ring_contact_capability_free(returns->pdata[i]);
  g_ptr_array_free(returns, TRUE);
}

static void
ring_connection_capabilities_iface_init(gpointer g_iface,
  gpointer iface_data)
{
  TpSvcConnectionInterfaceCapabilitiesClass *klass = g_iface;

#define IMPLEMENT(x)                                            \
  tp_svc_connection_interface_capabilities_implement_##x (      \
    klass, ring_connection_##x)
  IMPLEMENT(advertise_capabilities);
  IMPLEMENT(get_capabilities);
#undef IMPLEMENT
}

/** Add contact attributes belonging to interface
 * org.freedesktop.Telepathy.Connection.Interface.Capabilities
 */
static void
ring_connection_add_contact_capabilities(GObject *object,
  GArray const *handles,
  GHashTable *returns)
{
  RingConnection *self = RING_CONNECTION(object);
  RingConnectionPrivate *priv = self->priv;
  guint i, n = handles->len;
  guint const *array = (gpointer)handles->data;

  for (i = 0; i < n; i++) {
    GPtrArray *caps = g_ptr_array_sized_new(2);
    GValue *value;
    guint handle = array[i];

    if (handle != 0) {
      ring_media_manager_add_capabilities(priv->media, handle, caps);
      ring_text_manager_add_capabilities(priv->text, handle, caps);
    }

    if (caps->len) {
      value = tp_g_value_slice_new(TP_ARRAY_TYPE_CONTACT_CAPABILITY_LIST);

      g_value_take_boxed(value, caps);

      tp_contacts_mixin_set_contact_attribute(returns, handle,
        TP_IFACE_CONNECTION_INTERFACE_CAPABILITIES "/caps",
        value);
    }
    else {
      g_ptr_array_free(caps, TRUE);
    }
  }
}

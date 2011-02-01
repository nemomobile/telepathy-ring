/*
 * modem/sms-service.c - ModemSMSService class
 *
 * Copyright (C) 2008-2010 Nokia Corporation
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

#include "config.h"

#define MODEM_DEBUG_FLAG MODEM_LOG_SMS

#include "debug.h"

#include "modem/sms.h"
#include "modem/request-private.h"
#include "modem/errors.h"

#if nomore
#include "sms-glib/deliver.h"
#include "sms-glib/status-report.h"
#endif

#include "modem/ofono.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "signals-marshal.h"

#include <uuid/uuid.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ---------------------------------------------------------------------- */

G_DEFINE_TYPE (ModemSMSService, modem_sms_service, MODEM_TYPE_OFACE)

/* Signals we emit */
enum
{
  SIGNAL_INCOMING_MESSAGE,
  SIGNAL_IMMEDIATE_MESSAGE,
#if nomore
  SIGNAL_DELIVER,
  SIGNAL_OUTGOING_COMPLETE,
  SIGNAL_OUTGOING_ERROR,
  SIGNAL_STATUS_REPORT,
#endif
  N_SIGNALS
};

static guint signals[N_SIGNALS];

/* Properties */
enum
  {
    PROP_NONE,
    PROP_CONTENT_TYPES,
    PROP_SMSC,
    PROP_VALIDITY_PERIOD,
    PROP_REDUCED_CHARSET,
    LAST_PROPERTY
  };

/* private data */
struct _ModemSMSServicePrivate
{
  time_t connected;             /* Timestamp when got connected */

  char *smsc;
  guint validity_period;

#if nomore
  char **content_types;

  GHashTable *received;
#endif

  unsigned reduced_charset:1;
  unsigned signals:1, :0;
};

/* ------------------------------------------------------------------------ */

#if nomore
static void modem_sms_incoming_deliver (ModemSMSService *self,
    SMSGDeliver *deliver);
#endif

static void on_incoming_message (DBusGProxy *, char const *, GHashTable *, gpointer);
static void on_immediate_message (DBusGProxy *, char const *, GHashTable *, gpointer);
static void on_manager_message_added (DBusGProxy *, char const *, GHashTable *,
    gpointer);
static void on_manager_message_removed (DBusGProxy *, char const *, gpointer);

/* ------------------------------------------------------------------------ */
/* GObject interface */

static void
modem_sms_service_init (ModemSMSService *self)
{
  DEBUG ("enter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      MODEM_TYPE_SMS_SERVICE, ModemSMSServicePrivate);

#if nomore
  self->priv->received = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, /* Message object stored in hash owns the key */
      g_object_unref);
#endif
}

static void
modem_sms_service_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  ModemSMSService *self = MODEM_SMS_SERVICE (object);
  ModemSMSServicePrivate *priv = self->priv;

  switch (property_id)
    {
#if nomore
    case PROP_CONTENT_TYPES:
      g_value_set_boxed (value, priv->content_types);
      break;
#endif

    case PROP_SMSC:
      g_value_set_string (value, priv->smsc);
      break;

    case PROP_VALIDITY_PERIOD:
      g_value_set_uint (value, priv->validity_period);
      break;

    case PROP_REDUCED_CHARSET:
      g_value_set_boolean (value, priv->reduced_charset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_sms_service_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  ModemSMSService *self = MODEM_SMS_SERVICE (object);
  ModemSMSServicePrivate *priv = self->priv;
  gpointer old;

  switch (property_id)
    {
#if nomore
    case PROP_CONTENT_TYPES:
      old = priv->content_types;
      priv->content_types = g_value_dup_boxed (value);
      if (old) g_boxed_free (G_TYPE_STRV, old);
      break;
#endif

    case PROP_SMSC:
      old = priv->smsc;
      priv->smsc = g_value_dup_string (value);
      g_free (old);
      break;

    case PROP_VALIDITY_PERIOD:
      priv->validity_period = g_value_get_uint (value);
      break;

    case PROP_REDUCED_CHARSET:
      priv->reduced_charset = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_sms_service_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (modem_sms_service_parent_class)->constructed)
    G_OBJECT_CLASS (modem_sms_service_parent_class)->constructed (object);

  if (modem_oface_dbus_proxy (MODEM_OFACE (object)) == NULL)
	  g_warning("object created without dbus-proxy set");
}

static void
modem_sms_service_dispose (GObject *object)
{
  if (G_OBJECT_CLASS (modem_sms_service_parent_class)->dispose)
    G_OBJECT_CLASS (modem_sms_service_parent_class)->dispose (object);
}

static void
modem_sms_service_finalize (GObject *object)
{
  ModemSMSService *self = MODEM_SMS_SERVICE (object);
  ModemSMSServicePrivate *priv = self->priv;

  DEBUG ("enter");

  /* Free any data held directly by the object here */
  g_free (priv->smsc);

#if nomore
  if (priv->content_types)
    g_boxed_free (G_TYPE_STRV, priv->content_types);

  if (priv->received)
    g_hash_table_destroy (priv->received);
#endif

  G_OBJECT_CLASS (modem_sms_service_parent_class)->finalize (object);
}

/* ------------------------------------------------------------------------- */
/* oface */

static void reply_to_sms_manager_get_messages (ModemOface *_self,
    ModemRequest *request, GPtrArray *array, GError const *error,
    gpointer user_data);

static char const *
modem_sms_service_property_mapper (char const *name)
{
  if (!strcmp (name, "UseDeliveryReports"))
    return NULL;
  if (!strcmp (name, "ServiceCenterAddress"))
    return "service-centre";
  if (!strcmp (name, "Bearer"))
    return NULL;

  return NULL;
}

static void
modem_sms_service_connect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  ModemSMSService *self = MODEM_SMS_SERVICE (_self);
  ModemSMSServicePrivate *priv = self->priv;
  DBusGProxy *proxy = modem_oface_dbus_proxy (_self);

  if (!priv->signals)
    {
      priv->signals = TRUE;

#define CONNECT(handler, name, signature...) \
    dbus_g_proxy_add_signal (proxy, (name), ##signature); \
    dbus_g_proxy_connect_signal (proxy, (name), G_CALLBACK (handler), self, NULL)

      CONNECT (on_immediate_message, "ImmediateMessage",
          G_TYPE_STRING, MODEM_TYPE_DBUS_DICT, G_TYPE_INVALID);

      CONNECT (on_incoming_message, "IncomingMessage",
          G_TYPE_STRING, MODEM_TYPE_DBUS_DICT, G_TYPE_INVALID);

      CONNECT (on_manager_message_added, "MessageAdded",
          DBUS_TYPE_G_OBJECT_PATH, MODEM_TYPE_DBUS_DICT, G_TYPE_INVALID);

      CONNECT (on_manager_message_added, "MessageRemoved",
          DBUS_TYPE_G_OBJECT_PATH, MODEM_TYPE_DBUS_DICT, G_TYPE_INVALID);
    }

  modem_oface_connect_properties (_self, TRUE);

  modem_oface_add_connect_request (_self,
      modem_oface_request_managed (_self, "GetMessages",
          reply_to_sms_manager_get_messages, NULL));
}

static void
reply_to_sms_manager_get_messages (ModemOface *_self,
                                   ModemRequest *request,
                                   GPtrArray *array,
                                   GError const *error,
                                   gpointer user_data)
{
  ModemSMSService *self = MODEM_SMS_SERVICE (_self);

  DEBUG ("(%p): enter", _self);

  if (!error)
    {
      guint i;

      for (i = 0; i < array->len; i++)
        {
          GValueArray *va = g_ptr_array_index (array, i);
          char const *path = g_value_get_boxed (va->values + 0);
          GHashTable *properties = g_value_get_boxed (va->values + 1);

          on_manager_message_added (NULL, path, properties, self);
        }
    }

  modem_oface_check_connected (_self, request, error);
}

static void
modem_sms_service_connected (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  ModemSMSService *self = MODEM_SMS_SERVICE (_self);
  ModemSMSServicePrivate *priv = self->priv;
  time_t now = time (NULL);

  priv->connected = now;

#if nomore
  GHashTableIter i[1];
  gpointer key, value;

  for (g_hash_table_iter_init (i, priv->received);
       g_hash_table_iter_next (i, &key, &value);)
    {
      g_object_set (value, "time-delivered", (guint64)now, NULL);
    }
#endif
}

static void
modem_sms_service_disconnect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  ModemSMSService *self = MODEM_SMS_SERVICE (_self);
  ModemSMSServicePrivate *priv = self->priv;
  DBusGProxy *proxy = modem_oface_dbus_proxy (_self);

  if (priv->signals)
    {
      priv->signals = FALSE;

      dbus_g_proxy_disconnect_signal (proxy, "IncomingMessage",
          G_CALLBACK (on_immediate_message), self);
      dbus_g_proxy_disconnect_signal (proxy, "ImmediateMessage",
          G_CALLBACK (on_incoming_message), self);
      dbus_g_proxy_disconnect_signal (proxy, "MessageAdded",
          G_CALLBACK (on_manager_message_added), self);
      dbus_g_proxy_disconnect_signal (proxy, "MessageRemoved",
          G_CALLBACK (on_manager_message_removed), self);
    }

#if nomore
  g_hash_table_remove_all (priv->received);
#endif

  modem_oface_disconnect_properties (_self);
}

/* ------------------------------------------------------------------------- */
static void
modem_sms_service_class_init (ModemSMSServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ModemOfaceClass *oface_class = MODEM_OFACE_CLASS (klass);

  DEBUG ("enter");

  object_class->get_property = modem_sms_service_get_property;
  object_class->set_property = modem_sms_service_set_property;
  object_class->constructed = modem_sms_service_constructed;
  object_class->dispose = modem_sms_service_dispose;
  object_class->finalize = modem_sms_service_finalize;

  oface_class->ofono_interface = MODEM_OFACE_SMS;
  oface_class->property_mapper = modem_sms_service_property_mapper;
  oface_class->connect = modem_sms_service_connect;
  oface_class->connected = modem_sms_service_connected;
  oface_class->disconnect = modem_sms_service_disconnect;

#if nomore
  g_object_class_install_property (object_class, PROP_CONTENT_TYPES,
      g_param_spec_boxed ("content-types",
          "Content types used",
          "List of MIME content types used by application.",
          G_TYPE_STRV,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));
#endif

  g_object_class_install_property (object_class, PROP_SMSC,
      g_param_spec_string ("service-centre",
          "SMS Service Centre",
          "ISDN Address for SMS Service Centre",
          "", /* default value */
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_VALIDITY_PERIOD,
      g_param_spec_uint ("validity-period",
          "SMS Validity Period",
          "Period while SMS service centre "
          "keep trying to deliver SMS.",
          /* anything above 0 gets rounded up to 5 minutes */
          0,  /* 0 means no validity period */
          63 * 7 * 24 * 60 * 60, /* max - 63 weeks */
          0, /* no validity period - it is up to service centre */
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_REDUCED_CHARSET,
      g_param_spec_boolean ("reduced-charset",
          "SMS reduced character set support",
          "Whether SMS should be encoded with "
          "a reduced character set",
          FALSE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));

  signals[SIGNAL_IMMEDIATE_MESSAGE] =
    g_signal_new ("immediate-message",
        G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        _modem__marshal_VOID__STRING_BOXED,
        G_TYPE_NONE, 2,
        G_TYPE_STRING, G_TYPE_HASH_TABLE);

  signals[SIGNAL_INCOMING_MESSAGE] =
    g_signal_new ("incoming-message",
        G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        _modem__marshal_VOID__STRING_BOXED,
        G_TYPE_NONE, 2,
        G_TYPE_STRING, G_TYPE_HASH_TABLE);

#if nomore
  signals[SIGNAL_DELIVER] =
    g_signal_new ("deliver",
        G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        G_TYPE_OBJECT);

  signals[SIGNAL_OUTGOING_COMPLETE] =
    g_signal_new ("outgoing-complete",
        G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        _modem__marshal_VOID__STRING_STRING,
        G_TYPE_NONE, 2,
        G_TYPE_STRING, G_TYPE_STRING);

  signals[SIGNAL_OUTGOING_ERROR] =
    g_signal_new ("outgoing-error",
        G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        _modem__marshal_VOID__STRING_STRING_POINTER,
        G_TYPE_NONE, 3,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

  signals[SIGNAL_STATUS_REPORT] =
    g_signal_new ("state-report",
        G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        G_TYPE_OBJECT);

#endif

  g_type_class_add_private (klass, sizeof (ModemSMSServicePrivate));
}

/* ---------------------------------------------------------------------- */
/* Signal connection helpers */

#if nomore
gulong
modem_sms_connect_to_connected (ModemSMSService *self,
                                ModemSMSConnectedHandler *handler,
                                gpointer data)
{
  return g_signal_connect (self, "connected", G_CALLBACK (handler), data);
}

gulong
modem_sms_connect_to_deliver (ModemSMSService *self,
                              ModemSMSDeliverHandler *handler,
                              gpointer data)
{
  return g_signal_connect (self, "deliver", G_CALLBACK (handler), data);
}
#endif

gulong
modem_sms_connect_to_incoming_message (ModemSMSService *self,
                                       ModemSMSMessageHandler *handler,
                                       gpointer data)
{
  return g_signal_connect (self, "incoming-message",
      G_CALLBACK (handler), data);
}

gulong
modem_sms_connect_to_immediate_message (ModemSMSService *self,
                                        ModemSMSMessageHandler *handler,
                                        gpointer data)
{
  return g_signal_connect (self, "immediate-message",
      G_CALLBACK (handler), data);
}

/* ------------------------------------------------------------------------- */
/* modem_sms_service interface */

guint64
modem_sms_service_time_connected (ModemSMSService const *self)
{
  if (MODEM_IS_SMS_SERVICE (self))
    return (guint64)self->priv->connected;
  else
    return 0;
}

gint64
modem_sms_parse_time (gchar const *s)
{
  struct tm tm = { };
  char *rest;

  rest = strptime (s, "%Y-%m-%dT%H:%M:%S%z", &tm);

  return (gint64) mktime (&tm);
}


/* ------------------------------------------------------------------------- */

static void
on_manager_message_added (DBusGProxy *proxy,
                          char const *path,
                          GHashTable *properties,
                          gpointer user_data)
{
  ModemSMSService *self = MODEM_SMS_SERVICE (user_data);

  DEBUG ("%s", path);

  (void)self;
}

static void
on_manager_message_removed (DBusGProxy *proxy,
                            char const *path,
                            gpointer user_data)
{
  ModemSMSService *self = MODEM_SMS_SERVICE (user_data);

  DEBUG ("%s", path);

  (void)self;
}

/* FIXME: something for Ofono... */
#if nomore
static char *
modem_sms_generate_token (void)
{
  char *token;
  uuid_t uu;

  token = g_new (gchar, 37);
  uuid_generate_random (uu);
  uuid_unparse_lower (uu, token);

  return token;
}
#endif

static void
dump_message_dict (GHashTable *dict)
{
  char *key;
  GValue *value;
  GHashTableIter iter[1];

  g_hash_table_iter_init (iter, dict);
  while (g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&value))
    {
      char *s = g_strdup_value_contents (value);
      DEBUG ("%s = %s", key, s);
      g_free (s);
    }
}

#if nomore

static void
on_incoming_message (DBusGProxy *proxy,
                     char const *message,
                     GHashTable *dict,
                     gpointer _self)
{
  char const *smsc = "1234567";
  char const *type = "text/plain";
  char const *originator = "";

  char *token;
  GError *error = NULL;
  GValue *value;
  SMSGDeliver *d;
  ModemSMSService *self = MODEM_SMS_SERVICE (_self);

  DEBUG ("message = \"%s\"", message);

  dump_message_dict (dict);

  value = g_hash_table_lookup (dict, "Sender");
  if (value)
    originator = g_value_get_string (value);

  token = modem_sms_generate_token ();

  d = sms_g_deliver_incoming (message, token, originator,
      smsc, type, NULL, &error);

  if (!d)
    {
      modem_message (MODEM_LOG_SMS,
          "deserializing SMS-DELIVER \"%s\" failed: "
          GERROR_MSG_FMT, token, GERROR_MSG_CODE (error));
      g_clear_error (&error);
    }
  else {
    modem_sms_incoming_deliver (self, d);
    g_object_unref (d);
  }

  g_free (token);
}

#endif

/* ------------------------------------------------------------------------- */

gboolean
modem_sms_service_is_error_serious (GError *error)
{
  if (error == NULL)
    return FALSE;

  return TRUE;
}

/* ---------------------------------------------------------------------- */
/* Properties */

ModemRequest *
modem_sms_set_sc_address (ModemSMSService *self,
                          char const *address,
                          ModemSMSServiceReply *callback,
                          gpointer user_data)
{
  GValue value[1];

  g_value_init (memset (value, 0, sizeof value), G_TYPE_STRING);
  g_value_set_string (value, address);

  return modem_oface_set_property_req (MODEM_OFACE (self),
      "ServiceCenterAddress", value, (void *)callback, user_data);
}

ModemRequest *
modem_sms_set_srr (ModemSMSService *self,
                   gboolean srr,
                   ModemSMSServiceReply *callback,
                   gpointer user_data)
{
  GValue value[1];

  g_value_init (memset (value, 0, sizeof value), G_TYPE_BOOLEAN);
  g_value_set_boolean (value, srr);

  return modem_oface_set_property_req (MODEM_OFACE (self),
      "UseDeliveryReports", value, (void *)callback, user_data);
}

/* ---------------------------------------------------------------------- */
/* Message deliver */

#if nomore
static void
modem_sms_incoming_deliver (ModemSMSService *self, SMSGDeliver *deliver)
{
  ModemSMSServicePrivate *priv = self->priv;
  gchar const *content_type = sms_g_deliver_get_content_type (deliver);
  gchar const *token = sms_g_deliver_get_message_token (deliver);

  if (priv->content_types)
    {
      int i;

      for (i = 0; priv->content_types[i]; i++)
        {
          if (g_ascii_strcasecmp (content_type, priv->content_types[i]) == 0)
            break;
        }

      if (priv->content_types[i] == NULL)
        {
          DEBUG ("SMS-DELIVER containing %s is ignored", content_type);
          return;
        }
    }

  if (g_hash_table_lookup (priv->received, (gpointer)token))
    {
      DEBUG ("SMS-DELIVER %s already read, ignoring", token);
      return;
    }

  g_hash_table_insert (priv->received, (gpointer)token, g_object_ref (deliver));

  DEBUG ("SMS-DELIVER connected:%ld", priv->connected);

  if (priv->connected)
    g_signal_emit (self, signals[SIGNAL_DELIVER], 0, deliver);
}

static void
modem_sms_incoming_status_report (ModemSMSService *self,
                                  SMSGStatusReport *sr)
{
  ModemSMSServicePrivate *priv = self->priv;
  gchar const *token = sms_g_status_report_get_message_token (sr);

  if (g_hash_table_lookup (priv->received, (gpointer)token))
    {
      DEBUG ("SMS-STATUS REPORT %s already read, ignoring", token);
      return;
    }
  g_hash_table_insert (priv->received, (gpointer)token, g_object_ref (sr));

  if (priv->connected)
    g_signal_emit (self, signals[SIGNAL_STATUS_REPORT], 0, sr);
}
#endif

static void
on_immediate_message (DBusGProxy *proxy,
                      char const *message,
                      GHashTable *dict,
                      gpointer _self)
{
  ModemSMSService *self = MODEM_SMS_SERVICE (_self);
  ModemSMSServicePrivate *priv = self->priv;

  if (!priv->connected)
    return;

  DEBUG ("immediate = \"%50s\"%s", message, strlen (message) > 50 ? "..." : "");
  dump_message_dict (dict);

  g_signal_emit (self, signals[SIGNAL_IMMEDIATE_MESSAGE], 0,
      message, dict);
}

static void
on_incoming_message (DBusGProxy *proxy,
                     char const *message,
                     GHashTable *dict,
                     gpointer _self)
{
  ModemSMSService *self = MODEM_SMS_SERVICE (_self);
  ModemSMSServicePrivate *priv = self->priv;

  if (!priv->connected)
    return;

  DEBUG ("incoming = \"%50s\"%s", message, strlen (message) > 50 ? "..." : "");
  dump_message_dict (dict);

  g_signal_emit (self, signals[SIGNAL_INCOMING_MESSAGE], 0,
      message, dict);
}


/* ---------------------------------------------------------------------- */
/* Sending */

static void
reply_to_send_message (DBusGProxy *proxy,
                       DBusGProxyCall *call,
                       void *_request)
{
  ModemRequest *request = _request;
  ModemSMSService *self = modem_request_object (request);
  ModemSMSServiceSendReply *callback = modem_request_callback (request);
  gpointer user_data = modem_request_user_data (request);
  char const *message_path = NULL;

  GError *error = NULL;

  if (dbus_g_proxy_end_call (proxy, call, &error,
          DBUS_TYPE_G_OBJECT_PATH, &message_path,
          G_TYPE_INVALID))
    {
      char const *destination;

      destination = modem_request_get_data (request, "destination");
    }

  callback (self, request, message_path, error, user_data);

  g_clear_error (&error);
}

ModemRequest *
modem_sms_request_send (ModemSMSService *self,
                        char const *to, char const *message,
                        ModemSMSServiceSendReply *reply, gpointer user_data)
{
  ModemRequest *request;

  DEBUG (MODEM_OFACE_SMS ".SendMessage (%s,%s)", to, message);

  request = modem_request (self,
      modem_oface_dbus_proxy (MODEM_OFACE (self)),
      "SendMessage", reply_to_send_message,
      G_CALLBACK (reply), user_data,
      G_TYPE_STRING, to,
      G_TYPE_STRING, message,
      G_TYPE_INVALID);

  if (request)
    modem_request_add_data_full (request, "destination", g_strdup (to), g_free);

  return request;
}

static gchar const *
_modem_sms_is_valid_address (gchar const *address)
{
  size_t len;

  if (address == NULL)
    return "NULL";

  if (address[0] == '+')
    {
      address++;
    }

  len = strspn (address, "0123456789");

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
 * @retval TRUE - address is a valid SMS address
 * @retval FALSE - address is NULL, does not contain valid phone number, or it
 * is too long.
 */
gboolean
modem_sms_is_valid_address (gchar const *address)
{
  return !_modem_sms_is_valid_address (address);
}

/** Validate a SMS address @a address.
 *
 * A valid SMS address is a phone number with at most 20 digits either
 * in national or in international format (starting with +).
 *
 * @param address - ISDN address of address
 * @param error - return value for GError describing the validation error
 *
 * @retval TRUE - address is a valid SMS address
 * @retval FALSE - address is NULL, does not contain valid phone number, or it
 * is too long.
 */
gboolean
modem_sms_validate_address (gchar const *address, GError **error)
{
  gchar const *reason = _modem_sms_is_valid_address (address);

  if (reason)
    g_set_error (error, MODEM_SMS_ERRORS, MODEM_SMS_ERROR_INVALID_PARAMETER,
      "Invalid SMS address \"%s\": %s", address, reason);

  return !reason;
}

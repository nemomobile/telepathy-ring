/*
 * ring-text-channel.c - Source for RingTextChannel
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

#include "config.h"

#define DEBUG_FLAG RING_DEBUG_SMS
#include "ring-debug.h"

#include "ring-text-channel.h"
#include "ring-text-manager.h"
#include "ring-connection.h"
#include "ring-param-spec.h"
#include "ring-util.h"

#include <ring-extensions/ring-extensions.h>

#include <telepathy-glib/exportable-channel.h>
#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-generic.h>

#include <modem/sms.h>
#include <modem/errors.h>
#include <modem/call.h>

#include <sms-glib/enums.h>
#include <sms-glib/errors.h>
#include <sms-glib/submit.h>
#include <sms-glib/message.h>
#include <sms-glib/deliver.h>
#include <sms-glib/utils.h>

#include <string.h>

static void ring_text_channel_destroyable_iface_init(gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (RingTextChannel, ring_text_channel,
    TP_TYPE_BASE_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_TEXT,
        tp_message_mixin_text_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_DESTROYABLE,
        ring_text_channel_destroyable_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_SMS, NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_MESSAGES,
        tp_message_mixin_messages_iface_init));

static const char * const ring_text_channel_interfaces[] = {
  TP_IFACE_CHANNEL_INTERFACE_DESTROYABLE,
  TP_IFACE_CHANNEL_INTERFACE_MESSAGES,
  TP_IFACE_CHANNEL_INTERFACE_SMS,
  NULL
};

/* type definition stuff */

enum
{
  PROP_NONE,

  PROP_SMS_FLASH,
  PROP_SMS_CHANNEL,

  N_PROPS
};

struct _RingTextChannelPrivate
{
  char *destination;

  GQueue sending[1];

  unsigned sms_flash:1;         /* c.n.T.Channel.Interface.SMS.Flash */
  unsigned :0;

};

/* ---------------------------------------------------------------------- */

static void ring_text_base_channel_class_init (RingTextChannelClass *klass);
static void ring_text_channel_close (TpBaseChannel *base);

static void ring_text_channel_set_receive_timestamps(RingTextChannel *self,
  TpMessage *msg,
  gpointer sms);

/* Sending */

static void modem_sms_request_send_reply(ModemSMSService *,
  ModemRequest *request,
  char const *token,
  GError const *error,
  gpointer _self);

static void ring_text_channel_send(GObject *_self,
  TpMessage *message,
  TpMessageSendingFlags flags);

/* ---------------------------------------------------------------------- */

/* Message types to send on this channel */
static TpChannelTextMessageType
ring_text_channel_message_types[] = {
  TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
};

static char const text_plain[] = "text/plain";
static char const text_vcard[] = "text/x-vcard";        /* vcard 1.0 */
static char const text_vcalendar[] = "text/x-calendar"; /* vcal 1.0 */

/* Supported MIME types for messages mixin  */
char const * const *
ring_text_get_content_types(void)
{
  static char const * const content_types[] = {
    text_plain,
    text_vcard,
#if notyet
    text_vcalendar,
#endif
    NULL
  };

  return content_types;
}

/* ---------------------------------------------------------------------- */
/* GObject interface for RingTextChannel */

static void
ring_text_channel_init(RingTextChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, RING_TYPE_TEXT_CHANNEL, RingTextChannelPrivate);
  g_queue_init(self->priv->sending);
}

static void
ring_text_channel_constructed(GObject *object)
{
  RingTextChannel *self = RING_TEXT_CHANNEL(object);
  TpBaseChannel *base = TP_BASE_CHANNEL (self);
  RingTextChannelPrivate *priv = self->priv;
  TpBaseConnection *connection = tp_base_channel_get_connection (base);
  TpHandle target = tp_base_channel_get_target_handle (base);
  TpHandleRepoIface *repo;
  char const *target_id;
  gboolean valid;

  DEBUG ("(%p)", self);
  if (G_OBJECT_TYPE (object) != RING_TYPE_TEXT_CHANNEL)
    DEBUG ("Initializing derived text channel %s",
        G_OBJECT_TYPE_NAME (object));

  if (G_OBJECT_CLASS(ring_text_channel_parent_class)->constructed)
    G_OBJECT_CLASS(ring_text_channel_parent_class)->constructed(object);

  repo = tp_base_connection_get_handles (connection, TP_HANDLE_TYPE_CONTACT);
  target_id = tp_handle_inspect (repo, target);
  priv->destination = ring_text_channel_destination (target_id);

  valid = sms_g_is_valid_sms_address (priv->destination);
  if (!valid)
    /* Invalid destination - allow channel creation, but refuse sending */
    DEBUG ("Destination '%s' invalid", priv->destination);

  tp_message_mixin_init (object,
      G_STRUCT_OFFSET (RingTextChannel, message),
      connection);

  if (valid && !priv->sms_flash)
    {
      tp_message_mixin_implement_sending (object,
          ring_text_channel_send,
          G_N_ELEMENTS (ring_text_channel_message_types),
          ring_text_channel_message_types,
          0, /* No attachments */
          TP_DELIVERY_REPORTING_SUPPORT_FLAG_RECEIVE_FAILURES |
          TP_DELIVERY_REPORTING_SUPPORT_FLAG_RECEIVE_SUCCESSES,
          ring_text_get_content_types ());
    }
  else
    {
      tp_message_mixin_implement_sending (object, NULL,
          0, NULL, 0, 0, ring_text_get_content_types ());
    }

  tp_base_channel_register (base);
}


static void
ring_text_channel_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  RingTextChannel *self = RING_TEXT_CHANNEL (object);
  RingTextChannelPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_SMS_FLASH:
      g_value_set_boolean(value, priv->sms_flash);
      break;
    case PROP_SMS_CHANNEL:
      g_value_set_boolean (value, TRUE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
ring_text_channel_set_property(GObject *object,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  RingTextChannel *self = RING_TEXT_CHANNEL (object);
  RingTextChannelPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_SMS_FLASH:
      priv->sms_flash = g_value_get_boolean(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
ring_text_channel_dispose(GObject *object)
{
  RingTextChannel *self = RING_TEXT_CHANNEL (object);
  RingTextChannelPrivate *priv = self->priv;

  while (!g_queue_is_empty (priv->sending))
    modem_request_cancel (g_queue_pop_head (priv->sending));

  ((GObjectClass *)ring_text_channel_parent_class)->dispose (object);
}

static void
ring_text_channel_finalize(GObject *object)
{
  RingTextChannel *self = RING_TEXT_CHANNEL (object);
  RingTextChannelPrivate *priv = self->priv;

  g_free(priv->destination), priv->destination = NULL;

  while (!g_queue_is_empty(priv->sending))
    modem_request_cancel(g_queue_pop_head(priv->sending));

  tp_message_mixin_finalize(object);

  ((GObjectClass *)ring_text_channel_parent_class)->finalize (object);
}

/* Properties for o.f.T.Channel.Interface.SMS */
static TpDBusPropertiesMixinPropImpl sms_properties[] = {
  { "Flash", "sms-flash" },
#if HAVE_TP_SMS_CHANNEL
  { "SMSChannel", "sms-channel" },
#endif
  { NULL }
};

static TpDBusPropertiesMixinIfaceImpl
ring_text_channel_dbus_property_interfaces[] = {
  {
    TP_IFACE_CHANNEL_INTERFACE_SMS,
    tp_dbus_properties_mixin_getter_gobject_properties,
    NULL,
    sms_properties,
  },
  { NULL }
};

static void
ring_text_channel_class_init(RingTextChannelClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  static gboolean properties_initialized = FALSE;
  g_type_class_add_private(klass, sizeof (RingTextChannelPrivate));

  object_class->constructed = ring_text_channel_constructed;
  object_class->set_property = ring_text_channel_set_property;
  object_class->get_property = ring_text_channel_get_property;
  object_class->dispose = ring_text_channel_dispose;
  object_class->finalize = ring_text_channel_finalize;

  g_object_class_install_property(object_class, PROP_SMS_FLASH,
    g_param_spec_boolean("sms-flash",
      "Channel for Flash SMS Messages",
      "This channel is only used to receive "
      "Flash SMS messages",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_SMS_CHANNEL,
      g_param_spec_boolean ("sms-channel",
          "This channel is used with SMS",
          "Messages sent and received on this channel are transmitted via SMS",
          TRUE,
          G_PARAM_READABLE |
          G_PARAM_STATIC_STRINGS));

  ring_text_base_channel_class_init (klass);

  if (properties_initialized)
    return;
  properties_initialized = TRUE;

  klass->dbus_properties_class.interfaces =
    ring_text_channel_dbus_property_interfaces;

  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (RingTextChannelClass, dbus_properties_class));

  tp_message_mixin_init_dbus_properties (object_class);
}

/* ---------------------------------------------------------------------- */

static void
ring_text_channel_close (TpBaseChannel *base)
{
  if (tp_message_mixin_has_pending_messages ((gpointer)base, NULL))
    {
      DEBUG ("Resurrecting because of pending messages");
      tp_message_mixin_set_rescued ((gpointer)base);
      tp_base_channel_reopened (base, tp_base_channel_get_target_handle (base));
    }
  else
    {
      tp_base_channel_destroyed (base);
    }
}

static void
ring_text_channel_fill_immutable_properties (TpBaseChannel *base,
                                             GHashTable *properties)
{
  TpBaseChannelClass *base_class =
    TP_BASE_CHANNEL_CLASS (ring_text_channel_parent_class);

  base_class->fill_immutable_properties (base, properties);

  tp_dbus_properties_mixin_fill_properties_hash (G_OBJECT (base),
      properties,
      TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "MessagePartSupportFlags",
#if HAVE_TP_MESSAGE_MIXIN_WITH_DELI
      TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "DeliveryReportingSupport",
#endif
      TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "SupportedContentTypes",
      TP_IFACE_CHANNEL_INTERFACE_SMS, "Flash",
#if HAVE_TP_SMS_CHANNEL
      TP_IFACE_CHANNEL_INTERFACE_SMS, "SMSChannel",
#endif
      NULL);
}

static void
ring_text_base_channel_class_init (RingTextChannelClass *klass)
{
  TpBaseChannelClass *base_class = TP_BASE_CHANNEL_CLASS (klass);

  base_class->channel_type = TP_IFACE_CHANNEL_TYPE_TEXT;
  base_class->target_handle_type = TP_HANDLE_TYPE_CONTACT;
  base_class->interfaces = (gchar const **)ring_text_channel_interfaces;
  base_class->close = ring_text_channel_close;
  base_class->fill_immutable_properties =
    ring_text_channel_fill_immutable_properties;
}

/* ====================================================================== */
/* Channel.Interface.Destroyable */

static void
ring_text_channel_destroy (RingTextChannel *self)
{
  tp_message_mixin_clear ((gpointer)self);

  ring_text_channel_close (TP_BASE_CHANNEL (self));
}

static void
ring_text_channel_method_destroy(TpSvcChannelInterfaceDestroyable *iface,
  DBusGMethodInvocation *context)
{
  ring_text_channel_destroy(RING_TEXT_CHANNEL(iface));
  tp_svc_channel_interface_destroyable_return_from_destroy(context);
}

static void
ring_text_channel_destroyable_iface_init (gpointer iface,
                                          gpointer data)
{
  TpSvcChannelInterfaceDestroyableClass *klass = iface;

#define IMPLEMENT(x)                                    \
  tp_svc_channel_interface_destroyable_implement_##x    \
    (klass, ring_text_channel_method_ ## x)
  IMPLEMENT(destroy);
#undef IMPLEMENT
}

/* ---------------------------------------------------------------------- */
/* message_mixin interface */

static GValue const *
my_message_mixin_get_value(TpMessage const *message,
  guint part,
  char const *key)
{
  GHashTable const *dict;

  dict = tp_message_peek((TpMessage *)message, part);
  if (!dict)
    return NULL;

  return g_hash_table_lookup((GHashTable *)dict, key);
}

static char const *
my_message_mixin_get_string(TpMessage const *message,
  guint part,
  char const *key,
  char const *defaults)
{
  GValue const *value = my_message_mixin_get_value(message, part, key);

  if (value == NULL || !G_VALUE_HOLDS_STRING(value))
    return defaults;

  return g_value_get_string(value);
}

#if nomore
static GArray const *
my_message_mixin_get_bytearray(TpMessage const *message,
  guint part,
  char const *key,
  GArray const *defaults)
{
  GValue const *value = my_message_mixin_get_value(message, part, key);

  if (value == NULL || !G_VALUE_HOLDS(value, DBUS_TYPE_G_UCHAR_ARRAY))
    return defaults;

  return g_value_get_boxed(value);
}

static guint32
my_message_mixin_get_uint(TpMessage const *message,
  guint part,
  char const *key,
  guint32 defaults)
{
  GValue const *value = my_message_mixin_get_value(message, part, key);

  if (value == NULL || !G_VALUE_HOLDS_UINT(value))
    return defaults;

  return g_value_get_uint(value);
}
#endif

/** Convert handle inspection to a destination acceptable by modem.
 *
 * Remove supported service prefixes and dial strings.
 */
char *
ring_text_channel_destination(char const *inspection)
{
  if (modem_call_is_valid_address(inspection)) {
    /* Ignore prefix to suppress CLIR (*31#) */
    if (ring_str_starts_with(inspection, "*31#"))
      inspection += 4;

    /* Ignore dialstring */
    return g_strndup(inspection, strcspn(inspection, "PXw"));
  }
  else {
    return g_strdup("");
  }
}

static ModemSMSService *
ring_text_channel_get_sms_service (RingTextChannel *self)
{
  TpBaseChannel *base = TP_BASE_CHANNEL (self);
  TpBaseConnection *base_connection;
  RingConnection *connection;
  ModemOface *oface;

  base_connection = tp_base_channel_get_connection (base);
  connection = RING_CONNECTION (base_connection);
  oface = ring_connection_get_modem_interface (connection, MODEM_OFACE_SMS);

  if (oface)
    return MODEM_SMS_SERVICE (oface);
  else
    return NULL;
}

static void
ring_text_channel_send(GObject *_self,
  TpMessage *msg,
  TpMessageSendingFlags flags)
{
  RingTextChannel *self = RING_TEXT_CHANNEL(_self);
  RingTextChannelPrivate *priv = self->priv;
  ModemSMSService *sms_service = ring_text_channel_get_sms_service (self);
#if nomore
  gboolean srr;
  guint32 sms_class;
  char const *smsc;
#endif
  char const *type;
  char const *text;
  ModemRequest *request;
  GError *error;

  g_assert(tp_message_count_parts(msg) >= 1);

  error = NULL;

  type = my_message_mixin_get_string(msg, 1, "content-type", "");
  if (!type[0])
    type = my_message_mixin_get_string(msg, 1, "type", "");
  if (!type[0]) {
    GError invalid = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT, "No content type" };
    tp_message_mixin_sent(_self, msg, flags, NULL, &invalid);
    return;
  }

  if (sms_service == NULL)
    {
      GError failed = {
        TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "SMS service is not available"
      };
      tp_message_mixin_sent (_self, msg, flags, NULL, &failed);
      return;
    }

/* The nomore'd stuff is currently not supported by Ofono */
#if nomore
  /* Status report request */
  srr = (flags & TP_MESSAGE_SENDING_FLAG_REPORT_DELIVERY) != 0;

  sms_class = my_message_mixin_get_uint(msg, 0, "sms-class", 0xff);
  if (0 <= sms_class && sms_class <= 3)
    sms_g_submit_set_sms_class(submit, sms_class);

  smsc = my_message_mixin_get_string(msg, 0, "sms-service-centre", NULL);
  if (smsc == NULL || strlen(smsc) == 0)
    smsc = my_message_mixin_get_string(msg, 0, "sms-smsc", NULL);
  if (smsc == NULL || strlen(smsc) == 0)
    smsc = my_message_mixin_get_string(msg, 0, "smsc", NULL);
  if (smsc != NULL && strlen(smsc) != 0)
    sms_g_submit_set_smsc(submit, smsc);
#endif

  text = my_message_mixin_get_string(msg, 1, "content", "");

  if (g_strcasecmp(type, text_plain) == 0) {
    DEBUG("Send(destination = %s," /*class = %u,*/ "text = \"%s\")",
      priv->destination, /*sms_class,*/ text);
  }
#if nomore
  else if (g_strcasecmp(type, "text/x-vcard") == 0 ||
    g_strcasecmp(type, "text/directory") == 0 ||
    g_strcasecmp(type, "text/vcard") == 0) {
    GArray b = { (gpointer)text, strlen(text) };
    GArray const *binary;

    binary = my_message_mixin_get_bytearray(msg, 1, "content", &b);

    if (binary && binary->data) {
      encoded = sms_g_submit_binary(submit, binary, &error);
    }
    else {
      g_set_error(&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT, "No content");
    }
  }
#endif
  else {
    g_set_error(&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT, "Unknown content type");
  }

  request = modem_sms_request_send (sms_service,
      priv->destination, text,
      modem_sms_request_send_reply, self);

  if (request == NULL) {
    GError failed = { TP_ERRORS, TP_ERROR_NETWORK_ERROR,
                      "Modem connection failed" };
    tp_message_mixin_sent(_self, msg, flags, NULL, &failed);
    return;
  }

  modem_request_add_data(request, "tp-message", msg);
  modem_request_add_data(request, "tp-flags", GUINT_TO_POINTER(flags));

  g_queue_push_tail(priv->sending, request);
}

static void
modem_sms_request_send_reply(ModemSMSService *service,
  ModemRequest *request,
  char const *token,
  GError const *send_error,
  gpointer _self)
{
  RingTextChannel *self = RING_TEXT_CHANNEL(_self);
  RingTextChannelPrivate *priv = self->priv;
  TpMessage *msg = modem_request_get_data(request, "tp-message");
  GError *error = NULL;
  guint flags = GPOINTER_TO_UINT(modem_request_get_data(request, "tp-flags"));
  g_assert(msg);

  g_queue_remove(priv->sending, request);

  if (!send_error) {
    DEBUG("Send(%p) token=\"%s\"", msg, token);
    tp_message_set_int64(msg, 0, "message-sent", (gint64)time(NULL));
  }
  else {
    if (send_error && send_error->domain == DBUS_GERROR)
      g_set_error_literal(&error, TP_ERRORS, TP_ERROR_NETWORK_ERROR, send_error->message);
    else if (send_error)
      error = g_error_copy(send_error);
    else
      g_set_error_literal(&error, TP_ERRORS, TP_ERROR_NETWORK_ERROR, "Internal error");

    DEBUG("Send(%p) GError(%u, '%s, %s)",
      msg, error->code, g_quark_to_string(error->domain), error->message);
  }

  tp_message_mixin_sent((GObject *)self, msg, flags, token, error);
}

/* ------------------------------------------------------------------------ */
/* RingTextChannel interface */

gboolean
ring_text_channel_can_handle(gpointer sms)
{
  return sms_g_deliver_is_text(sms) || sms_g_deliver_is_vcard(sms);
}

void
ring_text_channel_receive_deliver(RingTextChannel *self,
  gpointer sms)
{
  TpBaseChannel *base = TP_BASE_CHANNEL (self);
  TpBaseConnection *connection = tp_base_channel_get_connection (base);
  char const *message_token = sms_g_deliver_get_message_token(sms);
  TpMessage *msg;
  guint id;

  DEBUG("enter");

  g_assert (ring_text_channel_can_handle(sms));

  msg = tp_message_new (connection, 2, 2);

  tp_message_set_handle (msg, 0, "message-sender",
      TP_HANDLE_TYPE_CONTACT, tp_base_channel_get_target_handle (base));
  tp_message_set_string(msg, 0, "message-token", message_token);

  ring_text_channel_set_receive_timestamps(self, msg, sms);

  guint32 sms_class = sms_g_deliver_get_sms_class(sms);
  if (0 <= sms_class && sms_class <= 3) {
    tp_message_set_uint32(msg, 0, "sms-class", sms_class);
  }

  tp_message_set_string(msg, 0, "sms-service-centre", sms_g_deliver_get_smsc(sms));

#if nomore
  {
    char *mwi_type = NULL;
    guint mwi_line = 0, mwi_messages = 0;
    gboolean mwi_active = FALSE, mwi_discard = FALSE;
    TpChannelTextMessageType msg_type;

    g_object_get(sms,
      "mwi-type", &mwi_type,
      "mwi-active", &mwi_active,
      "mwi-discard", &mwi_discard,
      "mwi-line", &mwi_line,
      "mwi-messages", &mwi_messages,
      NULL);

    if (mwi_type) {
      msg_type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE;

      /* XXX: waiting for upstream tp-glib to get these */
      if (g_str_equal(mwi_type, "voice"))
        tp_message_set_string(msg, 0, NOKIA_VOICEMAIL_TYPE, "tel");
      tp_message_set_string(msg, 0, "sms-mwi-type", mwi_type);
      tp_message_set_string(msg, 0, NOKIA_MAILBOX_NOTIFICATION, mwi_type);

      if (strcmp(mwi_type, "return-call")) {
        tp_message_set_boolean(msg, 0, NOKIA_MAILBOX_HAS_UNREAD, mwi_active);
        tp_message_set_boolean(msg, 0, "sms-mwi-active", mwi_active);
        if (mwi_line)
          tp_message_set_uint32(msg, 0, "sms-mwi-line", mwi_line);
        if (mwi_messages > 0 && mwi_messages < 255) {
          tp_message_set_uint32(msg, 0, "sms-mwi-messages", mwi_messages);
          tp_message_set_uint32(msg, 0, NOKIA_MAILBOX_UNREAD_COUNT,
            mwi_messages);
        }
        if (mwi_discard) {
          tp_message_set_boolean(msg, 0, "sms-mwi-discard", mwi_discard);
          tp_message_set_boolean(msg, 0, NOKIA_MAILBOX_DISCARD_TEXT,
            mwi_discard);
        }
      }
      g_free(mwi_type);
    }
    else {
      msg_type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
    }

    tp_message_set_uint32(msg, 0, "message-type", msg_type);
  }
#endif

  tp_message_set_uint32(msg, 0, "message-type",
                        TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);

  gboolean string = FALSE, bytes = FALSE;

  if (sms_g_deliver_is_text(sms)) {
    tp_message_set_string(msg, 1, "content-type", text_plain);
    tp_message_set_string(msg, 1, "type", text_plain);
    string = TRUE;
  }
  else if (sms_g_deliver_is_vcard(sms)) {
    tp_message_set_string(msg, 1, "content-type", text_vcard);
    tp_message_set_string(msg, 1, "type", text_vcard);
    bytes = TRUE;
  }
#if notyet
  else if (sms_g_deliver_is_vcalendar(sms)) {
    tp_message_set_string(msg, 1, "content-type", text_vcalendar);
    tp_message_set_string(msg, 1, "type", text_vcalendar);
    bytes = TRUE;
  }
#endif

  GArray const *binary = sms_g_deliver_get_binary(sms);
  char const *text = sms_g_deliver_get_text(sms);

  if (string) {
    if (text) {
      tp_message_set_string(msg, 1, "content", text);
    }
    else {
      tp_message_set_string(msg, 1, "content", "");
    }
  }
  else if (bytes) {
    if (binary) {
      tp_message_set_bytes(msg, 1, "content", binary->len, binary->data);
    }
    else if (text) {
      tp_message_set_bytes(msg, 1, "content", strlen(text), text);
    }
    else {
      tp_message_set_bytes(msg, 1, "content", 0, "");
    }
  }

  id = tp_message_mixin_take_received((GObject *) self, msg);

  DEBUG("message mixin received with id=%u", id);
}

static void
ring_text_channel_delivery_report(RingTextChannel *self,
  char const *token,
  guint delivery_status,
  gpointer sr,
  GError const *error)
{
  TpBaseChannel *base = TP_BASE_CHANNEL (self);
  TpBaseConnection *connection = tp_base_channel_get_connection (base);
  TpMessage *msg;
  guint id;

  msg = tp_message_new (connection, 1, 1);

  tp_message_set_handle (msg, 0, "message-sender",
      TP_HANDLE_TYPE_CONTACT, tp_base_channel_get_target_handle (base));
  tp_message_set_uint32 (msg, 0, "message-type",
      TP_CHANNEL_TEXT_MESSAGE_TYPE_DELIVERY_REPORT);

  tp_message_set_string(msg, 0, "delivery-token", token);

  if (delivery_status != TP_DELIVERY_STATUS_UNKNOWN)
    tp_message_set_uint32(msg, 0, "delivery-status", delivery_status);

  if (sr) {
    char const *message_token = sms_g_status_report_get_message_token(sr);
    guint8 failure_cause = sms_g_status_report_get_status(sr);

    tp_message_set_string(msg, 0, "message-token", message_token);
    tp_message_set_string(msg, 0, "sms-service-centre", sms_g_status_report_get_smsc(sr));
    tp_message_set_uint32(msg, 0, "sms-failure-cause", failure_cause);

    ring_text_channel_set_receive_timestamps(self, msg, sr);
  }
  else {
    GTimeVal gt[1];
    g_get_current_time(gt);
    tp_message_set_int64(msg, 0, "message-received", (gint64)gt->tv_sec);
    tp_message_set_int64(msg, 0, "message-sent", (gint64)gt->tv_sec);
    tp_message_set_string_printf(msg, 0, "message-token",
      "%s-%08ld-%08ld", token, gt->tv_sec, gt->tv_usec);
  }

  if (error) {
    char const *prefix = modem_error_domain_prefix(error->domain);

    if (prefix && prefix[0]) {
      char ebuffer[16];
      tp_message_set_string_printf(msg, 0, "delivery-dbus-error",
        "%s.%s", prefix, modem_error_name(error, ebuffer, sizeof ebuffer));
      tp_message_set_string(msg, 0, "delivery-error-message", error->message);
    }
    else if (error->domain == TP_ERRORS) {
      GEnumClass *klass = g_type_class_ref(TP_TYPE_ERROR);
      GEnumValue *ev = g_enum_get_value (klass, error->code);

      g_type_class_unref(klass);

      tp_message_set_string_printf(msg, 0, "delivery-dbus-error",
        "%s.%s", TP_ERROR_PREFIX, ev->value_nick);
      tp_message_set_string(msg, 0, "delivery-error-message", error->message);
    }
    else {
      tp_message_set_string_printf(msg, 0, "delivery-error-message",
        "%s %u: %s",
        g_quark_to_string(error->domain),
        error->code, error->message);
    }
  }

  id = tp_message_mixin_take_received((GObject *) self, msg);

  DEBUG("delivery report received with id=%u", id);
}


static void
ring_text_channel_set_receive_timestamps(RingTextChannel *self,
  TpMessage *msg,
  gpointer sms)
{
  g_return_if_fail(SMS_G_IS_MESSAGE(sms));

  ModemSMSService *sms_service = ring_text_channel_get_sms_service (self);
  gint64 sent = 0, received = 0, delivered = 0;
  gint64 now = (gint64)time(NULL);

  g_object_get(sms,
    "time-sent", &sent,
    "time-received", &received,
    "time-delivered", &delivered,
    NULL);

  tp_message_set_int64(msg, 0, "message-sent", sent);
  tp_message_set_int64(msg, 0, "message-received", received);
  if (delivered == 0) {
    tp_message_set_uint64(msg, 0, "stored-message-received", received);
  }
  else {
    tp_message_set_uint64(msg, 0, "stored-message-received", delivered);
    if (delivered > modem_sms_service_time_connected (sms_service))
      tp_message_set_boolean(msg, 0, "rescued", TRUE);
  }

  g_object_set(sms, "time-delivered", now, NULL);
}


void
ring_text_channel_outgoing_sms_complete(RingTextChannel *self,
  char const *token)
{
  ring_text_channel_delivery_report(self,
    token,
    TP_DELIVERY_STATUS_ACCEPTED,
    NULL,
    NULL);
}

void
ring_text_channel_outgoing_sms_error(RingTextChannel *self,
  char const *token,
  GError const *error)
{
  guint delivery_status;

  if (0 /*modem_sms_error_is_temporary(error)*/)
    delivery_status = TP_DELIVERY_STATUS_TEMPORARILY_FAILED;
  else
    delivery_status = TP_DELIVERY_STATUS_PERMANENTLY_FAILED;

  ring_text_channel_delivery_report(self, token, delivery_status, NULL, error);
}


void
ring_text_channel_receive_status_report(RingTextChannel *self,
  gpointer sr)
{
  guint delivery_status;

  if (sms_g_status_report_is_status_completed(sr))
    delivery_status = TP_DELIVERY_STATUS_DELIVERED;
  else if (sms_g_status_report_is_status_permanent(sr))
    delivery_status = TP_DELIVERY_STATUS_PERMANENTLY_FAILED;
  else if (sms_g_status_report_is_status_temporary(sr))
    delivery_status = TP_DELIVERY_STATUS_TEMPORARILY_FAILED;
  else
    delivery_status = TP_DELIVERY_STATUS_UNKNOWN;

  char const *token = sms_g_status_report_get_delivery_token(sr);

  if (token == NULL)
    token = sms_g_status_report_get_message_token(sr);

  ring_text_channel_delivery_report(self, token, delivery_status, sr, NULL);
}

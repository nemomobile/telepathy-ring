/*
 * modem/call.c - Interface towards Ofono call instances
 *
 * Copyright (C) 2008 Nokia Corporation
 *   @author Pekka Pessi <first.surname@nokia.com>
 *   @author Lassi Syrjala <first.surname@nokia.com>
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

#define MODEM_DEBUG_FLAG MODEM_LOG_CALL

#include "modem/debug.h"
#include "modem/errors.h"

#include "modem/call.h"
#include "modem/ofono.h"
#include "modem/request-private.h"

#include "modem/tones.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#include "signals-marshal.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

G_DEFINE_TYPE (ModemCall, modem_call, MODEM_TYPE_OFACE);

struct _ModemCallPrivate
{
  ModemCallService *service;
  gpointer handler;

  gchar *state_str;
  gchar *remote;
  gchar *emergency;
  gchar *start_time;

  unsigned char state;
  unsigned char causetype, cause;

  unsigned originating:1;
  unsigned terminating:1;
  unsigned onhold:1;
  unsigned multiparty:1;
  unsigned :0;
};

/* Properties */
enum
{
  PROP_NONE,
  PROP_SERVICE,
  PROP_OFONO_STATE,
  PROP_STATE,
  PROP_CAUSETYPE,
  PROP_CAUSE,
  PROP_ORIGINATING,
  PROP_TERMINATING,
  PROP_EMERGENCY,
  PROP_ONHOLD,
  PROP_MULTIPARTY,
  PROP_REMOTE,
  PROP_START_TIME,
  LAST_PROPERTY
};

/* Signals */
enum
{
  SIGNAL_STATE,
  SIGNAL_WAITING,
  SIGNAL_EMERGENCY,
  SIGNAL_TERMINATED,
  SIGNAL_ON_HOLD,
  SIGNAL_FORWARDED,
  SIGNAL_DIALSTRING,
  SIGNAL_DTMF_TONE,
  N_SIGNALS
};

static guint call_signals[N_SIGNALS];

/* ---------------------------------------------------------------------- */


static void on_notify_ofono_state (ModemCall *, GParamSpec *, gpointer);
static void reply_to_instance_request (DBusGProxy *, DBusGProxyCall *, void *);
static void on_disconnect_reason (DBusGProxy *, char const *, gpointer);
static void on_propertychanged (DBusGProxy *, char const *, GValue *, gpointer);

#if nomore /* Ofono does not provide this information */
static void on_on_hold (DBusGProxy *, gboolean onhold, ModemCall*);
static void on_forwarded (DBusGProxy *, ModemCall *);
static void on_waiting (DBusGProxy *proxy, ModemCall *);
static void on_emergency (DBusGProxy *proxy,
    char const *service, ModemCall *);

static void on_sending_dtmf (DBusGProxy *proxy,
    char const *dialstring, ModemCall *);
static void on_stopped_dtmf (DBusGProxy *proxy, ModemCall *);
#endif

static void reply_to_stop_dtmf (DBusGProxy *, DBusGProxyCall *, void *);

/* ---------------------------------------------------------------------- */

#define RETURN_IF_NOT_VALID(self)                       \
  g_return_if_fail (self != NULL &&                     \
      modem_oface_is_connected (MODEM_OFACE (self)))

#define RETURN_VAL_IF_NOT_VALID(self, val)                      \
  g_return_val_if_fail (self != NULL &&                         \
      modem_oface_is_connected (MODEM_OFACE (self)), (val))

#define RETURN_NULL_IF_NOT_VALID(self) RETURN_VAL_IF_NOT_VALID (self, NULL)

/* ---------------------------------------------------------------------- */

static void
modem_call_init (ModemCall *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MODEM_TYPE_CALL,
      ModemCallPrivate);
}

static void
modem_call_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (modem_call_parent_class)->constructed)
    G_OBJECT_CLASS (modem_call_parent_class)->constructed (object);

  DEBUG ("ModemCall for %s on %s",
      modem_oface_object_path (MODEM_OFACE (object)), MODEM_OFACE_CALL);
}

static void
modem_call_dispose (GObject *object)
{
  DEBUG ("enter");

  if (G_OBJECT_CLASS (modem_call_parent_class)->dispose)
    G_OBJECT_CLASS (modem_call_parent_class)->dispose (object);
}

static void
modem_call_finalize (GObject *object)
{
  ModemCall *self = MODEM_CALL (object);
  ModemCallPrivate *priv = self->priv;

  priv->service = NULL;
  g_free (priv->remote), priv->remote = NULL;
  g_free (priv->emergency), priv->emergency = NULL;
  g_free (priv->start_time), priv->start_time = NULL;

  G_OBJECT_CLASS (modem_call_parent_class)->finalize (object);
}

static void
modem_call_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  ModemCall *self = MODEM_CALL (object);
  ModemCallPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_SERVICE:
      g_value_set_object (value, priv->service);
      break;

    case PROP_OFONO_STATE:
      g_value_set_string (value, priv->state_str);
      break;

    case PROP_STATE:
      g_value_set_uint (value, priv->state);
      break;

    case PROP_CAUSE:
      g_value_set_uint (value, priv->cause);
      break;

    case PROP_CAUSETYPE:
      g_value_set_uint (value, priv->causetype);
      break;

    case PROP_ORIGINATING:
      g_value_set_boolean (value, priv->originating);
      break;

    case PROP_TERMINATING:
      g_value_set_boolean (value, priv->terminating);
      break;

    case PROP_EMERGENCY:
      g_value_set_string (value, priv->emergency);
      break;

    case PROP_ONHOLD:
      g_value_set_boolean (value, priv->onhold);
      break;

    case PROP_MULTIPARTY:
      g_value_set_boolean (value, priv->multiparty);
      break;

    case PROP_REMOTE:
      g_value_set_string (value, priv->remote);
      break;

    case PROP_START_TIME:
      g_value_set_string (value, priv->start_time);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
modem_call_set_property (GObject *obj,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  ModemCall *self = MODEM_CALL (obj);
  ModemCallPrivate *priv = self->priv;
  char *tbf;

  switch (property_id)
    {
    case PROP_SERVICE:
      priv->service = g_value_get_object (value);
      break;

      /* XXX/KV: ugly hack until usage of state is harmonized */
    case PROP_OFONO_STATE:
      g_free (priv->state_str);
      priv->state_str = g_value_dup_string (value);
      break;

    case PROP_STATE:
      priv->state = g_value_get_uint (value);
      break;

    case PROP_CAUSE:
      priv->cause = g_value_get_uint (value);
      break;

    case PROP_CAUSETYPE:
      priv->causetype = g_value_get_uint (value);
      break;

    case PROP_ORIGINATING:
      priv->originating = g_value_get_boolean (value);
      if (priv->originating)
        priv->terminating = FALSE;
      break;

    case PROP_TERMINATING:
      priv->terminating = g_value_get_boolean (value);
      if (priv->terminating)
        priv->originating = FALSE;
      break;

    case PROP_EMERGENCY:
      tbf = priv->emergency;
      priv->emergency = g_value_dup_string (value);
      g_free (tbf);
      break;

    case PROP_ONHOLD:
      priv->onhold = g_value_get_boolean (value);
      break;

    case PROP_MULTIPARTY:
      priv->multiparty = g_value_get_boolean (value);
      break;

    case PROP_REMOTE:
      tbf = priv->remote;
      priv->remote = g_value_dup_string (value);
      g_free (tbf);
      break;

    case PROP_START_TIME:
      g_free (priv->start_time);
      priv->start_time = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

/* ------------------------------------------------------------------------- */

/** Maps properties of org.ofono.VoiceCall to matching ModemCall properties */
char const *
modem_call_property_mapper (char const *name)
{
  if (!strcmp (name, "LineIdentification"))
    return "remote";
  if (!strcmp (name, "Multiparty"))
    return "multiparty";
  if (!strcmp (name, "State"))
    return "ofono-state";
  if (!strcmp (name, "StartTime"))
    return "start-time";
  if (!strcmp (name, "Information"))
    return NULL;
  if (!strcmp (name, "Icon"))
    return NULL;
  return NULL;
}

static void
modem_call_connect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  /* CallAdded gives us initial properties */
  modem_oface_connect_properties (_self, FALSE);

  /* Additionally, listen to 'DisconnectReason' signal*/
  ModemCall *self = MODEM_CALL (_self);
  DBusGProxy *proxy = modem_oface_dbus_proxy (_self);
  dbus_g_proxy_add_signal (proxy, "DisconnectReason",
	  G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (proxy, "DisconnectReason",
	  G_CALLBACK (on_disconnect_reason), self, NULL);
  dbus_g_proxy_add_signal (proxy, "PropertyChanged",
	  G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (proxy, "PropertyChanged",
	  G_CALLBACK (on_propertychanged), self, NULL);

  g_signal_connect (_self, "notify::ofono-state",
      G_CALLBACK(on_notify_ofono_state), _self);
}

static void
modem_call_connected (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  on_notify_ofono_state (MODEM_CALL (_self), NULL, _self);
}

static void
modem_call_disconnect (ModemOface *_self)
{
  DEBUG ("(%p): enter", _self);

  g_signal_handlers_disconnect_by_func (_self,
      G_CALLBACK(on_notify_ofono_state), _self);

  modem_oface_disconnect_properties (_self);
}

static void
modem_call_class_init (ModemCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ModemOfaceClass *oface_class = MODEM_OFACE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ModemCallPrivate));

  object_class->constructed = modem_call_constructed;
  object_class->dispose = modem_call_dispose;
  object_class->finalize = modem_call_finalize;
  object_class->get_property = modem_call_get_property;
  object_class->set_property = modem_call_set_property;

  oface_class->ofono_interface = MODEM_OFACE_CALL;
  oface_class->property_mapper = modem_call_property_mapper;
  oface_class->connect = modem_call_connect;
  oface_class->connected = modem_call_connected;
  oface_class->disconnect = modem_call_disconnect;

  /* Properties */
  g_object_class_install_property (object_class, PROP_SERVICE,
      g_param_spec_object ("call-service",
          "Call service",
          "The call service object that owns "
          "the modem call object",
          MODEM_TYPE_CALL_SERVICE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_OFONO_STATE,
      g_param_spec_string ("ofono-state",
          "Call State String",
          "State name of the call instance",
          NULL,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_STATE,
      g_param_spec_uint ("state",
          "Call state",
          "State of the call instance",
          MODEM_CALL_STATE_INVALID,
          MODEM_CALL_STATE_DISCONNECTED,
          MODEM_CALL_STATE_INVALID,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_CAUSE,
      g_param_spec_uint ("cause",
          "Call cause",
          "Cause of the latest state transition",
          0, 255, 0,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_CAUSETYPE,
      g_param_spec_uint ("causetype",
          "Call cause type",
          "Source of the latest state transition",
          0, MODEM_CALL_CAUSE_TYPE_REMOTE, 0,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ORIGINATING,
      g_param_spec_boolean ("originating",
          "Originated Call",
          "Terminal is originating this call",
          FALSE,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_TERMINATING,
      g_param_spec_boolean ("terminating",
          "Terminating Call",
          "Terminal is terminating this call",
          FALSE,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_EMERGENCY,
      g_param_spec_string ("emergency",
          "Emergency Service",
          "Emergency Service for this call",
          NULL,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ONHOLD,
      g_param_spec_boolean ("onhold",
          "Call is On Hold",
          "This call has been put on hold by remote party",
          FALSE,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MULTIPARTY,
      g_param_spec_boolean ("multiparty",
          "Multiparty Membership",
          "This instance is a member of a conference call",
          FALSE,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_REMOTE,
      g_param_spec_string ("remote",
          "Remote Party Address",
          "Address of remote party associated with this call",
          NULL,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_START_TIME,
      g_param_spec_string ("start-time",
          "Start Time",
          "The time the call started",
          NULL,
          G_PARAM_READWRITE |
          G_PARAM_STATIC_STRINGS));

  call_signals[SIGNAL_STATE] =
    g_signal_new ("state", G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__UINT,
        G_TYPE_NONE, 1,
        G_TYPE_UINT);

  /* XXX: not implemented */
  call_signals[SIGNAL_WAITING] =
    g_signal_new ("waiting", G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

  /* XXX: not implemented */
  call_signals[SIGNAL_EMERGENCY] =
    g_signal_new ("emergency", G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

  /* XXX: not implemented */
  call_signals[SIGNAL_TERMINATED] =
    g_signal_new ("terminated", G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

  call_signals[SIGNAL_ON_HOLD] =
    g_signal_new ("on-hold", G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE, 1,
        G_TYPE_BOOLEAN);

  /* XXX: not implemented */
  call_signals[SIGNAL_FORWARDED] =
    g_signal_new ("forwarded", G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

  /* XXX: not implemented */
  call_signals[SIGNAL_DIALSTRING] =
    g_signal_new ("dialstring", G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

  /* XXX: not implemented */
  call_signals[SIGNAL_DTMF_TONE] =
    g_signal_new ("dtmf-tone", G_OBJECT_CLASS_TYPE (klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0,
        NULL, NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);
}

/* ---------------------------------------------------------------------- */
/* ModemCall interface */

char const *
modem_call_get_name (ModemCall const *self)
{
  if (self == NULL)
    return "<nil>";

  char const *path = modem_oface_object_path (MODEM_OFACE (self));
  char const *last = strrchr (path, '/');

  if (last)
    return last + 1;

  return "<invalid>";
}

char const *
modem_call_get_path (ModemCall const *self)
{
  return modem_oface_object_path (MODEM_OFACE (self));
}

char const *
modem_call_get_state_name (int state)
{
  switch (state)
    {
    case MODEM_CALL_STATE_INVALID: return "INVALID";
    case MODEM_CALL_STATE_DIALING: return "DIALING";
    case MODEM_CALL_STATE_ALERTING: return "ALERTING";
    case MODEM_CALL_STATE_INCOMING: return "INCOMING";
    case MODEM_CALL_STATE_WAITING: return "WAITING";
    case MODEM_CALL_STATE_ACTIVE: return "ACTIVE";
    case MODEM_CALL_STATE_HELD: return "HELD";
    case MODEM_CALL_STATE_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
    }
}

ModemCallState
modem_call_get_state (ModemCall const *self)
{
  return MODEM_IS_CALL (self) ? self->priv->state : MODEM_CALL_STATE_INVALID;
}

gboolean
modem_call_has_path (ModemCall const *self,
                     char const *object_path)
{
  gchar const *my_path = modem_oface_object_path (MODEM_OFACE (self));

  return object_path && my_path && strcmp (my_path, object_path) == 0;
}

gboolean
modem_call_try_set_handler (ModemCall *self,
                            gpointer handler)
{
  if (!MODEM_IS_CALL (self))
    return FALSE;

  ModemCallPrivate *priv = self->priv;

  if (handler)
    {
      if (priv->handler != NULL && priv->handler != handler)
        {
          return FALSE;
        }
    }
  priv->handler = handler;
  return TRUE;
}

void
modem_call_set_handler (ModemCall *self,
                        gpointer handler)
{
  gboolean success = modem_call_try_set_handler (self, handler);
  g_assert (success);
}

gpointer
modem_call_get_handler (ModemCall *self)
{
  return MODEM_IS_CALL (self) ? self->priv->handler : NULL;
}

gboolean
modem_call_is_member (ModemCall const *self)
{
  return MODEM_IS_CALL (self) && self->priv->multiparty;
}

gboolean
modem_call_is_originating (ModemCall const *self)
{
  return MODEM_IS_CALL (self) && self->priv->originating;
}

gboolean
modem_call_is_terminating (ModemCall const *self)
{
  return MODEM_IS_CALL (self) && self->priv->terminating;
}

gboolean
modem_call_is_active (ModemCall const *self)
{
  return MODEM_IS_CALL (self) &&
    self->priv->state == MODEM_CALL_STATE_ACTIVE;
}

gboolean
modem_call_is_held (ModemCall const *self)
{
  return self &&
    self->priv->state == MODEM_CALL_STATE_HELD;
}

ModemCallState
modem_call_state_from_ofono_state (const char *state)
{
  if (G_UNLIKELY (!state))
    return MODEM_CALL_STATE_INVALID;
  else if (!strcmp (state, "active"))
    return MODEM_CALL_STATE_ACTIVE;
  else if (!strcmp (state, "held"))
    return MODEM_CALL_STATE_HELD;
  else if (!strcmp (state, "dialing"))
    return MODEM_CALL_STATE_DIALING;
  else if (!strcmp (state, "alerting"))
    return MODEM_CALL_STATE_ALERTING;
  else if (!strcmp (state, "incoming"))
    return MODEM_CALL_STATE_INCOMING;
  else if (!strcmp (state, "waiting"))
    return MODEM_CALL_STATE_WAITING;
  else if (!strcmp (state, "disconnected"))
    return MODEM_CALL_STATE_DISCONNECTED;

  return MODEM_CALL_STATE_INVALID;
}

ModemCallState
modem_call_cause_type_from_ofono_disconnect_reason (const char *reason)
{
  if (G_UNLIKELY (!reason))
    return MODEM_CALL_CAUSE_TYPE_UNKNOWN;
  else if (!strcmp (reason, "network"))
    return MODEM_CALL_CAUSE_TYPE_NETWORK;
  else if (!strcmp (reason, "local"))
    return MODEM_CALL_CAUSE_TYPE_LOCAL;
  else if (!strcmp (reason, "remote"))
    return MODEM_CALL_CAUSE_TYPE_REMOTE;

  return MODEM_CALL_CAUSE_TYPE_UNKNOWN;
}

static void
on_notify_ofono_state (ModemCall *self,
                       GParamSpec *pspec,
                       gpointer user_data)
{
  ModemCallPrivate *priv = self->priv;
  ModemCallState state;

  DEBUG ("enter with \"%s\"", priv->state_str);

  state = modem_call_state_from_ofono_state (priv->state_str);
  if (state == priv->state)
    return;

  g_object_set (self, "state", state, NULL);
  g_signal_emit (self, call_signals[SIGNAL_STATE], 0, state);
}

#if nomore
static void
on_call_state (DBusGProxy *proxy,
               guint state,
               guint causetype,
               guint cause,
               ModemCall *self)
{
  ModemCallPrivate *priv = self->priv;

  g_assert (proxy); g_assert (self); g_assert (priv->proxy == proxy);

  DEBUG ("CallState (%s (%u), %u, %u) from %s%s",
      modem_call_get_state_name (state),
      state, causetype, cause, dbus_g_proxy_get_path (proxy),
      priv->handler ? "" : " (no channel)");

  g_object_set (self, "state", state,
      causetype > MODEM_CALL_CAUSE_TYPE_NETWORK
      /* Unknown causetype, ignore */
      ? NULL
      : "causetype", causetype,
      "cause", cause,
      NULL);

  switch (state)
    {
    case MODEM_CALL_STATE_INCOMING:
      g_object_set (self, "terminating", TRUE, NULL);
      break;
    case MODEM_CALL_STATE_DIALING:
      g_object_set (self, "originating", TRUE, NULL);
      break;

    case MODEM_CALL_STATE_INVALID:
      g_object_set (self,
          "remote", NULL,
          "emergency", NULL,
          "originating", FALSE,
          "terminating", FALSE,
          "onhold", FALSE,
          "multiparty", FALSE,
          NULL);
      break;
    }

  if (priv->handler == NULL)
    {
      switch (state)
        {
        case MODEM_CALL_STATE_DIALING:
        case MODEM_CALL_STATE_ALERTING:
        case MODEM_CALL_STATE_INCOMING:
        case MODEM_CALL_STATE_WAITING:
        case MODEM_CALL_STATE_ACTIVE:
        case MODEM_CALL_STATE_HELD:
          {
            char const *remote = priv->remote ? priv->remote : "";
            if (priv->terminating)
              g_signal_emit_by_name (priv->service, "incoming", self, remote);
            else if (priv->originating)
              g_signal_emit_by_name (priv->service, "created", self, remote);
          }
          break;
        default:
          break;
        }
    }

  g_signal_emit (self, call_signals[SIGNAL_STATE], 0, state, causetype, cause);

  if (state == MODEM_CALL_STATE_TERMINATED)
    {
      g_object_set (self,
          "remote", NULL,
          "emergency", NULL,
          "originating", FALSE,
          "terminating", FALSE,
          "onhold", FALSE,
          "multiparty", FALSE,
          NULL);
    }
}
#endif

ModemRequest *
modem_call_request_answer (ModemCall *self,
                           ModemCallReply callback,
                           gpointer user_data)
{
  DEBUG ("enter");

  RETURN_NULL_IF_NOT_VALID (self);

  if (self->priv->state != MODEM_CALL_STATE_WAITING)
    {
      DEBUG ("%s.%s (%s)", MODEM_OFACE_CALL, "Answer",
          modem_call_get_path (self));
      return modem_request (MODEM_CALL (self),
          modem_oface_dbus_proxy (MODEM_OFACE (self)),
          "Answer", reply_to_instance_request,
          G_CALLBACK (callback), user_data,
          G_TYPE_INVALID);
    }
  else {
    DEBUG ("%s.%s (%s)", MODEM_OFACE_CALL_MANAGER, "HoldAndAnswer",
        modem_call_get_path (self));
    return modem_request (MODEM_CALL (self),
        modem_oface_dbus_proxy (MODEM_OFACE (self->priv->service)),
        "HoldAndAnswer", reply_to_instance_request,
        G_CALLBACK (callback), user_data,
        G_TYPE_INVALID);
  }
}

ModemRequest *
modem_call_request_release (ModemCall *self,
                            ModemCallReply callback,
                            gpointer user_data)
{
  DEBUG ("%s.%s (%s)", MODEM_OFACE_CALL, "Hangup", modem_call_get_path (self));
  RETURN_NULL_IF_NOT_VALID (self);

  return modem_request (MODEM_CALL (self),
      modem_oface_dbus_proxy (MODEM_OFACE (self)),
      "Hangup", reply_to_instance_request,
      G_CALLBACK (callback), user_data,
      G_TYPE_INVALID);
}

static void
reply_to_private_chat_request (DBusGProxy *proxy,
    DBusGProxyCall *call,
    void *_request)
{
  DEBUG ("enter");

  GPtrArray *paths;
  ModemRequest *request = _request;
  ModemCall *self = modem_request_object (request);
  ModemCallReply *callback = modem_request_callback (request);
  gpointer user_data = modem_request_user_data (request);
  GError *error = NULL;

  if (dbus_g_proxy_end_call (proxy, call, &error,
          MODEM_TYPE_ARRAY_OF_PATHS, &paths,
          G_TYPE_INVALID))
    {
      guint i;
      char const *path;

      for (i = 0; i < paths->len; i++)
        {
          path = g_ptr_array_index (paths, i);
          DEBUG("Calls in conf after split %u/%u: %s\n",
              i + 1, paths->len, path);
        }
      /* XXX: should this list be passed to the client as well? */
    }
  else {
    modem_error_fix (&error);
  }

  callback (self, request, error, user_data);
  g_clear_error (&error);
}

ModemRequest *
modem_call_request_split (ModemCall *self,
                          ModemCallReply callback,
                          gpointer user_data)
{
  DEBUG ("%s.%s (%s)", MODEM_OFACE_CALL_MANAGER, "PrivateChat",
      modem_call_get_path (self));
  RETURN_NULL_IF_NOT_VALID (self);

  return modem_request (MODEM_CALL (self),
      modem_oface_dbus_proxy (MODEM_OFACE (self->priv->service)),
      "PrivateChat", reply_to_private_chat_request,
      G_CALLBACK (callback), user_data,
      DBUS_TYPE_G_OBJECT_PATH, modem_call_get_path (self),
      G_TYPE_INVALID);
}

ModemRequest *
modem_call_request_hold (ModemCall *self,
                         int hold,
                         ModemCallReply callback,
                         gpointer user_data)
{
  ModemCallPrivate *priv = self->priv;

  DEBUG ("");

  RETURN_NULL_IF_NOT_VALID (self);

  if (!((priv->state == MODEM_CALL_STATE_HELD && !hold) ||
          (priv->state == MODEM_CALL_STATE_ACTIVE && hold)))
    {
      DEBUG ("invalid hold request %d in %s state\n",
          hold, priv->state_str);
      return NULL;
    }

  DEBUG ("%s.%s", MODEM_OFACE_CALL_MANAGER, "SwapCalls");

  return modem_request (MODEM_CALL (self),
      modem_oface_dbus_proxy (MODEM_OFACE (self->priv->service)),
      "SwapCalls", reply_to_instance_request,
      G_CALLBACK (callback), user_data,
      G_TYPE_INVALID);
}

static void
reply_to_instance_request (DBusGProxy *proxy,
                           DBusGProxyCall *call,
                           void *_request)
{
  DEBUG ("enter");

  ModemRequest *request = _request;
  ModemCall *self = modem_request_object (request);
  ModemCallReply *callback = modem_request_callback (request);
  gpointer user_data = modem_request_user_data (request);
  GError *error = NULL;

  if (dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
    ;
  else
    modem_error_fix (&error);

  if (callback)
    callback (self, request, error, user_data);

  g_clear_error (&error);
}

gboolean
modem_call_can_join (ModemCall const *self)
{
  if (!self)
    return FALSE;

  return (self->priv->state == MODEM_CALL_STATE_ACTIVE ||
      self->priv->state == MODEM_CALL_STATE_HELD);
}

ModemRequest *
modem_call_send_dtmf (ModemCall *self, char const *dialstring,
                      ModemCallReply *callback,
                      gpointer user_data)
{
  int i;
  char modemstring[256];

  RETURN_NULL_IF_NOT_VALID (self);
  g_return_val_if_fail (dialstring != NULL, NULL);

  for (i = 0; dialstring[i]; i++)
    {
      if (i == 255)
        return NULL; /* Too long */

      switch (dialstring[i])
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '#':
        case '*':
          modemstring[i] = dialstring[i];
        break;

        case 'p': case 'P': modemstring[i] = 'p'; break;
        case 'w': case 'W': modemstring[i] = 'w'; break;
        case 'a': case 'A': modemstring[i] = 'a'; break;
        case 'b': case 'B': modemstring[i] = 'b'; break;
        case 'c': case 'C': modemstring[i] = 'c'; break;
        case 'd': case 'D': modemstring[i] = 'd'; break;

        default:
          return NULL;
        }
    }

  modemstring[i] = '\0';

  return modem_request (self,
      modem_oface_dbus_proxy (MODEM_OFACE (self->priv->service)),
      "SendTones", reply_to_instance_request,
      G_CALLBACK (callback), user_data,
      G_TYPE_STRING, modemstring,
      G_TYPE_INVALID);
}

/* XXX: Ofono at the moment supports only fixed-duration tones */
ModemRequest *
modem_call_start_dtmf (ModemCall *self, char const tone,
                       ModemCallReply *callback,
                       gpointer user_data)
{
  char tones[2] = { tone };

  RETURN_NULL_IF_NOT_VALID (self);

  return modem_request (MODEM_CALL (self),
      modem_oface_dbus_proxy (MODEM_OFACE (self->priv->service)),
      "SendTones", reply_to_instance_request,
      G_CALLBACK (callback), user_data,
      G_TYPE_STRING, tones,
      G_TYPE_INVALID);
}

ModemRequest *
modem_call_stop_dtmf (ModemCall *self,
                      ModemCallReply *callback,
                      gpointer user_data)
{
  RETURN_NULL_IF_NOT_VALID (self);

  return modem_request (MODEM_CALL (self),
      modem_oface_dbus_proxy (MODEM_OFACE (self->priv->service)),
      "StopTones", reply_to_stop_dtmf,
      G_CALLBACK (callback), user_data,
      G_TYPE_INVALID);
}

static void
reply_to_stop_dtmf (DBusGProxy *proxy,
                    DBusGProxyCall *call,
                    void *_request)
{
  DEBUG ("enter");

  ModemRequest *request = _request;
  ModemCall *self = modem_request_object (request);
  ModemCallReply *callback = modem_request_callback (request);
  gpointer user_data = modem_request_user_data (request);
  char *stopped;
  GError *error = NULL;

  if (dbus_g_proxy_end_call (proxy, call, &error,
          G_TYPE_STRING, &stopped,
          G_TYPE_INVALID))
    {
      g_free (stopped);
    }
  else
    {
      modem_error_fix (&error);
      DEBUG ("got " GERROR_MSG_FMT, GERROR_MSG_CODE (error));
    }

  if (callback)
    callback (self, request, error, user_data);

  g_clear_error (&error);
}

GError *
modem_call_new_error (guint causetype, guint cause, char const *prefixed)
{
  GQuark domain;
  char const *msg;
  char *error = " Error";
  char printed[64];
  int code = cause;

  if (cause == 0)
    {
      domain = MODEM_CALL_ERRORS;
      code = MODEM_CALL_ERROR_NO_ERROR;
      msg = "None";
      error = "";
    }
  else if (causetype == MODEM_CALL_CAUSE_TYPE_NETWORK)
    {
      domain = MODEM_CALL_NET_ERRORS;
      code = cause;

#define _(x) MODEM_CALL_NET_ERROR_ ## x
      switch (cause)
        {
        case _(UNASSIGNED_NUMBER): msg = "Unassigned Number"; break;
        case _(NO_ROUTE): msg = "No Route To Destination"; break;
        case _(CH_UNACCEPTABLE): msg = "Channel Unacceptable"; break;
        case _(OPER_BARRING): msg = "Operator Determined Barring"; break;
        case _(NORMAL): msg = "Normal Call Clearing"; error = ""; break;
        case _(USER_BUSY): msg = "User Busy"; error = ""; break;
        case _(NO_USER_RESPONSE): msg = "No User Response"; break;
        case _(ALERT_NO_ANSWER): msg = "Alert No Answer"; break;
        case _(CALL_REJECTED): msg = "Call Rejected"; break;
        case _(NUMBER_CHANGED): msg = "Number Changed"; break;
        case _(NON_SELECT_CLEAR): msg = "Non-Selected Clearing"; break;
        case _(DEST_OUT_OF_ORDER): msg = "Destination Out Of Order"; break;
        case _(INVALID_NUMBER): msg = "Invalid Number"; break;
        case _(FACILITY_REJECTED): msg = "Facility Rejected"; break;
        case _(RESP_TO_STATUS): msg = "Response To Status"; error = ""; break;
        case _(NORMAL_UNSPECIFIED): msg = "Unspecified Normal"; error = ""; break;
        case _(NO_CHANNEL): msg = "No Channel Available"; break;
        case _(NETW_OUT_OF_ORDER): msg = "Network Out Of Order"; break;
        case _(TEMPORARY_FAILURE): msg = "Temporary Failure"; error =""; break;
        case _(CONGESTION): msg = "Congestion"; break;
        case _(ACCESS_INFO_DISC): msg = "Access Information Discarded"; break;
        case _(CHANNEL_NA): msg = "Channel Not Available"; break;
        case _(RESOURCES_NA): msg = "Resources Not Available"; break;
        case _(QOS_NA): msg = "QoS Not Available"; break;
        case _(FACILITY_UNSUBS): msg = "Requested Facility Not Subscribed"; break;
        case _(COMING_BARRED_CUG): msg = "Incoming Calls Barred Within CUG"; break;
        case _(BC_UNAUTHORIZED): msg = "Bearer Capability Unauthorized"; break;
        case _(BC_NA): msg = "Bearer Capability Not Available"; break;
        case _(SERVICE_NA): msg = "Service Not Available"; break;
        case _(BEARER_NOT_IMPL): msg = "Bearer Not Implemented"; break;
        case _(ACM_MAX): msg = "ACM Max"; break;
        case _(FACILITY_NOT_IMPL): msg = "Facility Not Implemented"; break;
        case _(ONLY_RDI_BC): msg = "Only Restricted DI Bearer Capability"; break;
        case _(SERVICE_NOT_IMPL): msg = "Service Not Implemented"; break;
        case _(INVALID_TI): msg = "Invalid Transaction Identifier"; break;
        case _(NOT_IN_CUG): msg = "Not In CUG"; break;
        case _(INCOMPATIBLE_DEST): msg = "Incompatible Destination"; break;
        case _(INV_TRANS_NET_SEL): msg = "Invalid Transit Net Selected"; break;
        case _(SEMANTICAL_ERR): msg = "Semantical"; break;
        case _(INVALID_MANDATORY): msg = "Invalid Mandatory Information"; break;
        case _(MSG_TYPE_INEXIST): msg = "Message Type Non-Existent"; break;
        case _(MSG_TYPE_INCOMPAT): msg = "Message Type Incompatible"; break;
        case _(IE_NON_EXISTENT): msg = "Information Element Non-Existent"; break;
        case _(COND_IE_ERROR): msg = "Conditional Information Element"; break;
        case _(MSG_INCOMPATIBLE): msg = "Incompatible Message"; break;
        case _(TIMER_EXPIRY): msg = "Timer Expiry"; break;
        case _(PROTOCOL_ERROR): msg = "Protocol"; break;
        case _(INTERWORKING):
          msg = "Error Cause Not Known Because of Interworking", error = "";
          break;
        default:
          code = MODEM_CALL_NET_ERROR_GENERIC;
          snprintf (printed, sizeof printed, "Error %u with type %u", cause, causetype);
          msg = printed, error = "";
        }
#undef _
    }
  else if (causetype == MODEM_CALL_CAUSE_TYPE_LOCAL ||
      causetype == MODEM_CALL_CAUSE_TYPE_REMOTE)
    {

      domain = MODEM_CALL_ERRORS;

#define _(x) MODEM_CALL_ERROR_ ## x
      switch (cause)
        {
        case _(NO_CALL): msg = "No Call"; break;
        case _(RELEASE_BY_USER): msg = "Release By User"; error = ""; break;
        case _(BUSY_USER_REQUEST): msg = "Busy User Request"; break;
        case _(ERROR_REQUEST): msg = "Request"; break;
        case _(CALL_ACTIVE): msg = "Call Active"; break;
        case _(NO_CALL_ACTIVE): msg = "No Call Active"; break;
        case _(INVALID_CALL_MODE): msg = "Invalid Call Mode"; break;
        case _(TOO_LONG_ADDRESS): msg = "Too Long Address"; break;
        case _(INVALID_ADDRESS): msg = "Invalid Address"; break;
        case _(EMERGENCY): msg = "Emergency"; break;
        case _(NO_SERVICE): msg = "No Service"; break;
        case _(NO_COVERAGE): msg = "No Coverage"; break;
        case _(CODE_REQUIRED): msg = "Code Required"; break;
        case _(NOT_ALLOWED): msg = "Not Allowed"; break;
        case _(DTMF_ERROR): msg = "DTMF Error"; break;
        case _(CHANNEL_LOSS): msg = "Channel Loss"; break;
        case _(FDN_NOT_OK): msg = "FDN Not Ok"; break;
        case _(BLACKLIST_BLOCKED): msg = "Blacklist Blocked"; break;
        case _(BLACKLIST_DELAYED): msg = "Blacklist Delayed"; break;
        case _(EMERGENCY_FAILURE): msg = "Emergency Failure"; break;
        case _(NO_SIM): msg = "No SIM"; break;
        case _(DTMF_SEND_ONGOING): msg = "DTMF Send Ongoing"; break;
        case _(CS_INACTIVE): msg = "CS Inactive"; break;
        case _(NOT_READY): msg = "Not Ready"; break;
        case _(INCOMPATIBLE_DEST): msg = "Incompatible Dest"; break;
        default:
          code = MODEM_CALL_ERROR_GENERIC;
          snprintf (printed, sizeof printed, "Error %u with type %u", cause, causetype);
          msg = printed, error = "";
        }
#undef _
    }
  else
    {
      domain = MODEM_CALL_ERRORS;
      code = MODEM_CALL_ERROR_GENERIC;
      snprintf (printed, sizeof printed, "Error %u with type %u", cause, causetype);
      msg = printed, error = "";
    }

  if (prefixed)
    {
      return g_error_new (domain, code, "%s: %s%s", prefixed, msg, error);
    }
  else
    {
      return g_error_new (domain, code, "%s%s", msg, error);
    }
}

/* TODO: these need to be revised once Ofono provides sufficient information. */
int
modem_call_event_tone (guint state,
                       guint causetype,
                       guint cause)
{
  switch (state)
    {
    case MODEM_CALL_STATE_DIALING:
    case MODEM_CALL_STATE_WAITING:
    case MODEM_CALL_STATE_INCOMING:
    case MODEM_CALL_STATE_ACTIVE:
      return TONES_STOP;

    case MODEM_CALL_STATE_ALERTING:
      return TONES_EVENT_RINGING;

    case MODEM_CALL_STATE_DISCONNECTED:
      if (causetype == MODEM_CALL_CAUSE_TYPE_NETWORK)
        {
          /* 3GPP TS 22.001 F.4 */
          switch (cause)
            {
            case MODEM_CALL_NET_ERROR_NORMAL:
            case MODEM_CALL_NET_ERROR_NORMAL_UNSPECIFIED:
              return TONES_EVENT_DROPPED;

            case MODEM_CALL_NET_ERROR_USER_BUSY:
            case MODEM_CALL_NET_ERROR_CALL_REJECTED:
              return TONES_EVENT_BUSY;

            case MODEM_CALL_NET_ERROR_RESP_TO_STATUS:
              return TONES_NONE;

            case MODEM_CALL_NET_ERROR_NO_CHANNEL:
            case MODEM_CALL_NET_ERROR_TEMPORARY_FAILURE:
            case MODEM_CALL_NET_ERROR_CONGESTION:
            case MODEM_CALL_NET_ERROR_CHANNEL_NA:
            case MODEM_CALL_NET_ERROR_QOS_NA:
            case MODEM_CALL_NET_ERROR_BC_NA:
              return TONES_EVENT_CONGESTION;

            default:
              return TONES_EVENT_SPECIAL_INFORMATION;
            }
        }
      else
        {
          switch (cause)
            {
            case MODEM_CALL_ERROR_RELEASE_BY_USER:
              if (causetype == MODEM_CALL_CAUSE_TYPE_LOCAL)
                return TONES_NONE;
              else
                return TONES_EVENT_DROPPED;

            case MODEM_CALL_ERROR_BLACKLIST_BLOCKED:
            case MODEM_CALL_ERROR_BLACKLIST_DELAYED:
              return TONES_EVENT_BUSY;

            case MODEM_CALL_ERROR_CHANNEL_LOSS:
            case MODEM_CALL_ERROR_NO_SERVICE:
            case MODEM_CALL_ERROR_NO_COVERAGE:
              return TONES_EVENT_CONGESTION;

            case MODEM_CALL_ERROR_BUSY_USER_REQUEST:
              if (causetype == MODEM_CALL_CAUSE_TYPE_LOCAL)
                return TONES_NONE;
              else
                return TONES_EVENT_SPECIAL_INFORMATION;
            default:
              return TONES_EVENT_SPECIAL_INFORMATION;
            }
        }
      break;
    }

  return TONES_NONE;
}

int
modem_call_error_tone (GError *error)
{
  if (error == NULL)
    return TONES_NONE;

  if (error->domain == MODEM_CALL_NET_ERRORS)
    return modem_call_event_tone (MODEM_CALL_STATE_DISCONNECTED,
        MODEM_CALL_CAUSE_TYPE_NETWORK, error->code);

  if (error->domain == MODEM_CALL_ERRORS)
    return modem_call_event_tone (MODEM_CALL_STATE_DISCONNECTED,
        MODEM_CALL_CAUSE_TYPE_REMOTE, error->code);

  return TONES_EVENT_SPECIAL_INFORMATION;
}


static void
on_disconnect_reason (DBusGProxy *proxy,
                      char const *reason,
                      gpointer user_data)
{
  ModemCall *self = MODEM_CALL(user_data);
  ModemCallCauseType causetype =
      modem_call_cause_type_from_ofono_disconnect_reason(reason);

  guint cause = MODEM_CALL_ERROR_RELEASE_BY_USER;
  if (causetype == MODEM_CALL_CAUSE_TYPE_NETWORK) {
    /*
     * TODO: Get the exact disconnect cause from modem
     * (not possible with current oFono API)
     */
    cause = MODEM_CALL_NET_ERROR_NETW_OUT_OF_ORDER;
  }

  /* Save the disconnect reason; a state signal will follow shortly */
  g_object_set (self, "cause", cause, NULL);
  g_object_set (self, "causetype", causetype, NULL);
}

static void
on_propertychanged (DBusGProxy *proxy,
		char const *property, 
		GValue *value,
		gpointer user_data)
{
  if (!strcmp (property, "RemoteHeld")) {
    g_signal_emit (self, call_signals[SIGNAL_ON_HOLD], 0, g_value_get_boolean (value));
  }  
}


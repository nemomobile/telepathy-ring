/*
 * modem/call-service.c - Interface towards oFono VoiceCallManager
 *
 * Copyright (C) 2008 Nokia Corporation
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

#define MODEM_DEBUG_FLAG MODEM_SERVICE_CALL

#include "modem/debug.h"

#include "modem/call.h"
#include "modem/ofono.h"
#include "modem/errors.h"

#include "modem/tones.h"

#include "modem/request-private.h"

#include <dbus/dbus-glib-lowlevel.h>

#include "signals-marshal.h"

#include <string.h>
#include <errno.h>

/* ---------------------------------------------------------------------- */

/* ---------------------------------------------------------------------- */

G_DEFINE_TYPE(ModemCallService, modem_call_service, G_TYPE_OBJECT);

/* Properties */
enum {
  PROP_NONE,
  PROP_EMERGENCY_NUMBERS,
  LAST_PROPERTY
};

/* Signals */
enum {
  SIGNAL_CONNECTED,
  SIGNAL_INCOMING,
  SIGNAL_CREATED,
  SIGNAL_USER_CONNECTION,
  SIGNAL_EMERGENCY_NUMBERS_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _ModemCallServicePrivate
{
  DBusGProxy *proxy;            /* D-Bus proxy to call server */

  /* < object_path, call instance > */
  GHashTable *instances;

  struct {
    GQueue queue[1];
    GQueue created[1];
  } dialing;

  struct {
    ModemCall *instance;
    ModemCallConference *conference;
  } conference;

  char **emergency_numbers;

  ModemCall *active, *hold;

  struct {
    GQueue queue[1];
    GError *error;
  } connecting;

  unsigned user_connection:1;   /* Do we have in-band connection? */

  unsigned connected:1, disconnected:1, signals :1, dispose_has_run:1;
  unsigned :0;
};

/* ---------------------------------------------------------------------- */

static ModemOfonoPropsReply reply_to_call_manager_get_properties;
static ModemOfonoPropChangedCb on_manager_property_changed;

static void modem_call_service_connect_to_instance(ModemCallService *self,
  ModemCall *ci);

static void modem_call_service_disconnect_instance(ModemCallService *self,
  ModemCall *ci);

typedef void MyDBusGProxyCallNotify(DBusGProxy *, DBusGProxyCall *, gpointer);

static ModemRequestCallNotify modem_call_request_dial_reply;

static ModemRequestCallNotify modem_call_conference_request_reply;

static void modem_call_service_check_connected(ModemCallService *self,
  ModemRequest *request,
  GError const *error);
#if nomore
static void on_user_connection(DBusGProxy *proxy,
  gboolean attached,
  ModemCallService *self);
#endif
static void on_modem_call_state(ModemCall *, ModemCallState,
  ModemCallService *);
static void on_modem_call_terminated(ModemCall *, ModemCallService *);

/* ---------------------------------------------------------------------- */

static void
modem_call_service_constructed(GObject *object)
{
  if (G_OBJECT_CLASS(modem_call_service_parent_class)->constructed)
    G_OBJECT_CLASS(modem_call_service_parent_class)->constructed(object);
}

static void
modem_call_service_init(ModemCallService *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, MODEM_TYPE_CALL_SERVICE, ModemCallServicePrivate);

  g_queue_init(self->priv->dialing.queue);
  g_queue_init(self->priv->dialing.created);

  g_queue_init(self->priv->connecting.queue);

  self->priv->instances = g_hash_table_new_full(
    g_str_hash, g_str_equal, g_free, g_object_unref);
}

static void
modem_call_service_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  ModemCallService *self = MODEM_CALL_SERVICE(object);

  switch(property_id) {

    case PROP_EMERGENCY_NUMBERS:
      g_value_set_boxed(value, modem_call_get_emergency_numbers(self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
modem_call_service_set_property(GObject *obj,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  ModemCallService *self = MODEM_CALL_SERVICE(obj);
  ModemCallServicePrivate *priv = self->priv;
  gpointer old;

  switch (property_id) {

    case PROP_EMERGENCY_NUMBERS:
      old = priv->emergency_numbers;
      priv->emergency_numbers = g_value_dup_boxed(value);
      g_strfreev(old);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
modem_call_service_dispose(GObject *object)
{
  DEBUG("enter");

  ModemCall *ci;
  GHashTableIter iter[1];
  ModemCallService *self = MODEM_CALL_SERVICE(object);
  ModemCallServicePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  modem_call_service_disconnect(self);

  priv->dispose_has_run = TRUE;

  g_hash_table_iter_init(iter, priv->instances);
  while (g_hash_table_iter_next (iter, NULL, (gpointer)&ci)) {
    modem_call_service_disconnect_instance(self, ci);
  }

  ci = priv->conference.instance;
  modem_call_service_disconnect_instance(self, ci);

  if (priv->proxy)
    g_object_run_dispose(G_OBJECT(priv->proxy));

  if (G_OBJECT_CLASS(modem_call_service_parent_class)->dispose)
    G_OBJECT_CLASS(modem_call_service_parent_class)->dispose(object);

  DEBUG("leave");
}


static void
modem_call_service_finalize(GObject *object)
{
  DEBUG("enter");

  ModemCallService *self = MODEM_CALL_SERVICE(object);
  ModemCallServicePrivate *priv = self->priv;

  if (priv->proxy) g_object_unref(priv->proxy), priv->proxy = NULL;
  g_strfreev(priv->emergency_numbers), priv->emergency_numbers = NULL;
  g_clear_error(&priv->connecting.error);

  g_hash_table_destroy(priv->instances);

  G_OBJECT_CLASS(modem_call_service_parent_class)->finalize(object);

  DEBUG("leave");
}


static void
modem_call_service_class_init(ModemCallServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  g_type_class_add_private(klass, sizeof (ModemCallServicePrivate));

  object_class->constructed = modem_call_service_constructed;
  object_class->get_property = modem_call_service_get_property;
  object_class->set_property = modem_call_service_set_property;
  object_class->dispose = modem_call_service_dispose;
  object_class->finalize = modem_call_service_finalize;

  /* Properties */
  g_object_class_install_property(
    object_class, PROP_EMERGENCY_NUMBERS,
    g_param_spec_boxed("emergency-numbers",
      "Emergency Numbers",
      "List of emergency numbers obtained from modem",
      G_TYPE_STRV,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS));

  /* Signals to emit */
  signals[SIGNAL_CONNECTED] =
    g_signal_new("connected",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[SIGNAL_INCOMING] =
    g_signal_new("incoming",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _modem__marshal_VOID__OBJECT_STRING,
      G_TYPE_NONE, 2,
      MODEM_TYPE_CALL, G_TYPE_STRING);

  signals[SIGNAL_CREATED] =
    g_signal_new("created",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _modem__marshal_VOID__OBJECT_STRING,
      G_TYPE_NONE, 2,
      MODEM_TYPE_CALL, G_TYPE_STRING);

  signals[SIGNAL_USER_CONNECTION] =
    g_signal_new("user-connection",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__BOOLEAN,
      G_TYPE_NONE, 1,
      G_TYPE_BOOLEAN);

  signals[SIGNAL_EMERGENCY_NUMBERS_CHANGED] =
    g_signal_new("emergency-numbers-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__BOXED,
      G_TYPE_NONE, 1,
      G_TYPE_STRV);


  modem_error_domain_prefix(0); /* Init errors */
}

/* ---------------------------------------------------------------------- */

#define RETURN_NULL_IF_NOT_VALID(self)                                  \
  g_return_val_if_fail(self != NULL && self->priv->proxy != NULL &&     \
    !self->priv->dispose_has_run, NULL)

/* ---------------------------------------------------------------------- */

static ModemOfonoGetDescsReply reply_to_call_manager_get_calls;

static void on_manager_call_added(DBusGProxy *proxy,
    char const *path,
    GHashTable *properties,
    gpointer user_data);

/* ---------------------------------------------------------------------- */

static void
modem_call_service_connect_to_instance(ModemCallService *self,
  ModemCall *instance)
{
  if (instance) {
    g_signal_connect(instance, "state",
      G_CALLBACK(on_modem_call_state), self);
    g_signal_connect_after(instance, "terminated",
      G_CALLBACK(on_modem_call_terminated), self);
  }
}

static void
modem_call_service_disconnect_instance(ModemCallService *self,
  ModemCall *instance)
{
  if (instance) {
    g_signal_handlers_disconnect_by_func(instance,
      on_modem_call_state, self);
    g_signal_handlers_disconnect_by_func(instance,
      on_modem_call_terminated, self);
  }
}

static ModemCall *
modem_call_service_ensure_instance(ModemCallService *self,
                                   char const *object_path,
                                   GHashTable *properties)
{
  ModemCallServicePrivate *priv = self->priv;
  char *key;
  GValue *value;
  GHashTableIter iter[1];
  char const *remote;
  ModemCallState state;
  gboolean incoming = FALSE, originating = FALSE;
  ModemCall *ci;

  DEBUG("path %s", object_path);

  if (DEBUGGING) {
    for (g_hash_table_iter_init(iter, properties);
         g_hash_table_iter_next(iter, (gpointer)&key, (gpointer)&value);) {
      char *s = g_strdup_value_contents(value);
      DEBUG("%s = %s", key, s);
      g_free(s);
    }
  }

  ci = g_hash_table_lookup(priv->instances, object_path);
  if (ci) {
    DEBUG("call already exists %p", (void *)ci);
    return ci;
  }

  value = g_hash_table_lookup(properties, "LineIdentification");
  remote = g_value_get_string(value);

  value = g_hash_table_lookup(properties, "State");
  state = modem_call_state_from_ofono_state(g_value_get_string(value));

  switch (state) {
  case MODEM_CALL_STATE_INCOMING:
  case MODEM_CALL_STATE_WAITING:
    incoming = TRUE;
    originating = FALSE;
    break;

  case MODEM_CALL_STATE_DIALING:
  case MODEM_CALL_STATE_ALERTING:
  case MODEM_CALL_STATE_ACTIVE:
  case MODEM_CALL_STATE_HELD:
    incoming = FALSE;
    originating = TRUE;
    break;

  case MODEM_CALL_STATE_INVALID:
  case MODEM_CALL_STATE_DISCONNECTED:
    DEBUG("call already in invalid state");
    return NULL;
  }

  ci = g_object_new(MODEM_TYPE_CALL,
      "call-service", self,
      "object-path", object_path,
      "remote", remote,
      "state", state,
      "terminating", !originating,
      "originating",  originating,
      NULL);

  modem_call_service_connect_to_instance(self, ci);
  g_hash_table_insert(priv->instances, g_strdup(object_path), ci);

  if (incoming) {
    DEBUG("emit \"incoming\"(%s (%p), %s)",
        modem_call_get_name(ci), ci, remote);
    g_signal_emit(self, signals[SIGNAL_INCOMING], 0, ci, remote);
  }
  else if (g_queue_is_empty(priv->dialing.queue)) {
    DEBUG("emit \"created\"(%s (%p), %s)",
        modem_call_get_name(ci), ci, remote);
    g_signal_emit(self, signals[SIGNAL_CREATED], 0, ci, remote);
  }
  else {
    g_queue_push_tail(priv->dialing.created, ci);
  }

  return ci;
}

static ModemCall *
modem_call_service_get_dialed(ModemCallService *self,
                              char const *object_path,
                              char const *remote)
{
  ModemCallServicePrivate *priv = self->priv;
  ModemCall *ci;

  ci = g_hash_table_lookup(priv->instances, object_path);
  if (ci) {
    DEBUG("call already exists %p", (void *)ci);

    if (g_queue_find(priv->dialing.created, ci))
      g_queue_remove(priv->dialing.created, ci);

    return ci;
  }

  ci = g_object_new(MODEM_TYPE_CALL,
      "call-service", self,
      "object-path", object_path,
      "remote", remote,
      "state", MODEM_CALL_STATE_DIALING,
      "terminating", FALSE,
      "originating", TRUE,
      NULL);

  modem_call_service_connect_to_instance(self, ci);
  g_hash_table_insert(priv->instances, g_strdup(object_path), ci);

  return ci;
}

/* ---------------------------------------------------------------------- */

DBusGProxy *
_modem_call_service_proxy(ModemCallService *self)
{
  RETURN_NULL_IF_NOT_VALID(self);

  return self->priv->proxy;
}

/* ---------------------------------------------------------------------- */
/* ModemCallService interface */

gboolean
modem_call_service_connect(ModemCallService *self,
  char const *object_path)
{
  ModemCallServicePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return FALSE;

  if (priv->disconnected)
    return FALSE;

  if (priv->connected)
    return TRUE;

  if (!g_queue_is_empty(priv->connecting.queue))
    return TRUE;

  g_clear_error(&priv->connecting.error);

  if (!priv->proxy)
    priv->proxy = modem_ofono_proxy(object_path,
                  OFONO_IFACE_CALL_MANAGER);

  if (!priv->proxy) {
    g_error("Failed to proxy the call service");
    return FALSE;
  }

  if (!priv->signals) {
    priv->signals = TRUE;

    modem_ofono_proxy_connect_to_property_changed(
        priv->proxy, on_manager_property_changed, self);

    dbus_g_proxy_add_signal(priv->proxy, "CallAdded",
        DBUS_TYPE_G_OBJECT_PATH, MODEM_TYPE_DBUS_DICT, G_TYPE_INVALID);

    dbus_g_proxy_connect_signal(priv->proxy, "CallAdded",
        G_CALLBACK(on_manager_call_added), self, NULL);
  }

  g_queue_push_tail(priv->connecting.queue,
      modem_ofono_proxy_request_properties(priv->proxy,
          reply_to_call_manager_get_properties, self, NULL));

  g_queue_push_tail(priv->connecting.queue,
      modem_ofono_request_descs(self, priv->proxy,
          "GetCalls", reply_to_call_manager_get_calls, NULL));

  return TRUE;
}

#if 0
static void
refresh_conference_memberships(ModemCallService *self,
  GPtrArray *members)
{
  guint i;
  char const *path;
  ModemCall *ci;
  GList *instances;

  instances = g_hash_table_get_values(self->priv->instances);

  for (i = 0; i < members->len; i++) {
    path = g_ptr_array_index(members, i);

    ci = g_hash_table_lookup(self->priv->instances, path);
    if (ci != NULL) {
      if (!modem_call_is_member(ci))
        g_object_set(ci, "member", TRUE, NULL);

      instances = g_list_remove(instances, ci);
    }
  }

  /* The remaining instances aren't members */
  while (instances) {
    ci = instances->data;

    if (modem_call_is_member(ci))
      g_object_set(ci, "member", FALSE, NULL);

    instances = g_list_delete_link(instances, instances);
  }
}
#endif

static void
on_manager_property_changed(DBusGProxy *proxy,
  char const *property,
  GValue const *value,
  gpointer user_data)
{
  ModemCallService *self = MODEM_CALL_SERVICE(user_data);

  if (DEBUGGING) {
    char *s;
    DEBUG("enter");
    s = g_strdup_value_contents(value);
    DEBUG("%s = %s", property, s);
    g_free(s);
  }

  if (!strcmp(property, "EmergencyNumbers")) {
    g_object_set_property(G_OBJECT (self), "emergency-numbers", value);
    g_signal_emit(self, signals[SIGNAL_EMERGENCY_NUMBERS_CHANGED], 0,
      modem_call_get_emergency_numbers(self));
  }
#if 0
  else if (!strcmp(property, "MultipartyCalls")) {
    refresh_conference_memberships(self, g_value_get_boxed(value));
  }
#endif
}

static void
reply_to_call_manager_get_properties(gpointer _self,
  ModemRequest *request,
  GHashTable *properties,
  GError const *error,
  gpointer user_data)
{
  ModemCallService *self = MODEM_CALL_SERVICE(_self);

  DEBUG("enter");

  if (!error) {
    GValue *value;

    value = g_hash_table_lookup(properties, "EmergencyNumbers");
    if (value) {
      g_object_set_property(G_OBJECT (self), "emergency-numbers", value);
    }

    if (DEBUGGING) {
      char *key;
      GHashTableIter iter[1];

      for (g_hash_table_iter_init(iter, properties);
           g_hash_table_iter_next(iter, (gpointer)&key, (gpointer)&value);) {
        char *s = g_strdup_value_contents(value);
        DEBUG("%s = %s", key, s);
        g_free(s);
      }
    }
  }

  modem_call_service_check_connected(self, request, error);
}

static void
on_manager_call_added(DBusGProxy *proxy,
                      char const *path,
                      GHashTable *properties,
                      gpointer user_data)
{
  ModemCallService *self = MODEM_CALL_SERVICE(user_data);

  DEBUG("%s", path);

  modem_call_service_ensure_instance(self, path, properties);
}

static void
reply_to_call_manager_get_calls(gpointer _self,
                                ModemRequest *request,
                                GPtrArray *array,
                                GError const *error,
                                gpointer user_data)
{
  ModemCallService *self = _self;

  DEBUG("enter");

  if (!error) {
    guint i;

    for (i = 0; i < array->len; i++) {
      GValueArray *va = g_ptr_array_index(array, i);
      char const *path = g_value_get_boxed(va->values + 0);
      GHashTable *properties = g_value_get_boxed(va->values + 1);

      modem_call_service_ensure_instance(self, path, properties);
    }
  }

  modem_call_service_check_connected(self, request, error);
}

static void
modem_call_service_check_connected(ModemCallService *self,
  ModemRequest *request,
  GError const *error)
{
  ModemCallServicePrivate *priv = self->priv;

  if (g_queue_find(priv->connecting.queue, request)) {
    g_queue_remove(priv->connecting.queue, request);

    if (error) {
      if (!priv->connecting.error ||
        priv->connecting.error->domain == DBUS_GERROR) {
        g_clear_error(&priv->connecting.error);
        g_set_error(&priv->connecting.error,
          error->domain, error->code,
          "%s", error->message);
      }

      modem_critical(MODEM_SERVICE_CALL, GERROR_MSG_FMT,
        GERROR_MSG_CODE(priv->connecting.error));
    }

    if (g_queue_is_empty(priv->connecting.queue)) {
      priv->connected = priv->connecting.error == NULL;
      DEBUG("emit \"connected\"(%s)", priv->connected ? "TRUE" : "FALSE");
      g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
    }
  }
}

/** Disconnect from call service */
void
modem_call_service_disconnect(ModemCallService *self)
{
  DEBUG("(%p): enter", self);

  ModemCallServicePrivate *priv = self->priv;
  unsigned was_connected = priv->connected;

  g_return_if_fail(!priv->dispose_has_run);

  if (priv->disconnected)
    return;

  priv->connected = FALSE;
  priv->disconnected = TRUE;

  while (!g_queue_is_empty(priv->connecting.queue)) {
    ModemRequest *request = g_queue_pop_head(priv->connecting.queue);
    modem_request_cancel(request);
  }

  while (!g_queue_is_empty(priv->dialing.queue)) {
    ModemRequest *request = g_queue_pop_head(priv->connecting.queue);
    modem_request_cancel(request);
  }

  if (priv->signals) {
    priv->signals = FALSE;

    modem_ofono_proxy_disconnect_from_property_changed(
      priv->proxy, on_manager_property_changed, self);

    dbus_g_proxy_disconnect_signal(priv->proxy, "CallAdded",
      G_CALLBACK(on_manager_call_added), self);
  }

  g_clear_error(&priv->connecting.error);

  if (was_connected)
    g_signal_emit(self, signals[SIGNAL_CONNECTED], 0);
}

gboolean
modem_call_service_is_connected(ModemCallService const *self)
{
  return MODEM_IS_CALL_SERVICE(self) && self->priv->connected;
}

gboolean
modem_call_service_is_connecting(ModemCallService const *self)
{
  return MODEM_IS_CALL_SERVICE(self)
    && !g_queue_is_empty(self->priv->connecting.queue);
}


void
modem_call_service_resume(ModemCallService *self)
{
  DEBUG("enter");

  GHashTableIter iter[1];
  ModemCall *membercall = NULL;
  ModemCall *ci;
#if nomore
  ModemCallConference *mcc;
#endif

  if (!self->priv->connected)
    return;

  g_signal_emit(self, signals[SIGNAL_EMERGENCY_NUMBERS_CHANGED], 0,
    modem_call_get_emergency_numbers(self));

  g_hash_table_iter_init(iter, self->priv->instances);
  while (g_hash_table_iter_next(iter, NULL, (gpointer)&ci)) {
    char *remote;
    gboolean terminating = FALSE, member = FALSE;
    ModemCallState state;

    g_object_get(ci,
      "state", &state,
      "member", &member,
      "remote", &remote,
      "terminating", &terminating,
      NULL);

    if (member)
      membercall = ci;

    if (state != MODEM_CALL_STATE_DISCONNECTED &&
      state != MODEM_CALL_STATE_INVALID) {

      /* XXX - atm the value of 'terminating' cannot be trusted.
       * oFono should probably provide the direction as a property
       * since we cannot rely on the call state here. */
      if (terminating) {
        modem_message(MODEM_SERVICE_CALL,
          "incoming [with state %s] call from \"%s\"",
          modem_call_get_state_name(state), remote);
        DEBUG("emit \"incoming\"(%s (%p), %s)",
          modem_call_get_name(ci), ci, remote);
        g_signal_emit(self, signals[SIGNAL_INCOMING], 0, ci, remote);
      }
      else {
        modem_message(MODEM_SERVICE_CALL,
          "created [with state %s] call to \"%s\"",
          modem_call_get_state_name(state), remote);
        DEBUG("emit \"created\"(%s (%p), %s)",
          modem_call_get_name(ci), ci, remote);
        g_signal_emit(self, signals[SIGNAL_CREATED], 0, ci, remote);
      }

      g_signal_emit_by_name(ci, "state", state, 0, 0);
    }

    g_free(remote);
  }

#if nomore
  mcc = self->priv->conference.conference;

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    ModemCall *ci = self->priv->instances[i].instance;
    gboolean member = FALSE;

    g_object_get(ci, "member", &member, NULL);

    if (!member)
      continue;

    g_signal_emit_by_name(mcc, "joined", ci);
  }

  g_signal_emit_by_name(mcc, "state",
    modem_call_get_state(MODEM_CALL(mcc)), 0, 0);
#endif
}

/* ---------------------------------------------------------------------- */
/* Obtain the list of emergency numbers (usually from SIM) */

static char const modem_call_sos[] = "urn:service:sos";

/** Get currently cached list of emergency numbers. */
char const * const *
modem_call_get_emergency_numbers(ModemCallService *self)
{
  static char const * const default_numbers[] = {
    "112", "911", "118", "119", "000", "110", "08", "999", NULL
  };

  if (MODEM_IS_CALL_SERVICE(self) && self->priv->emergency_numbers) {
    return (char const * const *)self->priv->emergency_numbers;
  }

  return default_numbers;
}

/** Get emergency service corresponding to number. */
char const *
modem_call_get_emergency_service(ModemCallService *self,
  char const *destination)
{
  char const * const *numbers;

  if (destination == NULL)
    return NULL;

  if (modem_call_get_valid_emergency_urn(destination))
    return modem_call_get_valid_emergency_urn(destination);

  numbers = modem_call_get_emergency_numbers(self);

  for (; *numbers; numbers++) {
    size_t n = strlen(*numbers);

    if (!g_str_has_prefix(destination, *numbers))
      continue;

    if (destination[n] && destination[n] != 'p' && destination[n] != 'w')
      continue;

    return modem_call_sos;
  }

  return NULL;
}

/** Check if @urn is an emergency service. */
char const *
modem_call_get_valid_emergency_urn(char const *urn)
{
  /* urn:service:sos see RFC 5031
   *
   * service-URN  = "URN:service:" service
   * service      = top-level *("." sub-service)
   * top-level    = let-dig [ *25let-dig-hyp let-dig ]
   * sub-service  = let-dig [ *let-dig-hyp let-dig ]
   * let-dig-hyp  = let-dig / "-"
   * let-dig      = ALPHA / DIGIT
   * ALPHA        = %x41-5A / %x61-7A   ; A-Z / a-z
   * DIGIT        = %x30-39 ; 0-9
   */
  char const *sos;
  int i, dot, hyp;
  int n = (sizeof modem_call_sos) - 1;

  if (urn == NULL)
    return NULL;

  if (g_ascii_strncasecmp(urn, modem_call_sos, n))
    return NULL;

  sos = urn + n;

  if (sos[0] == '\0')
    return urn;                 /* sos service */

  if (sos[0] != '.')
    return NULL;                /* Not a sos. subservice */

  for (i = 1, dot = 0, hyp = -2; ; i++) {
    /* A-Z a-z 0-9 - . */
    if (sos[i] == '\0') {
      if (i > hyp + 1 && i > dot + 1)
        return urn;
    }
    if (('a' <= sos[i] && sos[i] <= 'z') ||
      ('0' <= sos[i] && sos[i] <= '9') ||
      ('A' <= sos[i] && sos[i] <= 'Z'))
      continue;
    else if (sos[i] == '-') {
      if (i > dot + 1) {
        hyp = i;
        continue;
      }
    }
    else if (sos[i] == '.') {
      if (i > dot + 1 && i > hyp + 1) {
        dot = i;
        continue;
      }
    }

    return modem_call_sos;      /* Invalid syntax */
  }
}

/* ---------------------------------------------------------------------- */

#if nomore
static void
on_user_connection(DBusGProxy *proxy,
  gboolean attached,
  ModemCallService *self)
{
  DEBUG("(%p, %d, %p): enter", proxy, attached, self);

  MODEM_CALL_SERVICE(self)->priv->user_connection = attached;

  g_signal_emit(self, signals[SIGNAL_USER_CONNECTION], 0,
    attached);
}
#endif

static void request_notify_cancel(gpointer data);

ModemRequest *
modem_call_request_dial(ModemCallService *self,
  char const *destination,
  ModemClirOverride clir,
  ModemCallRequestDialReply callback,
  gpointer user_data)
{
  char const *clir_str;
  ModemRequest *request;
  ModemCallServicePrivate *priv = self->priv;

  RETURN_NULL_IF_NOT_VALID(self);

  DEBUG("called");

  g_return_val_if_fail(priv->connected, NULL);
  g_return_val_if_fail(destination != NULL, NULL);
  g_return_val_if_fail(callback != NULL, NULL);

  modem_message(MODEM_SERVICE_CALL,
    "trying to create call to \"%s\"",
    destination);

  if (clir == MODEM_CLIR_OVERRIDE_DISABLED)
    clir_str = "disabled";
  else if (clir == MODEM_CLIR_OVERRIDE_ENABLED)
    clir_str = "enabled";
  else
    clir_str = "";

  request = modem_request_begin(self, priv->proxy,
            "Dial", modem_call_request_dial_reply,
            G_CALLBACK(callback), user_data,
            G_TYPE_STRING, destination,
            G_TYPE_STRING, clir_str,
            G_TYPE_INVALID);


  modem_request_add_cancel_notify(request, request_notify_cancel);

  modem_request_add_data_full(request,
    "call-destination",
    g_strdup(destination),
    g_free);

  g_queue_push_tail(priv->dialing.queue, request);

  return request;
}

static void
request_notify_cancel(gpointer _request)
{
  modem_request_add_qdata(_request,
    g_quark_from_static_string("call-canceled"),
    GUINT_TO_POINTER(1));
}

static void
modem_call_request_dial_reply(DBusGProxy *proxy,
  DBusGProxyCall *call,
  void *_request)
{
  DEBUG("enter");

  ModemRequest *request = _request;
  ModemCallService *self = MODEM_CALL_SERVICE(modem_request_object(request));
  ModemCallServicePrivate *priv = self->priv;
  ModemCallRequestDialReply *callback = modem_request_callback(request);
  gpointer user_data = modem_request_user_data(request);
  char *destination = modem_request_get_data(request, "call-destination");
  GError *error = NULL;
  ModemCall *ci = NULL;
  char *object_path = NULL;

  if (dbus_g_proxy_end_call(proxy, call, &error,
      DBUS_TYPE_G_OBJECT_PATH, &object_path,
      G_TYPE_INVALID)) {
    ci = modem_call_service_get_dialed(self, object_path, destination);
  }
  else {
    object_path = NULL;
    modem_error_fix(&error);
  }

  if (ci) {
    DEBUG("%s: instance %s (%p)", OFONO_IFACE_CALL_MANAGER ".Dial",
      object_path, (void *)ci);

    modem_message(MODEM_SERVICE_CALL,
      "call create request to \"%s\" successful",
      destination);
  }
  else {
    char ebuffer[32];

    modem_message(MODEM_SERVICE_CALL,
      "call create request to \"%s\" failed: %s.%s: %s",
      destination,
      modem_error_domain_prefix(error->domain),
      modem_error_name(error, ebuffer, sizeof ebuffer),
      error->message);

    DEBUG("%s: " GERROR_MSG_FMT, OFONO_IFACE_CALL_MANAGER ".Dial",
      GERROR_MSG_CODE(error));
  }

  if (modem_request_get_data(request, "call-canceled")) {
    if (ci)
      modem_call_request_release(ci, NULL, NULL);
  }
  else {
    g_assert(ci || error);
    callback(self, request, ci, error, user_data);
  }

  if (g_queue_find(priv->dialing.queue, request))
    g_queue_remove(priv->dialing.queue, request);

  while (g_queue_is_empty(priv->dialing.queue) &&
      !g_queue_is_empty(priv->dialing.created)) {
    char *remote;

    ci = g_queue_pop_head(priv->dialing.created);

    g_object_get(ci, "remote", &remote, NULL);

    g_signal_emit(self, signals[SIGNAL_CREATED], 0, ci, remote);

    g_free(remote);
  }

  g_free(object_path);
  g_clear_error(&error);
}

static void
on_modem_call_state(ModemCall *ci,
  ModemCallState state,
  ModemCallService *self)
{
  ModemCallServicePrivate *priv = MODEM_CALL_SERVICE(self)->priv;
  gboolean releasing = FALSE;

  if (priv->disconnected)
    return;

  switch (state) {
    case MODEM_CALL_STATE_ACTIVE:
      if (priv->hold == ci)
        priv->hold = NULL;
      if (!modem_call_is_member(ci))
        priv->active = ci;
      break;

    case MODEM_CALL_STATE_HELD:
      if (priv->active == ci)
        priv->active = NULL;
      if (!modem_call_is_member(ci))
        priv->hold = ci;
      break;

    case MODEM_CALL_STATE_DISCONNECTED:
      releasing = TRUE;
      /* FALLTHROUGH */
    case MODEM_CALL_STATE_INVALID:
      if (priv->active == ci)
        priv->active = NULL;
      if (priv->hold == ci)
        priv->hold = NULL;
      break;

    default:
      break;
  }

#if nomore
  if (releasing) {
    GError *error = modem_call_new_error(causetype, cause, NULL);
    gboolean originating;
    char *remote;
    char const *what;

    g_object_get(ci,
      "originating", &originating,
      "remote", &remote,
      NULL);

    switch (state) {
      case MODEM_CALL_STATE_MO_RELEASE:
        what = "mo-released";
        break;
      case MODEM_CALL_STATE_MT_RELEASE:
        what = "mt-released";
        break;
      default:
        what = "terminated";
    }

    gboolean mpty = MODEM_IS_CALL_CONFERENCE(ci);

    modem_message(MODEM_SERVICE_CALL,
      "%s %s %s%s%s %s.%s: %s",
      what,
      mpty ? "conference"
      : originating ? "outgoing call to"
      : "incoming call from",
      mpty ? "" : "'",
      mpty ? "call" : remote,
      mpty ? "" : "'",
      modem_error_domain_prefix(error->domain),
      modem_error_name(error, NULL, 0),
      error->message);

    g_free(remote);
    g_error_free(error);
  }
#endif
}

static void
on_modem_call_terminated(ModemCall *ci,
  ModemCallService *self)
{
  char const *path;
  ModemCallServicePrivate *priv = self->priv;

  path = modem_call_get_path(ci);
  if (path)
    g_hash_table_remove (priv->instances, path);
}

ModemRequest *
modem_call_request_conference(ModemCallService *self,
  ModemCallServiceReply *callback,
  gpointer user_data)
{
  RETURN_NULL_IF_NOT_VALID(self);

  return modem_request(MODEM_CALL_SERVICE(self), self->priv->proxy,
    "CreateMultiparty",
    modem_call_conference_request_reply,
    G_CALLBACK(callback), user_data,
    G_TYPE_INVALID);
}

static void
modem_call_conference_request_reply(DBusGProxy *proxy,
  DBusGProxyCall *call,
  void *_request)
{
  DEBUG("enter");

  GPtrArray *paths;
  ModemRequest *request = _request;
  ModemCallService *self = modem_request_object(request);
  ModemCallServiceReply *callback = modem_request_callback(request);
  gpointer user_data = modem_request_user_data(request);
  GError *error = NULL;

  if (dbus_g_proxy_end_call(proxy, call, &error,
      MODEM_TYPE_ARRAY_OF_PATHS, &paths,
      G_TYPE_INVALID)) {

    guint i;
    char const *path;
    ModemCall *ci;

    for (i = 0; i < paths->len; i++) {
      path = g_ptr_array_index(paths, i);
      ci = g_hash_table_lookup(self->priv->instances, path);
      if (ci != NULL) {
        g_object_set(ci, "member", TRUE, NULL);
      }
    }

    g_boxed_free(MODEM_TYPE_ARRAY_OF_PATHS, paths);
  }
  else {
    modem_error_fix(&error);
  }

  callback(self, request, error, user_data);
  g_clear_error(&error);
}


ModemCall *
modem_call_service_get_call(ModemCallService *self, char const *object_path)
{
  ModemCallServicePrivate *priv = self->priv;

  return g_hash_table_lookup(priv->instances, object_path);
}

/**
 * modem_call_service_get_calls:
 * @self: ModemCallService object
 *
 * Obtains NULL-terminated list of ordinary ModemCall objects. Note that the
 * reference count of ModemCall objects is not incremented and object
 * pointers are valid only during the lifetime of the ModemCallService object.
 * The returned list can be reclaimed with g_free().
 *
 * Returns: NULL-terminated list of pointers to ModemCall objects.
 */
ModemCall **
modem_call_service_get_calls(ModemCallService *self)
{
  ModemCall *ci;
  GPtrArray *calls;
  GHashTableIter iter[1];

  calls = g_ptr_array_sized_new(MODEM_MAX_CALLS + 1);

  g_hash_table_iter_init(iter, self->priv->instances);
  while (g_hash_table_iter_next(iter, NULL, (gpointer)&ci)) {
    g_ptr_array_add(calls, ci);
  }

  g_ptr_array_add(calls, NULL);

  return (ModemCall **)g_ptr_array_free(calls, FALSE);
}

ModemCallConference *
modem_call_service_get_conference(ModemCallService *self)
{
  return MODEM_IS_CALL_SERVICE(self)
    ? self->priv->conference.conference
    : NULL;
}

/* ------------------------------------------------------------------------- */

static char const *
_modem_call_validate_address(char const *address)
{
  size_t n, m;

  if (address == NULL)
    return "no destination";

  n = (sizeof modem_call_sos) - 1;

  if (g_ascii_strncasecmp(address, modem_call_sos, n) == 0) {
    if (address[n] != '\0' && address[n] != '.')
      return "invalid service urn";
    return NULL;
  }

  /* Remove regocnized service prefixes */
  if (g_str_has_prefix(address, "*31#"))
    address += 4;
  else if (g_str_has_prefix(address, "#31#"))
    address += 4;

  if (*address == '+')
    address++;

  n = strspn(address, "0123456789abc*#");
  if (n == 0) {
    if (address[n])
      return "not a phone number";
    else
      return "too short";
  }
  if (n > 20)
    return "too long";

  if (address[n - 1] == '#')
    return "invalid service code";

  /* Possible dialstring */
  m = strspn(address + n, "0123456789abc*#pwPW");
  if (address[n + m] != '\0') {
    if (m == 0)
      return "invalid address";
    else
      return "invalid dial string";
  }

  if (m == 1)
    return "invalid dial string";

  /* OK */
  return NULL;
}

gboolean
modem_call_is_valid_address(char const *address)
{
  return _modem_call_validate_address(address) == NULL;
}

gboolean
modem_call_validate_address(char const *address, GError **error)
{
  char const *message = _modem_call_validate_address(address);

  if (message) {
    g_set_error_literal(error,
      MODEM_CALL_ERRORS, MODEM_CALL_ERROR_INVALID_ADDRESS, message);
    return FALSE;
  }

  return TRUE;
}

void
modem_call_split_address(char const *address,
  char **return_address,
  char **return_dialstring,
  ModemClirOverride *return_clir)
{
  char const *emergency;
  size_t nan;

  g_return_if_fail(address != NULL);
  g_return_if_fail(return_address != NULL);
  g_return_if_fail(return_dialstring != NULL);
  g_return_if_fail(return_clir);

  emergency = modem_call_get_valid_emergency_urn(address);
  if (emergency) {
    *return_address = g_strdup(emergency);
    *return_dialstring = NULL;
    *return_clir = MODEM_CLIR_OVERRIDE_DEFAULT;
    return;
  }

  if (g_str_has_prefix(address, "*31#")) {
    address += strlen("*31#");
    /* According to 3GPP 22.030, *31# suppresses CLIR */
    *return_clir = MODEM_CLIR_OVERRIDE_DISABLED;
  }

  if (g_str_has_prefix(address, "#31#")) {
    /* According to 3GPP 22.030, #31# invokes CLIR */
    address += strlen("#31#");
    *return_clir = MODEM_CLIR_OVERRIDE_ENABLED;
  }

  nan = strspn(address, "+0123456789*#ABCabc");

  *return_address = g_strndup(address, nan);
  if (address[nan])
    *return_dialstring = g_strdup(address + nan);
}

/*
 * ring-conference-manager.c - Manager for conference channels
 *
 * Copyright (C) 2007-2011 Nokia Corporation
 *   @author Pekka Pessi <first.surname@nokia.com>
 *   @author Lassi Syrjala <first.surname@nokia.com>
 *   @author Kai Vehmanen <first.surname@nokia.com>
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

/*
 * Based on telepathy-glib/examples/cm/echo/factory.c with notice:
 *
 * """
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 * """
 */

#include "config.h"

#define DEBUG_FLAG RING_DEBUG_CONNECTION
#include "ring-debug.h"

#include "ring-conference-manager.h"
#include "ring-conference-channel.h"
#include "ring-connection.h"
#include "ring-param-spec.h"
#include "ring-util.h"

#include "ring-extensions/ring-extensions.h"

#include "modem/call.h"
#include "modem/tones.h"

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include <dbus/dbus-glib.h>

#include <string.h>

static void manager_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (RingConferenceManager, ring_conference_manager,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_MANAGER, manager_iface_init));

struct _RingConferenceManagerPrivate
{
  RingConnection *connection;
  ModemCallService *call_service;

  GHashTable *channels;

  unsigned dispose_has_run:1, :0;

  struct {
    gulong status_changed;
  } signals;
};

enum
{
  PROP_NONE,
  PROP_CONNECTION,
  PROP_CALL_SERVICE,
  N_PROPS
};

typedef enum
  {
    METHOD_COMPATIBLE,
    METHOD_CREATE,
    METHOD_ENSURE
  } RequestotronMethod;

/* ---------------------------------------------------------------------- */

static void conference_manager_emit_new_channel (RingConferenceManager *,
    gpointer request, gpointer channel, GError *error);

static gboolean conference_requestotron (RingConferenceManager *,
    gpointer,
    GHashTable *);

static void set_call_service (RingConferenceManager *, ModemCallService *);
static void conference_removed (gpointer _channel);

static void on_connection_status_changed (TpBaseConnection *conn,
    guint status, guint reason,
    RingConferenceManager *self);

#define METHOD(i, x) (i ## _ ## x)

/* ---------------------------------------------------------------------- */
/* GObject interface */

static void
ring_conference_manager_init (RingConferenceManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (
      self, RING_TYPE_CONFERENCE_MANAGER, RingConferenceManagerPrivate);

  self->priv->channels = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, conference_removed);
}

static void
ring_conference_manager_constructed (GObject *object)
{
  RingConferenceManager *self = RING_CONFERENCE_MANAGER(object);
  RingConferenceManagerPrivate *priv = self->priv;

  priv->signals.status_changed = g_signal_connect (priv->connection,
      "status-changed", (GCallback) on_connection_status_changed, self);

  if (G_OBJECT_CLASS (ring_conference_manager_parent_class)->constructed)
    G_OBJECT_CLASS (ring_conference_manager_parent_class)->constructed (object);
}

static void
ring_conference_manager_dispose (GObject *object)
{
  RingConferenceManager *self = RING_CONFERENCE_MANAGER (object);
  RingConferenceManagerPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;

  ring_signal_disconnect (priv->connection, &priv->signals.status_changed);

  set_call_service (self, NULL);

  ((GObjectClass *) ring_conference_manager_parent_class)->dispose (object);
}

static void
ring_conference_manager_finalize (GObject *object)
{
  RingConferenceManager *self = RING_CONFERENCE_MANAGER (object);
  RingConferenceManagerPrivate *priv = self->priv;

  g_hash_table_destroy (priv->channels);
}

static void
ring_conference_manager_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
  RingConferenceManager *self = RING_CONFERENCE_MANAGER (object);
  RingConferenceManagerPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, priv->connection);
      break;
    case PROP_CALL_SERVICE:
      g_value_set_pointer (value, priv->call_service);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ring_conference_manager_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
  RingConferenceManager *self = RING_CONFERENCE_MANAGER (object);
  RingConferenceManagerPrivate *priv = self->priv;

  switch (property_id)
    {
    case PROP_CONNECTION:
      /* We don't ref the connection, because it owns a reference to the
       * factory, and it guarantees that the factory's lifetime is
       * less than its lifetime */
      priv->connection = g_value_get_object (value);
      break;
    case PROP_CALL_SERVICE:
      set_call_service (self, g_value_get_pointer (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ring_conference_manager_class_init (RingConferenceManagerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (RingConferenceManagerPrivate));

  object_class->constructed = ring_conference_manager_constructed;
  object_class->dispose = ring_conference_manager_dispose;
  object_class->finalize = ring_conference_manager_finalize;
  object_class->get_property = ring_conference_manager_get_property;
  object_class->set_property = ring_conference_manager_set_property;

  g_object_class_install_property (object_class, PROP_CONNECTION,
      ring_param_spec_connection ());

  g_object_class_install_property (object_class, PROP_CALL_SERVICE,
      g_param_spec_pointer ("call-service",
          "Call Manager Object",
          "oFono Call Manager",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/* ---------------------------------------------------------------------- */
/* RingConferenceManager interface */

RingConferenceChannel *
ring_conference_manager_lookup (RingConferenceManager *self,
                                char const *object_path)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (object_path != NULL, NULL);

  return g_hash_table_lookup (self->priv->channels, object_path);
}

static void
set_call_service (RingConferenceManager *self, ModemCallService *service)
{
  RingConferenceManagerPrivate *priv = self->priv;

  if (priv->call_service)
    {
      if (priv->call_service)
        g_object_unref (priv->call_service);
      priv->call_service = NULL;
    }

  if (service)
    {
      priv->call_service = g_object_ref (MODEM_CALL_SERVICE (service));
    }
}

static void
conference_removed (gpointer _channel)
{
  if (!tp_base_channel_is_destroyed (_channel))
    {
      /* Ensure "Closed" has been emitted */
      g_object_run_dispose (_channel);
    }

  g_object_unref (_channel);
}

static void
on_connection_status_changed (TpBaseConnection *conn,
                              guint status,
                              guint reason,
                              RingConferenceManager *self)
{
  RingConferenceManagerPrivate *priv = self->priv;

  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      g_hash_table_remove_all (priv->channels);
    }
}

/* ---------------------------------------------------------------------- */
/* TpChannelManagerIface interface */

static GHashTable *
conference_channel_fixed_properties (void)
{
  static GHashTable *hash;

  if (hash)
    return hash;

  hash = g_hash_table_new (g_str_hash, g_str_equal);

  char const *key;
  GValue *value;

  key = TP_IFACE_CHANNEL ".ChannelType";
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_static_string (value, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA);

  g_hash_table_insert (hash, (gpointer)key, value);

  return hash;
}

static char const * const conference_channel_allowed_properties[] =
  {
    TP_IFACE_CHANNEL ".InitialChannels",
    TP_IFACE_CHANNEL ".TargetHandleType",
    TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA ".InitialAudio",
    TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA ".InitialVideo",
    NULL
  };

RingInitialMembers *
tp_asv_get_initial_members (GHashTable *properties)
{
  return tp_asv_get_boxed (properties,
      TP_IFACE_CHANNEL ".InitialChannels",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);
}

static void
conference_manager_foreach_channel_class (TpChannelManager *_self,
                                          TpChannelManagerChannelClassFunc func,
                                          gpointer userdata)
{
  RingConferenceManager *self = RING_CONFERENCE_MANAGER (_self);

  /* If we're not connected, conferences aren't supported. */
  if (self->priv->call_service == NULL)
    return;

  func (_self,
      conference_channel_fixed_properties (),
      conference_channel_allowed_properties,
      userdata);
}

static void
conference_manager_foreach_channel (TpChannelManager *_self,
                                    TpExportableChannelFunc func,
                                    gpointer user_data)
{
  RingConferenceManager *self = RING_CONFERENCE_MANAGER (_self);
  GHashTableIter i[1];
  gpointer channel;

  for (g_hash_table_iter_init (i, self->priv->channels);
       g_hash_table_iter_next (i, NULL, &channel);)
    func (channel, user_data);
}

static gboolean
conference_manager_create_channel (TpChannelManager *_self,
                                   gpointer request,
                                   GHashTable *properties)
{
  RingConferenceManager *self = RING_CONFERENCE_MANAGER (_self);

  if (tp_asv_get_initial_members (properties) == NULL ||
      !ring_properties_satisfy (properties,
          conference_channel_fixed_properties (),
          conference_channel_allowed_properties))
    return FALSE;

  return conference_requestotron (self, request, properties);
}

static gboolean
conference_manager_ensure_channel (TpChannelManager *_self,
                                   gpointer request,
                                   GHashTable *properties)
{
  RingConferenceManager *self = RING_CONFERENCE_MANAGER (_self);

  if (tp_asv_get_initial_members (properties) == NULL ||
      !ring_properties_satisfy (properties,
          conference_channel_fixed_properties (),
          conference_channel_allowed_properties))
    return FALSE;

  return conference_requestotron (self, request, properties);
}

static void
manager_iface_init (gpointer ifacep, gpointer data)
{
  TpChannelManagerIface *iface = ifacep;

#define IMPLEMENT(x) iface->x = conference_manager_##x

  IMPLEMENT (foreach_channel);
  IMPLEMENT (foreach_channel_class);
  IMPLEMENT (create_channel);
  IMPLEMENT (ensure_channel);

#undef IMPLEMENT
}

/* ---------------------------------------------------------------------- */

static RingConferenceChannel *
conference_manager_find_existing (RingConferenceManager const *self,
                                       RingInitialMembers *initial)
{
  RingConferenceManagerPrivate const *priv = self->priv;
  GHashTableIter i[1];
  gpointer existing = NULL;

  g_hash_table_iter_init (i, priv->channels);

  while (g_hash_table_iter_next (i, NULL, &existing))
    {
      if (initial->len == 0 ||
          ring_conference_channel_check_initial_members (existing, initial))
        return existing;
    }

  return NULL;
}

static char *
conference_manager_new_object_path (RingConferenceManager const *self)
{
  RingConferenceManagerPrivate const *priv = self->priv;
  char const *base_path = TP_BASE_CONNECTION (priv->connection)->object_path;

  static unsigned object_index;
  static unsigned object_index_init;

  if (G_UNLIKELY (object_index_init == 0))
    {
      object_index = (guint)(time(NULL) - (41 * 365 * 24 * 60 * 60)) * 10;
      object_index_init = 1;
    }

  /* Find an unique D-Bus object_path */
  for (;;)
    {
      char *path = g_strdup_printf ("%s/conf%u", base_path, ++object_index);

      if (!g_hash_table_lookup (priv->channels, path))
        return path;

      g_free (path);
    }
}

static RingConferenceChannel *
conference_manager_new_conference (RingConferenceManager *self,
                                   RingInitialMembers *initial,
                                   gboolean initial_audio)
{
  RingConferenceManagerPrivate *priv = self->priv;
  char *object_path;
  RingConferenceChannel *channel;

  object_path = conference_manager_new_object_path (self);

  channel = g_object_new (RING_TYPE_CONFERENCE_CHANNEL,
      "connection", priv->connection,
      /* KVXXX: "tones", priv->tones, */
      "object-path", object_path,
      "initial-channels", initial,
      "initial-audio", initial_audio,
      "initiator-handle", tp_base_connection_get_self_handle (
          TP_BASE_CONNECTION (priv->connection)),
      "requested", TRUE,
      NULL);

  g_free (object_path);

  return channel;
}

static void
on_conference_channel_closed (GObject *_channel, RingConferenceManager *self)
{
  gchar *object_path;

  g_object_get (_channel, "object-path", &object_path, NULL);
  g_hash_table_remove (self->priv->channels, object_path);
  tp_channel_manager_emit_channel_closed (self, object_path);
  g_free (object_path);
}

static void
conference_manager_emit_new_channel (RingConferenceManager *self,
                                     gpointer request,
                                     gpointer _channel,
                                     GError *error)
{
  DEBUG ("%s (%p, %p, %p, %p) called", __func__,
      self, request, _channel, error);

  RingConferenceManagerPrivate *priv = RING_CONFERENCE_MANAGER (self)->priv;
  GSList *requests = request ? g_slist_prepend (NULL, request) : NULL;

  if (error == NULL)
    {
      RingConferenceChannel *channel = RING_CONFERENCE_CHANNEL (_channel);
      char *object_path = NULL;

      g_signal_connect (channel, "closed",
          G_CALLBACK (on_conference_channel_closed), self);

      g_object_get (channel, "object-path", &object_path, NULL);

      DEBUG ("got new channel %p nick %s type %s",
	  channel, channel->nick, G_OBJECT_TYPE_NAME (channel));

      g_hash_table_insert (priv->channels, object_path, channel);

      tp_channel_manager_emit_new_channel (self,
          TP_EXPORTABLE_CHANNEL (channel), requests);

      /* Emit Group and StreamedMedia signals */
      ring_conference_channel_emit_initial (channel);
    }
  else
    {
      if (_channel)
        {
          RingConferenceChannel *channel = RING_CONFERENCE_CHANNEL (_channel);
          DEBUG ("new channel %p nick %s failed with " GERROR_MSG_FMT,
              channel, channel->nick, GERROR_MSG_CODE (error));
        }

      if (request)
        {
          tp_channel_manager_emit_request_failed (self,
              request, error->domain, error->code, error->message);
        }

      if (_channel)
        {
          g_object_unref (_channel);
        }
    }

  g_slist_free (requests);
}

static gboolean
conference_requestotron (RingConferenceManager *self,
                         gpointer request,
                         GHashTable *properties)
{
  RingInitialMembers *initial_members;
  gboolean initial_audio, initial_video;
  RingConferenceChannel *channel;
  GError *error = NULL;

  if (self->priv->call_service == NULL)
    {
      tp_channel_manager_emit_request_failed (self, request,
          TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Modem does not support conferences");
      return TRUE;
    }

  initial_members = tp_asv_get_initial_members (properties);
  initial_audio = tp_asv_get_initial_audio (properties, TRUE);
  initial_video = tp_asv_get_initial_video (properties, FALSE);

  if (initial_video)
    {
      tp_channel_manager_emit_request_failed (self, request,
          TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Video calls are not supported");
      return TRUE;
    }

  if (tp_asv_get_uint32 (properties,
          TP_IFACE_CHANNEL ".TargetHandleType", NULL) != 0)
    {
      tp_channel_manager_emit_request_failed (self, request,
          TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Invalid TargetHandleType");
      return TRUE;
    }

  channel = conference_manager_find_existing (self, initial_members);
  if (channel)
    {
      tp_channel_manager_emit_request_already_satisfied (self, request,
          TP_EXPORTABLE_CHANNEL (channel));
      return TRUE;
    }

  if (!ring_conference_manager_validate_initial_members (self,
          initial_members, &error))
    {
      tp_channel_manager_emit_request_failed (self, request,
          error->domain, error->code, error->message);
      g_clear_error (&error);
      return TRUE;
    }

  channel = conference_manager_new_conference (self,
      initial_members, initial_audio);

  if (initial_audio)
    ring_conference_channel_initial_audio (channel);

  conference_manager_emit_new_channel (self, request, channel, NULL);

  return TRUE;
}

gboolean
ring_conference_manager_validate_initial_members (RingConferenceManager *self,
                                                  RingInitialMembers *initial,
                                                  GError **error)
{
  RingConferenceManagerPrivate *priv = self->priv;
  RingMemberChannel *ch1, *ch2;

  if (initial == NULL) {
    g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
        "No initial members");
    return FALSE;
  }

  if (initial->len != 2) {
    g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
        "Expecting exactly two initial members");
    return FALSE;
  }

  ch1 = ring_connection_lookup_channel (priv->connection, initial->odata[0]);
  ch2 = ring_connection_lookup_channel (priv->connection, initial->odata[1]);

  if (ch1 == ch2) {
    g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
        "Expecting distinct initial members");
    return FALSE;
  }

  if (!RING_IS_MEMBER_CHANNEL (ch1) || !RING_IS_MEMBER_CHANNEL (ch2)) {
    g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
        "Initial member is not a MediaStream channel");
    return FALSE;
  }

  if (!ring_member_channel_can_become_member (ch1, error)) {
    g_prefix_error (error, "First initial: ");
    return FALSE;
  }

  if (!ring_member_channel_can_become_member (ch2, error)) {
    g_prefix_error (error, "Second initial: ");
    return FALSE;
  }

  return TRUE;
}

#ifdef nomore
static void
on_modem_call_conference_joined (ModemCallConference *mcc,
                                 ModemCall *mc,
                                 RingConferenceManager *self)
{
  RingConferenceManagerPrivate *priv = RING_CONFERENCE_MANAGER (self)->priv;
  ModemCall **members;
  GPtrArray *initial;
  guint i;

  if (modem_call_get_handler (MODEM_CALL (mcc)))
    return;

  members = modem_call_service_get_calls (priv->call_service);
  initial = g_ptr_array_sized_new (MODEM_MAX_CALLS + 1);

  for (i = 0; members[i]; i++)
    {
      if (modem_call_is_member (members[i]) &&
          modem_call_get_handler (members[i]))
        {
          RingMemberChannel *member;
          char *object_path = NULL;

          member = RING_MEMBER_CHANNEL (modem_call_get_handler (members[i]));
          g_object_get (member, "object-path", &object_path, NULL);

          if (object_path)
            g_ptr_array_add (initial, object_path);
        }
    }

  if (initial->len >= 2)
    {
      RingConferenceChannel *channel;
      char *object_path;

      object_path = ring_conference_manager_new_object_path (self);

      channel = (RingConferenceChannel *)
        g_object_new (RING_TYPE_CONFERENCE_CHANNEL,
            "connection", priv->connection,
            "call-instance", mcc,
            /* KVXXX: "tones", priv->tones, */
            "object-path", object_path,
            "initial-channels", initial,
            "initial-audio", TRUE,
            "initiator-handle", tp_base_connection_get_self_handle (
                TP_BASE_CONNECTION (priv->connection)),
            "requested", TRUE,
            NULL);

      g_free (object_path);

      g_assert (channel == modem_call_get_handler (MODEM_CALL (mcc)));

      conference_manager_emit_new_channel (self, NULL, channel, NULL);
    }

  g_ptr_array_add (initial, NULL);
  g_strfreev ((char **)g_ptr_array_free (initial, FALSE));
  g_free (members);
}
#endif

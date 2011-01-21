/*
 * ring-conference-channel.c - Source for RingConferenceChannel
 *
 * Copyright (C) 2007-2010 Nokia Corporation
 *   @author Pekka Pessi <first.surname@nokia.com>
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
 * Based on telepathy-glib/examples/cm/echo/chan.c:
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

#define DEBUG_FLAG RING_DEBUG_MEDIA
#include "ring-debug.h"

#include "ring-conference-channel.h"
#include "ring-member-channel.h"
#include "ring-util.h"
#include "ring-extensions/gtypes.h"

#include "modem/call.h"
#include "modem/errors.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-generic.h>

#include <ring-extensions/ring-extensions.h>

#include "ring-connection.h"

#include "ring-param-spec.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MODEM_MAX_CALLS (7)

struct _RingConferenceChannelPrivate
{
  GQueue requests[1];           /* Requests towards the modem */

  RingInitialMembers *initial_members;

  RingMemberChannel *members[MODEM_MAX_CALLS];
  int is_current[MODEM_MAX_CALLS];

  TpIntSet *current, *pending;

  struct {
    gulong left, joined;
  } signals;

  struct stream_state {
    guint id;                   /* nonzero when active, 0 otherwise */
    TpHandle handle;
    TpMediaStreamType type;
    TpMediaStreamState state;
    TpMediaStreamDirection direction;
    TpMediaStreamPendingSend pending;
  } audio[1];

  struct {
    gboolean state;
    gboolean requested;
  } hold;

  unsigned streams_created:1, conf_created:1, hangup:1, closing:1, disposed:1, :0;
};

/* properties */
enum
{
  PROP_NONE,

  /* o.f.T.Channel.Interfaces */
  PROP_INITIAL_CHANNELS,
  PROP_CHANNELS,

  /* KVXXX: add PROP_TONES */

  LAST_PROPERTY
};

static TpDBusPropertiesMixinIfaceImpl
ring_conference_channel_dbus_property_interfaces[];

static void ring_conference_channel_dtmf_iface_init (gpointer g_iface, gpointer iface_data);
static void ring_conference_channel_hold_iface_init (gpointer g_iface, gpointer iface_data);
static void ring_conference_channel_streamed_media_iface_init (gpointer g_iface, gpointer iface_data);
static void ring_conference_channel_mergeable_conference_iface_init (gpointer, gpointer);

static gboolean ring_conference_channel_close (
    RingConferenceChannel *_self, gboolean immediately);
static gboolean ring_conference_channel_close_impl (
    RingConferenceChannel *self, gboolean immediately, gboolean hangup);
static gboolean ring_conference_channel_add_member(
  GObject *obj, TpHandle handle, const char *message, GError **error);
static gboolean ring_conference_channel_remove_member_with_reason(
  GObject *obj, TpHandle handle, const char *message,
  guint reason,
  GError **error);

static GPtrArray *ring_conference_get_channels(
  RingConferenceChannel const *self);

static void ring_conference_channel_fill_immutable_properties(TpBaseChannel *base,
  GHashTable *props);

static void ring_conference_channel_emit_channel_merged(
  RingConferenceChannel *channel,
  RingMemberChannel *member,
  gboolean current);

static void ring_conference_channel_release(RingConferenceChannel *self,
  unsigned causetype,
  unsigned cause,
  GError const *error);

static gboolean ring_conference_channel_create_streams(RingConferenceChannel *_self,
  guint handle,
  gboolean audio,
  gboolean video,
  GError **error);

/* ====================================================================== */
/* GObject interface */

G_DEFINE_TYPE_WITH_CODE(
  RingConferenceChannel, ring_conference_channel, TP_TYPE_BASE_CHANNEL,
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_DBUS_PROPERTIES,
    tp_dbus_properties_mixin_iface_init);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_DTMF,
    ring_conference_channel_dtmf_iface_init);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_HOLD,
    ring_conference_channel_hold_iface_init);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_TYPE_STREAMED_MEDIA,
    ring_conference_channel_streamed_media_iface_init);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_GROUP,
    tp_group_mixin_iface_init);
  G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CHANNEL_INTERFACE_CONFERENCE,
    NULL);
  G_IMPLEMENT_INTERFACE(RING_TYPE_SVC_CHANNEL_INTERFACE_MERGEABLE_CONFERENCE,
    ring_conference_channel_mergeable_conference_iface_init));

const char *ring_conference_channel_interfaces[] = {
  TP_IFACE_CHANNEL_INTERFACE_DTMF,
  TP_IFACE_CHANNEL_INTERFACE_HOLD,
  TP_IFACE_CHANNEL_INTERFACE_GROUP,
  TP_IFACE_CHANNEL_INTERFACE_CONFERENCE,
  RING_IFACE_CHANNEL_INTERFACE_MERGEABLE_CONFERENCE,
  NULL
};

static ModemRequest *
ring_conference_channel_queue_request (RingConferenceChannel *self,
  ModemRequest *request)
{
  if (request)
    g_queue_push_tail (self->priv->requests, request);
  return request;
}

static ModemRequest *
ring_conference_channel_dequeue_request (RingConferenceChannel *self,
  ModemRequest *request)
{
  if (request)
    g_queue_remove (self->priv->requests, request);
  return request;
}

static ModemCallService *
ring_conference_channel_get_call_service (RingConferenceChannel *self)
{
  TpBaseChannel *base = TP_BASE_CHANNEL (self);
  TpBaseConnection *base_connection;
  RingConnection *connection;
  ModemOface *oface;

  base_connection = tp_base_channel_get_connection (base);
  connection = RING_CONNECTION (base_connection);
  oface = ring_connection_get_modem_interface (connection,
      MODEM_OFACE_CALL_MANAGER);

  if (oface)
    return MODEM_CALL_SERVICE (oface);
  else
    return NULL;
}

static void
modem_call_service_join_reply (ModemCallService *service,
  ModemRequest *request,
  GError *error,
  gpointer _self)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingMemberChannel *member;
  char const *member_path;
  DBusGMethodInvocation *context;
  GError *clearerror = NULL;

  ring_conference_channel_dequeue_request (self, request);

  context = modem_request_steal_data(request, "tp-request");

  if (!error) {
    RingConnection *conn = RING_CONNECTION (
        tp_base_channel_get_connection (TP_BASE_CHANNEL (self)));

    member_path = modem_request_get_data(request, "member-object-path");
    member = ring_connection_lookup_channel(conn, member_path);

    if (ring_conference_channel_join(self, member, &error)) {
      ring_svc_channel_interface_mergeable_conference_return_from_merge
        (context);
      DEBUG("ok");
      return;
    }
    clearerror = error;
  }

  DEBUG("return error \"%s\"", error->message);
  dbus_g_method_return_error(context, error);
  g_clear_error(&clearerror);
}

static void
ring_mergeable_conference_merge(RingSvcChannelInterfaceMergeableConference *iface,
  const char *channel_path,
  DBusGMethodInvocation *context)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(iface);
  RingMemberChannel *member;
  GError *error = NULL;

  DEBUG("enter");

  member = ring_connection_lookup_channel(
    RING_CONNECTION(tp_base_channel_get_connection(TP_BASE_CHANNEL(self))),
    channel_path);

  if (!member || !RING_IS_MEMBER_CHANNEL(member)) {
    error = g_error_new(TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
            "Invalid channel path");
  }
  else if (!ring_member_channel_can_become_member(member, &error)) {
    ;
  }
  else {
    ModemRequest *request;
    ModemCallService *service;

    service = ring_conference_channel_get_call_service (self);

    request = modem_call_request_conference (service,
        modem_call_service_join_reply, self);

    ring_conference_channel_queue_request (self, request);

    modem_request_add_data_full(request,
      "member-object-path",
      g_strdup(channel_path),
      g_free);
    modem_request_add_data_full(request,
      "tp-request",
      context,
      ring_method_return_internal_error);
    return;
  }
  DEBUG("return error \"%s\"", error->message);
  dbus_g_method_return_error(context, error);
  g_clear_error(&error);
}

/*
 * Telepathy.Channel.Interface.DTMF DBus interface - version 0.19.6
 */

/** DBus method StartTone ( u: stream_id, y: event ) -> nothing
 *
 * Start sending a DTMF tone on this stream. Where possible, the tone will
 * continue until StopTone is called. On certain protocols, it may only be
 * possible to send events with a predetermined length. In this case, the
 * implementation may emit a fixed-length tone, and the StopTone method call
 * should return NotAvailable.
 */
static void
ring_conference_channel_dtmf_start_tone(TpSvcChannelInterfaceDTMF *iface,
  guint stream_id,
  guchar event,
  DBusGMethodInvocation *context)
{
  GError error[] = {{
      TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "Not implemented"
    }};
  dbus_g_method_return_error(context, error);
}

/** DBus method StopTone ( u: stream_id ) -> nothing
 *
 * Stop sending any DTMF tone which has been started using the StartTone
 * method. If there is no current tone, this method will do nothing.
 */
static void
ring_conference_channel_dtmf_stop_tone(TpSvcChannelInterfaceDTMF *iface,
  guint stream_id,
  DBusGMethodInvocation *context)
{
  GError error[] = {{
      TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
      "Not implemented"
    }};
  dbus_g_method_return_error(context, error);
}

static void
ring_conference_channel_dtmf_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcChannelInterfaceDTMFClass *klass = (TpSvcChannelInterfaceDTMFClass *)g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_dtmf_implement_##x(       \
    klass, ring_conference_channel_dtmf_##x)
  IMPLEMENT(start_tone);
  IMPLEMENT(stop_tone);
#undef IMPLEMENT
}

/* ---------------------------------------------------------------------- */
/* Implement org.freedesktop.Telepathy.Channel.Interface.Hold */

static
void ring_conference_channel_get_hold_state (TpSvcChannelInterfaceHold *iface,
  DBusGMethodInvocation *context)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL (iface);
  RingConferenceChannelPrivate *priv = self->priv;
  guint i;
  guint hold_state = NUM_TP_LOCAL_HOLD_STATES; /* known invalid value */
  guint hold_reason = NUM_TP_LOCAL_HOLD_STATE_REASONS;
  gboolean state_set = FALSE;

  for (i = 0; i < MODEM_MAX_CALLS; i++)
    {
      RingMemberChannel *member = priv->members[i];
      DEBUG("get_hold_state: %i/%p", i, member);
      if (member && priv->is_current[i])
        {
          guint nextstate;
          g_object_get (member, "hold-state", &nextstate, NULL);
          if (state_set == FALSE)
            {
              hold_state = nextstate;
              g_object_get (member, "hold-state-reason", &hold_reason, NULL);
              state_set = TRUE;
            }
          else
            {
              /* note: the current modem/ofono APIs do not provide means
               *       to get notifications about conf call state (only
               *       about individual calls that might be members of
               *       a conf-call, so we have to fall back to composing
               *       the state by looking at member channel states
               */
              if (hold_state != nextstate)
                {
                  ring_warning (
                      "member call hold state inconsistant (call %i %d, conf %d)\n",
                      i + 1, nextstate, hold_state);
                }
            }
        }
    }

  /*
   * As the member channels won't know about this, we have
   * to check separately whether it was the conf channel that
   * requested the hold.
   */
  if (hold_state &&
      priv->hold.requested)
    {
      hold_reason = TP_LOCAL_HOLD_STATE_REASON_REQUESTED;
    }

  tp_svc_channel_interface_hold_return_from_get_hold_state
    (context, hold_state, hold_reason);
}

static void
reply_to_request_swap_calls (ModemCallService *_service,
    ModemRequest *request,
    GError *error,
    gpointer _self)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = self->priv;
  guint hold = (priv->hold.requested == TRUE) ?
    TP_LOCAL_HOLD_STATE_HELD : TP_LOCAL_HOLD_STATE_UNHELD;

  ring_conference_channel_dequeue_request (self, request);

  DEBUG("");

  priv->hold.state = priv->hold.requested;

  /* XXX: this can potentially confuse the client as the individual
   *      member channel states might not be yet updated...
   *      should we perhaps stay in PENDING state until all
   *      the participant calls have changed state (or failed
   *      to do so)? */

  tp_svc_channel_interface_hold_emit_hold_state_changed(
    (TpSvcChannelInterfaceHold *)self,
    hold, TP_LOCAL_HOLD_STATE_REASON_REQUESTED);

}

static
void ring_conference_channel_request_hold (TpSvcChannelInterfaceHold *iface,
  gboolean hold,
  DBusGMethodInvocation *context)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL (iface);
  RingConferenceChannelPrivate *priv = self->priv;
  GError *error = NULL;

  DEBUG ("(%u) on %s", hold, self->nick);

  if (priv->conf_created == FALSE)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_DISCONNECTED,
          "No conference call available");
    }
  else if (hold == priv->hold.state ||
           hold == priv->hold.requested)
    {
      tp_svc_channel_interface_hold_return_from_request_hold(context);
      return;
    }
  else
    {
      ModemCallService *service =
        ring_conference_channel_get_call_service (self);
      guint holdstate = (hold == TRUE) ?
        TP_LOCAL_HOLD_STATE_PENDING_HOLD : TP_LOCAL_HOLD_STATE_PENDING_UNHOLD;

      priv->hold.requested = hold;

      tp_svc_channel_interface_hold_emit_hold_state_changed(
          (TpSvcChannelInterfaceHold *)self,
          holdstate, TP_LOCAL_HOLD_STATE_REASON_REQUESTED);

      ring_conference_channel_queue_request (self,
          modem_call_service_swap_calls(service,
              reply_to_request_swap_calls, self));

      tp_svc_channel_interface_hold_return_from_request_hold(context);
      return;
    }

  DEBUG ("request_hold(%u) on %s: %s", hold, self->nick, error->message);
  dbus_g_method_return_error (context, error);
  g_clear_error (&error);
}

static void
ring_conference_channel_hold_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcChannelInterfaceHoldClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_hold_implement_##x(       \
    klass, ring_conference_channel_##x)
  IMPLEMENT(get_hold_state);
  IMPLEMENT(request_hold);
#undef IMPLEMENT
}

/* ====================================================================== */

/**
 * Telepathy.Channel.Type.StreamedMedia DBus interface
 */

/*
 * KVXXX: move most of below to a common util file
 * (can be shared with ring-media-channel)
 */

#define TP_CHANNEL_STREAM_TYPE                  \
  (dbus_g_type_get_struct("GValueArray",        \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_UINT,                                \
    G_TYPE_INVALID))

enum {
  RING_MEDIA_STREAM_ID_AUDIO = 1,
};

/** Update media stream state.
 *
 * @retval 0 if nothing changed
 * @retval 1 (or nonzero) if state changed
 */
static int
update_media_stream(RingConferenceChannel *self,
  TpHandle handle,
  struct stream_state *ss,
  guint id,
  TpMediaStreamType type,
  TpMediaStreamState state,
  TpMediaStreamDirection direction,
  TpMediaStreamPendingSend pending)
{
  int changed = 0;

  if (type != TP_MEDIA_STREAM_TYPE_AUDIO)
    return 0;

  if (state == TP_MEDIA_STREAM_STATE_DISCONNECTED) {
    if (ss->id == id) {
      changed = 1;
      /* emit StreamRemoved */
      tp_svc_channel_type_streamed_media_emit_stream_removed(
        (TpSvcChannelTypeStreamedMedia *)self, id);
      memset(ss, 0, sizeof ss);
    }
    return changed;
  }

  /* emit StreamAdded */
  if (ss->id != id) {
    if (handle == 0)
      g_object_get(self, "peer", &handle, NULL);
    changed = 1;
    ss->id = id;
    ss->handle = handle;
    ss->type = type;

    if (DEBUGGING) {
      DEBUG("emitting StreamAdded(%u, %d, %s)",
        id, handle,
        type == TP_MEDIA_STREAM_TYPE_AUDIO ?
        "AUDIO" :
        type == TP_MEDIA_STREAM_TYPE_VIDEO ?
        "VIDEO" : "???");
    }

    tp_svc_channel_type_streamed_media_emit_stream_added(
      (TpSvcChannelTypeStreamedMedia *)self,
      ss->id, ss->handle, ss->type);
  }

  /* emit StreamStateChanged */
  if (ss->state != state) {
    changed = 1;
    ss->state = state;

    if (DEBUGGING) {
      DEBUG("emitting StreamStateChanged(%u, %s)",
        ss->id,
        state == TP_MEDIA_STREAM_STATE_DISCONNECTED ?
        "DISCONNECTED" :
        state == TP_MEDIA_STREAM_STATE_CONNECTING ?
        "CONNECTING" :
        state == TP_MEDIA_STREAM_STATE_CONNECTED ?
        "CONNECTED" : "???");
    }

    tp_svc_channel_type_streamed_media_emit_stream_state_changed (
      (TpSvcChannelTypeStreamedMedia *)self,
      ss->id, state);
  }

  /* emit StreamDirectionChanged */
  if (ss->direction != direction || ss->pending != pending) {
    changed = 1;
    ss->direction = direction;
    ss->pending = pending;

    if (DEBUGGING) {
      DEBUG("emitting StreamDirectionChanged(%u, %s,%s%s%s)",
        ss->id,
        direction == TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL
        ? "BIDIRECTIONAL" :
        direction == TP_MEDIA_STREAM_DIRECTION_SEND
        ? "SEND" :
        direction == TP_MEDIA_STREAM_DIRECTION_RECEIVE
        ? "RECV" :
        "NONE",
        pending & TP_MEDIA_STREAM_PENDING_REMOTE_SEND ?
        " remote" : "",
        pending & TP_MEDIA_STREAM_PENDING_LOCAL_SEND ?
        " local" : "",
        pending == 0 ? " 0" : "");
    }

    tp_svc_channel_type_streamed_media_emit_stream_direction_changed(
      (TpSvcChannelTypeStreamedMedia *)self,
      ss->id, ss->direction, ss->pending);
  }

  return changed;
}

static void
free_media_stream_list(GPtrArray *list)
{
  if (list) {
    const GType ElementType = TP_CHANNEL_STREAM_TYPE;
    guint i;

    for (i = list->len; i-- > 0;)
      g_boxed_free(ElementType, g_ptr_array_index(list, i));

    g_ptr_array_free(list, TRUE);
  }
}

/* Return a pointer to GValue with boxed stream struct */
static gpointer
describe_stream(struct stream_state *ss)
{
  const GType ElementType = TP_CHANNEL_STREAM_TYPE;
  GValue element[1] = {{ 0 }};

  g_value_init(element, ElementType);
  g_value_take_boxed(element, dbus_g_type_specialized_construct(ElementType));

  dbus_g_type_struct_set(element,
    0, ss->id,
    1, ss->handle,
    2, ss->type,
    3, ss->state,
    4, ss->direction,
    5, ss->pending,
    G_MAXUINT);

  return g_value_get_boxed(element);
}

static GPtrArray *
list_media_streams(RingConferenceChannel *self)
{
  RingConferenceChannelPrivate *priv = self->priv;
  GPtrArray *list;
  size_t size;

  size = (priv->audio->id != 0);

  list = g_ptr_array_sized_new(size);

  if (priv->audio->id)
    g_ptr_array_add(list, describe_stream(priv->audio));

  return list;
}

static gpointer
describe_null_media(TpMediaStreamType tptype)
{
  struct stream_state ss[1] = {{ 0 }};

  ss->type = tptype;

  return describe_stream(ss);
}

static gboolean
ring_conference_channel_validate_media_handle(RingConferenceChannel *_self,
  guint *handlep,
  GError **error)
{
  if (*handlep != 0) {
    g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Conference channel carries a multiparty stream only");
    return FALSE;
  }

  return TRUE;
}

/** DBus method ListStreams ( ) -> a(uuuuuu)
 *
 * Returns an array of structs representing the streams currently active
 * within this channel. Each stream is identified by an unsigned integer
 * which is unique for each stream within the channel.
 *
 * Returns
 *
 * a(uuuuuu)
 *     An array of structs containing:
 *
 *         * the stream identifier
 *         * the contact handle who the stream is with
 *           (or 0 if the stream represents more than a single member)
 *         * the type of the stream
 *         * the current stream state
 *         * the current direction of the stream
 *         * the current pending send flags
 */
static void
ring_conference_channel_list_streams(TpSvcChannelTypeStreamedMedia *iface,
  DBusGMethodInvocation *context)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(iface);
  GPtrArray *list;

  list = list_media_streams(self);

  tp_svc_channel_type_streamed_media_return_from_list_streams(context, list);

  free_media_stream_list(list);
}

/** DBus method RequestStreams ( u: contact_handle, au: types ) -> a(uuuuuu)
 *
 * Request that streams be established to exchange the given types of media
 * with the given member. In general this will try and establish a
 * bidirectional stream, but on some protocols it may not be possible to
 * indicate to the peer that you would like to receive media, so a send-only
 * stream will be created initially. In the cases where the stream requires
 * remote agreement (eg you wish to receive media from them), the
 * StreamDirectionChanged signal will be emitted with the
 * MEDIA_STREAM_PENDING_REMOTE_SEND flag set, and the signal emitted again
 * with the flag cleared when the remote end has replied.
 *
 */
static void
ring_conference_channel_request_streams(TpSvcChannelTypeStreamedMedia *iface,
  guint handle,
  const GArray *media_types,
  DBusGMethodInvocation *context)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(iface);
  RingConferenceChannelPrivate *priv = self->priv;
  GError *error = NULL;
  GPtrArray *list;
  guint i, create_audio_stream = 0;
  struct stream_state audio[1];

  DEBUG("(...) on %s", self->nick);

  if (!ring_conference_channel_validate_media_handle(self, &handle, &error)) {
    dbus_g_method_return_error(context, error);
    g_clear_error(&error);
    return;
  }

  *audio = *priv->audio;

  /* We can create media only when call has not been initiated */
  for (i = 0; i < media_types->len; i++) {
    guint media_type = g_array_index(media_types, guint, i);

    if (media_type == TP_MEDIA_STREAM_TYPE_AUDIO && !create_audio_stream)
      create_audio_stream = update_media_stream(self, handle,
                            audio, RING_MEDIA_STREAM_ID_AUDIO, media_type,
                            TP_MEDIA_STREAM_STATE_CONNECTING,
                            TP_MEDIA_STREAM_DIRECTION_NONE,
                            TP_MEDIA_STREAM_PENDING_LOCAL_SEND |
                            TP_MEDIA_STREAM_PENDING_REMOTE_SEND);
  }

  if (create_audio_stream)
    *priv->audio = *audio;

  list = g_ptr_array_sized_new(media_types->len);

  for (i = 0; i < media_types->len; i++) {
    guint media_type = g_array_index(media_types, guint, i);
    gpointer element;

    if (media_type == TP_MEDIA_STREAM_TYPE_AUDIO && create_audio_stream)
      element = describe_stream(priv->audio), create_audio_stream = 0;
    else
      element = describe_null_media(media_type);

    g_ptr_array_add(list, element);
  }

  ring_conference_channel_create_streams(
    self, handle, create_audio_stream, FALSE, &error);

  tp_svc_channel_type_streamed_media_return_from_request_streams(
    context, list);

  free_media_stream_list(list);

  DEBUG("exit");
}

static void
ring_conference_channel_streamed_media_iface_init (gpointer g_iface,
  gpointer iface_data)
{
  TpSvcChannelTypeStreamedMediaClass *klass =
    (TpSvcChannelTypeStreamedMediaClass *)g_iface;

#define IMPLEMENT(x) tp_svc_channel_type_streamed_media_implement_##x(  \
    klass, ring_conference_channel_##x)
  IMPLEMENT(list_streams);
  /* IMPLEMENT(remove_streams); */
  /* IMPLEMENT(request_stream_direction); */
  IMPLEMENT(request_streams);
#undef IMPLEMENT
}

static void
ring_conference_channel_mergeable_conference_iface_init(gpointer g_iface,
  gpointer iface_data)
{
  RingSvcChannelInterfaceMergeableConferenceClass *klass =
    (RingSvcChannelInterfaceMergeableConferenceClass *)g_iface;

#define IMPLEMENT(x)                                            \
  ring_svc_channel_interface_mergeable_conference_implement_##x \
    (klass, ring_mergeable_conference_##x)

  IMPLEMENT(merge);
#undef IMPLEMENT
}

static void
ring_conference_channel_init(RingConferenceChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, RING_TYPE_CONFERENCE_CHANNEL, RingConferenceChannelPrivate);
}

static void
ring_conference_channel_constructed(GObject *object)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(object);
  TpBaseChannel *base = TP_BASE_CHANNEL (self);
  TpBaseConnection *connection =
    tp_base_channel_get_connection(base);
  const gchar *object_path;
  char *nick;

  if (G_OBJECT_CLASS(ring_conference_channel_parent_class)->constructed)
    G_OBJECT_CLASS(ring_conference_channel_parent_class)->constructed(object);

  tp_group_mixin_init(
    object,
    G_STRUCT_OFFSET(RingConferenceChannel, group),
    tp_base_connection_get_handles(connection, TP_HANDLE_TYPE_CONTACT),
    connection->self_handle);

  object_path = tp_base_channel_get_object_path (base);
  g_assert(object_path != NULL);

  nick = strrchr(object_path, '/');
  ++nick;
  g_assert (nick != NULL);
  self->nick = g_strdup(nick);

  DEBUG("(%p) with %s", self, self->nick);

  tp_base_channel_register (base);
}

void
ring_conference_channel_emit_initial(RingConferenceChannel *_self)
{
  DEBUG("enter");

  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = self->priv;
  RingConnection *connection =
    RING_CONNECTION(tp_base_channel_get_connection(TP_BASE_CHANNEL(self)));
  TpGroupMixin *group = TP_GROUP_MIXIN(self);
  char const *message;
  TpChannelGroupChangeReason reason;
  TpChannelGroupFlags add = 0, del = 0;
  char const *member_path;
  RingMemberChannel *member;
  int i;

  message = "Conference created";
  reason = TP_CHANNEL_GROUP_CHANGE_REASON_INVITED;

  priv->current = tp_intset_new ();
  tp_intset_add (priv->current, group->self_handle);
  priv->pending = tp_intset_new ();

  for (i = 0; i < priv->initial_members->len; i++) {
    member_path = priv->initial_members->odata[i];
    member = ring_connection_lookup_channel (connection, member_path);
    if (member)
      {
	ring_conference_channel_emit_channel_merged (
            self, RING_MEMBER_CHANNEL(member), TRUE);
      }
    else
      {
        DEBUG("No member channel %s found\n", member_path);
      }
  }

  tp_group_mixin_change_members((GObject *)self, message,
    priv->current, NULL, NULL, priv->pending,
    group->self_handle, reason);
  tp_intset_destroy(priv->current); priv->current = NULL;
  tp_intset_destroy(priv->pending); priv->pending = NULL;

  add |= TP_CHANNEL_GROUP_FLAG_CAN_REMOVE;
  if (tp_handle_set_size(group->remote_pending))
    add |= TP_CHANNEL_GROUP_FLAG_CAN_RESCIND;
  add |= TP_CHANNEL_GROUP_FLAG_PROPERTIES;
  add |= TP_CHANNEL_GROUP_FLAG_CHANNEL_SPECIFIC_HANDLES;
  add |= TP_CHANNEL_GROUP_FLAG_MEMBERS_CHANGED_DETAILED;

  tp_group_mixin_change_flags((GObject *)self, add, del);
}

static void
ring_conference_channel_get_property(GObject *obj,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(obj);
  RingConferenceChannelPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_INITIAL_CHANNELS:
      g_value_set_boxed(value, priv->initial_members);
      break;
    case PROP_CHANNELS:
      g_value_take_boxed(value, ring_conference_get_channels(self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_conference_channel_set_property(GObject *obj,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(obj);
  RingConferenceChannelPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_INITIAL_CHANNELS:
      priv->initial_members = g_value_dup_boxed(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void
ring_conference_channel_dispose(GObject *obj)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(obj);

  if (self->priv->disposed)
    return;
  self->priv->disposed = TRUE;

  guint i;

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (self->priv->members[i]) {
      g_object_unref(self->priv->members[i]), self->priv->members[i] = NULL;
    }
  }

  while (!g_queue_is_empty (self->priv->requests)) {
    modem_request_cancel (g_queue_pop_head (self->priv->requests));
  }

  ((GObjectClass *)ring_conference_channel_parent_class)->dispose(obj);
}


static void
ring_conference_channel_finalize(GObject *obj)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(obj);
  RingConferenceChannelPrivate *priv = self->priv;

  tp_group_mixin_finalize(obj);

  g_boxed_free(TP_ARRAY_TYPE_OBJECT_PATH_LIST, priv->initial_members);
  g_free (self->nick);

  priv->initial_members = NULL;

  G_OBJECT_CLASS(ring_conference_channel_parent_class)->finalize(obj);

  DEBUG("exit");
}

/* ====================================================================== */
/* GObjectClass */

static void
ring_conference_channel_class_init(RingConferenceChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  TpBaseChannelClass *base_chan_class = TP_BASE_CHANNEL_CLASS (klass);

  g_type_class_add_private(klass, sizeof (RingConferenceChannelPrivate));

  object_class->constructed = ring_conference_channel_constructed;
  object_class->get_property = ring_conference_channel_get_property;
  object_class->set_property = ring_conference_channel_set_property;
  object_class->dispose = ring_conference_channel_dispose;
  object_class->finalize = ring_conference_channel_finalize;

  base_chan_class->channel_type = TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA;
  base_chan_class->close = (TpBaseChannelCloseFunc) ring_conference_channel_close;
  base_chan_class->interfaces = ring_conference_channel_interfaces;
  base_chan_class->target_handle_type = TP_HANDLE_TYPE_NONE;
  base_chan_class->fill_immutable_properties = ring_conference_channel_fill_immutable_properties;

  klass->dbus_properties_class.interfaces =
    ring_conference_channel_dbus_property_interfaces;
  tp_dbus_properties_mixin_class_init(
    object_class,
    G_STRUCT_OFFSET(RingConferenceChannelClass, dbus_properties_class));

  tp_group_mixin_class_init(
    object_class,
    G_STRUCT_OFFSET(RingConferenceChannelClass, group_class),
    ring_conference_channel_add_member,
    NULL);
  tp_group_mixin_class_set_remove_with_reason_func(
    object_class, ring_conference_channel_remove_member_with_reason);
  tp_group_mixin_init_dbus_properties(object_class);

  g_object_class_install_property(
    object_class, PROP_INITIAL_CHANNELS,
    g_param_spec_boxed("initial-channels",
      "Initial member channels",
      "The two initial member channels used "
      "when the conference channel is constructed.",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_CHANNELS,
    g_param_spec_boxed("channels",
      "Current member channels",
      "The current member channels participating in "
      "the conference.",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST,
      G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS));
}

/* ====================================================================== */
/**
 * org.freedesktop.DBus properties
 */

/* Properties for com.nokia.Telepathy.Channel.Interface.Conference */
static TpDBusPropertiesMixinPropImpl conference_properties[] = {
  { "InitialChannels", "initial-channels" },
  { "Channels", "channels" },
  { NULL }
};

static TpDBusPropertiesMixinIfaceImpl
ring_conference_channel_dbus_property_interfaces[] = {
  {
    TP_IFACE_CHANNEL_INTERFACE_CONFERENCE,
    tp_dbus_properties_mixin_getter_gobject_properties,
    NULL,
    conference_properties,
  },
  { NULL }
};

static void
ring_conference_channel_fill_immutable_properties(TpBaseChannel *base,
  GHashTable *props)
{
  TP_BASE_CHANNEL_CLASS (ring_conference_channel_parent_class)->fill_immutable_properties (
      base, props);

  tp_dbus_properties_mixin_fill_properties_hash (G_OBJECT(base), props,
    TP_IFACE_CHANNEL_INTERFACE_CONFERENCE, "InitialChannels",
    NULL);
}


/* ---------------------------------------------------------------------- */

/**
 * Interface Telepathy.Channel.Interface.Group (version 0.15)
 * using TpGroupMixin from telepathy-glib
 *
 * Methods:
 *   AddMembers ( au: contacts, s: message ) -> nothing
 *   GetAllMembers ( ) -> au, au, au
 *   GetGroupFlags ( ) -> u
 *   GetHandleOwners ( au: handles ) -> au
 *   GetLocalPendingMembers ( ) -> au
 *   GetLocalPendingMembersWithInfo ( ) -> a(uuus)
 *   GetMembers ( ) -> au
 *   GetRemotePendingMembers ( ) -> au
 *   GetSelfHandle ( ) -> u
 *   RemoveMembers ( au: contacts, s: message ) -> nothing
 * Signals:
 *    -> GroupFlagsChanged ( u: added, u: removed )
 *    -> MembersChanged ( s: message, au: added, au: removed,
 *                        au: local_pending, au: remote_pending,
 *                        u: actor, u: reason )
 *
 * TPGroupMixin interface:
 * ring_conference_channel_add_member()
 * ring_conference_channel_remove_member_with_reason()
 */

static gboolean
ring_conference_channel_add_member(GObject *iface,
  TpHandle handle,
  const char *message,
  GError **error)
{
  TpGroupMixin *mixin = TP_GROUP_MIXIN(iface);

  DEBUG("enter");

  if (tp_handle_set_is_member(mixin->members, handle))
    return TRUE;
  else
    return FALSE;
}

static void
reply_to_modem_call_request_hangup_conference (ModemCallService *_service,
    ModemRequest *request,
    GError *error,
    gpointer _self)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = self->priv;
  TpBaseChannel *base = TP_BASE_CHANNEL (self);

  ring_conference_channel_dequeue_request (self, request);

  DEBUG("");

  priv->conf_created = FALSE;
  tp_base_channel_destroyed (base);
}

/*
 * Requests modem to hangup the multiparty call.
 */
static void ring_conference_do_hangup (RingConferenceChannel *self)
{
  RingConferenceChannelPrivate *priv = self->priv;
  ModemCallService *service;

  priv->hangup = TRUE;
  service = ring_conference_channel_get_call_service (self);
  ring_conference_channel_queue_request (self,
      modem_call_request_hangup_conference (service,
          reply_to_modem_call_request_hangup_conference, self));
}

static gboolean
ring_conference_channel_remove_member_with_reason(GObject *iface,
  TpHandle handle,
  const char *message,
  guint reason,
  GError **error)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(iface);
  RingConferenceChannelPrivate *priv = self->priv;
  RingMemberChannel *member;
  TpGroupMixin *mixin = TP_GROUP_MIXIN(iface);
  TpIntSet *removing;
  TpHandle selfhandle, memberhandle;
  int i;

  DEBUG("enter");

  selfhandle = tp_base_connection_get_self_handle(
    tp_base_channel_get_connection(TP_BASE_CHANNEL(self)));

  removing = tp_intset_new();

  if (handle == selfhandle)
    {
      tp_intset_add(removing, handle);
      ring_conference_do_hangup (self);
    }

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    member = priv->members[i];
    if (!member)
      continue;

    g_object_get(member, "member-handle", &memberhandle, NULL);

    if (handle == memberhandle) {
      tp_intset_add(removing, memberhandle);
      if (!ring_member_channel_release(member, message, reason, error))
        goto error;
      break;
    }
  }

  if (tp_intset_size(removing) == 0) {
    g_set_error(error, TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
      "handle %u cannot be removed", handle);
    goto error;
  }

  /* Remove handles from member set */
  tp_group_mixin_change_members(iface, message,
    NULL, removing, NULL, NULL,
    selfhandle, reason);
  tp_intset_destroy(removing), removing = NULL;

  guint del = 0;

  if (tp_handle_set_size(mixin->members) < 1)
    del |= TP_CHANNEL_GROUP_FLAG_CAN_REMOVE;
  if (tp_handle_set_size(mixin->remote_pending) < 1)
    del |= TP_CHANNEL_GROUP_FLAG_CAN_RESCIND;

  if (del)
    tp_group_mixin_change_flags(iface, 0, del);

  return TRUE;

error:
  tp_intset_destroy(removing);
  return FALSE;
}


/* ---------------------------------------------------------------------- */
/* com.nokia.Telepathy.Channel.Interface.Conference */

guint
ring_conference_channel_has_members(RingConferenceChannel const *self)
{
  int i, n = 0;

  if (self) {
    for (i = 0; i < MODEM_MAX_CALLS; i++) {
      if (self->priv->members[i] != NULL)
        n++;
    }
  }

  return n;
}

static void
ring_conference_channel_emit_channel_merged(RingConferenceChannel *self,
  RingMemberChannel *member,
  gboolean current)
{
  RingConferenceChannelPrivate *priv = self->priv;
  int i;
  char *member_object_path;
  GHashTable *member_map;
  TpHandle member_handle;
  /* XXXKV: fill with correct values */
  GHashTable *member_props =
      tp_dbus_properties_mixin_make_properties_hash (G_OBJECT (member), NULL, NULL, NULL);

  g_object_get(member,
    "object-path", &member_object_path,
    "member-map", &member_map,
    "member-handle", &member_handle,
    NULL);

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (member == priv->members[i]) {
      priv->is_current[i] = current;
      goto emit_members_changed;
    }
  }

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (priv->members[i] == NULL)
      break;
  }

  g_assert(i < MODEM_MAX_CALLS);
  if (i >= MODEM_MAX_CALLS)
    goto error;

  priv->members[i] = g_object_ref(member);
  priv->is_current[i] = current;

  DEBUG("emitting MemberChannelAdded(%s)",
    strrchr(member_object_path, '/') + 1);

  /* XXX: This used to take member_map, which could be useful */
  tp_svc_channel_interface_conference_emit_channel_merged(
	  self, member_object_path, member_handle, member_props);

emit_members_changed:
  DEBUG("%s member handle %u for %s",
    current ? "current" : "pending",
    member_handle, strrchr(member_object_path, '/') + 1);

  if (current)
    ring_member_channel_joined(member, self);

  gpointer owner, owned;
  GHashTableIter j[1];

  for (g_hash_table_iter_init(j, member_map);
       g_hash_table_iter_next(j, &owned, &owner);) {
    tp_group_mixin_add_handle_owner((GObject *)self,
      GPOINTER_TO_UINT(owned),
      GPOINTER_TO_UINT(owner));
  }

  if (!priv->pending) {
    TpGroupMixin *mixin = TP_GROUP_MIXIN(self);
    TpIntSet *members = tp_intset_new();

    tp_intset_add(members, member_handle);
    tp_group_mixin_change_members((GObject *)self,
      "New conference members",
      current ? members : NULL, NULL,
      NULL, current ? NULL : members,
      current ? 0 : mixin->self_handle,
      TP_CHANNEL_GROUP_CHANGE_REASON_INVITED);
    tp_intset_destroy(members);

    TpChannelGroupFlags add = 0, del = 0;
    /* Allow removal and rescind */
    add = TP_CHANNEL_GROUP_FLAG_CAN_REMOVE;
    if (tp_handle_set_size(mixin->remote_pending) == 0)
      /* Deny rescind */
      del = TP_CHANNEL_GROUP_FLAG_CAN_RESCIND;
    else if (!current)
      add |= TP_CHANNEL_GROUP_FLAG_CAN_RESCIND;
    tp_group_mixin_change_flags((GObject *)self, add, del);
  }
  else {
    /* Report all handles at once */
    if (current)
      tp_intset_add(priv->current, member_handle);
    else
      tp_intset_add(priv->pending, member_handle);
  }

error:
  g_hash_table_destroy(member_map);
  g_hash_table_destroy(member_props);
  g_free(member_object_path);
}


void
ring_conference_channel_emit_channel_removed(
  RingConferenceChannel *self,
  RingMemberChannel *member,
  char const *message,
  TpHandle actor,
  TpChannelGroupChangeReason reason)
{
  RingConferenceChannelPrivate *priv = self->priv;
  guint i, n;
  char *object_path;
  TpHandle member_handle;
  GHashTable *details;

  if (!member)
    return;

  if (priv->closing)
    return;

  n = ring_conference_channel_has_members(self);

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (member == priv->members[i]) {
      priv->members[i] = NULL;
      priv->is_current[i] = 0;
      break;
    }
  }

  if (i >= MODEM_MAX_CALLS)
    return;

  /* note: as this function may lead to tearing down the whole
   *       conference, keep a local self-reference */
  g_object_ref (self);

  while (member) {
    TpIntSet *remove = tp_intset_new();

    g_object_get(member,
      "object-path", &object_path,
      "member-handle", &member_handle,
      NULL);

    tp_intset_add(remove, member_handle);

    DEBUG("emitting MemberChannelRemoved(%s, %u, %u)",
      strrchr(object_path, '/') + 1,
      actor, reason);

    details = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

    g_hash_table_insert (details, "actor",
      tp_g_value_slice_new_uint (actor));
    g_hash_table_insert (details, "change-reason",
      tp_g_value_slice_new_uint (reason));

    tp_svc_channel_interface_conference_emit_channel_removed(
	     self, object_path, details);

    g_free(object_path);
    g_hash_table_destroy(details);

    ring_member_channel_left(member);

    g_object_unref((GObject *)member), member = NULL;

    tp_group_mixin_change_members((GObject *)self,
      message,
      NULL, remove, NULL, NULL,
      actor, reason);
    tp_intset_destroy(remove);

    if (n > 2)
      break;

    /* The conference channel goes away */
    message = "Deactivating conference" ;
    actor = self->group.self_handle;
    reason = TP_CHANNEL_GROUP_CHANGE_REASON_SEPARATED;

    for (i = 0; i < MODEM_MAX_CALLS; i++) {
      if (priv->members[i]) {
        member = priv->members[i];
        priv->members[i] = NULL;
        priv->is_current[i] = 0;
        break;
      }
    }
  }

  if (n > 2)
    goto out;

  DEBUG("Too few members, close channel %p", self);

  /*
   * Last member channel removed, close channel but do
   * not hangup.
   */
  ring_conference_channel_close_impl (self, FALSE, FALSE);

out:
  g_object_unref (self);
}


static GPtrArray *
ring_conference_get_channels(RingConferenceChannel const *self)
{
  RingConferenceChannelPrivate const *priv = self->priv;
  GPtrArray *list;
  int i;
  char *object_path;

  list = g_ptr_array_sized_new(MODEM_MAX_CALLS);

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (priv->members[i] && priv->is_current[i]) {
      g_object_get(priv->members[i], "object-path", &object_path, NULL);
      g_ptr_array_add(list, object_path);
    }
  }

  return list;
}

gboolean
ring_conference_channel_check_initial_members(RingConferenceChannel const *channel,
  RingInitialMembers const *maybe)
{
  if (channel == NULL || maybe == NULL)
    return FALSE;

  if (!RING_IS_CONFERENCE_CHANNEL(channel))
    return FALSE;

  RingInitialMembers const *initial = channel->priv->initial_members;

  if (initial->len != maybe->len)
    return FALSE;

  guint i, j;

  for (i = 0; i < maybe->len; i++) {
    for (j = 0; j < initial->len; j++) {
      if (strcmp(initial->odata[j], maybe->odata[i]) == 0)
        break;
    }
    if (j == initial->len)
      return FALSE;
  }

  return TRUE;
}

gboolean
ring_conference_channel_join(RingConferenceChannel *self,
  RingMemberChannel *member,
  GError **error)
{
  RingConferenceChannelPrivate *priv = RING_CONFERENCE_CHANNEL(self)->priv;
  int i;

  if (!RING_IS_CONFERENCE_CHANNEL(self)) {
    g_set_error(error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
      "Not a Conference Channel");
    return FALSE;
  }
#if 0
  if (!ring_member_channel_can_become_member(member, error)) {
    return FALSE;
  }
#endif
  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    if (member == priv->members[i]) {
      priv->is_current[i] = TRUE;
    }
  }
  ring_conference_channel_emit_channel_merged(self, member, FALSE);

  return TRUE;
}

/* ====================================================================== */
/* RingMediaChannel interface */

/**
 * Implements closing the channel
 *
 * @see ring_conference_channel_close() for the interface callback
 */
static gboolean
ring_conference_channel_close_impl (RingConferenceChannel *self,
    gboolean immediately, gboolean hangup)
{
  RingConferenceChannelPrivate *priv = RING_CONFERENCE_CHANNEL(self)->priv;
  int i;

  priv->closing = TRUE;

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    RingMemberChannel *member = priv->members[i];
    if (member) {
      ring_member_channel_left(member);
      g_object_unref(member);
    }
    priv->members[i] = NULL;
  }

  if (hangup)
    {
      ring_conference_do_hangup (self);
    }
  else
    {
      TpBaseChannel *base = TP_BASE_CHANNEL (self);
      priv->conf_created = FALSE;
      DEBUG("emitting close without hanging up");
      tp_base_channel_destroyed (base);
    }

  return TRUE;
}

/**
 * Implements the TpBaseChannel close method.
 */
static gboolean
ring_conference_channel_close (RingConferenceChannel *self,
    gboolean immediately)
{
  /**
   * When Close() is requested explicitly, hangup the conference
   * call (releases all member calls as well).
   */
  return ring_conference_channel_close_impl (self, immediately, TRUE);
}

static void
ring_conference_channel_release(RingConferenceChannel *self,
  unsigned causetype,
  unsigned cause,
  GError const *error0)
{
  RingConferenceChannelPrivate *priv = self->priv;
  char const *message;
  TpIntSet *removed;
  TpChannelGroupChangeReason reason;
  GError *error = NULL;
  char *debug = NULL;
  char *dbus_error = NULL;
  int i, details;

  if (error0) {
    reason = TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;
    error = (GError *)error0;
  }
  else {
    reason = ring_channel_group_release_reason(causetype, cause);
    error = modem_call_new_error(causetype, cause, NULL);
  }
  message = error->message;
  dbus_error = modem_error_fqn(error);

  details = causetype && cause &&
    reason != TP_CHANNEL_GROUP_CHANGE_REASON_BUSY &&
    reason != TP_CHANNEL_GROUP_CHANGE_REASON_NONE;

  DEBUG("TERMINATED: message=\"%s\" reason=%s (%u) cause=%s.%s (%u.%u)",
    message, ring_util_reason_name(reason), reason,
    modem_error_domain_prefix(error->domain),
    modem_error_name(error, NULL, 0),
    causetype, cause);
  debug = g_strdup_printf("conference terminated: "
          "reason=%s (%u) cause=%s.%s (%u.%u)",
          ring_util_reason_name(reason), reason,
          modem_error_domain_prefix(error->domain),
          modem_error_name(error, NULL, 0),
          causetype, cause);

  /* update flags accordingly -- deny adding/removal/rescind */
  tp_group_mixin_change_flags((GObject *)self, 0,
    TP_CHANNEL_GROUP_FLAG_CAN_ADD |
    TP_CHANNEL_GROUP_FLAG_CAN_REMOVE |
    TP_CHANNEL_GROUP_FLAG_CAN_RESCIND);

  removed = tp_intset_new();
  tp_intset_add(removed, TP_GROUP_MIXIN(self)->self_handle);

  for (i = 0; i < MODEM_MAX_CALLS; i++) {
    RingMemberChannel *member = priv->members[i];
    if (member) {
      uint member_handle = 0;
      g_object_get(member, "member-handle", &member_handle, NULL);
      tp_intset_add(removed, member_handle);
    }
  }

  ring_util_group_change_members(self,
    NULL, removed, NULL, NULL,
    reason ? "change-reason" : "", G_TYPE_UINT, reason,
    message ? "message" : "", G_TYPE_STRING, message,
    dbus_error ? "error" : "", G_TYPE_STRING, dbus_error,
    "debug-message", G_TYPE_STRING, debug,
    NULL);

  tp_intset_destroy(removed);

  g_free(dbus_error);
  if (error0 == NULL)
    g_error_free(error);
  g_free(debug);
}

static void reply_to_modem_call_request_conference(ModemCallService *_service,
  ModemRequest *request, GError *error, gpointer _channel);

static gboolean
ring_conference_channel_create_streams(RingConferenceChannel *_self,
  guint handle,
  gboolean audio,
  gboolean video,
  GError **error)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = self->priv;
  ModemCallService *service;

  (void)audio; (void)video;

  if (priv->streams_created) {
    DEBUG("Already associated with a conference");
    return TRUE;
  }
  priv->streams_created = 1;

  if (!ring_connection_validate_initial_members(
      RING_CONNECTION(tp_base_channel_get_connection(TP_BASE_CHANNEL(self))),
      priv->initial_members,
      error))
    return FALSE;

  service = ring_conference_channel_get_call_service (self);

  ring_conference_channel_queue_request (self,
    modem_call_request_conference (service,
      reply_to_modem_call_request_conference, self));

  return TRUE;
}

void
ring_conference_channel_initial_audio(RingConferenceChannel *self,
  RingMediaManager *manager,
  gpointer channelrequest)
{
  RingConferenceChannelPrivate *priv = self->priv;
  ModemCallService *service;
  ModemRequest *request;

  DEBUG("%s(%p, %p, %p) called", __func__, self, manager, channelrequest);

  priv->streams_created = 1;

  service = ring_conference_channel_get_call_service (self),

  request = modem_call_request_conference (service,
            reply_to_modem_call_request_conference,
            self);

  ring_conference_channel_queue_request (self, request);

  modem_request_add_qdatas(
    request,
    g_type_qname(RING_TYPE_MEDIA_MANAGER), g_object_ref(manager), g_object_unref,
    /* XXX - GDestroyNotify for channelrequest  */
    g_quark_from_static_string("RingChannelRequest"), channelrequest, NULL,
    NULL);
}

static void
reply_to_modem_call_request_conference(ModemCallService *_service,
  ModemRequest *request,
  GError *error,
  gpointer _self)
{
  RingConferenceChannel *self = RING_CONFERENCE_CHANNEL(_self);
  RingConferenceChannelPrivate *priv = self->priv;
  gpointer channelrequest;
  GError error0[1] = {{
      TP_ERRORS, TP_ERROR_NOT_AVAILABLE, "Conference channel already exists"
    }};

  ring_conference_channel_dequeue_request (self, request);

  g_assert(priv->conf_created == FALSE);
  if (error)
    {
      DEBUG("Call.CreateMultiparty with channel %s (%p) failed: " GERROR_MSG_FMT,
          self->nick, self, GERROR_MSG_CODE(error));
    }
  else
    {
      priv->conf_created = TRUE;
      DEBUG("Call.CreateMultiparty with channel %s (%p) returned",
          self->nick, self);
    }


  channelrequest = modem_request_steal_data(request, "RingChannelRequest");
  if (channelrequest) {
    RingMediaManager *manager = modem_request_get_data(request, "RingMediaManager");

    ring_media_manager_emit_new_channel(manager,
      channelrequest, self, error ? error0 : NULL);
  }
  else if (error) {
    ring_conference_channel_release(_self, 0, 0, error);
    ring_conference_channel_close_impl(_self, FALSE, FALSE);
  }
  else {
    ring_conference_channel_emit_initial(_self);
  }
}

/* ---------------------------------------------------------------------- */

GType ring_object_path_list_get_type(void)
{
  static GType type;

  if (G_UNLIKELY(!type))
    type = dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH);

  return type;
}

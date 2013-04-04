/*
 * util.c - Source for Ring utility functions
 * Copyright (C) 2006-2007 Collabora Ltd.
 * Copyright (C) 2006-2007 Nokia Corporation
 *   @author Robert McQueen <robert.mcqueen@collabora.co.uk>
 *   @author Simon McVittie <simon.mcvittie@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "util.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gobject/gvaluecollector.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#define DEBUG_FLAG RING_DEBUG_JID

#include "ring-connection.h"

gchar *
sha1_hex (const gchar *bytes,
          guint len)
{
  gchar *hex = g_compute_checksum_for_string (G_CHECKSUM_SHA1, bytes, len);
  guint i;

  for (i = 0; i < SHA1_HASH_SIZE * 2; i++)
    {
      g_assert (hex[i] != '\0');
      hex[i] = g_ascii_tolower (hex[i]);
    }

  g_assert (hex[SHA1_HASH_SIZE * 2] == '\0');

  return hex;
}

void
sha1_bin (const gchar *bytes,
          guint len,
          guchar out[SHA1_HASH_SIZE])
{
  GChecksum *checksum = g_checksum_new (G_CHECKSUM_SHA1);
  gsize out_len = SHA1_HASH_SIZE;

  g_assert (g_checksum_type_get_length (G_CHECKSUM_SHA1) == SHA1_HASH_SIZE);
  g_checksum_update (checksum, (const guchar *) bytes, len);
  g_checksum_get_digest (checksum, out, &out_len);
  g_assert (out_len == SHA1_HASH_SIZE);
  g_checksum_free (checksum);
}


/** ring_generate_id:
 *
 * RFC4122 version 4 compliant random UUIDs generator.
 *
 * Returns: A string with RFC41122 version 4 random UUID, must be freed with
 *          g_free().
 */
gchar *
ring_generate_id (void)
{
  GRand *grand;
  gchar *str;
  struct {
      guint32 time_low;
      guint16 time_mid;
      guint16 time_hi_and_version;
      guint8 clock_seq_hi_and_rsv;
      guint8 clock_seq_low;
      guint16 node_hi;
      guint32 node_low;
  } uuid;

  /* Fill with random. Every new GRand are seede with 128 bit read from
   * /dev/urandom (or the current time on non-unix systems). This makes the
   * random source good enough for our usage, but may not be suitable for all
   * situation outside Ring. */
  grand = g_rand_new ();
  uuid.time_low = g_rand_int (grand);
  uuid.time_mid = (guint16) g_rand_int_range (grand, 0, G_MAXUINT16);
  uuid.time_hi_and_version = (guint16) g_rand_int_range (grand, 0, G_MAXUINT16);
  uuid.clock_seq_hi_and_rsv = (guint8) g_rand_int_range (grand, 0, G_MAXUINT8);
  uuid.clock_seq_low = (guint8) g_rand_int_range (grand, 0, G_MAXUINT8);
  uuid.node_hi = (guint16) g_rand_int_range (grand, 0, G_MAXUINT16);
  uuid.node_low = g_rand_int (grand);
  g_rand_free (grand);

  /* Set the two most significant bits (bits 6 and 7) of the
   * clock_seq_hi_and_rsv to zero and one, respectively. */
  uuid.clock_seq_hi_and_rsv = (uuid.clock_seq_hi_and_rsv & 0x3F) | 0x80;

  /* Set the four most significant bits (bits 12 through 15) of the
   * time_hi_and_version field to 4 */
  uuid.time_hi_and_version = (uuid.time_hi_and_version & 0x0fff) | 0x4000;

  str = g_strdup_printf ("%08x-%04x-%04x-%02x%02x-%04x%08x",
    uuid.time_low,
    uuid.time_mid,
    uuid.time_hi_and_version,
    uuid.clock_seq_hi_and_rsv,
    uuid.clock_seq_low,
    uuid.node_hi,
    uuid.node_low);

  return str;
}

/**
 * ring_get_room_handle_from_jid:
 * @room_repo: The %TP_HANDLE_TYPE_ROOM handle repository
 * @jid: A JID
 *
 * Given a JID seen in the from="" attribute on a stanza, work out whether
 * it's something to do with a MUC, and if so, return its handle.
 *
 * Returns: The handle of the MUC, if the JID refers to either a MUC
 *    we're in, or a contact's channel-specific JID inside a MUC.
 *    Returns 0 if the JID is either invalid, or nothing to do with a
 *    known MUC (typically this will mean it's the global JID of a contact).
 */
TpHandle
ring_get_room_handle_from_jid (TpHandleRepoIface *room_repo,
                                 const gchar *jid)
{
  TpHandle handle;
  gchar *room;

  room = ring_remove_resource (jid);
  if (room == NULL)
    return 0;

  handle = tp_handle_lookup (room_repo, room, NULL, NULL);
  g_free (room);
  return handle;
}

#define INVALID_HANDLE(e, f, ...) \
  G_STMT_START { \
  DEBUG (f, ##__VA_ARGS__); \
  g_set_error (e, TP_ERROR, TP_ERROR_INVALID_HANDLE, f, ##__VA_ARGS__);\
  } G_STMT_END

gchar *
ring_remove_resource (const gchar *jid)
{
  char *slash = strchr (jid, '/');
  gchar *buf;

  if (slash == NULL)
    return g_strdup (jid);

  /* The user and domain parts can't contain '/', assuming it's valid */
  buf = g_malloc (slash - jid + 1);
  strncpy (buf, jid, slash - jid);
  buf[slash - jid] = '\0';

  return buf;
}

gchar *
ring_encode_jid (
    const gchar *node,
    const gchar *domain,
    const gchar *resource)
{
  gchar *tmp, *ret;

  g_return_val_if_fail (domain != NULL, NULL);

  if (node != NULL && resource != NULL)
    tmp = g_strdup_printf ("%s@%s/%s", node, domain, resource);
  else if (node != NULL)
    tmp = g_strdup_printf ("%s@%s", node, domain);
  else if (resource != NULL)
    tmp = g_strdup_printf ("%s/%s", domain, resource);
  else
    tmp = g_strdup (domain);

  ret = g_utf8_normalize (tmp, -1, G_NORMALIZE_NFKC);
  g_free (tmp);
  return ret;
}

typedef struct {
    GObject *instance;
    GObject *user_data;
    gulong handler_id;
} WeakHandlerCtx;

static WeakHandlerCtx *
whc_new (GObject *instance,
         GObject *user_data)
{
  WeakHandlerCtx *ctx = g_slice_new0 (WeakHandlerCtx);

  ctx->instance = instance;
  ctx->user_data = user_data;

  return ctx;
}

static void
whc_free (WeakHandlerCtx *ctx)
{
  g_slice_free (WeakHandlerCtx, ctx);
}

static void user_data_destroyed_cb (gpointer, GObject *);

static void
instance_destroyed_cb (gpointer ctx_,
                       GObject *where_the_instance_was)
{
  WeakHandlerCtx *ctx = ctx_;

  /* No need to disconnect the signal here, the instance has gone away. */
  g_object_weak_unref (ctx->user_data, user_data_destroyed_cb, ctx);
  whc_free (ctx);
}

static void
user_data_destroyed_cb (gpointer ctx_,
                        GObject *where_the_user_data_was)
{
  WeakHandlerCtx *ctx = ctx_;

  g_signal_handler_disconnect (ctx->instance, ctx->handler_id);
  g_object_weak_unref (ctx->instance, instance_destroyed_cb, ctx);
  whc_free (ctx);
}

/**
 * ring_signal_connect_weak:
 * @instance: the instance to connect to.
 * @detailed_signal: a string of the form "signal-name::detail".
 * @c_handler: the GCallback to connect.
 * @user_data: an object to pass as data to c_handler calls.
 *
 * Connects a #GCallback function to a signal for a particular object, as if
 * with g_signal_connect(). Additionally, arranges for the signal handler to be
 * disconnected if @user_data is destroyed.
 *
 * This is intended to be a convenient way for objects to use themselves as
 * user_data for callbacks without having to explicitly disconnect all the
 * handlers in their finalizers.
 */
void
ring_signal_connect_weak (gpointer instance,
                            const gchar *detailed_signal,
                            GCallback c_handler,
                            GObject *user_data)
{
  GObject *instance_obj = G_OBJECT (instance);
  WeakHandlerCtx *ctx = whc_new (instance_obj, user_data);

  ctx->handler_id = g_signal_connect (instance, detailed_signal, c_handler,
      user_data);

  g_object_weak_ref (instance_obj, instance_destroyed_cb, ctx);
  g_object_weak_ref (user_data, user_data_destroyed_cb, ctx);
}

typedef struct {
    GSourceFunc function;
    GObject *object;
    guint source_id;
} WeakIdleCtx;

static void
idle_weak_ref_notify (gpointer data,
                      GObject *dead_object)
{
  g_source_remove (GPOINTER_TO_UINT (data));
}

static void
idle_removed (gpointer data)
{
  WeakIdleCtx *ctx = (WeakIdleCtx *) data;

  g_slice_free (WeakIdleCtx, ctx);
}

static gboolean
idle_callback (gpointer data)
{
  WeakIdleCtx *ctx = (WeakIdleCtx *) data;

  if (ctx->function ((gpointer) ctx->object))
    {
      return TRUE;
    }
  else
    {
      g_object_weak_unref (
          ctx->object, idle_weak_ref_notify, GUINT_TO_POINTER (ctx->source_id));
      return FALSE;
    }
}

/* Like g_idle_add(), but cancel the callback if the provided object is
 * finalized.
 */
guint
ring_idle_add_weak (GSourceFunc function,
                      GObject *object)
{
  WeakIdleCtx *ctx;

  ctx = g_slice_new0 (WeakIdleCtx);
  ctx->function = function;
  ctx->object = object;
  ctx->source_id = g_idle_add_full (
      G_PRIORITY_DEFAULT_IDLE, idle_callback, ctx, idle_removed);

  g_object_weak_ref (
      object, idle_weak_ref_notify, GUINT_TO_POINTER (ctx->source_id));
  return ctx->source_id;
}

GPtrArray *
ring_g_ptr_array_copy (GPtrArray *source)
{
  GPtrArray *ret = g_ptr_array_sized_new (source->len);
  guint i;

  for (i = 0; i < source->len; i++)
    g_ptr_array_add (ret, g_ptr_array_index (source, i));

  return ret;
}

gchar *
ring_peer_to_jid (RingConnection *conn,
    TpHandle peer,
    const gchar *resource)
{
  TpHandleRepoIface *repo = tp_base_connection_get_handles (
    TP_BASE_CONNECTION (conn), TP_HANDLE_TYPE_CONTACT);
  const gchar *target = tp_handle_inspect (repo, peer);

  if (resource == NULL)
    return g_strdup (target);

  return g_strdup_printf ("%s/%s", target, resource);
}

/* Like wocky_enum_from_nick, but for GFlagsValues instead. */
gboolean
ring_flag_from_nick (GType flag_type,
    const gchar *nick,
    guint *value)
{
  GFlagsClass *klass = g_type_class_ref (flag_type);
  GFlagsValue *flag_value;

  g_return_val_if_fail (klass != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  flag_value = g_flags_get_value_by_nick (klass, nick);
  g_type_class_unref (klass);

  if (flag_value != NULL)
    {
      *value = flag_value->value;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

/**
 * ring_simple_async_succeed_or_fail_in_idle:
 * @self: the source object for an asynchronous function
 * @callback: a callback to call when @todo things have been done
 * @user_data: user data for the callback
 * @source_tag: the source tag for a #GSimpleAsyncResult
 * @error: (allow-none): %NULL to indicate success, or an error on failure
 *
 * Create a new #GSimpleAsyncResult and schedule it to call its callback
 * in an idle. If @error is %NULL, report success with
 * tp_simple_async_report_success_in_idle(); if @error is non-%NULL,
 * use g_simple_async_report_gerror_in_idle().
 */
void
ring_simple_async_succeed_or_fail_in_idle (gpointer self,
    GAsyncReadyCallback callback,
    gpointer user_data,
    gpointer source_tag,
    const GError *error)
{
  if (error == NULL)
    {
      tp_simple_async_report_success_in_idle (self, callback, user_data,
          source_tag);
    }
  else
    {
      /* not const-correct yet: GNOME #622004 */
      g_simple_async_report_gerror_in_idle (self, callback, user_data,
          (GError *) error);
    }
}

/**
 * ring_simple_async_countdown_new:
 * @self: the source object for an asynchronous function
 * @callback: a callback to call when @todo things have been done
 * @user_data: user data for the callback
 * @source_tag: the source tag for a #GSimpleAsyncResult
 * @todo: number of things to do before calling @callback (at least 1)
 *
 * Create a new #GSimpleAsyncResult that will call its callback when a number
 * of asynchronous operations have happened.
 *
 * An internal counter is initialized to @todo, incremented with
 * ring_simple_async_countdown_inc() or decremented with
 * ring_simple_async_countdown_dec().
 *
 * When that counter reaches zero, if an error has been set with
 * g_simple_async_result_set_from_error() or similar, the operation fails;
 * otherwise, it succeeds.
 *
 * The caller must not use the operation result functions, such as
 * g_simple_async_result_get_op_res_gssize() - this async result is only
 * suitable for "void" async methods which return either success or a #GError,
 * i.e. the same signature as g_async_initable_init_async().
 *
 * Returns: (transfer full): a counter
 */
GSimpleAsyncResult *
ring_simple_async_countdown_new (gpointer self,
    GAsyncReadyCallback callback,
    gpointer user_data,
    gpointer source_tag,
    gssize todo)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (todo >= 1, NULL);

  simple = g_simple_async_result_new (self, callback, user_data, source_tag);
  /* We (ab)use the op_res member as a count of things to do. When
   * it reaches zero, the operation completes with any error that has been
   * set, or with success. */
  g_simple_async_result_set_op_res_gssize (simple, todo);

  /* we keep one extra reference as long as the counter is nonzero */
  g_object_ref (simple);

  return simple;
}

/**
 * ring_simple_async_countdown_inc:
 * @simple: a result created by ring_simple_async_countdown_new()
 *
 * Increment the counter in @simple, indicating that an additional async
 * operation has been started. An additional call to
 * ring_simple_async_countdown_dec() will be needed to make @simple
 * call its callback.
 */
void
ring_simple_async_countdown_inc (GSimpleAsyncResult *simple)
{
  gssize todo = g_simple_async_result_get_op_res_gssize (simple);

  g_return_if_fail (todo >= 1);
  g_simple_async_result_set_op_res_gssize (simple, todo + 1);
}

/**
 * ring_simple_async_countdown_dec:
 * @simple: a result created by ring_simple_async_countdown_new()
 *
 * Decrement the counter in @simple. If the number of things to do has
 * reached zero, schedule @simple to call its callback in an idle, then
 * unref it.
 *
 * When one of the asynchronous operations needed for @simple succeeds,
 * this should be signalled by a call to this function.
 *
 * When one of the asynchronous operations needed for @simple fails,
 * this should be signalled by a call to g_simple_async_result_set_from_error()
 * (or one of the similar functions), followed by a call to this function.
 * If more than one async operation fails in this way, the #GError from the
 * last failure will be used.
 */
void
ring_simple_async_countdown_dec (GSimpleAsyncResult *simple)
{
  gssize todo = g_simple_async_result_get_op_res_gssize (simple);

  g_simple_async_result_set_op_res_gssize (simple, --todo);

  if (todo <= 0)
    {
      g_simple_async_result_complete_in_idle (simple);
      g_object_unref (simple);
    }
}

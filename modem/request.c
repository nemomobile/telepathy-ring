/*
 * modem/request.c - Extensible closure for asyncronous DBus calls
 *
 * Copyright (C) 2008-2010 Nokia Corporation
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

#include "modem/request-private.h"

#include <string.h>

/* This is a specially laid out GPtrArray */
struct _ModemRequest
{
  ModemRequestPrivate *priv;
  guint len;
};

typedef struct _ModemRequestNotify ModemRequestNotify;

struct _ModemRequestPrivate {
  GObject *object;
  GCallback callback;
  gpointer user_data;

  DBusGProxy *proxy;
  DBusGProxyCall *call;

  struct _ModemRequestNotify {
    gpointer quark;
    GDestroyNotify destroy;
    gpointer data;
  } notify[];
};

enum {
  N_SIZED = offsetof(ModemRequestPrivate, notify[4]) / sizeof (gpointer),
  N_SIZE = offsetof(ModemRequestPrivate, notify[0]) / sizeof (gpointer),
  N_NOTIFY = sizeof(struct _ModemRequestNotify) / sizeof (gpointer)
};

#define DUMMYQ modem_request_dummy_quark()
GQuark
modem_request_dummy_quark(void)
{
  static GQuark quark;
  if (G_UNLIKELY(!quark))
    quark = g_quark_from_static_string("modem_request_dummy_quark");
  return quark;
}

#define MODEM_REQUEST_CANCEL_QUARK modem_request_cancel_quark()
GQuark
modem_request_cancel_quark(void)
{
  static GQuark quark;
  if (G_UNLIKELY(!quark))
    quark = g_quark_from_static_string("modem_request_cancel");
  return quark;
}


ModemRequest *
_modem_request_new(gpointer object,
  DBusGProxy *proxy,
  GCallback callback,
  gpointer user_data)
{
  GPtrArray *container = g_ptr_array_sized_new(N_SIZED);

  g_ptr_array_set_size(container, N_SIZE);

  ModemRequest *request = (gpointer)container;
  ModemRequestPrivate *priv = request->priv;

  if (object)
    priv->object = g_object_ref(object);
  priv->proxy = DBUS_G_PROXY(g_object_ref(proxy));
  priv->callback = callback;
  priv->user_data = user_data;

  return request;
}

void
_modem_request_add_proxy_call(ModemRequest *request,
  DBusGProxyCall *call)
{

  request->priv->call = call;
}

void
modem_request_add_qdata_full(ModemRequest *request,
  GQuark quark,
  gpointer user_data,
  GDestroyNotify destroy)
{
  GPtrArray *container = (GPtrArray *)request;
  gpointer qpointer = GUINT_TO_POINTER(quark);
  guint i;

  g_assert(quark != DUMMYQ);

  for (i = N_SIZE; i < container->len; i += N_NOTIFY) {
    ModemRequestNotify *notify = (gpointer)(container->pdata + i);

    if (notify->quark == qpointer) {
      ModemRequestNotify save;
      save = *notify;
      notify->destroy = destroy;
      notify->data = user_data;

      if (save.destroy)
        save.destroy(save.data);

      return;
    }
  }

  g_ptr_array_add(container, qpointer);
  g_ptr_array_add(container, destroy);
  g_ptr_array_add(container, user_data);
}

void
modem_request_add_qdata(ModemRequest *request,
  GQuark quark,
  gpointer data)
{
  modem_request_add_qdata_full(request, quark, data, NULL);
}

void
modem_request_add_qdatas(ModemRequest *request,
  GQuark quark,
  gpointer user_data,
  GDestroyNotify destroy,
  ...)
{
  va_list ap;

  va_start(ap, destroy);

  for (;;) {
    modem_request_add_qdata_full(request, quark, user_data, destroy);
    quark = va_arg(ap, GQuark);
    if (!quark)
      break;
    user_data = va_arg(ap, gpointer);
    destroy = va_arg(ap, GDestroyNotify);
  }

  va_end(ap);
}

void
modem_request_add_notifys(ModemRequest *request,
  GDestroyNotify destroy,
  gpointer user_data,
  ...)
{
  GPtrArray *container = (GPtrArray *)request;
  gpointer qpointer = GUINT_TO_POINTER(DUMMYQ);

  va_list ap;

  va_start(ap, user_data);

  for (;;) {
    g_ptr_array_add(container, qpointer);
    g_ptr_array_add(container, destroy);
    g_ptr_array_add(container, user_data);

    destroy = va_arg(ap, GDestroyNotify);
    if (!destroy)
      break;
    user_data = va_arg(ap, gpointer);
  }

  va_end(ap);
}

gpointer
modem_request_steal_qdata(ModemRequest *request,
  GQuark quark)
{
  GPtrArray *container = (GPtrArray *)request;
  gpointer qpointer = GUINT_TO_POINTER(quark);
  guint i;

  g_assert(quark != DUMMYQ);

  for (i = N_SIZE; i < container->len; i += N_NOTIFY) {
    ModemRequestNotify *notify = (gpointer)(container->pdata + i);

    if (notify->quark == qpointer) {
      gpointer data = notify->data;
      g_ptr_array_remove_range(container, i, N_NOTIFY);
      return data;
    }
  }

  return NULL;
}

gpointer
modem_request_get_qdata(ModemRequest *request,
  GQuark quark)
{
  GPtrArray *container = (GPtrArray *)request;
  gpointer qpointer = GUINT_TO_POINTER(quark);

  guint i;

  for (i = N_SIZE; i < container->len; i += N_NOTIFY) {
    ModemRequestNotify *notify = (gpointer)(container->pdata + i);
    if (notify->quark == qpointer)
      return notify->data;
  }

  return NULL;
}

void
modem_request_add_data(ModemRequest *request,
  char const *key,
  gpointer data)
{
  modem_request_add_data_full(request, key, data, NULL);
}

void
modem_request_add_data_full(ModemRequest *request,
  char const *key,
  gpointer data,
  GDestroyNotify destroy)
{
  modem_request_add_qdata_full(request, g_quark_from_string(key), data, destroy);
}

gpointer
modem_request_get_data(ModemRequest *request,
  char const *key)
{
  GQuark quark = g_quark_try_string(key);
  if (!quark)
    return NULL;
  return modem_request_get_qdata(request, quark);
}

gpointer
modem_request_steal_data(ModemRequest *request,
  char const *key)
{
  GQuark quark = g_quark_try_string(key);
  if (!quark)
    return NULL;
  return modem_request_steal_qdata(request, quark);
}

void
_modem_request_destroy_notify(gpointer _request)
{
  ModemRequest *request = _request;
  ModemRequestPrivate *priv = request->priv;
  GObject *object = priv->object;
  DBusGProxy *proxy = priv->proxy;

  priv->object = NULL;
  priv->proxy = NULL;
  priv->call = NULL;

  GPtrArray *container = (GPtrArray *)request;
  guint i;

  for (i = N_SIZE; i < container->len; i += N_NOTIFY) {
    ModemRequestNotify *notify = (gpointer)(container->pdata + i);

    GDestroyNotify destroy = notify->destroy;
    gpointer data = notify->data;

    memset(notify, 0, sizeof notify);

    if (destroy)
      destroy(data);
  }

  if (proxy)
    g_object_unref((GObject*)proxy);
  if (object)
    g_object_unref(object);

  g_ptr_array_free(_request, TRUE);
}

/**  Cancel request
 */
void
modem_request_cancel(ModemRequest *request)
{
  ModemRequestPrivate *priv = request->priv;
  GDestroyNotify cancel;

  cancel = modem_request_steal_qdata(request, MODEM_REQUEST_CANCEL_QUARK);

  if (cancel) {
    cancel(request);
    return;
  }

  if (priv->call)
    dbus_g_proxy_cancel_call(priv->proxy, priv->call);
  else
    _modem_request_destroy_notify(request);
}

void
modem_request_add_cancel_notify(ModemRequest *request,
  GDestroyNotify notify)
{
  modem_request_add_qdata(request, MODEM_REQUEST_CANCEL_QUARK,
    (gpointer)notify);
}

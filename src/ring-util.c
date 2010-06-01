/*
 * ring-util.c - Miscellaneous utility functions
 *
 * Copyright (C) 2007-2010 Nokia Corporation
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

#include "ring-util.h"

#include "modem/call.h"
#include "modem/errors.h"

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/group-mixin.h>

#include <string.h>

char *
ring_str_starts_with_case(char const *string, char const *prefix)
{
  size_t len = strlen(prefix);

  if (g_ascii_strncasecmp(string, prefix, len))
    return NULL;

  return (char *)string + len;
}


char *
ring_str_starts_with(char const *string, char const *prefix)
{
  while (*prefix && *string)
    if (*prefix++ != *string++)
      return 0;
  return *prefix == '\0' ? (char *)string : NULL;
}


char const *
ring_connection_status_as_string(TpConnectionStatus st)
{
  switch ((int)st) {
    case TP_INTERNAL_CONNECTION_STATUS_NEW:
      return "new";
    case TP_CONNECTION_STATUS_CONNECTED:
      return "connected";
    case TP_CONNECTION_STATUS_CONNECTING:
      return "connecting";
    case TP_CONNECTION_STATUS_DISCONNECTED:
      return "disconnected";
  }
  return "<UNKNOWN>";
}

char *
ring_normalize_isdn(char const *s)
{
  int i, j;
  char *isdn = g_strdup(s ? s : "");

  for (i = 0, j = 0; isdn[i]; i++) {
    switch (isdn[i]) {
      case ' ': case '.': case '(': case ')': case '-':
        continue;
      case '+':
        if (j != 0) {
          g_free(isdn);
          return NULL;
        }
        /* FALLTHROUGH */
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        if (i != j)
          isdn[j] = isdn[i];
        j++;
        break;
      default:
        g_free(isdn);
        return NULL;
    }
  }

  if (i != j)
    isdn[j] = '\0';

  return isdn;
}


int
ring_str_has_token(char const *string, char const *token)
{
  char **strv;
  int i, found;

  if (!string || !token)
    return 0;

  if (g_ascii_strcasecmp(string, token) == 0)
    return 1;

  strv = g_strsplit_set(string, " \t\r\n,;", G_MAXINT);

  if (strv == NULL)
    return 0;

  for (i = 0; strv[i]; i++) {
    if (g_ascii_strcasecmp(string, token) == 0)
      break;
  }

  found = strv[i] != NULL;

  g_strfreev(strv);

  return found;
}

#define DEBUG_FLAG RING_DEBUG_CONNECTION

#include <ring-debug.h>

gboolean
ring_properties_satisfy(GHashTable *requested_properties,
  GHashTable *fixed_properties,
  char const * const *allowed)
{
  GHashTableIter i[1];
  gpointer keyp, valuep;

  for (g_hash_table_iter_init(i, fixed_properties);
       g_hash_table_iter_next(i, &keyp, &valuep);) {
    GValue *fixed = valuep;

    gpointer requestedp = g_hash_table_lookup(requested_properties, keyp);

    if (!requestedp) {
      DEBUG("** expecting %s with %s",
        (char *)keyp, g_type_name(G_VALUE_TYPE(fixed)));
      return 0;
    }

    GValue *requested = requestedp;

    if (G_VALUE_HOLDS(fixed, G_TYPE_UINT)) {
      guint64 u = 0;
      gint64 i = 0;

      switch (G_VALUE_TYPE(requested)) {
        case G_TYPE_UCHAR:
          u = g_value_get_uchar(requested);
          break;
        case G_TYPE_UINT:
          u = g_value_get_uint(requested);
          break;
        case G_TYPE_UINT64:
          u = g_value_get_uint64(requested);
          break;
        case G_TYPE_INT:
          i = g_value_get_int(requested);
          u = (guint64)i;
          break;
        case G_TYPE_INT64:
          i = g_value_get_int64(requested);
          u = (guint64)i;
          break;
      }

      if (i < 0) {
        DEBUG("*** expecting %u for %s, got %lld",
          g_value_get_uint(fixed), (char *)keyp, (long long)i);
        return 0;
      }

      if ((guint64)g_value_get_uint(fixed) == u)
        continue;

      DEBUG("*** expecting %u for %s, got %llu",
        g_value_get_uint(fixed), (char *)keyp, (unsigned long long)u);

      return 0;
    }

    if (!G_VALUE_HOLDS(requested, G_VALUE_TYPE(fixed))) {
      DEBUG("*** expecting type %s for %s, got type %s",
        g_type_name(G_VALUE_TYPE(fixed)),
        (char *)keyp,
        g_type_name(G_VALUE_TYPE(requested)));
    }
    else if (G_VALUE_HOLDS(requested, G_TYPE_BOOLEAN)) {
      if (g_value_get_boolean(fixed) == g_value_get_boolean(requested))
        continue;
      DEBUG("*** expecting %u for %s, got %u",
        g_value_get_boolean(fixed), (char *)keyp, g_value_get_boolean(requested));
    }
    else if (G_VALUE_HOLDS(requested, G_TYPE_STRING)) {
      if (!tp_strdiff(g_value_get_string(fixed), g_value_get_string(requested)))
        continue;
      DEBUG("*** expecting \"%s\" for %s, got \"%s\"",
        g_value_get_string(fixed), (char *)keyp, g_value_get_string(requested));
    }
    else {
      g_warning("*** fixed-properties contains %s ***", G_VALUE_TYPE_NAME(fixed));
    }

    return 0;
  }

  for (g_hash_table_iter_init(i, requested_properties);
       g_hash_table_iter_next(i, &keyp, &valuep);) {
    if (g_hash_table_lookup(fixed_properties, keyp))
      continue;
    char const *name = keyp;
    if (!tp_strv_contains(allowed, name)) {
      if (DEBUGGING) {
        char *value = g_strdup_value_contents(valuep);
        DEBUG("Unknown property %s=%s in request", name, value);
        g_free(value);
      }
      return 0;
    }
  }

  return 1;
}

GHashTable *
ring_channel_add_properties(gpointer obj,
  GHashTable *hash,
  char const *interface,
  char const *member,
  ...)
{
  va_list ap;
  va_start(ap, member);

  if (hash == NULL)
    hash = g_hash_table_new_full(g_str_hash, g_str_equal,
           g_free,
           (GDestroyNotify)tp_g_value_slice_free);

  for (;;) {
    char *key = g_strdup_printf("%s.%s", interface, member);

    GValue *value = g_slice_new0(GValue);
    tp_dbus_properties_mixin_get(obj, interface, member, value, NULL);
    g_assert(G_IS_VALUE(value));

    g_hash_table_insert(hash, key, value);

    interface = va_arg(ap, char const *);
    if (interface == NULL)
      break;

    member = va_arg(ap, char const *);
  }

  va_end(ap);

  return hash;
}

/** Return internal error to a pending DBus method call */
void
ring_method_return_internal_error(gpointer _context)
{
  GError error =
    {
      TP_ERRORS, TP_ERROR_DISCONNECTED, "Internal error - request canceled"
    };
  dbus_g_method_return_error((DBusGMethodInvocation *)_context, &error);
}

/** Emit MembersChanged(Detailed) signal */
gboolean
ring_util_group_change_members(gpointer obj,
  TpIntSet *add,
  TpIntSet *del,
  TpIntSet *local_pending,
  TpIntSet *remote_pending,
  char const *key, /* gtype */ /* value */
                               /* actor, G_TYPE_UINT, value, */
                               /* change-reason, G_TYPE_UINT, value */
                               /* message, G_TYPE_STRING, value */
                               /* error, G_TYPE_STRING, value */
                               /* debug-message, G_TYPE_STRING, value */
  ...)
{
  GHashTable *details;
  va_list ap;
  gboolean emitted;

  details = g_hash_table_new_full(
    g_str_hash, g_str_equal,
    NULL, (GDestroyNotify) tp_g_value_slice_free);

  for (va_start(ap, key); key; key = va_arg(ap, char const *)) {
    GType type = va_arg(ap, GType);
    GValue value[1], *copy = NULL;

    g_value_init(memset(value, 0, (sizeof value)), type);

    if (type == G_TYPE_UINT) {
      g_value_set_uint(value, va_arg(ap, guint));
    }
    else if (type == G_TYPE_STRING) {
      g_value_take_string(copy = value, va_arg(ap, char *));
    }
    else {
      DEBUG("detail \"%s\" with unknown type %s", key, g_type_name(type));
      break;
    }

    if (g_str_equal(key, "")) {
      /* empty name, skip the value */
      continue;
    }

    g_value_copy(value, copy = tp_g_value_slice_new(type));
    g_hash_table_insert(details, (gpointer)key, copy);
  }

  va_end(ap);

  emitted = tp_group_mixin_change_members_detailed(G_OBJECT(obj),
            add, del, local_pending, remote_pending, details);

  g_hash_table_destroy(details);

  return emitted;
}

/* Map group change reason to string */
char const *
ring_util_reason_name(TpChannelGroupChangeReason reason)
{
  switch (reason) {
    case TP_CHANNEL_GROUP_CHANGE_REASON_NONE: return "NONE";
    case TP_CHANNEL_GROUP_CHANGE_REASON_OFFLINE: return "OFFLINE";
    case TP_CHANNEL_GROUP_CHANGE_REASON_KICKED: return "KICKED";
    case TP_CHANNEL_GROUP_CHANGE_REASON_BUSY: return "BUSY";
    case TP_CHANNEL_GROUP_CHANGE_REASON_INVITED: return "INVITED";
    case TP_CHANNEL_GROUP_CHANGE_REASON_BANNED: return "BANNED";
    case TP_CHANNEL_GROUP_CHANGE_REASON_ERROR: return "ERROR";
    case TP_CHANNEL_GROUP_CHANGE_REASON_INVALID_CONTACT: return "INVALID_CONTACT";
    case TP_CHANNEL_GROUP_CHANGE_REASON_NO_ANSWER: return "NO_ANSWER";
    case TP_CHANNEL_GROUP_CHANGE_REASON_RENAMED: return "RENAMED";
    case TP_CHANNEL_GROUP_CHANGE_REASON_PERMISSION_DENIED: return "PERMISSION_DENIED";
    case TP_CHANNEL_GROUP_CHANGE_REASON_SEPARATED: return "SEPARATED";
  }
  return "<UNKNOWN>";
}

/* ---------------------------------------------------------------------- */

/* Map modem causetype and cause to reason */
TpChannelGroupChangeReason
ring_channel_group_release_reason(guint causetype, guint cause)
{
  if (causetype == MODEM_CALL_CAUSE_TYPE_NETWORK) {
    /* 3GPP TS 22.001 F.4 */
    switch (cause) {
      case MODEM_CALL_NET_ERROR_NORMAL:
        return TP_CHANNEL_GROUP_CHANGE_REASON_NONE;

      case MODEM_CALL_NET_ERROR_USER_BUSY:
        return TP_CHANNEL_GROUP_CHANGE_REASON_BUSY;

      case MODEM_CALL_NET_ERROR_NUMBER_CHANGED:
        return TP_CHANNEL_GROUP_CHANGE_REASON_INVALID_CONTACT;

      case MODEM_CALL_NET_ERROR_RESP_TO_STATUS:
      case MODEM_CALL_NET_ERROR_NORMAL_UNSPECIFIED:
        return TP_CHANNEL_GROUP_CHANGE_REASON_NONE;

      case MODEM_CALL_NET_ERROR_NO_CHANNEL:
      case MODEM_CALL_NET_ERROR_TEMPORARY_FAILURE:
      case MODEM_CALL_NET_ERROR_CONGESTION:
      case MODEM_CALL_NET_ERROR_CHANNEL_NA:
      case MODEM_CALL_NET_ERROR_QOS_NA:
      case MODEM_CALL_NET_ERROR_BC_NA:
        /* Congestion */
        return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;

      default:
        return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;
    }
  }
  else {
    switch (cause) {
      case MODEM_CALL_ERROR_NO_ERROR:
      case MODEM_CALL_ERROR_RELEASE_BY_USER:
        return TP_CHANNEL_GROUP_CHANGE_REASON_NONE;

      case MODEM_CALL_ERROR_NO_CALL:
      case MODEM_CALL_ERROR_CALL_ACTIVE:
      case MODEM_CALL_ERROR_NO_CALL_ACTIVE:
        return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;
      case MODEM_CALL_ERROR_BUSY_USER_REQUEST:
        return TP_CHANNEL_GROUP_CHANGE_REASON_BUSY;
      case MODEM_CALL_ERROR_EMERGENCY:
      case MODEM_CALL_ERROR_DTMF_SEND_ONGOING:
        return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;

      case MODEM_CALL_ERROR_BLACKLIST_BLOCKED:
      case MODEM_CALL_ERROR_BLACKLIST_DELAYED:
        return TP_CHANNEL_GROUP_CHANGE_REASON_PERMISSION_DENIED;

      case MODEM_CALL_ERROR_CHANNEL_LOSS:
      case MODEM_CALL_ERROR_NO_SERVICE:
      case MODEM_CALL_ERROR_NO_COVERAGE:
        return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;

      case MODEM_CALL_ERROR_ERROR_REQUEST:
      case MODEM_CALL_ERROR_INVALID_CALL_MODE:
      case MODEM_CALL_ERROR_CODE_REQUIRED:
      case MODEM_CALL_ERROR_NOT_ALLOWED:
      case MODEM_CALL_ERROR_DTMF_ERROR:
      case MODEM_CALL_ERROR_FDN_NOT_OK:
      case MODEM_CALL_ERROR_EMERGENCY_FAILURE:
        return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;

      case MODEM_CALL_ERROR_TOO_LONG_ADDRESS:
      case MODEM_CALL_ERROR_INVALID_ADDRESS:
        return TP_CHANNEL_GROUP_CHANGE_REASON_INVALID_CONTACT;

      case MODEM_CALL_ERROR_NO_SIM:
      case MODEM_CALL_ERROR_CS_INACTIVE:
      default:
        return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;
    }
  }

  return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;
}

TpChannelGroupChangeReason
ring_channel_group_error_reason(GError *error)
{
  if (error == NULL)
    return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;

  if (error->domain == MODEM_CALL_ERRORS)
    return ring_channel_group_release_reason(MODEM_CALL_CAUSE_TYPE_REMOTE, error->code);
  if (error->domain == MODEM_CALL_NET_ERRORS)
    return ring_channel_group_release_reason(MODEM_CALL_CAUSE_TYPE_NETWORK, error->code);

  return TP_CHANNEL_GROUP_CHANGE_REASON_ERROR;
}

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

#ifndef __RING_UTIL_H__
#define __RING_UTIL_H__

#include "telepathy-glib/util.h"
#include "telepathy-glib/enums.h"
#include "telepathy-glib/util.h"
#include "telepathy-glib/intset.h"

G_BEGIN_DECLS

#define RING_STR_EMPTY(s) (s == NULL || s[0] == '\0')

char *ring_str_starts_with(char const *string, char const *prefix);

char *ring_str_starts_with_case(char const *string, char const *prefix);

int ring_str_has_token(char const *string, char const *token);

gboolean ring_properties_satisfy(GHashTable *requested_properties,
  GHashTable *fixed_properties,
  char const * const * allowed);

char const *ring_connection_status_as_string(TpConnectionStatus st);
char *ring_normalize_isdn(gchar const *s);

GHashTable *ring_channel_add_properties(gpointer obj,
  GHashTable *hash,
  char const *interface,
  char const *member,
  ...) G_GNUC_NULL_TERMINATED;

void ring_method_return_internal_error(gpointer _context);

gpointer ring_network_normalization_context(void);

GValueArray *ring_contact_capability_new(guint handle,
  char const *channel_type, guint generic, guint specific);
void ring_contact_capability_free(gpointer value);

gboolean ring_util_group_change_members(gpointer object,
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
  ...) G_GNUC_NULL_TERMINATED;

char const *ring_util_reason_name(TpChannelGroupChangeReason reason);

/* initial-members */

typedef struct
{
  char const **odata;
  guint         len;
} RingInitialMembers;

TpChannelGroupChangeReason ring_channel_group_release_reason(
  guint causetype, guint cause);
TpChannelGroupChangeReason ring_channel_group_error_reason(GError *);

G_END_DECLS

#endif /* #ifndef __RING_UTIL_H__*/

/*
 * ring-text-manager.h - Manager for text channels
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

#ifndef RING_TEXT_MANAGER_H
#define RING_TEXT_MANAGER_H

#include <telepathy-glib/enums.h>
#include <telepathy-glib/channel-manager.h>

#include "modem/sms.h"

G_BEGIN_DECLS

typedef struct _RingTextManager RingTextManager;
typedef struct _RingTextManagerClass RingTextManagerClass;
typedef struct _RingTextManagerPrivate RingTextManagerPrivate;

struct _RingTextManagerClass {
  GObjectClass parent_class;
};

struct _RingTextManager {
  GObject parent;
  RingTextManagerPrivate *priv;
};

GType ring_text_manager_get_type(void);

/* TYPE MACROS */
#define RING_TYPE_TEXT_MANAGER (ring_text_manager_get_type())
#define RING_TEXT_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST(     \
  (obj), RING_TYPE_TEXT_MANAGER, RingTextManager))
#define RING_TEXT_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST(        \
  (klass), RING_TYPE_TEXT_MANAGER, RingTextManagerClass))
#define RING_IS_TEXT_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE(  \
  (obj), RING_TYPE_TEXT_MANAGER))
#define RING_IS_TEXT_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE(     \
  (klass), RING_TYPE_TEXT_MANAGER))
#define RING_TEXT_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS(    \
  (obj), RING_TYPE_TEXT_MANAGER, RingTextManagerClass))

gpointer ring_text_manager_lookup(RingTextManager *self,
  char const *object_path);

#if nomore
void ring_text_manager_deliver_stored_messages(RingTextManager *,
  char const **messages,
  gpointer context);

void ring_text_manager_expunge_messages(RingTextManager *,
  char const **message_identities,
  gpointer context);

void ring_text_manager_set_storage_status(RingTextManager *,
  gboolean out_of_storage,
  gpointer context);

char **ring_text_manager_list_stored_messages(RingTextManager const *);

#endif

void ring_text_manager_add_capabilities(RingTextManager *self,
  guint handle, GPtrArray *returns);

G_END_DECLS

#endif

/*
 * ring-media-manager.h - Manager for media channels
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

#ifndef RING_MEDIA_MANAGER_H
#define RING_MEDIA_MANAGER_H

G_BEGIN_DECLS

typedef struct _RingMediaManager RingMediaManager;
typedef struct _RingMediaManagerClass RingMediaManagerClass;
typedef struct _RingMediaManagerPrivate RingMediaManagerPrivate;

G_END_DECLS

#include <telepathy-glib/channel-manager.h>
#include <ring-media-channel.h>
#include <ring-emergency-service.h>
#include <ring-util.h>
#include "modem/call.h"

G_BEGIN_DECLS

struct _RingMediaManagerClass {
  GObjectClass parent_class;
};

struct _RingMediaManager {
  GObject parent;
  RingMediaManagerPrivate *priv;
};

GType ring_media_manager_get_type(void);

/* TYPE MACROS */
#define RING_TYPE_MEDIA_MANAGER                 \
  (ring_media_manager_get_type())
#define RING_MEDIA_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),      \
      RING_TYPE_MEDIA_MANAGER, RingMediaManager))
#define RING_MEDIA_MANAGER_CLASS(cls) (G_TYPE_CHECK_CLASS_CAST((cls),   \
      RING_TYPE_MEDIA_MANAGER, RingMediaManagerClass))
#define RING_IS_MEDIA_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),   \
      RING_TYPE_MEDIA_MANAGER))
#define RING_IS_MEDIA_MANAGER_CLASS(cls) (G_TYPE_CHECK_CLASS_TYPE((cls), \
      RING_TYPE_MEDIA_MANAGER))
#define RING_MEDIA_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
      RING_TYPE_MEDIA_MANAGER, RingMediaManagerClass))

RingMediaChannel *ring_media_manager_lookup(RingMediaManager *self,
  char const *object_path);

RingEmergencyServiceInfoList *ring_media_manager_emergency_services(
  RingMediaManager *self);

void ring_media_manager_add_capabilities(RingMediaManager *,
  TpHandle, GPtrArray *);

void ring_media_manager_emit_new_channel(RingMediaManager *self,
  gpointer request, gpointer channel, GError *error);

gboolean ring_media_manager_validate_initial_members(RingMediaManager *self,
  RingInitialMembers *initial,
  GError **error);

G_END_DECLS

#endif

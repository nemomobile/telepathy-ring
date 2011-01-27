/*
 * ring-conference-manager.h - Manager for conference channels
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

#ifndef RING_CONFERENCE_MANAGER_H
#define RING_CONFERENCE_MANAGER_H

#include <telepathy-glib/channel-manager.h>
#include <ring-util.h>
#include "modem/call.h"

G_BEGIN_DECLS

typedef struct _RingConferenceManager RingConferenceManager;
typedef struct _RingConferenceManagerClass RingConferenceManagerClass;
typedef struct _RingConferenceManagerPrivate RingConferenceManagerPrivate;

G_END_DECLS

#include <ring-conference-channel.h>

G_BEGIN_DECLS

struct _RingConferenceManagerClass {
  GObjectClass parent_class;
};

struct _RingConferenceManager {
  GObject parent;
  RingConferenceManagerPrivate *priv;
};

GType ring_conference_manager_get_type (void);

/* TYPE MACROS */
#define RING_TYPE_CONFERENCE_MANAGER \
  (ring_conference_manager_get_type())
#define RING_CONFERENCE_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
      RING_TYPE_CONFERENCE_MANAGER, RingConferenceManager))
#define RING_CONFERENCE_MANAGER_CLASS(cls) \
  (G_TYPE_CHECK_CLASS_CAST ((cls), \
      RING_TYPE_CONFERENCE_MANAGER, RingConferenceManagerClass))
#define RING_IS_CONFERENCE_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
      RING_TYPE_CONFERENCE_MANAGER))
#define RING_IS_CONFERENCE_MANAGER_CLASS(cls) \
  (G_TYPE_CHECK_CLASS_TYPE ((cls), \
      RING_TYPE_CONFERENCE_MANAGER))
#define RING_CONFERENCE_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
      RING_TYPE_CONFERENCE_MANAGER, RingConferenceManagerClass))

RingConferenceChannel *ring_conference_manager_lookup (RingConferenceManager *,
    char const *object_path);

gboolean ring_conference_manager_validate_initial_members (
    RingConferenceManager *, RingInitialMembers *, GError **error);

G_END_DECLS

#endif

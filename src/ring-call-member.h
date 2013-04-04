/*
 * ring-call-member.h - Header for CallMember
 * Copyright (C) 2010 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 * @author Tom Swindell <t.swindell@rubyx.co.uk>
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

#ifndef __RING_CALL_MEMBER_H__
#define __RING_CALL_MEMBER_H__

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

typedef struct _RingCallMember RingCallMember;
typedef struct _RingCallMemberPrivate RingCallMemberPrivate;
typedef struct _RingCallMemberClass RingCallMemberClass;

struct _RingCallMemberClass {
    GObjectClass parent_class;
};

struct _RingCallMember {
    GObject parent;
    RingCallMemberPrivate *priv;
};

GType ring_call_member_get_type (void);

/* TYPE MACROS */
#define RING_TYPE_CALL_MEMBER \
  (ring_call_member_get_type ())
#define RING_CALL_MEMBER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), RING_TYPE_CALL_MEMBER, \
    RingCallMember))
#define RING_CALL_MEMBER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_CALL_MEMBER, \
    RingCallMemberClass))
#define RING_IS_CALL_MEMBER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_CALL_MEMBER))
#define RING_IS_CALL_MEMBER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_CALL_MEMBER))
#define RING_CALL_MEMBER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), RING_TYPE_CALL_MEMBER, \
   RingCallMemberClass))

TpHandle ring_call_member_get_handle(RingCallMember *self);

TpCallMemberFlags ring_call_member_get_flags(RingCallMember *self);

RingConnection * ring_call_member_get_connection(RingCallMember *self);

void ring_call_member_shutdown(RingCallMember *self);

G_END_DECLS

#endif /* #ifndef __RING_CALL_MEMBER_H__*/

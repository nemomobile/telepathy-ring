/*
 * base.h -
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

#ifndef BASE_H
#define BASE_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _Base Base;
typedef struct _BaseClass BaseClass;
typedef struct _BasePrivate BasePrivate;

struct _BaseClass {
  GObjectClass parent_class;
};

struct _Base {
  GObject parent;
  BasePrivate *priv;
};

GType base_get_type(void);

/* TYPE MACROS */
#define TYPE_BASE                               \
  (base_get_type())
#define BASE(obj)                                       \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_BASE, Base))
#define BASE_CLASS(klass)                                       \
  (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_BASE, BaseClass))
#define IS_BASE(obj)                                    \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_BASE))
#define IS_BASE_CLASS(klass)                    \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_BASE))
#define BASE_GET_CLASS(obj)                                     \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_BASE, BaseClass))

G_END_DECLS

#endif /* #ifndef BASE_H */

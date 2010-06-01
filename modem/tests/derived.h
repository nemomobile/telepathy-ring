/*
 * derived.h -
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

#ifndef DERIVED_H
#define DERIVED_H

#include <glib-object.h>

#include "base.h"

G_BEGIN_DECLS

typedef struct _Derived Derived;
typedef struct _DerivedClass DerivedClass;
typedef struct _DerivedPrivate DerivedPrivate;

struct _DerivedClass {
  BaseClass parent_class;
};

struct _Derived {
  Base parent;
  DerivedPrivate *priv;
};

GType derived_get_type(void);

/* TYPE MACROS */
#define TYPE_DERIVED                            \
  (derived_get_type())
#define DERIVED(obj)                                            \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_DERIVED, Derived))
#define DERIVED_CLASS(klass)                                            \
  (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_DERIVED, DerivedClass))
#define IS_DERIVED(obj)                                 \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_DERIVED))
#define IS_DERIVED_CLASS(klass)                         \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_DERIVED))
#define DERIVED_GET_CLASS(obj)                                          \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_DERIVED, DerivedClass))

G_END_DECLS

#endif /* #ifndef DERIVED_H */

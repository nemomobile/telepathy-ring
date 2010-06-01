/*
 * derived.c -
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

#include "derived.h"

void checktag(char const *);

#define ENTER(format, ...)                                              \
  do { checktag(G_STRFUNC); DEBUG(format, ##__VA_ARGS__); } while(0)

#if CHECK_DEBUG_DERIVED
#define DEBUG(format, ...)                      \
  g_log("ring-tests", G_LOG_LEVEL_DEBUG,        \
    "%s: " format, G_STRFUNC, ##__VA_ARGS__)
#else
#define DEBUG(format, ...) ((void)0)
#endif

G_DEFINE_TYPE(Derived, derived, TYPE_BASE);

/* Properties */
enum
{
  PROP_NONE,
  PROP_READWRITE,
  PROP_CONSTRUCT,
  PROP_CONSTRUCT_ONLY,
};

/* private data */
struct _DerivedPrivate
{
  unsigned readwrite:1, construct:1, construct_only:1, dispose_has_run:1, :0;
};

static GObject *
derived_constructor(GType type,
  guint n_props,
  GObjectConstructParam *props)
{
  GObject *object;

  ENTER("enter (%s, %u@%p)", g_type_name(type), n_props, props);

  object = G_OBJECT_CLASS(derived_parent_class)
    ->constructor(type, n_props, props);

  DEBUG("return %p", object);

  return object;
}

static void
derived_init(Derived *self)
{
  ENTER("enter (%p)", self);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
    self, TYPE_DERIVED, DerivedPrivate);
}

static void
derived_get_property(GObject *object,
  guint property_id,
  GValue *value,
  GParamSpec *pspec)
{
  ENTER("enter (%p, %d)", object, property_id);

  Derived *self = DERIVED(object);
  DerivedPrivate *priv = self->priv;

  switch(property_id) {
    case PROP_READWRITE:
      g_value_set_boolean(value, priv->readwrite);
      break;

    case PROP_CONSTRUCT:
      g_value_set_boolean(value, priv->construct);
      break;

    case PROP_CONSTRUCT_ONLY:
      g_value_set_boolean(value, priv->construct_only);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }

  DEBUG("return value=%s", g_strdup_value_contents(value));
}

static void
derived_set_property(GObject *object,
  guint property_id,
  const GValue *value,
  GParamSpec *pspec)
{
  ENTER("enter (%p, %d, %s, %p)",
    object, property_id, g_strdup_value_contents(value), pspec);

  Derived *self = DERIVED(object);
  DerivedPrivate *priv = self->priv;

  switch(property_id) {
    case PROP_READWRITE:
      priv->readwrite = g_value_get_boolean(value);
      break;

    case PROP_CONSTRUCT:
      priv->construct = g_value_get_boolean(value);
      break;

    case PROP_CONSTRUCT_ONLY:
      priv->construct_only = g_value_get_boolean(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
  }

  DEBUG("return (%p)", object);
}

static void
derived_constructed(GObject *object)
{
  ENTER("(%p): enter", object);

  if (G_OBJECT_CLASS(derived_parent_class)->constructed)
    G_OBJECT_CLASS(derived_parent_class)->constructed(object);
}

static void
derived_dispose(GObject *object)
{
  Derived *self = DERIVED(object);
  DerivedPrivate *priv = self->priv;

  ENTER("enter (%p): %s", object,
    priv->dispose_has_run ? "already" : "disposing");

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;

  if (G_OBJECT_CLASS(derived_parent_class)->dispose)
    G_OBJECT_CLASS(derived_parent_class)->dispose(object);

  DEBUG("(%p): return from disposing", object);
}

static void
derived_finalize(GObject *object)
{
  Derived *self = DERIVED(object);
  DerivedPrivate *priv = self->priv;

  ENTER("enter (%p)", object);

  (void)priv;

  G_OBJECT_CLASS(derived_parent_class)->finalize(object);

  DEBUG("(%p): return", object);
}

static void
derived_class_init(DerivedClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  ENTER("enter");

  g_type_class_add_private(klass, sizeof (DerivedPrivate));

  object_class->constructor = derived_constructor;
  object_class->get_property = derived_get_property;
  object_class->set_property = derived_set_property;
  object_class->constructed = derived_constructed;
  object_class->dispose = derived_dispose;
  object_class->finalize = derived_finalize;

  /* No Signals */

  /* Properties */
  g_object_class_install_property(
    object_class, PROP_READWRITE,
    g_param_spec_boolean("derived-readwrite",
      "Derived readwrite property",
      "Readwrite property in derived object",
      TRUE, /* default value */
      G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_CONSTRUCT,
    g_param_spec_boolean("derived-construct",
      "Derived construct property",
      "Construct property in derived object",
      TRUE, /* default value */
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT |
      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
    object_class, PROP_CONSTRUCT_ONLY,
    g_param_spec_boolean("derived-construct-only",
      "Derived construct-only property",
      "Construct-only property in derived object",
      TRUE, /* default value */
      G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  DEBUG("return");
}

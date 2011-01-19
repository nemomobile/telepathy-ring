/*
 * modem/ofono.c - Ofono
 *
 * Copyright (C) 2010 Nokia Corporation
 *   @author Lassi Syrjala <first.surname@nokia.com>
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

#define MODEM_DEBUG_FLAG MODEM_LOG_MODEM
#include "modem/debug.h"

#include "modem/ofono.h"
#include "modem/request-private.h"
#include "modem/errors.h"
#include "modem/service.h"

/* ---------------------------------------------------------------------- */

GType
modem_type_dbus_dict (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type))
    {
      GType t = dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE);
      g_once_init_leave (&type, t);
    }

  return type;
}

GType
modem_type_dbus_ao (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type))
    {
      GType t = dbus_g_type_get_collection ("GPtrArray",
	  DBUS_TYPE_G_OBJECT_PATH);
      g_once_init_leave (&type, t);
    }

  return type;
}

GType
modem_type_dbus_managed_array (void)
{
  static gsize type = 0;

  /* a(oa{sv}) */

  if (g_once_init_enter (&type))
    {
      GType stype = dbus_g_type_get_struct ("GValueArray",
	  DBUS_TYPE_G_OBJECT_PATH,
	  MODEM_TYPE_DBUS_DICT,
	  G_TYPE_INVALID);
      GType t = dbus_g_type_get_collection ("GPtrArray", stype);
      g_once_init_leave (&type, t);
    }

  return type;
}

void
modem_ofono_debug_managed (char const *name,
			char const *object_path,
			GHashTable *properties)
{
  char *key;
  GValue *value;
  GHashTableIter iter[1];

  DEBUG ("%s (\"%s\")", name, object_path);

  for (g_hash_table_iter_init (iter, properties);
       g_hash_table_iter_next (iter, (gpointer)&key, (gpointer)&value);)
    {
      char *s = g_strdup_value_contents (value);
      DEBUG ("%s = %s", key, s);
      g_free (s);
    }
}

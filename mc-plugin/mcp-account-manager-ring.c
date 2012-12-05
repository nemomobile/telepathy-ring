/*
 * Copyright (C) 2012 Jolla Ltd.
 * Contact: John Brooks <john.brooks@jollamobile.com>
 *
 * Based on Empathy ubuntu-online-accounts:
 * Copyright (C) 2012 Collabora Ltd.
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "mcp-account-manager-ring.h"
#include <string.h>

#define PLUGIN_NAME "ring-account"
#define PLUGIN_PRIORITY (MCP_ACCOUNT_STORAGE_PLUGIN_PRIO_DEFAULT - 10)
#define PLUGIN_DESCRIPTION "Provide account for telepathy-ring"
#define PLUGIN_PROVIDER "im.telepathy.Account.Storage.Ring"

static void account_storage_iface_init(McpAccountStorageIface *iface);

G_DEFINE_TYPE_WITH_CODE (McpAccountManagerRing, mcp_account_manager_ring,
    G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (MCP_TYPE_ACCOUNT_STORAGE,
    account_storage_iface_init));

struct _McpAccountManagerRingPrivate
{
    gchar *account_name;
    GHashTable *params;
};

static void mcp_account_manager_ring_dispose(GObject *object)
{
    McpAccountManagerRing *self = (McpAccountManagerRing*) object;
    
    g_hash_table_unref(self->priv->params);

    G_OBJECT_CLASS (mcp_account_manager_ring_parent_class)->dispose(object);
}

static void mcp_account_manager_ring_init(McpAccountManagerRing *self)
{
    g_debug("MC Ring account plugin initialized");

    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, MCP_TYPE_ACCOUNT_MANAGER_RING,
            McpAccountManagerRingPrivate);
    self->priv->account_name = "ring/tel/account0";
    self->priv->params = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(self->priv->params, g_strdup("manager"), g_strdup("ring"));
    g_hash_table_insert(self->priv->params, g_strdup("protocol"), g_strdup("tel"));
    g_hash_table_insert(self->priv->params, g_strdup("DisplayName"), g_strdup("Cellular"));
    g_hash_table_insert(self->priv->params, g_strdup("Enabled"), g_strdup("true"));
    g_hash_table_insert(self->priv->params, g_strdup("ConnectAutomatically"), g_strdup("true"));
    g_hash_table_insert(self->priv->params, g_strdup("always_dispatch"), g_strdup("true"));
}

static void mcp_account_manager_ring_class_init(McpAccountManagerRingClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = mcp_account_manager_ring_dispose;

    g_type_class_add_private(gobject_class, sizeof(McpAccountManagerRingPrivate));
}

static GList *account_manager_ring_list(const McpAccountStorage *storage, const McpAccountManager *am)
{
    McpAccountManagerRing *self = (McpAccountManagerRing*) storage;
    GList *accounts = NULL;
    g_debug("%s", G_STRFUNC);
    accounts = g_list_prepend(accounts, g_strdup(self->priv->account_name));
    return accounts;
}

static gboolean account_manager_ring_get(const McpAccountStorage *storage, const McpAccountManager *am,
        const gchar *account_name, const gchar *key)
{
    McpAccountManagerRing *self = (McpAccountManagerRing*) storage;

    if (strcmp(account_name, self->priv->account_name))
        return FALSE;

    if (key == NULL) {
        GHashTableIter iter;
        gpointer itkey, value;
        g_hash_table_iter_init(&iter, self->priv->params);
        while (g_hash_table_iter_next(&iter, &itkey, &value)) {
            g_debug("%s: %s, %s %s", G_STRFUNC, account_name, (char*)itkey, (char*)value);
            mcp_account_manager_set_value(am, account_name, itkey, value);
        }
    } else {
        gchar *value = g_hash_table_lookup(self->priv->params, key);
        g_debug("%s: %s, %s %s", G_STRFUNC, account_name, (char*)key, (char*)value);
        mcp_account_manager_set_value(am, account_name, key, value);
    }

    return TRUE;
}

static gboolean account_manager_ring_set(const McpAccountStorage *storage, const McpAccountManager *am,
        const gchar *account_name, const gchar *key, const gchar *val)
{
    return FALSE;
}

static gchar *account_manager_ring_create(const McpAccountStorage *storage, const McpAccountManager *am,
        const gchar *cm_name, const gchar *protocol_name, GHashTable *params, GError **error)
{
    g_set_error(error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT, "Ring account manager cannot create accounts");
    return NULL;
}

static gboolean account_manager_ring_delete(const McpAccountStorage *storage, const McpAccountManager *am,
        const gchar *account_name, const gchar *key)
{
    g_debug("%s: %s, %s", G_STRFUNC, account_name, key);
    return FALSE;
}

static gboolean account_manager_ring_commit(const McpAccountStorage *storage, const McpAccountManager *am)
{
    g_debug("%s", G_STRFUNC);
    return FALSE;
}

static void account_manager_ring_get_identifier(const McpAccountStorage *storage, const gchar *account_name,
        GValue *identifier)
{
    McpAccountManagerRing *self = (McpAccountManagerRing*) storage;

    if (strcmp(account_name, self->priv->account_name))
        return;

    g_debug("%s: %s", G_STRFUNC, account_name);
    g_value_init(identifier, G_TYPE_UINT);
    g_value_set_uint(identifier, 0);
}

static guint account_manager_ring_get_restrictions(const McpAccountStorage *storage, const gchar *account_name)
{
    McpAccountManagerRing *self = (McpAccountManagerRing*) storage;

    if (strcmp(account_name, self->priv->account_name))
        return G_MAXUINT;

    return TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_PARAMETERS |
        TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_ENABLED |
        TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_PRESENCE |
        TP_STORAGE_RESTRICTION_FLAG_CANNOT_SET_SERVICE;
}

static void account_storage_iface_init(McpAccountStorageIface *iface)
{
    mcp_account_storage_iface_set_name(iface, PLUGIN_NAME);
    mcp_account_storage_iface_set_desc(iface, PLUGIN_DESCRIPTION);
    mcp_account_storage_iface_set_priority(iface, PLUGIN_PRIORITY);
    mcp_account_storage_iface_set_provider(iface, PLUGIN_PROVIDER);

#define IMPLEMENT(x) mcp_account_storage_iface_implement_##x(iface, account_manager_ring_##x)
    IMPLEMENT (get);
    IMPLEMENT (list);
    IMPLEMENT (set);
    IMPLEMENT (create);
    IMPLEMENT (delete);
    IMPLEMENT (commit);
    IMPLEMENT (get_identifier);
    IMPLEMENT (get_restrictions);
#undef IMPLEMENT
}

McpAccountManagerRing *mcp_account_manager_ring_new(void)
{
    return g_object_new(MCP_TYPE_ACCOUNT_MANAGER_RING, NULL);
}


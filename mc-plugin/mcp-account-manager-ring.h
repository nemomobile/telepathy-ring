#ifndef __MCP_ACCOUNT_MANAGER_RING_H__
#define __MCP_ACCOUNT_MANAGER_RING_H__

#include <mission-control-plugins/mission-control-plugins.h>

G_BEGIN_DECLS

#define MCP_TYPE_ACCOUNT_MANAGER_RING \
    (mcp_account_manager_ring_get_type ())
#define MCP_ACCOUNT_MANAGER_RING(o) \
    (G_TYPE_CHECK_INSTANCE_CAST ((o), MCP_TYPE_ACCOUNT_MANAGER_RING,   \
     McpAccountManagerUoa))

#define MCP_ACCOUNT_MANAGER_RING_CLASS(k)     \
    (G_TYPE_CHECK_CLASS_CAST((k), MCP_TYPE_ACCOUNT_MANAGER_RING, \
     McpAccountManagerRingClass))

#define MCP_IS_ACCOUNT_MANAGER_RING(o) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((o), MCP_TYPE_ACCOUNT_MANAGER_RING))

#define MCP_IS_ACCOUNT_MANAGER_RING_CLASS(k)  \
    (G_TYPE_CHECK_CLASS_TYPE ((k), MCP_TYPE_ACCOUNT_MANAGER_RING))

#define MCP_ACCOUNT_MANAGER_RING_GET_CLASS(o) \
    (G_TYPE_INSTANCE_GET_CLASS ((o), MCP_TYPE_ACCOUNT_MANAGER_RING, \
     McpAccountManagerRingClass))

typedef struct _McpAccountManagerRingPrivate McpAccountManagerRingPrivate;

typedef struct {
    GObject parent;

    McpAccountManagerRingPrivate *priv;
} _McpAccountManagerRing;

typedef struct {
      GObjectClass parent_class;
} _McpAccountManagerRingClass;

typedef _McpAccountManagerRing McpAccountManagerRing;
typedef _McpAccountManagerRingClass McpAccountManagerRingClass;

GType mcp_account_manager_ring_get_type (void) G_GNUC_CONST;

McpAccountManagerRing *mcp_account_manager_ring_new (void);

G_END_DECLS

#endif

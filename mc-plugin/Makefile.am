AM_CPPFLAGS = $(MCP_CFLAGS) $(ERROR_CFLAGS)

pluginsdir = $(MISSION_CONTROL_PLUGINS_DIR)
plugins_LTLIBRARIES = mcp-account-manager-ring.la

mcp_account_manager_ring_la_SOURCES = \
	mission-control-plugin.c \
	mcp-account-manager-ring.h \
	mcp-account-manager-ring.c

mcp_account_manager_ring_la_LIBADD = $(MCP_LIBS)
mcp_account_manager_ring_la_LDFLAGS = -shared -module -avoid-version


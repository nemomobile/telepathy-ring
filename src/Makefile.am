#
# Makefile.am for telepathy-ring/src
#
# Copyright (C) 2007 Nokia Corporation
# Contact: Pekka pessi <first.surname@nokia.com>
# See file COPYING.
#

# -----------------------------------------------------------------------------
# Automake options

# -----------------------------------------------------------------------------
# Headers and libraries

AM_CFLAGS = $(ERROR_CFLAGS) @GLIB_CFLAGS@ @DBUS_CFLAGS@ @TP_CFLAGS@
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/tests -I${top_builddir}
AM_LDFLAGS =

# Build targets

libexec_PROGRAMS = telepathy-ring
noinst_LTLIBRARIES = libtpring.la

# -----------------------------------------------------------------------------
# Tests

test_PROGRAMS = test-ring

TESTS = ${test_PROGRAMS}

test_ring_SOURCES = tests/test-ring.h tests/test-ring.c tests/test-ring-util.c

test_ring_LDADD = \
	libtpring.la $(TP_EXTLIB) \
	../modem/libmodem-glib.la \
	../tests/libtestcommon.la \
	@TP_LIBS@ @DBUS_LIBS@ @GLIB_LIBS@ \
	@CHECK_LIBS@ -lpthread

# -----------------------------------------------------------------------------
# Rules for building the targets

libtpring_la_SOURCES = \
    ring-connection-manager.h ring-connection-manager.c \
    ring-protocol.h ring-protocol.c \
    ring-connection.h ring-connection.c \
    ring-debug.h ring-debug.c \
    ring-text-manager.h ring-text-manager.c \
    ring-text-channel.h ring-text-channel.c \
    ring-media-manager.h ring-media-manager.c \
    ring-media-channel.h ring-media-channel.c \
    ring-call-channel.h ring-call-channel.c \
    ring-streamed-media-mixin.h ring-streamed-media-mixin.c \
    ring-member-channel.h ring-member-channel.c \
    ring-conference-manager.h ring-conference-manager.c \
    ring-conference-channel.h ring-conference-channel.c \
    ring-param-spec.h ring-param-spec.c \
    ring-emergency-service.h ring-emergency-service.c \
    ring-util.h ring-util.c

TP_EXTLIB =
TP_EXTLIB += $(top_builddir)/ring-extensions/libtpextensions.a

telepathy_ring_LDADD = \
	libtpring.la $(TP_EXTLIB) \
	../modem/libmodem-glib.la \
	@TP_LIBS@ @DBUS_LIBS@ @GLIB_LIBS@ \
	-lpthread

# ----------------------------------------------------------------------
# Install and distribution rules

managerdir = $(datadir)/telepathy/managers
dist_manager_DATA = ring.manager

service = org.freedesktop.Telepathy.ConnectionManager.ring.service

# configure script would insert ${prefix}/lib/telepathy-ring in service file
.service-in.service:
	@-rm -f $@
	$(AM_V_GEN)sed -e 's![@]libexecdir[@]!${libexecdir}!' '${srcdir}/$@-in' > $@

# Install services
servicedir = $(DBUS_SERVICES_DIR)

service_DATA = ${service}

${service} : Makefile

EXTRA_DIST = ${service}-in ${handlers}

CLEANFILES = ${service}

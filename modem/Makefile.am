#
# Makefile.am for modem libraries
#
# Copyright (C) 2007 Nokia Corporation
# Contact: Pekka Pessi <first.surname@nokia.com>
# See file COPYING.
#

SUBDIRS = . tests

# -----------------------------------------------------------------------------
# Headers and libraries

AM_CFLAGS = $(ERROR_CFLAGS) @GLIB_CFLAGS@ @DBUS_CFLAGS@ @TP_CFLAGS@ \
	    @UUID_CFLAGS@

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/modem

AM_LDFLAGS = -static

LIBADD = @TP_LIBS@ @DBUS_LIBS@ @GLIB_LIBS@ @UUID_LIBS@

# Build targets

lib_LTLIBRARIES = \
	libmodem-glib.la

noinst_LTLIBRARIES =

modemdir = ${includemodemdir}/modem

modem_HEADERS = request.h errors.h

libmodem_glib_la_SOURCES = request.c request-private.h \
	ofono.h ofono.c errors.c oface.h oface.c \
	service.c service.h modem.c modem.h \
	debug.h debug.c

nodist_libmodem_glib_la_SOURCES = $(BUILT_SOURCES)

libmodem_glib_la_LIBADD = ${LIBADD}

modem_HEADERS += call.h tones.h

libmodem_glib_la_SOURCES += call-service.c call.c tones.c

modem_HEADERS += sms.h sms-message.h

libmodem_glib_la_SOURCES += sms-service.c sms-message.c

modem_HEADERS += sim.h

libmodem_glib_la_SOURCES += sim-service.c

modem_HEADERS += radio-settings.h

libmodem_glib_la_SOURCES += radio-settings.c

# ----------------------------------------------------------------------

EXTRA_DIST = signals-marshal.list

BUILT_SOURCES = \
		signals-marshal.h \
		signals-marshal.c

signals-marshal.c: ${srcdir}/signals-marshal.list
	$(AM_V_GEN)glib-genmarshal --body --prefix=_modem__marshal $< >$@

signals-marshal.h: ${srcdir}/signals-marshal.list
	$(AM_V_GEN)glib-genmarshal --header --prefix=_modem__marshal $< >$@

CLEANFILES = ${BUILT_SOURCES}

# -----------------------------------------------------------------------------

#pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = modem.pc

# -----------------------------------------------------------------------------

# include Makefile-tests.am

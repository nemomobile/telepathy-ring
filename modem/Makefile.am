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
	    @UUID_CFLAGS@ -I$(top_srcdir) -I$(top_srcdir)/modem

AM_LDFLAGS =

LIBADD = @TP_LIBS@ @DBUS_LIBS@ @GLIB_LIBS@ @UUID_LIBS@

# Build targets

lib_LTLIBRARIES = \
	libmodem-glib.la \
	libmodem-call-glib.la \
	libmodem-sms-glib.la \
	libmodem-sim-glib.la

noinst_LTLIBRARIES =

modemdir = ${includemodemdir}/modem

modem_HEADERS = request.h errors.h

libmodem_glib_la_SOURCES = request.c request-private.h \
	ofono.h ofono.c errors.c \
	service.c service.h \
	debug.h debug.c

nodist_libmodem_glib_la_SOURCES = $(BUILT_SOURCES)

libmodem_glib_la_LIBADD = ${LIBADD}

modem_HEADERS += call.h tones.h

libmodem_call_glib_la_SOURCES = call-service.c call.c tones.c

libmodem_call_glib_la_LIBADD = libmodem-glib.la ${LIBADD}

modem_HEADERS += sms.h

libmodem_sms_glib_la_SOURCES = sms-service.c

libmodem_sms_glib_la_LIBADD = libmodem-glib.la ../sms-glib/libsms-glib.la ${LIBADD}

modem_HEADERS += sim.h

libmodem_sim_glib_la_SOURCES = sim-service.c

libmodem_sim_glib_la_LIBADD = libmodem-glib.la ${LIBADD}

# ----------------------------------------------------------------------

EXTRA_DIST = signals-marshal.list

BUILT_SOURCES = \
		signals-marshal.h \
		signals-marshal.c

signals-marshal.c: ${srcdir}/signals-marshal.list
	glib-genmarshal --body --prefix=_modem__marshal $< >$@

signals-marshal.h: ${srcdir}/signals-marshal.list
	glib-genmarshal --header --prefix=_modem__marshal $< >$@

CLEANFILES = ${BUILT_SOURCES}

# -----------------------------------------------------------------------------

#pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = modem.pc

# -----------------------------------------------------------------------------

# include Makefile-tests.am

noinst_LTLIBRARIES += libtest-modem-sms.la

libtest_modem_sms_la_SOURCES = \
	$(libmodem_sms_la_SOURCES) \
	$(nodist_libmodem_sms_glib_la_SOURCES)

libtest_modem_sms_la_LIBADD = libmodem-glib.la

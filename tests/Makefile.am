SUBDIRS = twisted

AM_CFLAGS = $(ERROR_CFLAGS) \
	@GLIB_CFLAGS@ @DBUS_CFLAGS@ \
	-I$(top_srcdir) @CHECK_CFLAGS@

dist_noinst_SCRIPTS = ring-monitor

noinst_LTLIBRARIES = libtestcommon.la

libtestcommon_la_SOURCES = \
    test-common.c test-common.h

tools_dir = $(top_srcdir)/tools

MOSTLYCLEANFILES = $(BUILT_SOURCES)

test_DATA = tests.xml

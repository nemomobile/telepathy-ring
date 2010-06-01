AM_CFLAGS = $(ERROR_CFLAGS) -I$(top_srcdir) \
            @GLIB_CFLAGS@

AM_CFLAGS += -DENABLE_DEBUG=1

lib_LTLIBRARIES = libsms-glib.la

sms_g_includedir = $(includesmsdir)/sms-glib

sms_g_include_HEADERS = \
	message.h deliver.h status-report.h submit.h \
	enums.h utils.h errors.h

libsms_glib_la_SOURCES = \
	message.c deliver.c status-report.c submit.c utils.c \
	errors.c param-spec.h param-spec.c debug.h debug.c

libsms_glib_la_LIBADD = @GLIB_LIBS@

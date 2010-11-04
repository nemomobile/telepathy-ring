AM_CFLAGS = $(ERROR_CFLAGS) \
	@TP_CFLAGS@ @GLIB_CFLAGS@ @DBUS_CFLAGS@ @CHECK_CFLAGS@

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/tests -I$(top_srcdir)/modem -I..

TESTS = test-modem

test_PROGRAMS = $(TESTS)

test_modem_SOURCES = \
		test-modem.c test-modem.h \
		test-modem-call-service.c \
		test-modem-call.c \
		test-modem-tones.c \
		test-sim.c \
		test-modem-request.c \
		base.h base.c derived.h derived.c
#		test-modem-sms.c

test_modem_LDADD = \
	../libmodem-glib.la \
	../../sms-glib/libsms-glib.la \
	../../tests/libtestcommon.la \
	@TP_LIBS@ @DBUS_LIBS@ @GLIB_LIBS@ \
	@CHECK_LIBS@
	-lpthread

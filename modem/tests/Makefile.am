AM_CFLAGS = $(ERROR_CFLAGS) \
	@GLIB_CFLAGS@ @DBUS_CFLAGS@ \
	-I$(top_srcdir) -I$(top_srcdir)/tests @CHECK_CFLAGS@

TESTS = test-modem

test_PROGRAMS = $(TESTS)

test_modem_SOURCES = \
		test-modem.c \
		test-modem-call-service.c \
		test-modem-call.c \
		test-modem-tones.c \
		test-sim.c \
		test-modem-request.c \
		base.h base.c derived.h derived.c
#		test-modem-sms.c

test_modem_LDADD = ../libmodem-call-glib.la \
	../libmodem-sim-glib.la \
	../libtest-modem-sms.la \
	../../tests/libtestcommon.la \
	@TP_LIBS@ @DBUS_LIBS@ @GLIB_LIBS@ \
	@CHECK_LIBS@
	-lpthread

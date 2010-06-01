#
# Makefile.am for telepathy-ring
#
# Copyright (C) 2007 Nokia Corporation
# Contact: Pekka Pessi <first.surname@nokia.com>
# See file COPYING.
#

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = tests ring-extensions sms-glib modem src docs tools scripts

TAGS:
	find src sms-glib call ring-extensions -name '*.[hc]' | xargs etags

.PHONY: TAGS

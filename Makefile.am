#
# Makefile.am for telepathy-ring
#
# Copyright (C) 2007 Nokia Corporation
# Contact: Pekka Pessi <first.surname@nokia.com>
# See file COPYING.
#

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = tests ring-extensions modem src docs tools scripts mc-plugin

TAGS:
	find src modem ring-extensions -name '*.[hc]' | \
	xargs etags

EXTRA_DIST = \
    autogen.sh

.PHONY: TAGS

tools_dir = $(top_srcdir)/tools

EXTRA_DIST = all.xml.in $(EXT_IFACES)

EXT_IFACES = \
    $(srcdir)/Channel_Future.xml \
    $(srcdir)/Channel_Interface_Splittable.xml \
    $(srcdir)/Channel_Interface_Conference_DRAFT.xml \
    $(srcdir)/Channel_Interface_Mergeable_Conference.xml \
    $(srcdir)/Channel_Interface_Service_Point.xml \
    $(srcdir)/Connection_Interface_Service_Point.xml

NOT_IFACES = \
    $(srcdir)/Channel_Interface_Messages.xml \
    $(srcdir)/Channel_Interface_Dialstrings.xml

if HAVE_TP_EXTENSIONS

all.xml: all.xml.in Makefile.am
	$(SHELL) $(srcdir)/all.xml.in $(EXT_IFACES) > $@

noinst_LIBRARIES = libtpextensions.a

libtpextensions_a_SOURCES = \
    ring-extensions.h \
    gtypes.h \
    body.c

#    message-mixin.h message-mixin.c

nodist_libtpextensions_a_SOURCES = \
    _gen/signals-marshal.c \
    _gen/signals-marshal.h \
    _gen/signals-marshal.list \
    _gen/enums.h \
    _gen/gtypes.h \
    _gen/interfaces.h \
    _gen/interfaces-body.h \
    _gen/svc.h _gen/svc.c

BUILT_SOURCES = \
    all.xml \
    _gen/all.xml \
    $(nodist_libtpextensions_a_SOURCES)

# extensions.html

CLEANFILES = $(BUILT_SOURCES)

AM_CFLAGS = $(ERROR_CFLAGS) @DBUS_CFLAGS@ @GLIB_CFLAGS@ @TP_CFLAGS@
AM_LDFLAGS = @TP_LIBS@ @DBUS_LIBS@ @GLIB_LIBS@

# Generated stuff

DROP_NAMESPACE = sed -e 's@xmlns:tp="http://telepathy\.freedesktop\.org/wiki/DbusSpec.extensions-v0"@@g'
XSLTPROCFLAGS = --nonet --novalid

_gen/all.xml: all.xml $(EXT_IFACES) Makefile.am
	$(mkdir_p) _gen
	$(XSLTPROC) $(XSLTPROCFLAGS) --xinclude $(tools_dir)/identity.xsl all.xml > $@

extensions.html: all.xml $(tools_dir)/doc-generator.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		$(tools_dir)/doc-generator.xsl all.xml > $@

_gen/svc.c _gen/svc.h: _gen/all.xml $(tools_dir)/glib-ginterface-gen.py
	$(PYTHON) $(tools_dir)/glib-ginterface-gen.py \
		--filename=_gen/svc \
		--signal-marshal-prefix=ring_tp \
		--include='<telepathy-glib/dbus.h>' \
		--include='"_gen/signals-marshal.h"' \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		--allow-unstable \
		_gen/all.xml Ring_Svc_

_gen/signals-marshal.list: _gen/all.xml $(tools_dir)/glib-signals-marshal-gen.py
	$(PYTHON) $(tools_dir)/glib-signals-marshal-gen.py _gen/all.xml > $@

_gen/signals-marshal.h: _gen/signals-marshal.list
	$(GLIB_GENMARSHAL) --header --prefix=ring_tp_marshal $< > $@

_gen/signals-marshal.c: _gen/signals-marshal.list
	$(GLIB_GENMARSHAL) --body --prefix=ring_tp_marshal $< > $@

_gen/enums.h: _gen/all.xml $(tools_dir)/c-constants-gen.py
	$(PYTHON) $(tools_dir)/c-constants-gen.py Tp $< > $@

_gen/gtypes.h _gen/gtypes-body.h: _gen/all.xml \
	$(tools_dir)/glib-gtypes-generator.py Makefile.am
	$(PYTHON) $(tools_dir)/glib-gtypes-generator.py \
		$< _gen/gtypes Ring

_gen/interfaces-body.h _gen/interfaces.h: _gen/all.xml \
	$(tools_dir)/glib-interfaces-gen.py Makefile.am
	$(PYTHON) $(tools_dir)/glib-interfaces-gen.py \
		Ring _gen/interfaces-body.h _gen/interfaces.h $<

endif

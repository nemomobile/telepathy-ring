abs_top_builddir = @abs_top_builddir@

noinst_SCRIPTS = telepathy-glib-env

telepathy-glib-env: telepathy-glib-env.in Makefile
	$(AM_V_GEN)sed -e 's![@]abs_top_builddir[@]!$(abs_top_builddir)!' $< > $@
	$(AM_V_at)chmod +x $@

EXTRA_DIST = \
    c-constants-gen.py \
    check-coding-style.mk \
    check-c-style.sh \
    check-misc.sh \
    check-whitespace.sh \
    doc-generator.xsl \
    glib-client-gen.py \
    glib-client-marshaller-gen.py \
    glib-errors-enum-body-gen.py \
    glib-errors-enum-header-gen.py \
    glib-ginterface-gen.py \
    glib-gtypes-generator.py \
    glib-interfaces-gen.py \
    glib-signals-marshal-gen.py \
    gobject-foo.py \
    identity.xsl \
    libtpcodegen.py \
    libglibcodegen.py \
    make-version-script.py \
    telepathy-glib-env.in \
    with-session-bus.sh

CLEANFILES = libtpcodegen.pyc libtpcodegen.pyo libglibcodegen.pyc libglibcodegen.pyo $(noinst_SCRIPTS)

all: $(EXTRA_DIST)

libglibcodegen.py: libtpcodegen.py
	touch $@
c-constants-gen.py: libglibcodegen.py
	touch $@
glib-client-marshaller-gen.py: libglibcodegen.py
	touch $@
glib-errors-enum-body-gen.py: libglibcodegen.py
	touch $@
glib-errors-enum-header-gen.py: libglibcodegen.py
	touch $@
glib-ginterface-gen.py: libglibcodegen.py
	touch $@
glib-gtypes-generator.py: libglibcodegen.py
	touch $@
glib-interfaces-gen.py: libglibcodegen.py
	touch $@
glib-signals-marshal-gen.py: libglibcodegen.py
	touch $@

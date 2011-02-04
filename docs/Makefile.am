EXTRA_DIST = design.txt DTMF-playback

# Man page
man_MANS = telepathy-ring.8
CLEANFILES = $(man_MANS)
EXTRA_DIST += telepathy-ring.8.in

%.8: %.8.in Makefile
	sed -e 's,[@]libexecdir[@],@libexecdir@,' < $< > $@

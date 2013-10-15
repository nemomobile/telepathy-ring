TWISTED_TESTS = \
		cm/protocol.py \
		connect/connect-success.py \
		$(NULL)

config.py: Makefile
	$(AM_V_GEN) { \
		echo "PACKAGE_STRING = \"$(PACKAGE_STRING)\""; \
	} > $@

if WANT_TWISTED_TESTS

check-local: check-twisted

CHECK_TWISTED_SLEEP=0

check-twisted: $(BUILT_SOURCES)
	$(MAKE) -C tools
	if test "x$(CHECK_TWISTED_SLEEP)" = x0; then \
		ring_test_sleep= ; \
	else \
		ring_test_sleep=--sleep=$(CHECK_TWISTED_SLEEP); \
	fi; \
	RING_TEST_UNINSTALLED=1 \
	  RING_ABS_TOP_SRCDIR=@abs_top_srcdir@ \
	  RING_ABS_TOP_BUILDDIR=@abs_top_builddir@ \
	  RING_TEST_SLEEP=$$ring_test_sleep \
	  ./run-test.sh "$(TWISTED_TESTS)"

ring-twisted-tests.list: Makefile
	$(AM_V_GEN)echo $(TWISTED_TESTS) > $@

BUILT_SOURCES = \
	config.py \
	ring-twisted-tests.list \
	run-test.sh \
	$(NULL)

# We don't really use ringtestsdir yet - we only support uninstalled testing
# so far - but I'm substituting it to keep the script more similar to Gabble's.
# ${pkglibexecdir}/tests is what GNOME's InstalledTests goal recommends.
run-test.sh: run-test.sh.in Makefile
	$(AM_V_GEN)sed \
			-e 's![@]ringtestsdir[@]!${pkglibexecdir}/tests!' \
			-e 's![@]TEST_PYTHON[@]!$(TEST_PYTHON)!' \
			< $< > $@.tmp && \
		chmod +x $@.tmp && \
		mv $@.tmp $@

endif

EXTRA_DIST = \
	     $(TWISTED_TESTS) \
	     run-test.sh.in \
	     servicetest.py \
	     ringtest.py \
	     constants.py \
	     $(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	ring-[1-9]*.log \
	*.pyc \
	*/*.pyc \
	$(NULL)

SUBDIRS = tools

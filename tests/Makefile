# busybox/tests/Makefile - Run through all defined tests.
# ------------------------
# Copyright (C) 2000  Karl M. Hegbloom <karlheg@debian.org>  GPL

all:: message_header

message_header:
	@echo
	@echo BusyBox Test Suite.
	@echo
	(cd ..; tests/busybox.REGRESS.sh)

clean::

distclean: clean

.PHONY: all clean distclean message_header

include $(wildcard *_tests.mk)

BBL := $(shell pushd .. >/dev/null &&		\
	         ${MAKE} busybox.links >/dev/null && \
	       popd >/dev/null &&		\
	       cat ../busybox.links |		\
	         sed -e 's,.*/\(.*\)$$,\1,')

../busybox:
	cd .. && ${MAKE} busybox

${BBL}: ../busybox
	rm -f $@
	ln ../busybox $@

syslog_test: syslog_test.c

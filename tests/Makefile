all test_all: message_header cp_tests mv_tests ln_tests

clean: cp_clean mv_clean ln_clean

message_header:
	@echo
	@echo If tests faile due to differences in timestamps in commands that are not set
	@echo to preserve timestamps, just run the tests again.
	@echo

include cp_tests.mk
include mv_tests.mk
include ln_tests.mk

BBL := $(shell pushd .. >/dev/null &&		\
	         ${MAKE} busybox.links >/dev/null && \
	       popd >/dev/null &&		\
	       cat ../busybox.links |		\
	         sed -e 's,.*/\(.*\)$$,\1,')

../busybox:
	cd .. && ${MAKE} busybox

$(BBL): ../busybox
	rm -f $@
	ln ../busybox $@

.PHONY: all test_all message_header

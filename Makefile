# Makefile for busybox
#
# Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#

#--------------------------------------------------------------
# You shouldn't need to mess with anything beyond this point...
#--------------------------------------------------------------
noconfig_targets := menuconfig config oldconfig randconfig \
	defconfig allyesconfig allnoconfig clean distclean \
	release tags
TOPDIR=./
include Rules.mak

DIRS:=applets archival archival/libunarchive coreutils console-tools \
	debianutils editors findutils init miscutils modutils networking \
	networking/libiproute networking/udhcp procps loginutils shell \
	sysklogd util-linux libpwdgrp coreutils/libcoreutils libbb

ifeq ($(strip $(CONFIG_SELINUX)),y)
CFLAGS += -I/usr/include/selinux
LIBRARIES += -lsecure
endif

CONFIG_CONFIG_IN = sysdeps/$(TARGET_OS)/Config.in
CONFIG_DEFCONFIG = sysdeps/$(TARGET_OS)/defconfig

ifeq ($(strip $(HAVE_DOT_CONFIG)),y)

all: busybox busybox.links doc

# In this section, we need .config
-include .config.cmd
include $(patsubst %,%/Makefile.in, $(DIRS))
-include $(TOPDIR).depend

busybox: .depend include/config.h $(libraries-y)
	$(CC) $(LDFLAGS) -o $@ -Wl,--start-group $(libraries-y) $(LIBRARIES) -Wl,--end-group
	$(STRIPCMD) $@

busybox.links: applets/busybox.mkll include/config.h
	- $(SHELL) $^ >$@

install: applets/install.sh busybox busybox.links
	$(SHELL) $< $(PREFIX)

uninstall: busybox.links
	rm -f $(PREFIX)/bin/busybox
	for i in `cat busybox.links` ; do rm -f $(PREFIX)$$i; done

install-hardlinks: applets/install.sh busybox busybox.links
	$(SHELL) $< $(PREFIX) --hardlinks


# Documentation Targets
doc: docs/busybox.pod docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

docs/busybox.pod : docs/busybox_header.pod include/usage.h docs/busybox_footer.pod
	- ( cat docs/busybox_header.pod; \
	    docs/autodocifier.pl include/usage.h; \
	    cat docs/busybox_footer.pod ) > docs/busybox.pod

docs/BusyBox.txt: docs/busybox.pod
	@echo
	@echo BusyBox Documentation
	@echo
	-mkdir -p docs
	-pod2text $< > $@

docs/BusyBox.1: docs/busybox.pod
	- mkdir -p docs
	- pod2man --center=BusyBox --release="version $(VERSION)" \
		$< > $@

docs/BusyBox.html: docs/busybox.net/BusyBox.html
	- mkdir -p docs
	-@ rm -f docs/BusyBox.html
	-@ ln -s busybox.net/BusyBox.html docs/BusyBox.html

docs/busybox.net/BusyBox.html: docs/busybox.pod
	-@ mkdir -p docs/busybox.net
	-  pod2html --noindex $< > \
	    docs/busybox.net/BusyBox.html
	-@ rm -f pod2htm*

# The nifty new buildsystem stuff
scripts/mkdep: scripts/mkdep.c
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/mkdep scripts/mkdep.c

scripts/split-include: scripts/split-include.c
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/split-include scripts/split-include.c

.depend: scripts/mkdep
	rm -f .depend .hdepend;
	mkdir -p include/config;
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/mkdep scripts/mkdep.c
	scripts/mkdep -I include -- \
		`find -name \*.c -print | sed -e "s,^./,,"` >> .depend;
	scripts/mkdep -I include -- \
		`find -name \*.h -print | sed -e "s,^./,,"` >> .hdepend;

depend dep: include/config.h .depend

include/config/MARKER: depend scripts/split-include
	scripts/split-include include/config.h include/config
	@ touch include/config/MARKER

include/config.h: .config
	@if [ ! -x ./scripts/config/conf ] ; then \
	    $(MAKE) -C scripts/config conf; \
	fi;
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

finished2:
	@echo
	@echo Finished installing...
	@echo

else # ifeq ($(strip $(HAVE_DOT_CONFIG)),y)

all: menuconfig

# configuration
# ---------------------------------------------------------------------------

scripts/config/conf:
	$(MAKE) -C scripts/config conf
	-@if [ ! -f .config ] ; then \
		cp $(CONFIG_DEFCONFIG) .config; \
	fi
scripts/config/mconf:
	$(MAKE) -C scripts/config ncurses conf mconf
	-@if [ ! -f .config ] ; then \
		cp $(CONFIG_DEFCONFIG) .config; \
	fi

menuconfig: scripts/config/mconf
	@./scripts/config/mconf $(CONFIG_CONFIG_IN)

config: scripts/config/conf
	@./scripts/config/conf $(CONFIG_CONFIG_IN)

oldconfig: scripts/config/conf
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

randconfig: scripts/config/conf
	@./scripts/config/conf -r $(CONFIG_CONFIG_IN)

allyesconfig: scripts/config/conf
	@./scripts/config/conf -y $(CONFIG_CONFIG_IN)
	sed -i -e "s/^CONFIG_DEBUG.*/# CONFIG_DEBUG is not set/" .config
	sed -i -e "s/^USING_CROSS_COMPILER.*/# USING_CROSS_COMPILER is not set/" .config
	sed -i -e "s/^CONFIG_STATIC.*/# CONFIG_STATIC is not set/" .config
	sed -i -e "s/^CONFIG_SELINUX.*/# CONFIG_SELINUX is not set/" .config
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

allnoconfig: scripts/config/conf
	@./scripts/config/conf -n $(CONFIG_CONFIG_IN)

defconfig: scripts/config/conf
	@./scripts/config/conf -d $(CONFIG_CONFIG_IN)

test tests: busybox
	# Note that 'tests' is depricated.  Use 'make check' instead
	# To use the nice new testsuite....
	cd tests && ./tester.sh

check: busybox
	cd testsuite && ./runtest

clean:
	- $(MAKE) -C tests clean
	- rm -f docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pod docs/busybox.net/busybox.html \
	    docs/busybox pod2htm* *.gdb *.elf *~ core .*config.log \
	    docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.net/BusyBox.html busybox.links libbb/loop.h \
	    .config.old .hdepend busybox
	- rm -rf _install
	- find . -name .\*.flags -exec rm -f {} \;
	- find . -name \*.o -exec rm -f {} \;
	- find . -name \*.a -exec rm -f {} \;

distclean: clean
	- rm -f scripts/split-include scripts/mkdep
	- rm -rf include/config include/config.h
	- find . -name .depend -exec rm -f {} \;
	rm -f .config .config.old .config.cmd
	- $(MAKE) -C scripts/config clean

release: distclean #doc
	cd ..;					\
	rm -rf $(PROG)-$(VERSION);		\
	cp -a busybox $(PROG)-$(VERSION);	\
						\
	find $(PROG)-$(VERSION)/ -type d	\
				 -name CVS	\
				 -print		\
		-exec rm -rf {} \; ;            \
						\
	find $(PROG)-$(VERSION)/ -type f	\
				 -name .\#*	\
				 -print		\
		-exec rm -f {} \;  ;            \
						\
	tar -cvzf $(PROG)-$(VERSION).tar.gz $(PROG)-$(VERSION)/;

tags:
	ctags -R .


endif # ifeq ($(strip $(HAVE_DOT_CONFIG)),y)

.PHONY: dummy subdirs release distclean clean config oldconfig \
	menuconfig tags check test tests depend



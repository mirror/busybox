# Makefile for busybox
#
# Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
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
	sysklogd util-linux libbb libpwdgrp coreutils/libcoreutils

ifeq ($(strip $(CONFIG_SELINUX)),y)
CFLAGS += -I/usr/include/selinux
LIBRARIES += -lsecure
endif

ifeq ($(strip $(HAVE_DOT_CONFIG)),y)

all: busybox busybox.links #doc

# In this section, we need .config
-include .config.cmd
include $(patsubst %,%/Makefile.in, $(DIRS))

busybox: .depend include/config.h $(libraries-y)
	$(CC) $(LDFLAGS) -o $@ $(libraries-y) $(LIBRARIES)
	$(STRIPCMD) $@

busybox.links: applets/busybox.mkll include/config.h
	- $(SHELL) $^ >$@

install: applets/install.sh busybox busybox.links
	$(SHELL) $< $(PREFIX)

uninstall: busybox busybox.links
	for i in `cat busybox.links` ; do rm -f $$PREFIX$$i; done

install-hardlinks: applets/install.sh busybox busybox.links
	$(SHELL) $< $(PREFIX) --hardlinks


# Documentation Targets
doc: olddoc

# Old Docs...
olddoc: docs/busybox.pod docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

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


# New docs based on DOCBOOK SGML
newdoc: docs/busybox.txt docs/busybox.pdf docs/busybox/busyboxdocumentation.html

docs/busybox.txt: docs/busybox.sgml
	@echo
	@echo BusyBox Documentation
	@echo
	- mkdir -p docs
	(cd docs; sgmltools -b txt busybox.sgml)

docs/busybox.dvi: docs/busybox.sgml
	- mkdir -p docs
	(cd docs; sgmltools -b dvi busybox.sgml)

docs/busybox.ps: docs/busybox.sgml
	- mkdir -p docs
	(cd docs; sgmltools -b ps busybox.sgml)

docs/busybox.pdf: docs/busybox.ps
	- mkdir -p docs
	(cd docs; ps2pdf busybox.ps)

docs/busybox/busyboxdocumentation.html: docs/busybox.sgml
	- mkdir -p docs
	(cd docs/busybox.net; sgmltools -b html ../busybox.sgml)

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
		`find . -name \*.c -print` >> .depend;
	scripts/mkdep -I include -- \
		`find . -name \*.h -print` >> .hdepend;
	$(MAKE) $(patsubst %,_sfdep_%,$(DIRS)) _FASTDEP_ALL_SUB_DIRS="$(DIRS)" ;

depend dep: include/config.h .depend

include/config/MARKER: depend scripts/split-include
	scripts/split-include include/config.h include/config
	@ touch include/config/MARKER

include/config.h: .config
	@if [ ! -x ./scripts/config/conf ] ; then \
	    make -C scripts/config conf; \
	fi;
	@./scripts/config/conf -o sysdeps/$(TARGET_OS)/Config.in

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
	make -C scripts/config conf
	-@if [ ! -f .config ] ; then \
		cp sysdeps/$(TARGET_OS)/defconfig .config; \
	fi
scripts/config/mconf:
	make -C scripts/config ncurses conf mconf
	-@if [ ! -f .config ] ; then \
		cp sysdeps/$(TARGET_OS)/defconfig .config; \
	fi

menuconfig: scripts/config/mconf
	@./scripts/config/mconf sysdeps/$(TARGET_OS)/Config.in

config: scripts/config/conf
	@./scripts/config/conf sysdeps/$(TARGET_OS)/Config.in

oldconfig: scripts/config/conf
	@./scripts/config/conf -o sysdeps/$(TARGET_OS)/Config.in

randconfig: scripts/config/conf
	@./scripts/config/conf -r sysdeps/$(TARGET_OS)/Config.in

allyesconfig: scripts/config/conf
	@./scripts/config/conf -y sysdeps/$(TARGET_OS)/Config.in

allnoconfig: scripts/config/conf
	@./scripts/config/conf -n sysdeps/$(TARGET_OS)/Config.in

defconfig: scripts/config/conf
	@./scripts/config/conf -d sysdeps/$(TARGET_OS)/Config.in

test tests: busybox
	# Note that 'tests' is depricated.  Use 'make check' instead
	# To use the nice new testsuite....
	cd tests && ./tester.sh

check: busybox
	cd testsuite && ./runtest

clean:
	- $(MAKE) -C tests clean
	- rm -f docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.net/BusyBox.html
	- rm -f docs/busybox.txt docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pdf docs/busybox.pod docs/busybox.net/busybox.html \
	    docs/busybox _install pod2htm* *.gdb *.elf *~ core
	- rm -f busybox busybox.links libbb/loop.h .config.old .hdepend
	- rm -f .*config.log
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
	rm -rf busybox-$(VERSION);		\
	cp -a busybox busybox-$(VERSION);	\
						\
	find busybox-$(VERSION)/ -type d	\
				 -name CVS	\
				 -print		\
		-exec rm -rf {} \; ;            \
						\
	find busybox-$(VERSION)/ -type f	\
				 -name .\#*	\
				 -print		\
		-exec rm -f {} \;  ;            \
						\
	tar -cvzf busybox-$(VERSION).tar.gz busybox-$(VERSION)/;

tags:
	ctags -R .


endif # ifeq ($(strip $(HAVE_DOT_CONFIG)),y)

.PHONY: dummy subdirs release distclean clean config oldconfig \
	menuconfig tags check test tests depend



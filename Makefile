# Makefile for busybox
#
# Copyright (C) 1999-2002 Erik Andersen <andersee@debian.org>
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

TOPDIR:= $(shell /bin/pwd)/
include $(TOPDIR).config
include $(TOPDIR)Rules.mak
SUBDIRS:=applets archival archival/libunarchive console-tools \
	editors fileutils findutils init miscutils modutils networking \
	procps loginutils shell shellutils sysklogd \
	textutils util-linux libbb libpwdgrp

all:    do-it-all

#
# Make "config" the default target if there is no configuration file or
# "depend" the target if there is no top-level dependency information.
ifeq (.config,$(wildcard .config))
include .config
ifeq (.depend,$(wildcard .depend))
include .depend 
do-it-all:      busybox busybox.links #doc
include $(patsubst %,%/Makefile.in, $(SUBDIRS))
else
CONFIGURATION = depend
do-it-all:      depend
endif
else
CONFIGURATION = menuconfig
do-it-all:      menuconfig
endif


busybox: depend $(libraries-y)
	$(CC) $(LDFLAGS) -o $@ $(libraries-y) $(LIBRARIES)
	$(STRIPCMD) $@

busybox.links: applets/busybox.mkll
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
$(TOPDIR)scripts/mkdep: scripts/mkdep.c
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/mkdep scripts/mkdep.c

$(TOPDIR)scripts/split-include: scripts/split-include.c
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/split-include scripts/split-include.c

$(TOPDIR).depend: $(TOPDIR)scripts/mkdep
	rm -f .depend .hdepend;
	mkdir -p $(TOPDIR)include/config;
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/mkdep scripts/mkdep.c
	scripts/mkdep -I $(TOPDIR)include -- \
		`find $(TOPDIR) -name \*.c -print` >> .depend;
	scripts/mkdep -I $(TOPDIR)include -- \
		`find $(TOPDIR) -name \*.h -print` >> .hdepend;
	$(MAKE) $(patsubst %,_sfdep_%,$(SUBDIRS)) _FASTDEP_ALL_SUB_DIRS="$(SUBDIRS)" ;
	@ echo -e "\n\nNow run 'make' to build BusyBox\n\n"

depend dep: $(TOPDIR)include/config.h $(TOPDIR).depend

BB_SHELL := ${shell if [ -x "$$BASH" ]; then echo $$BASH; \
	else if [ -x /bin/bash ]; then echo /bin/bash; \
	else echo sh; fi ; fi}

include/config/MARKER: depend $(TOPDIR)scripts/split-include
	scripts/split-include include/config.h include/config
	@ touch include/config/MARKER

$(TOPDIR)include/config.h:
	@if [ ! -f $(TOPDIR)include/config.h ] ; then \
		make oldconfig; \
	fi;

$(TOPDIR).config:
	@if [ ! -f $(TOPDIR).config ] ; then \
	    cp $(TOPDIR)sysdeps/$(TARGET_OS)/defconfig $(TOPDIR).config; \
	fi;

menuconfig: $(TOPDIR).config
	mkdir -p $(TOPDIR)include/config
	$(MAKE) -C scripts/lxdialog all
	$(BB_SHELL) scripts/Menuconfig sysdeps/$(TARGET_OS)/config.in

config: $(TOPDIR).config
	mkdir -p $(TOPDIR)include/config
	$(BB_SHELL) scripts/Configure sysdeps/$(TARGET_OS)/config.in

oldconfig: $(TOPDIR).config
	mkdir -p $(TOPDIR)include/config
	$(BB_SHELL) scripts/Configure -d sysdeps/$(TARGET_OS)/config.in


ifdef CONFIGURATION
..$(CONFIGURATION):
	@echo
	@echo "You have a bad or nonexistent" .$(CONFIGURATION) ": running 'make" $(CONFIGURATION)"'"
	@echo
	$(MAKE) $(CONFIGURATION)
	@echo
	@echo "Successful. Try re-making (ignore the error that follows)"
	@echo
	exit 1

dummy:

else

dummy:

endif


%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<


# Testing...
test tests:
	# old way of doing it
	#cd tests && $(MAKE) all
	# new way of doing it
	cd tests && ./tester.sh

# Cleanup
clean:
	- $(MAKE) -C tests clean
	- $(MAKE) -C scripts/lxdialog clean
	- rm -f docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.net/BusyBox.html
	- rm -f docs/busybox.txt docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pdf docs/busybox.net/busybox.html \
	    docs/busybox _install pod2htm* *.gdb *.elf *~ core
	- rm -f busybox busybox.links libbb/loop.h .config.old .hdepend
	- rm -f scripts/split-include scripts/mkdep .*config.log
	- rm -rf include/config include/config.h
	- find . -name .\*.flags -exec rm -f {} \;   
	- find . -name .depend -exec rm -f {} \;
	- find . -name \*.o -exec rm -f {} \;
	- find . -name \*.a -exec rm -f {} \;

distclean: clean
	- rm -f busybox 
	- cd tests && $(MAKE) distclean

dist release: distclean doc
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



.PHONY: tags check depend

tags:
	ctags -R .

check: busybox
	cd testsuite && ./runtest


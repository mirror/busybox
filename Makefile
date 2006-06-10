# Makefile for busybox
#
# Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
#
# Licensed under GPLv2, see the file LICENSE in this tarball for details.
#

# You shouldn't have to edit anything in this file for configuration
# purposes, try "make help" or read http://busybox.net/FAQ.html.

.PHONY: dummy subdirs release distclean clean config oldconfig menuconfig \
        tags check test depend dep buildtree hosttools _all checkhelp \
        sizes bloatcheck baseline objsizes

noconfig_targets := menuconfig config oldconfig randconfig hosttools \
	defconfig allyesconfig allnoconfig allbareconfig \
	clean distclean help \
	release tags

nocheck_targets := clean distclean help release tags

# the toplevel sourcedir
ifndef top_srcdir
top_srcdir=$(CURDIR)
endif
# toplevel directory of the object-tree
ifndef top_builddir
top_builddir=$(CURDIR)
endif

export srctree=$(top_srcdir)
vpath %/Config.in $(srctree)

DIRS:=applets archival archival/libunarchive coreutils console-tools \
	debianutils editors findutils init miscutils modutils networking \
	networking/libiproute networking/udhcp procps loginutils shell \
	sysklogd util-linux e2fsprogs libpwdgrp coreutils/libcoreutils libbb

SRC_DIRS:=$(patsubst %,$(top_srcdir)/%,$(DIRS))

# That's our default target when none is given on the command line
_all:

CONFIG_CONFIG_IN = $(top_srcdir)/Config.in

ifeq ($(KBUILD_SRC),)

ifdef O
  ifeq ("$(origin O)", "command line")
    KBUILD_OUTPUT := $(O)
    top_builddir := $(O)
  endif
else
# If no alternate output-dir was specified, we build in cwd
# We are using KBUILD_OUTPUT nevertheless to make sure that we create
# Rules.mak and the toplevel Makefile, in case they don't exist.
  KBUILD_OUTPUT := $(top_builddir)
endif

ifneq ($(strip $(HAVE_DOT_CONFIG)),y)
# pull in settings early
-include $(top_srcdir)/Rules.mak
endif

# All object directories.
OBJ_DIRS := $(DIRS)
all_tree := $(patsubst %,$(top_builddir)/%,$(OBJ_DIRS) scripts scripts/config include)
all_tree: $(all_tree)
$(all_tree):
	@mkdir -p "$@"

ifneq ($(KBUILD_OUTPUT),)
# Invoke a second make in the output directory, passing relevant variables
# Check that the output directory actually exists
saved-output := $(KBUILD_OUTPUT)
KBUILD_OUTPUT := $(shell cd $(KBUILD_OUTPUT) && /bin/pwd)
$(if $(wildcard $(KBUILD_OUTPUT)),, \
     $(error output directory "$(saved-output)" does not exist))

.PHONY: $(MAKECMDGOALS)

$(filter-out _all,$(MAKECMDGOALS)) _all: $(KBUILD_OUTPUT)/Rules.mak $(KBUILD_OUTPUT)/Makefile all_tree
	$(Q)$(MAKE) -C $(KBUILD_OUTPUT) \
	top_srcdir=$(top_srcdir) \
	top_builddir=$(top_builddir) \
	KBUILD_SRC=$(top_srcdir) \
	-f $(CURDIR)/Makefile $@

$(KBUILD_OUTPUT)/Rules.mak:
	@echo > $@
	@echo top_srcdir=$(top_srcdir) >> $@
	@echo top_builddir=$(KBUILD_OUTPUT) >> $@
	@echo include $(top_srcdir)/Rules.mak >> $@

$(KBUILD_OUTPUT)/Makefile:
	@echo > $@
	@echo top_srcdir=$(top_srcdir) >> $@
	@echo top_builddir=$(KBUILD_OUTPUT) >> $@
	@echo KBUILD_SRC='$$(top_srcdir)' >> $@
	@echo include '$$(KBUILD_SRC)'/Makefile >> $@

# Leave processing to above invocation of make
skip-makefile := 1
endif # ifneq ($(KBUILD_OUTPUT),)
endif # ifeq ($(KBUILD_SRC),)

ifeq ($(skip-makefile),)

# We only need a copy of the Makefile for the config targets and reuse
# the rest from the source directory, i.e. we do not cp ALL_MAKEFILES.
scripts/config/Makefile: $(top_srcdir)/scripts/config/Makefile
	cp $< $@

_all: all

help:
	@echo 'Cleaning:'
	@echo '  clean			- delete temporary files created by build'
	@echo '  distclean		- delete all non-source files (including .config)'
	@echo
	@echo 'Build:'
	@echo '  all			- Executable and documentation'
	@echo '  busybox		- the swiss-army executable'
	@echo '  doc			- docs/BusyBox.{txt,html,1}'
	@echo
	@echo 'Configuration:'
	@echo '  allnoconfig		- disable all symbols in .config'
	@echo '  allyesconfig		- enable all symbols in .config (see defconfig)'
	@echo '  allbareconfig		- enable all applets without any sub-features'
	@echo '  config		- text based configurator (of last resort)'
	@echo '  defconfig		- set .config to largest generic configuration'
	@echo '  menuconfig		- interactive curses-based configurator'
	@echo '  oldconfig		- resolve any unresolved symbols in .config'
	@echo '  hosttools  		- build sed for the host.'
	@echo '  			  You can use these commands if the commands on the host'
	@echo '  			  is unusable. Afterwards use it like:'
	@echo '			  make SED="$(top_builddir)/sed"'
	@echo
	@echo 'Installation:'
	@echo '  install		- install busybox into $(PREFIX)'
	@echo '  uninstall'
	@echo
	@echo 'Development:'
	@echo '  baseline		- create busybox_old for bloatcheck.'
	@echo '  bloatcheck		- show size difference between old and new versions'
	@echo '  check			- run the test suite for all applets'
	@echo '  checkhelp		- check for missing help-entries in Config.in'
	@echo '  randconfig		- generate a random configuration'
	@echo '  release		- create a distribution tarball'
	@echo '  sizes			- show size of all enabled busybox symbols'
	@echo '  objsizes		- show size of each .o object built'
	@echo


include $(top_srcdir)/Rules.mak

ifneq ($(strip $(HAVE_DOT_CONFIG)),y)

# Default target if none was requested explicitly
all: menuconfig

# warn if no configuration exists and we are asked to build a non-config target
.config:
	@echo ""
	@echo "No $(top_builddir)/$@ found!"
	@echo "Please refer to 'make  help', section Configuration."
	@echo ""
	@exit 1

# configuration
# ---------------------------------------------------------------------------

scripts/config/conf: scripts/config/Makefile
	$(Q)$(MAKE) -C scripts/config conf
	-@if [ ! -f .config ] ; then \
		touch .config; \
	fi

scripts/config/mconf: scripts/config/Makefile
	$(Q)$(MAKE) -C scripts/config ncurses conf mconf
	-@if [ ! -f .config ] ; then \
		touch .config; \
	fi

menuconfig: scripts/config/mconf
	@[ -f .config ] || $(MAKE) $(MAKEFLAGS) defconfig
	@./scripts/config/mconf $(CONFIG_CONFIG_IN)

config: scripts/config/conf
	@./scripts/config/conf $(CONFIG_CONFIG_IN)

oldconfig: scripts/config/conf
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

randconfig: scripts/config/conf
	@./scripts/config/conf -r $(CONFIG_CONFIG_IN)

allyesconfig: scripts/config/conf
	@./scripts/config/conf -y $(CONFIG_CONFIG_IN) > /dev/null
	@$(SED) -i -r -e "s/^(USING_CROSS_COMPILER)=.*/# \1 is not set/" .config
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN) > /dev/null

allnoconfig: scripts/config/conf
	@./scripts/config/conf -n $(CONFIG_CONFIG_IN) > /dev/null

# defconfig is allyesconfig minus any features that are specialized enough
# or cause enough behavior change that the user really should switch them on
# manually if that's what they want.  Sort of "maximum sane config".

defconfig: scripts/config/conf
	@./scripts/config/conf -y $(CONFIG_CONFIG_IN) > /dev/null
	@$(SED) -i -r -e "s/^(USING_CROSS_COMPILER|CONFIG_(DEBUG.*|STATIC|SELINUX|BUILD_(AT_ONCE|LIBBUSYBOX)|FEATURE_(DEVFS|FULL_LIBBUSYBOX|SHARED_BUSYBOX|MTAB_SUPPORT|CLEAN_UP|UDHCP_DEBUG)|INSTALL_NO_USR))=.*/# \1 is not set/" .config
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN) > /dev/null


allbareconfig: scripts/config/conf
	@./scripts/config/conf -y $(CONFIG_CONFIG_IN) > /dev/null
	@$(SED) -i -r -e "s/^(USING_CROSS_COMPILER|CONFIG_(DEBUG|STATIC|SELINUX|DEVFSD|NC_GAPING_SECURITY_HOLE|BUILD_AT_ONCE)).*/# \1 is not set/" .config
	@$(SED) -i -e "/FEATURE/s/=.*//;/^[^#]/s/.*FEATURE.*/# \0 is not set/;" .config
	@echo "CONFIG_FEATURE_BUFFERS_GO_ON_STACK=y" >> .config
	@yes n | ./scripts/config/conf -o $(CONFIG_CONFIG_IN) > /dev/null

hosttools:
	$(Q)cp .config .config.bak || noold=yea
	$(Q)$(MAKE) CC="$(HOSTCC)" CFLAGS="$(HOSTCFLAGS) $(INCS)" allnoconfig
	$(Q)mv .config .config.in
	$(Q)(grep -v CONFIG_SED .config.in ; \
	 echo "CONFIG_SED=y" ; ) > .config
	$(Q)$(MAKE) CC="$(HOSTCC)" CFLAGS="$(HOSTCFLAGS) $(INCS)" oldconfig include/bb_config.h
	$(Q)$(MAKE) CC="$(HOSTCC)" CFLAGS="$(HOSTCFLAGS) $(INCS)" busybox
	$(Q)[ -f .config.bak ] && mv .config.bak .config || rm .config
	mv busybox sed
	@echo "Now do: $(MAKE) SED=$(top_builddir)/sed <target>"

else # ifneq ($(strip $(HAVE_DOT_CONFIG)),y)

all: busybox busybox.links doc

# In this section, we need .config
-include $(top_builddir)/.config.cmd
include $(patsubst %,%/Makefile.in, $(SRC_DIRS))

endif # ifneq ($(strip $(HAVE_DOT_CONFIG)),y)

-include $(top_builddir)/.config
-include $(top_builddir)/.depend


ifeq ($(strip $(CONFIG_BUILD_AT_ONCE)),y)
libraries-y:=
# Which parts of the internal libs are requested?
# Per default we only want what was actually selected.
# -a denotes all while -y denotes the selected ones.
ifeq ($(strip $(CONFIG_FEATURE_FULL_LIBBUSYBOX)),y)
LIBRARY_DEFINE:=$(LIBRARY_DEFINE-a)
LIBRARY_SRC   :=$(LIBRARY_SRC-a)
else # CONFIG_FEATURE_FULL_LIBBUSYBOX
LIBRARY_DEFINE:=$(LIBRARY_DEFINE-y)
LIBRARY_SRC   :=$(LIBRARY_SRC-y)
endif # CONFIG_FEATURE_FULL_LIBBUSYBOX
APPLET_SRC:=$(APPLET_SRC-y)
APPLETS_DEFINE:=$(APPLETS_DEFINE-y)
else  # CONFIG_BUILD_AT_ONCE
# no --combine, build archives out of the individual .o
# This was the old way the binary was built.
libbusybox-obj:=archival/libunarchive/libunarchive.a \
	networking/libiproute/libiproute.a \
	libpwdgrp/libpwdgrp.a \
	coreutils/libcoreutils/libcoreutils.a \
	libbb/libbb.a
libbusybox-obj:=$(patsubst %,$(top_builddir)/%,$(libbusybox-obj))

ifeq ($(strip $(CONFIG_FEATURE_SHARED_BUSYBOX)),y)
# linking against libbusybox, so don't build the .a already contained in the .so
libraries-y:=$(filter-out $(libbusybox-obj),$(libraries-y))
endif # CONFIG_FEATURE_SHARED_BUSYBOX
endif # CONFIG_BUILD_AT_ONCE


ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)
LD_LIBBUSYBOX:=libbusybox.so
LIBBUSYBOX_SONAME:=$(LD_LIBBUSYBOX).$(MAJOR_VERSION).$(MINOR_VERSION).$(SUBLEVEL_VERSION)
DO_INSTALL_LIBS:=$(LD_LIBBUSYBOX) \
	$(LD_LIBBUSYBOX).$(MAJOR_VERSION) \
	$(LD_LIBBUSYBOX).$(MAJOR_VERSION).$(MINOR_VERSION)
endif # CONFIG_BUILD_LIBBUSYBOX

ifeq ($(strip $(CONFIG_BUILD_AT_ONCE)),y)
ifneq ($(strip $(CONFIG_FEATURE_SHARED_BUSYBOX)),y)
# --combine but not linking against libbusybox, so compile all
BUSYBOX_SRC   := $(LIBRARY_SRC)
BUSYBOX_DEFINE:= $(LIBRARY_DEFINE)
endif # !CONFIG_FEATURE_SHARED_BUSYBOX
$(LIBBUSYBOX_SONAME): $(LIBRARY_SRC)
else # CONFIG_BUILD_AT_ONCE
$(LIBBUSYBOX_SONAME): $(libbusybox-obj)
endif # CONFIG_BUILD_AT_ONCE

ifeq ($(strip $(CONFIG_FEATURE_SHARED_BUSYBOX)),y)
LDBUSYBOX:=-L$(top_builddir) -lbusybox
endif

ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)
$(LIBBUSYBOX_SONAME):
ifndef MAJOR_VERSION
	$(error MAJOR_VERSION needed for $@ is not defined)
endif
	$(do_link) $(LIB_CFLAGS) $(CFLAGS_COMBINE) \
	-Wl,-soname=$(LD_LIBBUSYBOX).$(MAJOR_VERSION) \
	-Wl,-z,combreloc $(LIB_LDFLAGS) \
	-o $(@) \
	$(LD_START_GROUP) $(LD_WHOLE_ARCHIVE) \
	$(LIBRARY_DEFINE) $(^) \
	$(LD_NO_WHOLE_ARCHIVE) $(LD_END_GROUP)
	@rm -f $(DO_INSTALL_LIBS)
	@for i in $(DO_INSTALL_LIBS); do ln -s $(@) $$i ; done
	$(do_strip)

endif # ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)

busybox_unstripped: .depend $(LIBBUSYBOX_SONAME) $(BUSYBOX_SRC) $(APPLET_SRC) $(libraries-y)
	$(do_link) $(PROG_CFLAGS) $(PROG_LDFLAGS) $(CFLAGS_COMBINE) \
	-o $@ $(LD_START_GROUP)  \
	$(APPLETS_DEFINE) $(APPLET_SRC) \
	$(BUSYBOX_DEFINE) $(BUSYBOX_SRC) $(libraries-y) \
	$(LDBUSYBOX) $(LIBRARIES) \
	$(LD_END_GROUP)

busybox: busybox_unstripped
	$(Q)cp busybox_unstripped busybox
	$(do_strip)

%.bflt: %_unstripped
	$(do_elf2flt)

busybox.links: $(top_srcdir)/applets/busybox.mkll include/bb_config.h $(top_srcdir)/include/applets.h
	$(Q)-$(SHELL) $^ >$@

install: $(top_srcdir)/applets/install.sh busybox busybox.links
	$(Q)DO_INSTALL_LIBS="$(strip $(LIBBUSYBOX_SONAME) $(DO_INSTALL_LIBS))" \
		$(SHELL) $< $(PREFIX) $(INSTALL_OPTS)
ifeq ($(strip $(CONFIG_FEATURE_SUID)),y)
	@echo
	@echo
	@echo --------------------------------------------------
	@echo You will probably need to make your busybox binary
	@echo setuid root to ensure all configured applets will
	@echo work properly.
	@echo --------------------------------------------------
	@echo
endif

uninstall: busybox.links
	rm -f $(PREFIX)/bin/busybox
	for i in `cat busybox.links` ; do rm -f $(PREFIX)$$i; done
ifneq ($(strip $(DO_INSTALL_LIBS)),n)
	for i in $(LIBBUSYBOX_SONAME) $(DO_INSTALL_LIBS); do \
		rm -f $(PREFIX)$$i; \
	done
endif

# see if we are in verbose mode
KBUILD_VERBOSE :=
ifdef V
  ifeq ("$(origin V)", "command line")
    KBUILD_VERBOSE := $(V)
  endif
endif
ifneq ($(strip $(KBUILD_VERBOSE)),)
  CHECK_VERBOSE := -v
# ARFLAGS+=v
endif

check test: busybox
	bindir=$(top_builddir) srcdir=$(top_srcdir)/testsuite SED="$(SED)" \
	$(SHELL) $(top_srcdir)/testsuite/runtest $(CHECK_VERBOSE)

checkhelp:
	$(Q)$(top_srcdir)/scripts/checkhelp.awk \
		$(wildcard $(patsubst %,%/Config.in,$(SRC_DIRS) ./))

sizes: busybox_unstripped
	$(NM) --size-sort $(<)

bloatcheck: busybox_old busybox_unstripped
	@$(top_srcdir)/scripts/bloat-o-meter busybox_old busybox_unstripped

baseline: busybox_unstripped
	@mv busybox_unstripped busybox_old

objsizes: busybox_unstripped
	$(SHELL) $(top_srcdir)/scripts/objsizes

# Documentation Targets
doc: docs/busybox.pod docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

docs/busybox.pod : $(top_srcdir)/docs/busybox_header.pod $(top_srcdir)/include/usage.h $(top_srcdir)/docs/busybox_footer.pod $(top_srcdir)/docs/autodocifier.pl
	$(disp_doc)
	$(Q)-mkdir -p docs
	$(Q)-( cat $(top_srcdir)/docs/busybox_header.pod ; \
	    $(top_srcdir)/docs/autodocifier.pl $(top_srcdir)/include/usage.h ; \
	    cat $(top_srcdir)/docs/busybox_footer.pod ; ) > docs/busybox.pod

docs/BusyBox.txt: docs/busybox.pod
	$(disp_doc)
	$(Q)-mkdir -p docs
	$(Q)-pod2text $< > $@

docs/BusyBox.1: docs/busybox.pod
	$(disp_doc)
	$(Q)-mkdir -p docs
	$(Q)-pod2man --center=BusyBox --release="version $(VERSION)" \
		$< > $@

docs/BusyBox.html: docs/busybox.net/BusyBox.html
	$(disp_doc)
	$(Q)-mkdir -p docs
	$(Q)-rm -f docs/BusyBox.html
	$(Q)-cp docs/busybox.net/BusyBox.html docs/BusyBox.html

docs/busybox.net/BusyBox.html: docs/busybox.pod
	$(Q)-mkdir -p docs/busybox.net
	$(Q)-pod2html --noindex $< > \
	    docs/busybox.net/BusyBox.html
	$(Q)-rm -f pod2htm*

# The nifty new dependency stuff
scripts/bb_mkdep: $(top_srcdir)/scripts/bb_mkdep.c
	$(do_link.h)

DEP_INCLUDES := include/bb_config.h

ifeq ($(strip $(CONFIG_BBCONFIG)),y)
DEP_INCLUDES += include/bbconfigopts.h

include/bbconfigopts.h: .config
	$(disp_gen)
	$(Q)$(top_srcdir)/scripts/config/mkconfigs > $@
endif

ifeq ($(strip $(CONFIG_FEATURE_COMPRESS_USAGE)),y)
USAGE_BIN:=scripts/usage
$(USAGE_BIN): $(top_srcdir)/scripts/usage.c
	$(do_link.h)

DEP_INCLUDES += include/usage_compressed.h

include/usage_compressed.h: .config $(USAGE_BIN)
	$(Q)SED="$(SED)" $(SHELL) $(top_srcdir)/scripts/usage_compressed "$(top_builddir)/scripts" > $@
endif # CONFIG_FEATURE_COMPRESS_USAGE

# workaround alleged bug in make-3.80, make-3.81
.NOTPARALLEL: .depend

depend dep: .depend
.depend: scripts/bb_mkdep $(USAGE_BIN) $(DEP_INCLUDES)
	$(disp_gen)
	$(Q)rm -f .depend
	$(Q)mkdir -p include/config
	$(Q)scripts/bb_mkdep -I $(top_srcdir)/include $(top_srcdir) > $@.tmp
	$(Q)mv $@.tmp $@

include/bb_config.h: .config
	@if [ ! -x $(top_builddir)/scripts/config/conf ] ; then \
	    $(MAKE) -C scripts/config conf; \
	fi;
	@$(top_builddir)/scripts/config/conf -o $(CONFIG_CONFIG_IN)

clean:
	- $(MAKE) -C scripts/config $@
	- rm -f docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pod docs/busybox.net/busybox.html \
	    docs/busybox pod2htm* *.gdb *.elf *~ core .*config.log \
	    docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.net/BusyBox.html busybox.links \
	    libbusybox.so* \
	    .config.old busybox busybox_unstripped
	- rm -r -f _install testsuite/links
	- find . -name .\*.flags -o -name \*.o  -o -name \*.om \
	    -o -name \*.os -o -name \*.osm -o -name \*.a | xargs rm -f

distclean: clean
	rm -f scripts/bb_mkdep scripts/usage
	rm -r -f include/config include/config.h $(DEP_INCLUDES)
	find . -name .depend'*' -print0 | xargs -0 rm -f
	rm -f .hdepend
	rm -f .config .config.old .config.cmd

release: distclean #doc
	cd ..; \
	rm -r -f $(PROG)-$(VERSION); \
	cp -a busybox $(PROG)-$(VERSION); \
	\
	find $(PROG)-$(VERSION)/ -type d \
		-name .svn \
		-print \
		-exec rm -r -f {} \; ; \
	\
	find $(PROG)-$(VERSION)/ -type f \
		-name .\#* \
		-print \
		-exec rm -f {} \; ; \
	\
	tar -cvzf $(PROG)-$(VERSION).tar.gz $(PROG)-$(VERSION)/;

tags:
	ctags -R .


endif # ifeq ($(skip-makefile),)


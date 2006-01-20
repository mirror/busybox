# Makefile for busybox
#
# Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
#
# Licensed under GPLv2, see the file LICENSE in this tarball for details.
#

#--------------------------------------------------------------
# You shouldn't need to mess with anything beyond this point...
#--------------------------------------------------------------
noconfig_targets := menuconfig config oldconfig randconfig \
	defconfig allyesconfig allnoconfig allbareconfig \
	clean distclean \
	release tags

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
.PHONY: _all
_all:

CONFIG_CONFIG_IN = $(top_srcdir)/Config.in
CONFIG_DEFCONFIG = $(top_srcdir)/defconfig

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
# pull in OS specific commands like cp, mkdir, etc. early
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
	$(MAKE) -C $(KBUILD_OUTPUT) \
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
	@echo '  allyesconfig		- enable (almost) all symbols in .config'
	@echo '  allbareconfig		- enable all basics without any features'
	@echo '  config		- text based configurator (of last resort)'
	@echo '  defconfig		- set .config to defaults'
	@echo '  menuconfig		- interactive curses-based configurator'
	@echo '  oldconfig		- resolve any unresolved symbols in .config'
	@echo
	@echo 'Installation:'
	@echo '  install		- install busybox into $prefix'
	@echo '  uninstall'
	@echo
	@echo 'Development:'
	@echo '  check			- run the test suite for all applets'
	@echo '  randconfig		- generate a random configuration'
	@echo '  release		- create a distribution tarball'
	@echo '  sizes			- show size of all enabled busybox symbols'
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
	$(MAKE) -C scripts/config conf
	-@if [ ! -f .config ] ; then \
		cp $(CONFIG_DEFCONFIG) .config; \
	fi

scripts/config/mconf: scripts/config/Makefile
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
	sed -i -r -e "s/^(CONFIG_DEBUG|USING_CROSS_COMPILER|CONFIG_STATIC|CONFIG_SELINUX|CONFIG_FEATURE_DEVFS).*/# \1 is not set/" .config
	echo "CONFIG_FEATURE_SHARED_BUSYBOX=y" >> .config
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

allnoconfig: scripts/config/conf
	@./scripts/config/conf -n $(CONFIG_CONFIG_IN)

defconfig: scripts/config/conf
	@./scripts/config/conf -d $(CONFIG_CONFIG_IN)

allbareconfig: scripts/config/conf
	@./scripts/config/conf -y $(CONFIG_CONFIG_IN)
	@sed -i -r -e "s/^(USING_CROSS_COMPILER|CONFIG_(DEBUG|STATIC|SELINUX|DEVFSD|NC_GAPING_SECURITY_HOLE)).*/# \1 is not set/" .config
	@sed -i -e "/FEATURE/s/=.*//;/^[^#]/s/.*FEATURE.*/# \0 is not set/;" .config
	@echo "CONFIG_FEATURE_BUFFERS_GO_ON_STACK=y" >> .config
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

else # ifneq ($(strip $(HAVE_DOT_CONFIG)),y)

all: busybox busybox.links doc

# In this section, we need .config
-include $(top_builddir)/.config.cmd
include $(patsubst %,%/Makefile.in, $(SRC_DIRS))

endif # ifneq ($(strip $(HAVE_DOT_CONFIG)),y)

-include $(top_builddir)/.config
-include $(top_builddir)/.depend


ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)

LD_LIBBUSYBOX:=libbusybox.so
LIBBUSYBOX_SONAME:=$(LD_LIBBUSYBOX).$(MAJOR_VERSION).$(MINOR_VERSION).$(SUBLEVEL_VERSION)
DO_INSTALL_LIBS:=$(LD_LIBBUSYBOX) \
	$(LD_LIBBUSYBOX).$(MAJOR_VERSION) \
	$(LD_LIBBUSYBOX).$(MAJOR_VERSION).$(MINOR_VERSION)
 
ifeq ($(CONFIG_BUILD_AT_ONCE),y)
# Which parts of the internal libs are requested?
# Per default we only want what was actually selected.
ifeq ($(CONFIG_FEATURE_FULL_LIBBUSYBOX),y)
LIBRARY_DEFINE:=$(LIBRARY_DEFINE-a)
LIBRARY_SRC   :=$(LIBRARY_SRC-a)
$(LIBBUSYBOX_SONAME): $(LIBRARY_SRC)
else
LIBRARY_DEFINE:=$(LIBRARY_DEFINE-y)
LIBRARY_SRC   :=$(LIBRARY_SRC-y)
$(LIBBUSYBOX_SONAME): $(LIBRARY_SRC)
endif
else  # CONFIG_BUILD_AT_ONCE
libbusybox-obj:=archival/libunarchive/libunarchive.a \
	networking/libiproute/libiproute.a \
	libpwdgrp/libpwdgrp.a \
	coreutils/libcoreutils/libcoreutils.a \
	libbb/libbb.a
libbusybox-obj:=$(patsubst %,$(top_builddir)/%,$(libbusybox-obj))

$(LIBBUSYBOX_SONAME): $(libbusybox-obj)

LIBRARY_DEFINE:=
LIBRARY_SRC   :=
endif # CONFIG_BUILD_AT_ONCE


$(LIBBUSYBOX_SONAME):
ifndef MAJOR_VERSION
	$(error MAJOR_VERSION needed for $@ is not defined)
endif
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -shared \
	$(CFLAGS_PIC) \
	-Wl,-soname=$(LD_LIBBUSYBOX).$(MAJOR_VERSION) \
	-Wl,--enable-new-dtags -Wl,--reduce-memory-overheads \
	-Wl,-z,combreloc -Wl,-shared -Wl,--as-needed -Wl,--warn-shared-textrel \
	-o $(@) \
	-Wl,--start-group -Wl,--whole-archive \
	$(LIBRARY_DEFINE) $(^) \
	-Wl,--no-whole-archive -Wl,--end-group
	$(RM_F) $(DO_INSTALL_LIBS)
	for i in $(DO_INSTALL_LIBS); do $(LN_S) -v $(@) $$i ; done
	$(STRIPCMD) $@

endif # ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)


ifeq ($(strip $(CONFIG_FEATURE_SHARED_BUSYBOX)),y)
libraries-y:=$(filter-out $(libbusybox-obj),$(libraries-y))
LDBUSYBOX:=-L$(top_builddir) -lbusybox
BUSYBOX_SRC   :=
BUSYBOX_DEFINE:=
else
#LDBUSYBOX:=
BUSYBOX_SRC   := $(LIBRARY_SRC)
BUSYBOX_DEFINE:= $(LIBRARY_DEFINE)
endif # ifeq ($(strip $(CONFIG_FEATURE_SHARED_BUSYBOX)),y)


ifeq ($(strip $(CONFIG_BUILD_AT_ONCE)),y)
libraries-y:=
else
BUSYBOX_SRC:=
BUSYBOX_DEFINE:=
APPLET_SRC-y:=
APPLETS_DEFINE-y:=
endif

busybox: .depend $(LIBBUSYBOX_SONAME) $(BUSYBOX_SRC) $(libraries-y)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(PROG_CFLAGS) $(LDFLAGS)  \
	-o $@ -Wl,--start-group  \
	$(APPLETS_DEFINE-y) $(APPLET_SRC-y) $(BUSYBOX_DEFINE) $(BUSYBOX_SRC) $(libraries-y) $(LDBUSYBOX) $(LIBRARIES) \
	-Wl,--end-group
	$(STRIPCMD) $@

busybox.links: $(top_srcdir)/applets/busybox.mkll include/bb_config.h $(top_srcdir)/include/applets.h
	- $(SHELL) $^ >$@

install: $(top_srcdir)/applets/install.sh busybox busybox.links
	DO_INSTALL_LIBS="$(strip $(LIBBUSYBOX_SONAME) $(DO_INSTALL_LIBS))" \
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
	$(RM_F) $(PREFIX)/bin/busybox
	for i in `cat busybox.links` ; do $(RM_F) $(PREFIX)$$i; done
ifneq ($(strip $(DO_INSTALL_LIBS)),n)
	for i in $(LIBBUSYBOX_SONAME) $(DO_INSTALL_LIBS); do \
		$(RM_F) $(PREFIX)$$i; \
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
	bindir=$(top_builddir) srcdir=$(top_srcdir)/testsuite \
	$(top_srcdir)/testsuite/runtest $(CHECK_VERBOSE)

sizes:
	-$(RM_F) busybox
	$(MAKE) top_srcdir=$(top_srcdir) top_builddir=$(top_builddir) \
		-f $(top_srcdir)/Makefile STRIPCMD=/bin/true
	$(NM) --size-sort busybox

# Documentation Targets
doc: docs/busybox.pod docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

docs/busybox.pod : $(top_srcdir)/docs/busybox_header.pod $(top_srcdir)/include/usage.h $(top_srcdir)/docs/busybox_footer.pod
	-mkdir -p docs
	- ( cat $(top_srcdir)/docs/busybox_header.pod; \
	    $(top_srcdir)/docs/autodocifier.pl $(top_srcdir)/include/usage.h; \
	    cat $(top_srcdir)/docs/busybox_footer.pod ) > docs/busybox.pod

docs/BusyBox.txt: docs/busybox.pod
	$(SECHO)
	$(SECHO) BusyBox Documentation
	$(SECHO)
	-mkdir -p docs
	-pod2text $< > $@

docs/BusyBox.1: docs/busybox.pod
	- mkdir -p docs
	- pod2man --center=BusyBox --release="version $(VERSION)" \
		$< > $@

docs/BusyBox.html: docs/busybox.net/BusyBox.html
	- mkdir -p docs
	-@ $(RM_F) docs/BusyBox.html
	-@ cp docs/busybox.net/BusyBox.html docs/BusyBox.html

docs/busybox.net/BusyBox.html: docs/busybox.pod
	-@ mkdir -p docs/busybox.net
	-  pod2html --noindex $< > \
	    docs/busybox.net/BusyBox.html
	-@ $(RM_F) pod2htm*

# The nifty new buildsystem stuff
scripts/bb_mkdep: $(top_srcdir)/scripts/bb_mkdep.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

DEP_INCLUDES := include/config.h include/bb_config.h

ifeq ($(strip $(CONFIG_BBCONFIG)),y)
DEP_INCLUDES += include/bbconfigopts.h

include/bbconfigopts.h: .config
	$(top_srcdir)/scripts/config/mkconfigs > $@
endif

depend dep: .depend
.depend: scripts/bb_mkdep $(DEP_INCLUDES)
	@$(RM_F) .depend
	@mkdir -p include/config
	scripts/bb_mkdep -c include/config.h -c include/bb_config.h \
			-I $(top_srcdir)/include $(top_srcdir) > $@.tmp
	mv $@.tmp $@

include/config.h: .config
	@if [ ! -x $(top_builddir)/scripts/config/conf ] ; then \
	    $(MAKE) -C scripts/config conf; \
	fi;
	@$(top_builddir)/scripts/config/conf -o $(CONFIG_CONFIG_IN)

include/bb_config.h: include/config.h
	@echo -e "#ifndef BB_CONFIG_H\n#define BB_CONFIG_H" > $@
	@sed -e 's/#undef CONFIG_\(.*\)/#define ENABLE_\1 0/' \
	    -e 's/#define CONFIG_\(.*\)/#define CONFIG_\1\n#define ENABLE_\1/' \
		< $< >> $@
	@echo "#endif" >> $@

clean:
	- $(MAKE) -C scripts/config $@
	- $(RM_F) docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pod docs/busybox.net/busybox.html \
	    docs/busybox pod2htm* *.gdb *.elf *~ core .*config.log \
	    docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.net/BusyBox.html busybox.links libbb/loop.h \
	    $(DO_INSTALL_LIBS) $(LIBBUSYBOX_SONAME) \
	    .config.old busybox
	- rm -rf _install testsuite/links
	- find . -name .\*.flags -exec $(RM_F) {} \;
	- find . -name \*.o -exec $(RM_F) {} \;
	- find . -name \*.a -exec $(RM_F) {} \;
	- find . -name \*.so -exec $(RM_F) {} \;

distclean: clean
	- $(RM_F) scripts/bb_mkdep
	- rm -rf include/config include/config.h include/bb_config.h include/bbconfigopts.h
	- find . -name .depend -exec $(RM_F) {} \;
	$(RM_F) .config .config.old .config.cmd

release: distclean #doc
	cd ..; \
	rm -rf $(PROG)-$(VERSION); \
	cp -a busybox $(PROG)-$(VERSION); \
	\
	find $(PROG)-$(VERSION)/ -type d \
		-name .svn \
		-print \
		-exec rm -rf {} \; ; \
	\
	find $(PROG)-$(VERSION)/ -type f \
		-name .\#* \
		-print \
		-exec $(RM_F) {} \; ; \
	\
	tar -cvzf $(PROG)-$(VERSION).tar.gz $(PROG)-$(VERSION)/;

tags:
	ctags -R .


endif # ifeq ($(skip-makefile),)

.PHONY: dummy subdirs release distclean clean config oldconfig \
	menuconfig tags check test depend dep buildtree

# Makefile for BusyBox
#
# Copyright (C) 2002 Erik Andersen <andersen@codepoet.org>

TOPDIR=../../
include $(TOPDIR)Rules.mak


all: ncurses conf mconf

#HOSTCFLAGS=-Wall -g -O0
LIBS = -lncurses
ifeq (/usr/include/ncurses/ncurses.h, $(wildcard /usr/include/ncurses/ncurses.h))
	HOSTCFLAGS += -I/usr/include/ncurses -DCURSES_LOC="<ncurses.h>"
else
ifeq (/usr/include/ncurses/curses.h, $(wildcard /usr/include/ncurses/curses.h))
	HOSTCFLAGS += -I/usr/include/ncurses -DCURSES_LOC="<ncurses/curses.h>"
else
ifeq (/usr/include/ncurses.h, $(wildcard /usr/include/ncurses.h))
	HOSTCFLAGS += -DCURSES_LOC="<ncurses.h>"
else
	HOSTCFLAGS += -DCURSES_LOC="<curses.h>"
endif
endif
endif


CONF_SRC  =conf.c zconf.tab.c
MCONF_SRC =mconf.c zconf.tab.c
LXDLG_SRC =checklist.c menubox.c textbox.c yesno.c inputbox.c util.c msgbox.c
CONF_OBJS =$(patsubst %.c,%.o, $(CONF_SRC))
MCONF_OBJS=$(patsubst %.c,%.o, $(MCONF_SRC))
LXDLG_OBJS=$(patsubst %.c,%.o, $(LXDLG_SRC))

conf: $(CONF_OBJS) 
	$(HOSTCC) $(HOSTCFLAGS) $(NATIVE_LDFLAGS) $^ -o $@

mconf: $(MCONF_OBJS) $(LXDLG_OBJS)
	$(HOSTCC) $(HOSTCFLAGS) $(NATIVE_LDFLAGS) $^ -o $@ $(LIBS)

lkc_deps:=lkc.h lkc_proto.h lkc_defs.h expr.h zconf.tab.h

conf.o: conf.c $(lkc_deps)

mconf.o: mconf.c $(lkc_deps)

zconf.tab.o: zconf.tab.c lex.zconf.c confdata.c expr.c symbol.c menu.c $(lkc_deps)

lex.zconf.o: lex.zconf.c $(lkc_deps)

%.o : %.c
	$(HOSTCC) $(HOSTCFLAGS) -I. -c $< -o $@

lkc_defs.h: lkc_proto.h
	@sed < $< > $@ 's/P(\([^,]*\),.*/#define \1 (\*\1_p)/'

###
# The following requires flex/bison
# By default we use the _shipped versions, uncomment the 
# following line if you are modifying the flex/bison src.
#LKC_GENPARSER := 1

ifdef LKC_GENPARSER

%.tab.c %.tab.h: %.y
	bison -t -d -v -b $* -p $(notdir $*) $<

lex.%.c: %.l
	flex -P$(notdir $*) -o$@ $<
else

lex.zconf.c: lex.zconf.c_shipped
	cp lex.zconf.c_shipped lex.zconf.c

zconf.tab.c: zconf.tab.c_shipped
	cp zconf.tab.c_shipped zconf.tab.c

zconf.tab.h: zconf.tab.h_shipped
	cp zconf.tab.h_shipped zconf.tab.h
endif

.PHONY: ncurses

ncurses:
	@echo "main() {}" > lxtemp.c
	@if $(HOSTCC) lxtemp.c $(LIBS) ; then \
		rm -f lxtemp.c a.out; \
	else \
		rm -f lxtemp.c; \
		echo -e "\007" ;\
		echo ">> Unable to find the Ncurses libraries." ;\
		echo ">>" ;\
		echo ">> You must have Ncurses installed in order" ;\
		echo ">> to use 'make menuconfig'" ;\
		echo ;\
		exit 1 ;\
	fi

clean:
	rm -f *.o *~ core $(TARGETS) $(MCONF_OBJS) $(CONF_OBJS) \
		conf mconf zconf.tab.c zconf.tab.h lex.zconf.c lkc_defs.h


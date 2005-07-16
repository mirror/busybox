# Silly wrapper makefile.  This Makefile is _not_ used by the build system for
# busybox, it is just to make working on libbb more conveinient.
#  -Erik Andersen

all:
	make -C .. libbb.a

clean:
	- rm -rf libbb.a
	- find . -name \*.o -exec rm -f {} \;


# busybox/docs/Makefile - Create the documentation
# ------------------------
# Copyright (C) 2000  Erik Andersen <andersee@debian.org>  GPL

all: clean doc

doc:
	@echo
	@echo BusyBox Documentation
	@echo
	pod2html busybox.pod > ../BusyBox.html 
	@rm pod2html-*
	pod2man --center=BusyBox --release="version $(VERSION)" busybox.pod > ../BusyBox.1
	pod2text busybox.pod > ../BusyBox.txt
	@rm -f pod2html*

clean:
	@rm -f ../BusyBox.html ../BusyBox.1 ../BusyBox.txt pod2html*

distclean: clean


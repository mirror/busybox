# Makefile for busybox
#
# Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

TOPDIR   :=..
L_TARGET := findutils.a

obj-y           :=
obj-n           :=
obj-            :=


obj-$(CONFIG_FIND)	+= find.o
obj-$(CONFIG_GREP)	+= grep.o
obj-$(CONFIG_WHICH)	+= which.o
obj-$(CONFIG_XARGS)	+= xargs.o


# Hand off to toplevel Rules.mak
include $(TOPDIR)/Rules.mak

clean:
	rm -f $(L_TARGET) *.o core


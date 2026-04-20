#!/bin/sh
#
# PiPlayInit applet consistency check
#
# This script verifies that every PiPlayInit applet has a corresponding
# configuration option enabling it.
#
# Run this script after applets.h is generated.

# CONFIG_<applet> names
grep ^IF_ applets.h | grep -v ^IF_FEATURE_ \
| sed 's/IF_\([A-Z0-9._-]*\)(.*/\1/' \
| sort | uniq \
>applets_APP1

# Command-line applet names
grep ^IF_ applets.h \
| sed -e's/ //g' -e's/.*(\([a-z[][^,]*\),.*/\1/' \
| grep -v '^bash$' \
| grep -v '^sh$' \
| tr a-z A-Z \
| sed 's/^SYSCTL$/PPI_SYSCTL/' \
| sed 's/^\[\[$/TEST1/' \
| sed 's/^\[$/TEST2/' \
| sort | uniq \
>applets_APP2

diff -u applets_APP1 applets_APP2 >applets_APP.diff

# Cleanup intentionally commented out to allow inspection
# rm applets_APP1 applets_APP2
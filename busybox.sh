#!/bin/sh
sed -n -e 's/^#define.*BB_FEATURE.*$//g;y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;' \
    -e '/^#define/{s/.*bb_//;s/$/.o/p;}' busybox.def.h

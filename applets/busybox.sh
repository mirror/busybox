#!/bin/sh
sed -n '/^#define/{s/.*BB_//; s/$/.o/p; }' busybox.def.h | \
tr [:upper:] [:lower:]


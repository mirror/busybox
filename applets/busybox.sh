#!/bin/sh
ls -1 `sed -n '/^#define/{s/.*BB_// ; s/$/.c/p; }' busybox.def.h | \
tr [:upper:] [:lower:]` 2> /dev/null | sed -e 's/\.c$/\.o/g'


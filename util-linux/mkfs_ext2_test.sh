#!/bin/sh

test_mke2fs() {
    echo Testing $kilobytes

    >image_std
    dd seek=$((kilobytes-1)) bs=1K count=1 </dev/zero of=image_std >/dev/null 2>&1 || exit 1
    /usr/bin/mke2fs -F image_std $kilobytes >image_std.raw_out 2>&1 || return 1
    cat image_std.raw_out \
    | grep -v '^mke2fs ' \
    | grep -v '^Maximum filesystem' \
    | grep -v '^warning: .* blocks unused' \
    | grep -v '^Writing inode tables' \
    | grep -v '^Writing superblocks and filesystem accounting information' \
    | grep -v '^This filesystem will be automatically checked every' \
    | grep -v '^180 days, whichever comes first' \
    | sed 's/block groups/block group/' \
    | sed 's/ *$//' \
    | sed 's/blocks (.*%) reserved/blocks reserved/' \
    | grep -v '^$' \
    >image_std.out

    >image_bb
    dd seek=$((kilobytes-1)) bs=1K count=1 </dev/zero of=image_bb >/dev/null 2>&1 || exit 1
    ./busybox mke2fs -F image_bb $kilobytes >image_bb.raw_out 2>&1 || return 1
    cat image_bb.raw_out \
    | grep -v '^mke2fs ' \
    | grep -v '^Maximum filesystem' \
    | grep -v '^warning: .* blocks unused' \
    | grep -v '^Writing inode tables' \
    | grep -v '^Writing superblocks and filesystem accounting information' \
    | grep -v '^This filesystem will be automatically checked every' \
    | grep -v '^180 days, whichever comes first' \
    | sed 's/block groups/block group/' \
    | sed 's/ *$//' \
    | sed 's/blocks (.*%) reserved/blocks reserved/' \
    | grep -v '^$' \
    >image_bb.out

    diff -ua image_bb.out image_std.out >image.out.diff || {
	cat image.out.diff
	return 1
    }
}

kilobytes=24908 test_mke2fs
kilobytes=81940 test_mke2fs
kilobytes=98392 test_mke2fs
exit

while true; do
    kilobytes=$(( (RANDOM*RANDOM) % 100000 + 100))
    test_mke2fs || exit 1
done

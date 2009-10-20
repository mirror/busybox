#!/bin/sh

system_mke2fs='/sbin/mke2fs'
bbox_mke2fs='./busybox mke2fs'

gen_image() { # params: mke2fs_invocation image_name
    >$2
    dd seek=$((kilobytes-1)) bs=1K count=1 </dev/zero of=$2 >/dev/null 2>&1 || exit 1
    $1 -F $2 $kilobytes >$2.raw_out 2>&1 || return 1

#off    | sed 's/inodes, [0-9]* blocks/inodes, N blocks/' \

    cat $2.raw_out \
    | grep -v '^mke2fs [0-9]*\.[0-9]*\.[0-9]* ' \
    | grep -v '^Maximum filesystem' \
    | grep -v '^Writing inode tables' \
    | grep -v '^Writing superblocks and filesystem accounting information' \
    | grep -v '^This filesystem will be automatically checked every' \
    | grep -v '^180 days, whichever comes first' \
    | sed 's/blocks* unused./blocks unused/' \
    | sed 's/block groups*/block groups/' \
    | sed 's/ *$//' \
    | sed 's/blocks (.*%) reserved/blocks reserved/' \
    | grep -v '^$' \
    >$2.out
}

test_mke2fs() {
    echo Testing $kilobytes

    gen_image "$system_mke2fs" image_std || return 1
    gen_image "$bbox_mke2fs"   image_bb  || return 1

    diff -ua image_bb.out image_std.out >image.out.diff || {
	cat image.out.diff
	return 1
    }

    e2fsck -f -n image_bb >image_bb_e2fsck.out 2>&1 || {
	echo "e2fsck error on image_bb"
	cat image_bb_e2fsck.out
	exit 1
    }
}

# -:bbox +:standard

# kilobytes=60 is the minimal allowed size
kilobytes=60
while true; do
    test_mke2fs || exit 1
    : $((kilobytes++))
    test $kilobytes = 200 && break
done

# Transition from one block group to two
# fails in [8378..8410] range
kilobytes=$((8*1024 - 20))
while true; do
    test_mke2fs #|| exit 1
    : $((kilobytes++))
    test $kilobytes = $((8*1024 + 300)) && break
done

# Transition from two block groups to three
# works
kilobytes=$((16*1024 - 40))
while true; do
    test_mke2fs || exit 1
    : $((kilobytes++))
    test $kilobytes = $((16*1024 + 500)) && break
done
exit

# Specific sizes with known differences:

# -6240 inodes, 24908 blocks
# +6240 inodes, 24577 blocks
# -4 block group
# +3 block group
# -1560 inodes per group
# +2080 inodes per group
kilobytes=24908 test_mke2fs

# -304 inodes, N blocks
# +152 inodes, N blocks
# -304 inodes per group
# +152 inodes per group
kilobytes=1218 test_mke2fs

# -14464 inodes, N blocks
# +14448 inodes, N blocks
# -8 block group
# +7 block group
# -1808 inodes per group
# +2064 inodes per group
kilobytes=57696 test_mke2fs

# -warning: 239 blocks unused.
# +warning: 242 blocks unused.
kilobytes=49395 test_mke2fs

## This size results in "warning: 75 blocks unused"
#kilobytes=98380 test_mke2fs

while true; do
    kilobytes=$(( (RANDOM*RANDOM) % 1000000 + 60))
    test_mke2fs || exit 1
done

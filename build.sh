#!/bin/bash
set -eux

OUTDIR=build

TARGETPREFIXLIST=(x86_64-linux-gnu mipseb-linux-musl mipsel-linux-musl arm-linux-musleabi aarch64-linux-musl mips64eb-linux-musl mips64el-linux-musl)
TARGETNAMELIST=(  x86_64           mips              mips              arm                aarch64            mips64              mips64             )
TARGETFLAGSLIST=( ""               -mips32r3         -mips32r3         ""                 ""                 -mips64r2           -mips64r2)

for i in "${!TARGETNAMELIST[@]}"; do
    if [[ "${TARGETPREFIXLIST[i]}" == "mips64eb-linux-musl" ]]
    then
        SKIP_STRIP=y
    else
        SKIP_STRIP=n
    fi

    SKIP_STRIP=y

    make ARCH=${TARGETNAMELIST[i]} CROSS_COMPILE=${TARGETPREFIXLIST[i]}- CFLAGS="${TARGETFLAGSLIST[i]}" SKIP_STRIP="${SKIP_STRIP}"

    # copy the unstripped version if the stripped version doesn't exist
    mv busybox $OUTDIR/busybox.${TARGETPREFIXLIST[i]} \
        || mv busybox_unstripped $OUTDIR/busybox.${TARGETPREFIXLIST[i]}

    # busybox doesn't have any way to configure build artifact location,
    # so we need to clean up when we're done in case the next arch's build
    # fails. also we need to build 1-by-1
    make clean
done

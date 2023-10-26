#!/bin/bash

OUTDIR=build

TARGETPREFIXLIST=(x86_64-linux-gnu i686-linux-musl mipseb-linux-musl mipsel-linux-musl arm-linux-musleabi aarch64-linux-musl mips64eb-linux-musl mips64el-linux-musl)
TARGETNAMELIST=(  x86_64           i686            mips              mips              arm                aarch64            mips64              mips64             )

mkdir $OUTDIR

for i in "${!TARGETNAMELIST[@]}"; do
    make ARCH=${TARGETNAMELIST[i]} CROSS_COMPILE=${TARGETPREFIXLIST[i]}-
    mv busybox $OUTDIR/busybox.${TARGETPREFIXLIST[i]} || mv busybox_unstripped $OUTDIR/busybox.${TARGETPREFIXLIST[i]}
    make clean
done

#!/bin/bash

DIR=`dirname $0`
MOD=gpiomem-dummy.ko

pushd $DIR

make

if `lsmod | grep -q $MOD`; then
   sudo rmmod $MOD
fi

sudo insmod $MOD

popd

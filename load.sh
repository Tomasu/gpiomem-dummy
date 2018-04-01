#!/bin/bash

DIR=`dirname $0`
MOD=gpiomem_dummy.ko

pushd $DIR

make

if $(lsmod | grep -q $MOD); then
   sudo rmmod $MOD
fi

sudo insmod $MOD

popd

#!/bin/bash

DIR=`dirname $0`
MOD=gpiomem_dummy.ko

pushd $DIR

make

lsmod | grep -q $MOD
if [ $? -ne 0 ]; then
   echo unload $MOD
   sudo rmmod -v $MOD
fi

echo load $MOD
if sudo insmod $MOD; then
   echo loaded $MOD
else
   echo failed to load $MOD
fi


popd

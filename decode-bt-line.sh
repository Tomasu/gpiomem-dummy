#!/bin/bash

#KERN_VER=`uname -r`
#VMLINUX_DEBUG=/usr/lib/debug/lib/modules/$KERN_VER/vmlinux
#VMLINUX_BUILD=/lib/modules/$KERN_VER/build/vmlinuz
#VMLINUX_BOOT=/boot/vmlinuz-$KVER
#VMLINUX=

#if [ -f $VMLINUX_DEBUG ]; then
#   VMLINUX=$VMLINUX_DEBUG
#elif [ -f $VMLINUX_BUILD ]; then
#   VMLINUX=$VMLINUX_BUILD
#else
#   VMLINUX=$VMLINUX_BOOT
#fi

#if [ -z "$VMLINUX" -o ! -f "$VMLINUX" ]; then
#   echo "no kernel image found"
#   exit 1
#fi

ARG=$1

if [ -z "$ARG" ]; then
   echo "usage: $1 *sym+0x000/000"
   exit 1
fi

gdb --batch -ex l "*$ARG" gpiomem_dummy.ko

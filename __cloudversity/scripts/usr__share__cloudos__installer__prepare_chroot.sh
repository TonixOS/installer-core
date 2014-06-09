#!/bin/bash -e

CHROOT_ROOT=$1

if [ ! -d "$1" ]; then
  echo "need the first argument to be our CHROOT_ROOT..."
  echo "won't do anything"
  exit 1
fi

mount -v --bind /dev $CHROOT_ROOT/dev
mount -vt devpts devpts $CHROOT_ROOT/dev/pts || true # might fail as we're just rebinding...
mount -vt proc proc $CHROOT_ROOT/proc
mount -vt sysfs sysfs $CHROOT_ROOT/sys

if [ -h /dev/shm ]; then
  rm -f $CHROOT_ROOT/dev/shm
  mkdir $CHROOT_ROOT/dev/shm
fi

mount -vt tmpfs shm $CHROOT_ROOT/dev/shm

echo 'ready!'
echo "please chroot now with:"
echo "  chroot $CHROOT_ROOT /usr/bin/env -i HOME=/root TERM="$TERM" PS1='\u:\w\\\$ ' PATH=/bin:/usr/bin:/sbin:/usr/sbin /bin/bash --login"

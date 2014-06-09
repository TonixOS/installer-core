#!/bin/bash -xe

# redirect all output to a log file
exec >/tmp/create_vdisk.log 2>&1
env|sort || true

# load loop, if not already loaded
lsmod | grep loop || modprobe loop max_part=5 max_loop=2 || true

IMG=/tmp/mgt.raw
INSTALLER_DIR=/usr/share/cloudversity/installer/core
TEMPLATE_NAME=management.template.qcow2

# copy image template to RAM
# prepare image template for usage
cp -v $INSTALLER_DIR/$TEMPLATE_NAME.xz /tmp/
unxz /tmp/$TEMPLATE_NAME.xz 
qemu-img convert -f qcow2 -O raw /tmp/$TEMPLATE_NAME $IMG

losetup --detach-all
losetup --partscan /dev/loop0 $IMG

echo "/dev/loop0" > /tmp/mgt_dest_disk

exit 0


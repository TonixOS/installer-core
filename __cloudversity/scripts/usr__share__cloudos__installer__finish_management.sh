#!/bin/bash

exec >/tmp/finish_mgt.log 2>&1

. /usr/lib/cloudos/libinstaller.lib.sh

IMG=/tmp/mgt.raw
ROOT_DIR=$INSTALL_DIR
INSTALLER_DIR=/usr/share/cloudos/installer

. $ROOT_DIR/etc/openstack/shell.config.sh

generate_passwords


cloudos_installer_replace_common_vars "$ROOT_DIR/etc/openstack/horizon/setup_settings.py"
assign_password
cloudos_search_replace_config __SECRET_KEY__ "$CURRENT_PW" $ROOT_DIR/etc/openstack/horizon/setup_settings.py

sync

for mountpoint in dev/shm dev/pts sys proc dev; do
  mountpoint $ROOT_DIR/$mountpoint && umount $ROOT_DIR/$mountpoint || true
done
umount $ROOT_DIR/

losetup -D

qemu-img convert -f raw -O qcow2 $IMG /tmp/cloudos/installer/host-disk/srv/management_root.qcow2

sync



#!/bin/bash -xe

# Requires:
#  - LOGFILE
#  - DEST_DISK
#  - SERVICE_DATA_SIZE
#  - INSTALL_DIR

. /usr/lib/cloudos/libdisk.lib.sh


# get max size
MAX_SIZE=`parted $DEST_DISK print | egrep "^Disk /" | egrep -o "([0-9.]+)[TGP]B" | egrep -o "([0-9\.]+)"`
PART_COUNTER=1
PREV_PART_POS="2048s"

PARTED="parted -s $DEST_DISK"

mkdir -p $INSTALL_DIR

#
# P A R T I T I O N T A B L E
#

# create partition table
#$PARTED mklabel $CONFIG_PART_MODE


if [ "$CONFIG_PART_MODE" = "gpt" ]; then
  # create buffer part
  $PARTED mkpart non-fs 2048s 100M
  PART_COUNTER=$(($PART_COUNTER+1))
  PREV_PART_POS="100M"
fi

DISK=$DEST_DISK

#
#  P A R T I T I O N S
#


if [ "$MGT_MODE" ]; then
  # R O O T
#  $PARTED mkpart primary ext2 200M 5G
#  $PARTED set $PART_COUNTER root on
#  $PARTED set $PART_COUNTER boot on
  #cloudos_disk_get_device_partition_file $DEST_DISK $PART_COUNTER
  #cloudos_disk_create_partition ext4 ROOT $INSTALL_DIR
  DISK_PARTITION=${DISK}p2
  mkfs -t ext4 ${DISK_PARTITION}
  ROOT_UUID=`tune2fs -l $DISK_PARTITION | grep UUID | egrep -o '([a-z0-9-]*)$'`
  ROOT_DISK=${DISK_PARTITION}
  ROOT_MOUNT=$INSTALL_DIR
  export ROOT_UUID ROOT_DISK ROOT_MOUNT
  mount -t ext4 $ROOT_DISK $ROOT_MOUNT

else
  #$PARTED mkpart primary linux-swap $PREV_PART_POS 200M
  cloudos_disk_get_device_partition_file $DEST_DISK $PART_COUNTER
  cloudos_disk_create_partition swap SWAP
  PART_COUNTER=$(($PART_COUNTER+1))

  # R O O T
#  $PARTED mkpart primary ext2 200M 5G
#  $PARTED set $PART_COUNTER root on
#  $PARTED set $PART_COUNTER boot on
  cloudos_disk_get_device_partition_file $DEST_DISK $PART_COUNTER
  cloudos_disk_create_partition ext4 ROOT $INSTALL_DIR
  PART_COUNTER=$(($PART_COUNTER+1))

  # TODO: unterscheide zwischen management und host, management bekommt ersteinmal kein LVM
  # S R V
  mkdir -p $INSTALL_DIR/srv

#  $PARTED mkpart primary ext2 5G ${MAX_SIZE}G
#  $PARTED set $PART_COUNTER lvm on
  cloudos_disk_get_device_partition_file $DEST_DISK $PART_COUNTER
  vgcreate vg_service_data $DISK_PARTITION
  sleep 2 # workaround
  lvcreate -L ${SERVICE_DATA_SIZE}G  -n lv_srv  vg_service_data
  DISK_PARTITION="/dev/vg_service_data/lv_srv"
  cloudos_disk_create_partition ext4 SRV $INSTALL_DIR/srv
fi


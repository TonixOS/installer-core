#!/bin/bash -xe

# redirect all output to a log file
exec >$LOGFILE 2>&1

echo "Will install cloudos to: $1"

env|sort


# if in management mode, we need to fetch our dest-disk from a file
# as create_vdisk will create special dev-names for our vdisk
# because grub-install won't work
if [ "$MGT_MODE" = "on" -a -f "/tmp/mgt_dest_disk" ]; then
  DEST_DISK=`cat /tmp/mgt_dest_disk`
fi

DISK=$DEST_DISK
ROOT_DIR=$INSTALL_DIR
RPMS_DIR="$ROOT_DIR/rpms"
# our installer binary needs the root dir too...
echo -n $ROOT_DIR > /tmp/INSTALLER_ROOT_DIR

INSTALLER_DIR=/usr/share/cloudversity/installer/core

. /usr/lib/cloudos/libinstaller.lib.sh
. /usr/share/cloudversity/installer/core/prepare_disk.sh


# ===========================
#     Prepare chroot
# ===========================

mkdir -pv $RPMS_DIR
cd $RPMS_DIR

# this will create 2 subdiretories:
#   prepare => stuff like filesystem rpms
#   system => all other rpms
tar -xvf /tmp/basesystem-rpms.tar

# if we should install some additional rpms (like the cloudos-installer for a live cd) too...
if [ -d /usr/share/cloudversity/installer/core/additional_rpms/ ]; then
  cp -av /usr/share/cloudversity/installer/core/additional_rpms/*.rpm core/
fi

cd $ROOT_DIR

# prepare MGT
if [ "$MGT_MODE" ]; then
  # mount boot too
  mkdir -pv $ROOT_DIR/boot
  mount -t ext2 ${DEST_DISK}p1 $ROOT_DIR/boot
fi

# extract all rpms to create the base system
# start with our filesystem structure
for pkg in $RPMS_DIR/prepare/*.rpm; do
  rpm2cpio $pkg | cpio -idvm
done

# and now all other rpms
for pkg in $RPMS_DIR/core/*.rpm; do
  rpm2cpio $pkg | cpio -idvm
done

# use our live-cd settings
cp /etc/shadow etc/shadow

#
# prepare chrooting
#
$INSTALLER_DIR/prepare_chroot.sh $ROOT_DIR

# ===========================
#          FIRST CHROOT
#  install all packages with zypper
# ===========================
cp $INSTALLER_DIR/zypper_install_rpms.sh $ROOT_DIR/tmp/
chmod +x $ROOT_DIR/tmp/zypper_install_rpms.sh
chroot $ROOT_DIR /tmp/zypper_install_rpms.sh /rpms "$MGT_MODE" "$HOST_MODE"

# configure grub
chroot $ROOT_DIR /usr/sbin/grub-mkconfig -o /boot/grub/grub.cfg

# install grub
if [ ! "$MGT_MODE" ]; then
  chroot $ROOT_DIR /usr/sbin/grub-install --debug ${DISK}
fi

# ===========================
#     generate /etc/fstab
# ===========================
#cloudos_search_replace_config __DISK_ROOT__ "UUID=$ROOT_UUID" $ROOT_DIR/etc/fstab
#cloudos_search_replace_config __DISK_SWAP__ "UUID=$SWAP_UUID" $ROOT_DIR/etc/fstab
#if [ "$MGT_MODE" ]; then
#  cloudos_search_replace_config "__DISK_SRV__.*" "" $ROOT_DIR/etc/fstab
#else
#  cloudos_search_replace_config __DISK_SRV__ "UUID=$SRV_UUID" $ROOT_DIR/etc/fstab
#fi
if [ "$MGT_MODE" ]; then
  # we might install our system in a virtual context, but need to be able to boot our vm
  # the vm itself will only have one vdisk attached, therefor correct the dev path in our destination env...
  sed -i "s#loop0p#vda#g" $ROOT_DIR/boot/grub/grub.cfg
  # we have to fix grubs "root" settings
  sed -i "s#set default=\"0\"#set default=\"0\"\nset root=hd0,msdos2#g" $ROOT_DIR/boot/grub/grub.cfg
  
  # remove srv and swap if we're installing the management vm
  cloudos_search_replace_config "__DISK_SWAP__.*" "" $ROOT_DIR/etc/fstab
  cloudos_search_replace_config "__DISK_SRV__.*" "" $ROOT_DIR/etc/fstab
fi


cloudos_search_replace_config __DISK_ROOT__ "$ROOT_DISK" $ROOT_DIR/etc/fstab
cloudos_search_replace_config __DISK_SWAP__ "$SWAP_DISK" $ROOT_DIR/etc/fstab
cloudos_search_replace_config __DISK_SRV__ "$SRV_DISK" $ROOT_DIR/etc/fstab

# TODO remove static
#CONFIG_HOST_IP="10.150.0.2/24"
# FIXME use values form installer
#CONFIG_PUBLIC_IP_POOL="10.150.0.0/24"
if [ "$MGT_MODE" ]; then
  echo "IP=10.151.0.2/24" > $ROOT_DIR/etc/network/circuits/eth0.interface
  echo "GATEWAY=10.151.0.1" >> $ROOT_DIR/etc/network/circuits/eth0.interface
  chroot $ROOT_DIR /usr/bin/systemctl enable network@eth0.service
  chroot $ROOT_DIR /usr/bin/systemctl disable cloudos-postinstall.service || true
#else
#  echo "IP=$CONFIG_HOST_IP" > $ROOT_DIR/etc/network/circuits/br-ex.interface
#  echo "GATEWAY=10.150.0.1" >> $ROOT_DIR/etc/network/circuits/br-ex.interface
#fi
#if [ "$HOST_MODE" -o "$MGT_MODE" ]; then
#  # we doesn't support the prefix of the ip from now on...
#  CONFIG_HOST_IP=`echo "$CONFIG_HOST_IP"| egrep -o '^([\.[0-9]+)'`
#  echo "CONFIG_MGT_MODE=$MGT_MODE"                           >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "CONFIG_HOST_IP=$CONFIG_HOST_IP"                      >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "CONFIG_DBMS_HOST=$CONFIG_HOST_IP"                    >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "CONFIG_KEYSTONE_IP=$CONFIG_HOST_IP"                  >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "CONFIG_RABBITMQ_IP=$CONFIG_HOST_IP"                  >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "CONFIG_GLANCE_IP=$CONFIG_HOST_IP"                    >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "CONFIG_QUANTUM_IP=$CONFIG_HOST_IP"                   >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "CONFIG_KEYMAP=$CONFIG_KEYMAP"                        >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "USER_ADMIN_NAME=operator"                            >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "CONFIG_PRIMARY_INTERFACE=$CONFIG_PRIMARY_INTERFACE"  >> $ROOT_DIR/etc/openstack/shell.config.sh
#  echo "CONFIG_PUBLIC_IP_POOL=$CONFIG_PUBLIC_IP_POOL"        >> $ROOT_DIR/etc/openstack/shell.config.sh
else
  echo "IP=$CONFIG_IP_HOST" > $ROOT_DIR/etc/network/circuits/br-ex.interface
  echo "GATEWAY=$CONFIG_IP_GATEWAY" >> $ROOT_DIR/etc/network/circuits/br-ex.interface
fi
CONFIG_HOST_IP="$CONFIG_IP_HOST"
if [ "$HOST_MODE" -o "$MGT_MODE" ]; then
  # we doesn't support the prefix of the ip from now on...
  CONFIG_HOST_IP=`echo "$CONFIG_HOST_IP"| egrep -o '^([\.[0-9]+)'`
  echo "CONFIG_MGT_MODE=$MGT_MODE"                           >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_HOST_IP=$CONFIG_HOST_IP"                      >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_IP_MGT=$CONFIG_IP_MGT"                        >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_IP_GATEWAY=$CONFIG_IP_GATEWAY"                >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_DBMS_HOST=$CONFIG_HOST_IP"                    >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_KEYSTONE_IP=$CONFIG_HOST_IP"                  >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_RABBITMQ_IP=$CONFIG_HOST_IP"                  >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_GLANCE_IP=$CONFIG_HOST_IP"                    >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_QUANTUM_IP=$CONFIG_HOST_IP"                   >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_KEYMAP=$CONFIG_KEYMAP"                        >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "USER_ADMIN_NAME=operator"                            >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_PRIMARY_INTERFACE=$CONFIG_PRIMARY_INTERFACE"  >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_PUBLIC_IP_POOL=$CONFIG_PUBLIC_IP_POOL"        >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_IP_POOL_START=$CONFIG_IP_POOL_START"          >> $ROOT_DIR/etc/openstack/shell.config.sh
  echo "CONFIG_IP_POOL_END=$CONFIG_IP_POOL_END"              >> $ROOT_DIR/etc/openstack/shell.config.sh
  
  # remove unwanted libvirt stuff
  rm $ROOT_DIR/etc/libvirt/qemu/networks/default.xml || true
  rm $ROOT_DIR/etc/libvirt/qemu/networks/autostart/default.xml || true
  
  SYSTEM_HOSTNAME=cloudos-host
  if [ "$MGT_MODE" ]; then
    SYSTEM_HOSTNAME=cloudos-mgt
    echo "ServerName $SYSTEM_HOSTNAME"        >> $ROOT_DIR/etc/apache-httpd/httpd.conf
    echo "ServerAdmin ninja@$SYSTEM_HOSTNAME" >> $ROOT_DIR/etc/apache-httpd/httpd.conf
  fi
  echo "$CONFIG_HOST_IP $SYSTEM_HOSTNAME" >> $ROOT_DIR/etc/hosts
  echo "$SYSTEM_HOSTNAME" > $ROOT_DIR/etc/hostname

  # set our dns servers
  echo "nameserver 78.138.97.33" >> $ROOT_DIR/etc/resolv.conf
  echo "nameserver 78.138.98.82" >> $ROOT_DIR/etc/resolv.conf

  # set root password
  echo "cloudos"  > /tmp/root_pw
  echo "cloudos" >> /tmp/root_pw
  chroot $ROOT_DIR passwd root < /tmp/root_pw
  rm /tmp/root_pw
fi

if [ "$HOST_MODE" ]; then
  # enable postinstall
  chroot $ROOT_DIR /usr/bin/systemctl enable cloudos-postinstall.service


  #
  # S E T U P   F I R E W A L L
  #
  iptables -I INPUT -m state --state RELATED,ESTABLISHED -j ACCEPT
  iptables -A INPUT -s $CONFIG_PUBLIC_IP_POOL -j ACCEPT
  iptables -A INPUT -i lo -j ACCEPT
  iptables -A INPUT -p icmp -j ACCEPT
  iptables -A INPUT -p tcp --dport 22 -j ACCEPT
  iptables -A INPUT -p tcp --dport 6080 -m comment --comment "allow access to VNC proxy" -j ACCEPT
  iptables -P INPUT DROP
  iptables-save -c > $ROOT_DIR/etc/network/firewall/ipv4.rules

  ip6tables -A INPUT -p icmp -j ACCEPT
  ip6tables -P INPUT DROP
  ip6tables-save -c > $ROOT_DIR/etc/network/firewall/ipv6.rules

fi

sync

# END
for mountpoint in dev/shm dev/pts sys proc dev boot; do
  mountpoint $ROOT_DIR/$mountpoint && umount $ROOT_DIR/$mountpoint || true
  sleep 1
done

# save our logfile to disk
cp /tmp/*.log /tmp/cloudversity/installer/core/host-disk/root/

sync

exit 0


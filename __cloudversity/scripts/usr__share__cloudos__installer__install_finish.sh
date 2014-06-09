#!/bin/bash -xe

# redirect all output to a log file
exec >>/tmp/installer.log 2>&1

ROOT_DIR=`cat /tmp/INSTALLER_ROOT_DIR`
INSTALLER_DIR=/usr/share/cloudos/installer

#
# prepare chrooting
#
$INSTALLER_DIR/prepare_chroot.sh $ROOT_DIR

# enable all configured interfaces
cat > $ROOT_DIR/tmp/enable_interfaces.sh <<"EOF"
#!/bin/bash
source /usr/lib/cloudos/libnetwork.lib.sh
cloudos_network_fetch_configured_interfaces
for i in $IFACE_CONFIGURED_LIST; do
  systemctl enable network\@$i.service
done
EOF

chmod +x $ROOT_DIR/tmp/enable_interfaces.sh
chroot $ROOT_DIR /tmp/enable_interfaces.sh

# enable our repository
#  -G => disable sign check
#  -f => refresh before using zypper
chroot $ROOT_DIR /usr/bin/zypper ar -G -f http://obs.internet.de:82/test/standard/ standard

# set root pw
echo "cloudos"  > /root/root_pw
echo "cloudos" >> /root/root_pw
chroot $ROOT_DIR passwd root < /root/root_pw
rm /root/root_pw


# END
for mountpoint in dev/shm sys proc dev/pts dev; do
  umount $ROOT_DIR/$mountpoint
done

# clean up some stuff:
rm -rv $ROOT_DIR/rpms
rm -rv $ROOT_DIR/usr/lib/{python2.7/test,debug}
rm -rv $ROOT_DIR/usr/share/locale/[a-c]*
rm -rv $ROOT_DIR/usr/share/locale/[f-z]*
rm -rv $ROOT_DIR/usr/share/locale/e{l,o,s,t,u}*
rm -v  $ROOT_DIR/var/cache/zypper/RPMS/*.rpm


#!/bin/bash -x

RPMS_DIR=$1

MGT_MODE=$2
HOST_MODE=$3

# --non-interactive => don't ask anything, use default answer
zypper --non-interactive install $RPMS_DIR/prepare/*.rpm $RPMS_DIR/core/*.rpm

# install management node packages
if [ "$MGT_MODE" ]; then
  zypper --non-interactive install $RPMS_DIR/openstack/*.rpm $RPMS_DIR/management/*.rpm
fi

# install host node packages
if [ "$HOST_MODE" ]; then
  zypper --non-interactive install $RPMS_DIR/openstack/*.rpm $RPMS_DIR/host/*.rpm

  # disable start-up scripts for postinstall
  for i in `ls /usr/lib/systemd/system/openstack-*`; do
    s=`echo $i | xargs basename`
    systemctl disable $s
  done
  systemctl disable rabbitmq-server mariadb openvswitch-ovsdb-server.service openvswitch-ovs-vswitchd.service || true
fi




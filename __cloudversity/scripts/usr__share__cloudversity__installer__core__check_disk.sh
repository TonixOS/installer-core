#!/bin/bash -xe

exec >/tmp/check_disk.log 2>&1

DISK=$DEST_DISK

# remove all visible volume groups which are belonging to our disk
while pvs --noheadings --separator : --options pv_name,vg_name | egrep "(^  $DISK[a-z0-9]{1,2}:)"; do
  # get volume group
  VG=`pvs --noheadings --separator : --options pv_name,vg_name | egrep "(^  $DISK[a-z0-9]{1,2}:)" | head -n 1 | cut -d: -f2`
  if [ "$VG" ]; then
    vgremove -f $VG
  else
    break
  fi
done

exit 0


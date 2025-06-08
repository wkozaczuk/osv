#!/bin/bash

DISK_PATH=$1
DISK_SIZE_IN_MB=${2-256}

echo $DISK_PATH
echo $DISK_SIZE_IN_MB

#Create empty file and ext filesystem on it
dd if=/dev/zero of=$DISK_PATH bs=1M count=$DISK_SIZE_IN_MB
sudo mkfs.ext4 $DISK_PATH

#Mounting Disk as Loop Device
LOOP_DEV=$(sudo losetup -o 0 -f --show $DISK_PATH 2>/dev/null| grep "/dev/loop")
mkdir $DISK_PATH.image
sudo mount $LOOP_DEV $DISK_PATH.image

#Copy files in the manifest
OSV_ROOT=$(dirname $(readlink -f $0))/..
EXPORT_DIR=$(readlink -f $DISK_PATH.image)
pushd "$OSV_ROOT/build/release"
sudo "$OSV_ROOT/scripts/export_manifest.py" -m usr.manifest -e $EXPORT_DIR -D libgcc_s_dir="$libgcc_s_dir"
popd

#Unmount
sudo umount $DISK_PATH.image
rmdir $DISK_PATH.image
sudo losetup -d $LOOP_DEV

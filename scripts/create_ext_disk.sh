#!/bin/bash

argv0=${0##*/}
usage() {
	cat <<-EOF
	Manipulate ext4 disk - create and optionally populate it, mount and unmount.

	Usage: ${argv0} create <disk_path> <disk_size> (<populate_bool>) |
	                            mount <disk_path> |
	                            unmount <disk_path> <loop_dev>

	Where:
	  disk_path       path to a new or existing ext4 disk file
	  disk_size_in_mb size of the disk when creating
	  loop_dev        full path to the loop device the disk is mounted (for example /dev/loop1)

	Examples:
	  ${argv0} create build/last/disk.ext $((256*1024*1024)) true   # Create disk at build/last/disk.ext and populate it with the files from usr.manifest
	  ${argv0} mount build/last/disk.ext                   # Mount OSv image from build/last/disk.ext
	  ${argv0} unmount build/last/disk.ext /dev/loop1      # Unmount OSv image mounted as /dev/loop1 device
	EOF
	exit ${1:-0}
}

create_disk() {
    if [[ "$DISK_PATH" == "" ]]; then
        echo "The disk_path has not been specified!"
        exit 1
    fi
    if [[ "$DISK_SIZE_IN_MB" == "" ]]; then
        echo "The disk_size_in_mb has not been specified!"
        exit 1
    fi
    #Create empty file and ext filesystem on it
    if [[ -f $DISK_PATH ]]; then
        echo "There is already file at $DISK_PATH. Please delete it first!"
        exit 1
    fi
    dd if=/dev/zero of=$DISK_PATH bs=1M count=$DISK_SIZE_IN_MB 2>1 1>/dev/null
    sudo -p "password for %p to run mkfs.ext4:" mkfs.ext4 $DISK_PATH 1>/dev/null
    echo "Created empty ext4 disk at $DISK_PATH of the size $DISK_SIZE_IN_MB mb"
}

populate_disk() {
    #Copy files in the manifest
    OSV_ROOT=$(dirname $(readlink -f $0))/..
    EXPORT_DIR=$(readlink -f $DISK_PATH.image)
    pushd "$OSV_ROOT/build/last" 1>/dev/null
    sudo "$OSV_ROOT/scripts/export_manifest.py" -m usr.manifest -e $EXPORT_DIR -D libgcc_s_dir="$libgcc_s_dir"
    popd 1>/dev/null
    echo "Populated ext4 disk $DISK_PATH"
}

mount_disk() {
    if [[ "$DISK_PATH" == "" ]]; then
        echo "The disk_path has not been specified!"
        exit 1
    fi
    #Mounting disk as a loop device
    if [[ -f "$DISK_PATH.image" ]]; then
        echo "There is already file at $DISK_PATH.image - disk is potentially mounted. Please delete it first!"
        exit 1
    fi
    if [[ $(losetup | grep "$DISK_PATH" | wc -l) != 0 ]]; then
        echo "The $DISK_PATH is already created and mounted. Please unmount and delete it first!"
        exit 1
    fi
    LOOP_DEV=$(sudo -p "password for %p to run losetup to create loop device:" losetup -o 0 -f --show $DISK_PATH 2>/dev/null| grep "/dev/loop")
    mkdir $DISK_PATH.image
    sudo mount $LOOP_DEV $DISK_PATH.image
    echo "Mounted the ext4 disk at $DISK_PATH.image under the loop device $LOOP_DEV"
}

unmount_disk() {
    if [[ "$DISK_PATH" == "" ]]; then
        echo "The disk_path has not been specified!"
        exit 1
    fi
    if [[ "$LOOP_DEV" == "" ]]; then
        echo "The loop_dev has not been specified!"
        exit 1
    fi
    sudo -p "password for %p to unmount disk:" umount $DISK_PATH.image
    rmdir $DISK_PATH.image
    sudo losetup -d $LOOP_DEV
}

command=$1
shift
case $command in 
    create)
        DISK_PATH=${1}
        DISK_SIZE_IN_MB=${2:-256}
        POPULATE=${3:-false}
        create_disk
        if [[ "$POPULATE" == "true" ]]; then
            mount_disk
            populate_disk
            unmount_disk
        fi
        ;;
    mount)
        DISK_PATH=${1}
        mount_disk
        ;;
    unmount)
        DISK_PATH=${1}
        LOOP_DEV=${2}
        unmount_disk
        ;;
    *)
        usage
        ;;
esac

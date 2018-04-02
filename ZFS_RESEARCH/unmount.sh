zfs unmount osv/zfs
sleep 1
zpool export osv
sleep 1
qemu-nbd --disconnect /dev/nbd0

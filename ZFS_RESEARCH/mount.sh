#modprobe nbd max_part=63
qemu-nbd --connect /dev/nbd0 build/release.x64/usr.img 
sleep 1
zpool import osv -N
sleep 1
zfs mount osv/zfs

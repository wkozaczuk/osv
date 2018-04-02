sudo guestfish -N disk:128M
sudo chown wkozaczuk:wkozaczuk test1.img 
qemu-img convert -O qcow2 test1.img test1_QCOW2.img
sudo qemu-nbd --connect /dev/nbd0 test1_QCOW2.img 
#sudo zpool create test1-zpool -d -m /dev/vlbk1 nbd0
sudo zpool create test1-zpool -d -m /test1 nbd0
### MOZE ZADZIALA
sudo zfs create test1-zpool/test1
#sudo zpool status -v
sudo umount test1-zpool/test1
sudo zpool export test1-zpool
#sudo qemu-nbd --disconnect /dev/nbd0

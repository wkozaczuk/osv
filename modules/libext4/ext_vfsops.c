/*
 * Copyright (C) 2024 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <sys/types.h>
#include <errno.h>

#include <osv/device.h>
#include <osv/bio.h>
#include <osv/prex.h>
#include <osv/vnode.h>
#include <osv/mount.h>
#include <osv/debug.h>

#include <ext4_blockdev.h>
#include <ext4.h>

int ext_init(void) { return 0;}

static int blockdev_open(struct ext4_blockdev *bdev)
{
    printf("[ext4] Called blockdev_open()\n");
    return EOK;
    //TODO: Should open here or below in ext4_mount()?
}

//TODO: Collapse into single re-usable function
static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
    struct bio *bio = alloc_bio();
    if (!bio)
        return ENOMEM;

    bio->bio_cmd = BIO_READ;
    bio->bio_dev = (struct device*)bdev->bdif->p_user;
    bio->bio_data = buf;
    bio->bio_offset = blk_id * bdev->bdif->ph_bsize;
    bio->bio_bcount = blk_cnt * bdev->bdif->ph_bsize;

    //TODO: Copy only if not malloced
    void *_buf = malloc(bio->bio_bcount);
    kprintf("[ext4] Trying to read %ld bytes at offset %ld to %p\n", bio->bio_bcount, bio->bio_offset, _buf);
    bio->bio_data = _buf;
    bio->bio_dev->driver->devops->strategy(bio);
    int error = bio_wait(bio);
    memcpy(buf, _buf, bio->bio_bcount);
    free(_buf);

    destroy_bio(bio);
    kprintf("[ext4] Read %ld bytes at offset %ld to %p\n", bio->bio_bcount, bio->bio_offset, _buf);

    return error;
}

static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf,
                           uint64_t blk_id, uint32_t blk_cnt)
{
    struct bio *bio = alloc_bio();
    if (!bio)
        return ENOMEM;

    bio->bio_cmd = BIO_WRITE;
    bio->bio_dev = (struct device*)bdev->bdif->p_user;
    //TODO bio->bio_data = buf;
    bio->bio_offset = blk_id * bdev->bdif->ph_bsize;
    bio->bio_bcount = blk_cnt * bdev->bdif->ph_bsize;

    bio->bio_dev->driver->devops->strategy(bio);
    int error = bio_wait(bio);

    destroy_bio(bio);

    return error;
}

static int blockdev_close(struct ext4_blockdev *bdev)
{
    return EOK;
}

EXT4_BLOCKDEV_STATIC_INSTANCE(ext_blockdev, 512, 0, blockdev_open,
                              blockdev_bread, blockdev_bwrite, blockdev_close, 0, 0);

extern struct vnops ext_vnops;

static int
ext_mount(struct mount *mp, const char *dev, int flags, const void *data)
{
    struct device *device;
    int error = -1;
    const char *dev_name;

    dev_name = "vblk1";//dev + 5;
    kprintf("[ext4] Trying to open device: [%s]\n", dev_name);
    error = device_open(dev_name, DO_RDWR, &device);

    if (error) {
        kprintf("[ext4] Error opening device!\n");
        return error;
    }

    ext4_device_register(&ext_blockdev, strdup(dev_name));
    //
    // Save a reference to our superblock
    //TODO mp->m_data = rofs;
    mp->m_dev = device;
    ext_blockdev.bdif->p_user = device;
    ext_blockdev.part_offset = 0;
    ext_blockdev.part_size = 134217728;
    ext_blockdev.bdif->ph_bcnt = ext_blockdev.part_size / ext_blockdev.bdif->ph_bsize;

    //TODO: Setup os_locks

    kprintf("[ext4] Trying to mount ext4 on device: [%s]\n", dev_name);
    auto ret = ext4_mount(dev_name, "/data/", false); 
    kprintf("[ext4] Mounted ext4 on device: [%s] with code:%d\n", dev_name, ret);
    return ret;
}

static int
ext_unmount(struct mount *mp, int flags)
{
    int ret = ext4_umount("/data/");
    kprintf("[ext4] Trying to unmount ext4 (after %d)!\n", ret);
    return device_close((struct device*)ext_blockdev.bdif->p_user);
}

static int
ext_sync(struct mount *mp)
{
    return EIO;
}

static int
ext_statfs(struct mount *mp, struct statfs *statp)
{
    return EIO;
}

// We are relying on vfsops structure defined in kernel
extern struct vfsops ext_vfsops;

// Overwrite "null" vfsops structure fields with "real"
// functions upon loading libext4.so shared object
void __attribute__((constructor)) initialize_vfsops() {
    ext_vfsops.vfs_mount = ext_mount;
    ext_vfsops.vfs_unmount = ext_unmount;
    ext_vfsops.vfs_sync = ext_sync;
    ext_vfsops.vfs_vget = ((vfsop_vget_t)vfs_nullop);
    ext_vfsops.vfs_statfs = ext_statfs;
    ext_vfsops.vfs_vnops = &ext_vnops;
}

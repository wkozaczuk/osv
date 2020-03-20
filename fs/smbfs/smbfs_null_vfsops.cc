/*
 * Copyright (C) 2018 Waldemar Kozaczuk.
 *
 * Based on ramfs code Copyright (c) 2006-2007, Kohsuke Ohtani
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <dlfcn.h>
#include <osv/mount.h>
#include <osv/debug.h>

#define smbfs_mount   ((vfsop_mount_t)vfs_nullop)
#define smbfs_umount  ((vfsop_umount_t)vfs_nullop)
#define smbfs_sync    ((vfsop_sync_t)vfs_nullop)
#define smbfs_vget    ((vfsop_vget_t)vfs_nullop)
#define smbfs_statfs  ((vfsop_statfs_t)vfs_nullop)

/*
 * File system operations
 *
 * This provides a "null" vfsops that is replaced when libsmbfs.so is
 * loaded upon VFS initialization.
 */
struct vfsops smbfs_vfsops = {
    smbfs_mount,      /* mount */
    smbfs_umount,     /* umount */
    smbfs_sync,       /* sync */
    smbfs_vget,       /* vget */
    smbfs_statfs,     /* statfs */
    nullptr,          /* vnops */
};

int smbfs_init(void)
{
    return 0;
}

/*
 * Copyright (C) 2024 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>

#include <osv/device.h>
#include <osv/prex.h>
#include <osv/vnode.h>
#include <osv/mount.h>
#include <osv/debug.h>

#include <ext4.h>

typedef	struct vnode vnode_t;
typedef	struct file file_t;
typedef struct uio uio_t;
typedef	off_t offset_t;
typedef	struct vattr vattr_t;

static int
ext_open(struct file *fp)
{
    return (EINVAL);
}

static int
ext_close(vnode_t *vp, file_t *fp)
{
    return (EINVAL);
}

static int
ext_read(vnode_t *vp, struct file* fp, uio_t *uio, int ioflag)
{
    void *buf;
    int ret;
    size_t read_count = 0;
    ext4_file *efile = (ext4_file *) vp->v_mount->m_data;

    /* Cant read directories */
    if (vp->v_type == VDIR)
        return EISDIR;

    /* Cant read anything but reg */
    if (vp->v_type != VREG)
        return EINVAL;

    /* Cant start reading before the first byte */
    if (uio->uio_offset < 0)
        return EINVAL;

    /* Need to read more than 1 byte */
    if (uio->uio_resid == 0)
        return 0;
    
    ext4_file f;
    f.mp = efile->mp;
    f.inode = efile->inode;
    f.flags = efile->flags;
    f.fpos = uio->uio_offset;

    buf = malloc(uio->uio_resid);
    ret = ext4_fread(&f, buf, uio->uio_resid, &read_count);
    if (ret) {
        kprintf("[ext_read] Error reading data\n");
        free(buf);
        return ret;
    }

    ret = uiomove(buf, read_count, uio);
    uio->uio_resid -= read_count;
    free(buf);
   
    return ret;
}

static int
ext_write(vnode_t *vp, uio_t *uio, int ioflag)
{
    void *buf;
    int ret;
    size_t write_count = 0;
    uio_t uio_copy = *uio;
    ext4_file *efile = (ext4_file *) vp->v_mount->m_data;

    /* Cant write directories */
    if (vp->v_type == VDIR)
        return EISDIR;

    /* Cant write anything but reg */
    if (vp->v_type != VREG)
        return EINVAL;

    /* Cant start writing before the first byte */
    if (uio->uio_offset < 0)
        return EINVAL;

    /* Need to write more than 1 byte */
    if (uio->uio_resid == 0)
        return 0;
    
    buf = malloc(uio->uio_resid);
    ret = uiomove(buf, uio->uio_resid, &uio_copy);
    if (ret) {
        kprintf("[ext_write] Error copying data\n");
        free(buf);
        return ret;
    }

    ext4_file f;
    f.mp = efile->mp;
    f.inode = efile->inode;
    f.flags = efile->flags;
    f.fpos = uio->uio_offset;

    ret = ext4_fwrite(&f, buf, uio->uio_resid, &write_count);

    uio->uio_resid -= write_count;
    free(buf);
   
    return ret;
}

static int
ext_seek(vnode_t *vp, file_t *fp, offset_t ooff, offset_t noffp)
{
    return (EINVAL);
}

static int
ext_ioctl(vnode_t *vp, file_t *fp, u_long com, void *data)
{
    return (EINVAL);
}

static int
ext_fsync(vnode_t *vp, file_t *fp)
{
    return (EINVAL);
}

static int
ext_readdir(struct vnode *dvp, struct file *fp, struct dirent *dir)
{
    return (EINVAL);
}

static int
ext_lookup(struct vnode *dvp, char *nm, struct vnode **vpp)
{
    return (EINVAL);
}

static int
ext_create(struct vnode *dvp, char *name, mode_t mode)
{
    return (EINVAL);
}

static int
ext_remove(struct vnode *dvp, struct vnode *vp, char *name)
{
    return (EINVAL);
}

static int
ext_rename(struct vnode *sdvp, struct vnode *svp, char *snm,
            struct vnode *tdvp, struct vnode *tvp, char *tnm)
{
    return (EINVAL);
}

static int
ext_mkdir(struct vnode *dvp, char *dirname, mode_t mode)
{
    return (EINVAL);
}

static int
ext_rmdir(vnode_t *dvp, vnode_t *vp, char *name)
{
    return (EINVAL);
}

static int
ext_getattr(vnode_t *vp, vattr_t *vap)
{
    return (EINVAL);
}

static int
ext_setattr(vnode_t *vp, vattr_t *vap)
{
    return (EINVAL);
}

static int
ext_inactive(vnode_t *vp)
{
    return (EINVAL);
}

static int
ext_truncate(struct vnode *vp, off_t new_size)
{
    return (EINVAL);
}

static int
ext_link(vnode_t *tdvp, vnode_t *svp, char *name)
{
    return (EINVAL);
}

static int
ext_arc(vnode_t *vp, struct file* fp, uio_t *uio)
{
    return (EINVAL);
}

static int
ext_fallocate(vnode_t *vp, int mode, loff_t offset, loff_t len)
{
    return (EINVAL);
}

static int
ext_readlink(vnode_t *vp, uio_t *uio)
{
    return (EINVAL);
}

static int
ext_symlink(vnode_t *dvp, char *name, char *link)
{
    return (EINVAL);
}

struct vnops ext_vnops = {
    ext_open,       /* open */
    ext_close,      /* close */
    ext_read,       /* read */
    ext_write,      /* write */
    ext_seek,       /* seek */
    ext_ioctl,      /* ioctl */
    ext_fsync,      /* fsync */
    ext_readdir,    /* readdir */
    ext_lookup,     /* lookup */
    ext_create,     /* create */
    ext_remove,     /* remove */
    ext_rename,     /* rename */
    ext_mkdir,      /* mkdir */
    ext_rmdir,      /* rmdir */
    ext_getattr,    /* getattr */
    ext_setattr,    /* setattr */
    ext_inactive,   /* inactive */
    ext_truncate,   /* truncate */
    ext_link,       /* link */
    ext_arc,        /* arc */
    ext_fallocate,  /* fallocate */
    ext_readlink,   /* read link */
    ext_symlink,    /* symbolic link */
};


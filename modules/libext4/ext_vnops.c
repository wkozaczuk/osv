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
#include <osv/file.h>

#include <ext4_errno.h>
#include <ext4_dir.h>
#include <ext4_inode.h>
#include <ext4_fs.h>

typedef	struct vnode vnode_t;
typedef	struct file file_t;
typedef struct uio uio_t;
typedef	off_t offset_t;
typedef	struct vattr vattr_t;

static int
ext_open(struct file *fp)
{
    kprintf("[ext4] Opening file\n");
    return (EOK);
}

static int
ext_close(vnode_t *vp, file_t *fp)
{
    kprintf("[ext4] Closing file\n");
    return (EOK);
}

static int
ext_internal_read(struct ext4_fs *fs, struct ext4_inode_ref *ref, uint64_t offset, void *buf, size_t size, size_t *rcnt)
{
    uint32_t unalg;
    uint32_t iblock_idx;
    uint32_t iblock_last;
    uint32_t block_size;

    ext4_fsblk_t fblock;
    ext4_fsblk_t fblock_start;
    uint32_t fblock_count;

    uint8_t *u8_buf = buf;
    int r;

    if (!size)
        return EOK;

    struct ext4_sblock *const sb = &fs->sb;

    if (rcnt)
        *rcnt = 0;

    /*Sync file size*/
    uint64_t fsize = ext4_inode_get_size(sb, ref->inode);

    block_size = ext4_sb_get_block_size(sb);
    size = ((uint64_t)size > (fsize - offset))
        ? ((size_t)(fsize - offset)) : size;

    iblock_idx = (uint32_t)((offset) / block_size);
    iblock_last = (uint32_t)((offset + size) / block_size);
    unalg = (offset) % block_size;

    if (unalg) {
        size_t len =  size;
        if (size > (block_size - unalg))
            len = block_size - unalg;

        r = ext4_fs_get_inode_dblk_idx(ref, iblock_idx, &fblock, true);
        if (r != EOK)
            goto Finish;

        /* Do we get an unwritten range? */
        if (fblock != 0) {
            uint64_t off = fblock * block_size + unalg;
            r = ext4_block_readbytes(fs->bdev, off, u8_buf, len);
            if (r != EOK)
                goto Finish;

        } else {
            /* Yes, we do. */
            memset(u8_buf, 0, len);
        }

        u8_buf += len;
        size -= len;
        offset += len;

        if (rcnt)
            *rcnt += len;

        iblock_idx++;
    }

    fblock_start = 0;
    fblock_count = 0;
    while (size >= block_size) {
        while (iblock_idx < iblock_last) {
            r = ext4_fs_get_inode_dblk_idx(ref, iblock_idx,
                               &fblock, true);
            if (r != EOK)
                goto Finish;

            iblock_idx++;

            if (!fblock_start)
                fblock_start = fblock;

            if ((fblock_start + fblock_count) != fblock)
                break;

            fblock_count++;
        }

        kprintf("[ext4] ext4_blocks_get_direct: block_start:%ld, block_count:%d\n", fblock_start, fblock_count);
        r = ext4_blocks_get_direct(fs->bdev, u8_buf, fblock_start,
                       fblock_count);
        if (r != EOK)
            goto Finish;

        size -= block_size * fblock_count;
        u8_buf += block_size * fblock_count;
        offset += block_size * fblock_count;

        if (rcnt)
            *rcnt += block_size * fblock_count;

        fblock_start = fblock;
        fblock_count = 1;
    }

    if (size) {
        uint64_t off;
        r = ext4_fs_get_inode_dblk_idx(ref, iblock_idx, &fblock, true);
        if (r != EOK)
            goto Finish;

        off = fblock * block_size;
        kprintf("[ext4] ext4_block_readbytes: off:%ld, size:%ld\n", off, size);
        r = ext4_block_readbytes(fs->bdev, off, u8_buf, size);
        if (r != EOK)
            goto Finish;

        offset += size;

        if (rcnt)
            *rcnt += size;
    }

Finish:
    return r;
}

static int
ext_read(vnode_t *vp, struct file *fp, uio_t *uio, int ioflag)
{
    kprintf("[ext4] Reading %ld bytes at offset:%ld from file i-node:%ld\n", uio->uio_resid, uio->uio_offset, vp->v_ino);
    void *buf;
    int ret;
    size_t read_count = 0;

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
    
    struct ext4_fs *fs = (struct ext4_fs *)vp->v_mount->m_data;
    struct ext4_inode_ref inode_ref;

    int r = ext4_fs_get_inode_ref(fs, vp->v_ino, &inode_ref);
    if (r != EOK) {
        return r;
    }

    // Total read amount is what they requested, or what is left
    uint64_t fsize = ext4_inode_get_size(&fs->sb, inode_ref.inode);
    uint64_t read_amt = fsize - uio->uio_offset > uio->uio_resid ? uio->uio_resid : fsize - uio->uio_offset;
    buf = malloc(read_amt);

    ret = ext_internal_read(fs, &inode_ref, uio->uio_offset, buf, read_amt, &read_count);
    if (ret) {
        kprintf("[ext_read] Error reading data\n");
        free(buf);
        ext4_fs_put_inode_ref(&inode_ref);
        return ret;
    }

    ret = uiomove(buf, read_count, uio);
    free(buf);

    ext4_fs_put_inode_ref(&inode_ref);

    return ret;
}

static int
ext_write(vnode_t *vp, uio_t *uio, int ioflag)
{/*
    void *buf;
    int ret;
    size_t write_count = 0;
    uio_t uio_copy = *uio;
    ext4_file *efile = (ext4_file *) vp->v_mount->m_data;*/

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
/*
    
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
   
    return ret;*/
    return (EINVAL);
}

static int
ext_seek(vnode_t *vp, file_t *fp, offset_t ooff, offset_t noffp)
{
    kprintf("[ext4] Seeking file i-node:%ld\n", vp->v_ino);
    return (EINVAL);
}

static int
ext_ioctl(vnode_t *vp, file_t *fp, u_long com, void *data)
{
    kprintf("[ext4] ioctl\n");
    return (EINVAL);
}

static int
ext_fsync(vnode_t *vp, file_t *fp)
{
    kprintf("[ext4] fsync\n");
    return (EINVAL);
}

static int
ext_readdir(struct vnode *dvp, struct file *fp, struct dirent *dir)
{
#define EXT4_DIR_ENTRY_OFFSET_TERM (uint64_t)(-1)
    struct ext4_fs *fs = (struct ext4_fs *)dvp->v_mount->m_data;
    struct ext4_inode_ref inode_ref;

    if (file_offset(fp) == EXT4_DIR_ENTRY_OFFSET_TERM) {
        return ENOENT;
    }

    int r = ext4_fs_get_inode_ref(fs, dvp->v_ino, &inode_ref);
    if (r != EOK) {
        return r;
    }

    /* Check if node is directory */
    if (!ext4_inode_is_type(&fs->sb, inode_ref.inode, EXT4_INODE_MODE_DIRECTORY)) {
        ext4_fs_put_inode_ref(&inode_ref);
        return ENOTDIR;
    }

    kprintf("[ext4] Reading directory with i-node:%ld at offset:%ld\n", dvp->v_ino, file_offset(fp));
    struct ext4_dir_iter it;
    int rc = ext4_dir_iterator_init(&it, &inode_ref, file_offset(fp));
    if (rc != EOK) {
        kprintf("[ext4] Reading directory with i-node:%ld at offset:%ld -> FAILED to init iterator\n", dvp->v_ino, file_offset(fp));
        ext4_fs_put_inode_ref(&inode_ref);
        return rc;
    }

    /* Test for non-empty directory entry */
    if (it.curr != NULL) {
        if (ext4_dir_en_get_inode(it.curr) != 0) {
            memset(dir->d_name, 0, sizeof(dir->d_name));
            uint16_t name_length = ext4_dir_en_get_name_len(&fs->sb, it.curr);
            memcpy(dir->d_name, it.curr->name, name_length);
            kprintf("[ext4] Reading directory with i-node:%ld at offset:%ld => entry name:%s\n", dvp->v_ino, file_offset(fp), dir->d_name);

            dir->d_ino = ext4_dir_en_get_inode(it.curr);

            uint8_t i_type = ext4_dir_en_get_inode_type(&fs->sb, it.curr);
            if (i_type == EXT4_DE_DIR) {
                dir->d_type = DT_DIR;
            } else if (i_type == EXT4_DE_REG_FILE) {
                dir->d_type = DT_REG;
            } else if (i_type == EXT4_DE_SYMLINK) {
                dir->d_type = DT_LNK;
            }

            ext4_dir_iterator_next(&it);

            off_t f_offset = file_offset(fp);
            dir->d_fileno = f_offset; //TODO: Does it matter?
            dir->d_off = f_offset + 1;
            file_setoffset(fp, it.curr ? it.curr_off : EXT4_DIR_ENTRY_OFFSET_TERM);
        } else {
            kprintf("[ext4] Reading directory with i-node:%ld at offset:%ld -> cos ni tak\n", dvp->v_ino, file_offset(fp));
        }
    } else {
        ext4_dir_iterator_fini(&it);
        ext4_fs_put_inode_ref(&inode_ref);
        kprintf("[ext4] Reading directory with i-node:%ld at offset:%ld -> ENOENT\n", dvp->v_ino, file_offset(fp));
        return ENOENT;
    }

    rc = ext4_dir_iterator_fini(&it);
    ext4_fs_put_inode_ref(&inode_ref);
    if (rc != EOK)
        return rc;

    return EOK;
}

static int
ext_lookup(struct vnode *dvp, char *nm, struct vnode **vpp)
{
    kprintf("[ext4] Looking up %s in directory with i-node:%ld\n", nm, dvp->v_ino);
    struct ext4_fs *fs = (struct ext4_fs *)dvp->v_mount->m_data;
    struct ext4_inode_ref inode_ref;

    int r = ext4_fs_get_inode_ref(fs, dvp->v_ino, &inode_ref);
    if (r != EOK) {
        return r;
    }

    /* Check if node is directory */
    if (!ext4_inode_is_type(&fs->sb, inode_ref.inode, EXT4_INODE_MODE_DIRECTORY)) {
        ext4_fs_put_inode_ref(&inode_ref);
        return ENOTDIR;
    }

    struct ext4_dir_search_result result;
    r = ext4_dir_find_entry(&result, &inode_ref, nm, strlen(nm));
    if (r == EOK) {
        uint32_t inode_no = ext4_dir_en_get_inode(result.dentry);
        vget(dvp->v_mount, inode_no, vpp);

        struct ext4_inode_ref inode_ref2;

        int r = ext4_fs_get_inode_ref(fs, inode_no, &inode_ref2);
        if (r != EOK) {
            return r;
        }
        uint32_t i_type = ext4_inode_type(&fs->sb, inode_ref2.inode);
        if (i_type == EXT4_INODE_MODE_DIRECTORY) {
            (*vpp)->v_type = VDIR; 
        } else if (i_type == EXT4_INODE_MODE_FILE) {
            (*vpp)->v_type = VREG;
        } else if (i_type == EXT4_INODE_MODE_SOFTLINK) {
            (*vpp)->v_type = VLNK;
        }
        ext4_fs_put_inode_ref(&inode_ref2);

        kprintf("[ext4] Looked up %s in directory with i-node:%ld as i-node:%d\n", nm, dvp->v_ino, inode_no);
    } else {
        r = ENOENT;
    }

    ext4_dir_destroy_result(&inode_ref, &result);
    ext4_fs_put_inode_ref(&inode_ref);

    return r;
}

static int
ext_create(struct vnode *dvp, char *name, mode_t mode)
{
    kprintf("[ext4] create\n");
    return (EINVAL);
}

static int
ext_remove(struct vnode *dvp, struct vnode *vp, char *name)
{
    kprintf("[ext4] remove\n");
    return (EINVAL);
}

static int
ext_rename(struct vnode *sdvp, struct vnode *svp, char *snm,
            struct vnode *tdvp, struct vnode *tvp, char *tnm)
{
    kprintf("[ext4] rename\n");
    return (EINVAL);
}

static int
ext_mkdir(struct vnode *dvp, char *dirname, mode_t mode)
{
    kprintf("[ext4] mkdir\n");
    return (EINVAL);
}

static int
ext_rmdir(vnode_t *dvp, vnode_t *vp, char *name)
{
    kprintf("[ext4] rmdir\n");
    return (EINVAL);
}

static int
ext_getattr(vnode_t *vp, vattr_t *vap)
{
    kprintf("[ext4] Getting attributes at i-node:%ld\n", vp->v_ino);
    struct ext4_fs *fs = (struct ext4_fs *)vp->v_mount->m_data;
    struct ext4_inode_ref inode_ref;

    int r = ext4_fs_get_inode_ref(fs, vp->v_ino, &inode_ref);
    if (r != EOK) {
        return r;
    }

    vap->va_mode = ext4_inode_get_mode(&fs->sb, inode_ref.inode);

    uint32_t i_type = ext4_inode_type(&fs->sb, inode_ref.inode);
    if (i_type == EXT4_INODE_MODE_DIRECTORY) {
       vap->va_type = VDIR; 
    } else if (i_type == EXT4_INODE_MODE_FILE) {
        vap->va_type = VREG;
    } else if (i_type == EXT4_INODE_MODE_SOFTLINK) {
        vap->va_type = VLNK;
    }

    vap->va_nodeid = vp->v_ino;
    vap->va_size = ext4_inode_get_size(&fs->sb, inode_ref.inode);
    kprintf("[ext4] getattr: va_size:%ld\n", vap->va_size);

    vap->va_atime.tv_sec = ext4_inode_get_access_time(inode_ref.inode);
    vap->va_mtime.tv_sec = ext4_inode_get_modif_time(inode_ref.inode);
    vap->va_ctime.tv_sec = ext4_inode_get_change_inode_time(inode_ref.inode);

    //auto *fsid = &vnode->v_mount->m_fsid; //TODO
    //attr->va_fsid = ((uint32_t)fsid->__val[0]) | ((dev_t) ((uint32_t)fsid->__val[1]) << 32);

    ext4_fs_put_inode_ref(&inode_ref);
    return (EOK);
}

static int
ext_setattr(vnode_t *vp, vattr_t *vap)
{
    kprintf("[ext4] setattr\n");
    return (EINVAL);
}

static int
ext_inactive(vnode_t *vp)
{
    kprintf("[ext4] inactive\n");
    return (EINVAL);
}

static int
ext_truncate(struct vnode *vp, off_t new_size)
{
    kprintf("[ext4] truncate\n");
    return (EINVAL);
}

static int
ext_link(vnode_t *tdvp, vnode_t *svp, char *name)
{
    kprintf("[ext4] link\n");
    return (EINVAL);
}

static int
ext_arc(vnode_t *vp, struct file* fp, uio_t *uio)
{
    kprintf("[ext4] arc\n");
    return (EINVAL);
}

static int
ext_fallocate(vnode_t *vp, int mode, loff_t offset, loff_t len)
{
    kprintf("[ext4] fallocate\n");
    return (EINVAL);
}

static int
ext_readlink(vnode_t *vp, uio_t *uio)
{
    kprintf("[ext4] readlink\n");
    return (EINVAL);
}

static int
ext_symlink(vnode_t *dvp, char *name, char *link)
{
    kprintf("[ext4] symlink\n");
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


/*
 * Copyright (C) 2018 Waldemar Kozaczuk
 * Based on NFS implementation Benoit Canet
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <sys/stat.h>
#include <dirent.h>
#include <sys/param.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <osv/prex.h>
#include <osv/vnode.h>
#include <osv/file.h>
#include <osv/debug.h>

#include <sys/types.h>
#include "smbfs.hh"

static inline struct smb2_context *get_smb2_context(struct vnode *node,
                                                    int &err_no)
{
    return smbfs::get_mount_context(node->v_mount, err_no)->smbfs();
}

static inline struct smb2fh *get_file_handle(struct vnode *node)
{
    return static_cast<struct smb2fh *>(node->v_data);
}

static inline struct smb2dir *get_dir_handle(struct vnode *node)
{
    return static_cast<struct smb2dir *>(node->v_data);
}

static const char *get_node_name(struct vnode *node)
{
    if (LIST_EMPTY(&node->v_names) == 1) {
        return nullptr;
    }

    auto name = LIST_FIRST(&node->v_names)->d_path;
    if (name && *name == '/')
        return name + 1;
    else
        return name;
}

static inline std::string mkpath(struct vnode *node, const char *name)
{
    std::string path(get_node_name(node));
    if (path.length() > 0)
       return path + "/" + name;
    else
       return name;
}

static inline struct timespec to_timespec(uint64_t sec, uint64_t nsec)
{
    struct timespec t;

    t.tv_sec = sec;
    t.tv_nsec = nsec;

    return t;
}

static int smbfs_open(struct file *fp)
{
    struct vnode *vp = file_dentry(fp)->d_vnode;

    // already opened reuse the nfs handle
    if (vp->v_data) {
        return 0;
    }

    // clear read write flags
    int flags = file_flags(fp);
    flags &= ~(O_RDONLY | O_WRONLY | O_RDWR);

    // check our rights
    bool read  = !vn_access(vp, VREAD);
    bool write = !vn_access(vp, VWRITE);

    // Set updated flags
    if (read && write) {
        flags |= O_RDWR;
    } else if (read) {
        flags |= O_RDONLY;
    } else if (write) {
        flags |= O_WRONLY;
    }

    int err_no;
    auto smb2 = get_smb2_context(vp, err_no);
    if (err_no) {
        return err_no;
    }

    std::string path(fp->f_dentry->d_path);
    //
    // It's a directory or a file.
    int type = vp->v_type;
    if (type == VDIR) {
        struct smb2dir *handle = smb2_opendir(smb2, path.c_str() + 1);
        if (handle) {
            vp->v_data = handle;
        }
        else {
            debugf("smbfs_open [%s]: smb2_opendir failed\n", path.c_str());
            return EIO;
        }
    } else if (type == VREG) {
        struct smb2fh *handle = smb2_open(smb2, path.c_str() + 1, flags);
        if (handle) {
            vp->v_data = handle;
        }
        else {
            debugf("smbfs_open [%s]: smb2_open failed\n", path.c_str());
            return EIO;
        }
    } else {
        return EIO;
    }

    return 0;
}

//TODO: Understand why nfs_close does not do anything - was it for a reason?
static int smbfs_close(struct vnode *vp, struct file *fp)
{
    // not opened - ignore
    if (!vp->v_data) {
        return 0;
    }

    int err_no;
    auto smb2 = get_smb2_context(vp, err_no);
    if (err_no) {
        return err_no;
    }

    int type = vp->v_type;
    //
    // It's a directory or a file.
    if (type == VDIR) {
        smb2_closedir(smb2, get_dir_handle(vp));
        vp->v_data = nullptr;
    } else if (type == VREG) {
        smb2_close(smb2, get_file_handle(vp));
        vp->v_data = nullptr;
    } else {
        return EIO;
    }

    return 0;
}

//
// This function reads as much data as requested per uio
static int smbfs_read(struct vnode *vp, struct file *fp, struct uio *uio, int ioflag)
{
    if (vp->v_type == VDIR) {
        return EISDIR;
    }
    if (vp->v_type != VREG) {
        return EINVAL;
    }
    if (uio->uio_offset < 0) {
        return EINVAL;
    }
    if (uio->uio_resid == 0) {
        return 0;
    }

    if (uio->uio_offset >= (off_t)vp->v_size) {
        return 0;
    }

    size_t len;
    if (vp->v_size - uio->uio_offset < uio->uio_resid)
        len = vp->v_size - uio->uio_offset;
    else
        len = uio->uio_resid;

    int err_no;
    auto smb2 = get_smb2_context(vp, err_no);
    if (err_no) {
        return err_no;
    }

    auto handle = get_file_handle(vp);

    // FIXME: remove this temporary buffer
    //TODO: libsmb2 claims zero-copy capability so I wonder if can advantage of
    //it somehow and avoid this memory copy
    auto buf = std::unique_ptr<unsigned char>(new unsigned char[len + 1]());
    int ret = smb2_pread(smb2, handle, buf.get(), len, uio->uio_offset);
    if (ret < 0) {
        return -ret;
    }

    return uiomove(buf.get(), ret, uio);
}

//
// This functions reads directory information (dentries) based on information in memory
// under rofs->dir_entries table
static int smbfs_readdir(struct vnode *vp, struct file *fp, struct dirent *dir)
{
    if (vp->v_type != VDIR) {
        return ENOTDIR;
    }

    int err_no;
    auto smb2 = get_smb2_context(vp, err_no);
    if (err_no) {
        return err_no;
    }

    // query the SMBFS server about this directory entry.
    auto handle = get_dir_handle(vp);
    auto smb2dirent = smb2_readdir(smb2, handle);

    //TODO: Please note that smb2_readdir adds full stat information
    //for each dirent. Would it be possible to somehow cache stat info
    //so that we do not have to make network trip for each smbfs_lookup?

    // We finished iterating on the directory.
    if (!smb2dirent) {
        return ENOENT;
    }

    // Fill dirent infos
    assert(sizeof(ino_t) == sizeof(smb2dirent->st.smb2_ino));
    dir->d_ino = smb2dirent->st.smb2_ino;
    if (smb2dirent->st.smb2_type == SMB2_TYPE_DIRECTORY ) {
        dir->d_type = DT_DIR;
    }
    else {
        dir->d_type = DT_REG;
    }
    // FIXME: not filling dir->d_off
    // FIXME: not filling dir->d_reclen

    strlcpy((char *) &dir->d_name, smb2dirent->name, sizeof(dir->d_name));

    // iterate
    fp->f_offset++;

    return 0;
}

//
// This functions looks up directory entry
static int smbfs_lookup(struct vnode *dvp, char *name, struct vnode **vpp)
{
    int err_no;
    auto smb2 = get_smb2_context(dvp, err_no);
    if (err_no) {
        return err_no;
    }

    std::string path = mkpath(dvp, name);
    struct vnode *vp;

    // Make sure we don't accidentally return garbage.
    *vpp = nullptr;

    // Following 4 checks inspired by ZFS code
    if (!path.size())
        return ENOENT;

    if (dvp->v_type != VDIR)
        return ENOTDIR;

    assert(path != ".");
    assert(path != "..");

    // We must get the inode number so we query the NFS server.
    struct smb2_stat_64 st;
    int ret = smb2_stat(smb2, path.c_str(), &st);
    if (ret) {
        debugf("smbfs_lookup [%s]: smb2_stat failed with %d\n", name, ret);
        return -ret;
    }

    // Filter by inode type: only keep files and directories
    // Symbolic links for now do not seem to be supported by smb2/3 or this library
    if (st.smb2_type != SMB2_TYPE_DIRECTORY && st.smb2_type != SMB2_TYPE_FILE) {
        debugf("smbfs_lookup [%s]: wrong type\n", name);
        // FIXME: Not sure it's the right error code.
        return EINVAL;
    }

    // Create the new vnode or get it from the cache.
    if (vget(dvp->v_mount, st.smb2_ino, &vp)) {
        // Present in the cache
        *vpp = vp;
        return 0;
    }

    if (!vp) {
        return ENOMEM;
    }

    // Fill in the new vnode informations.
    vp->v_size = st.smb2_size;
    vp->v_mount = dvp->v_mount;
    vp->v_data = nullptr;

    // Get the entry type.
    if (st.smb2_type == SMB2_TYPE_DIRECTORY) {
        vp->v_type = VDIR;
    } else if (st.smb2_type == SMB2_TYPE_FILE) {
        vp->v_type = VREG;
    }

    //TODO: vp->v_mode -> It looks like high level API does not have
    // a way to retrieve flags or any other security info
    // May need to use raw API

    *vpp = vp;

    return 0;
}

static int smbfs_getattr(struct vnode *vp, struct vattr *attr)
{
    int err_no;
    auto smb2 = get_smb2_context(vp, err_no);
    if (err_no) {
        return err_no;
    }

    auto path = get_node_name(vp);
    if (!path) {
        return ENOENT;
    }

    // Get the file infos.
    struct smb2_stat_64 st;
    int ret = smb2_stat(smb2, path, &st);
    if (ret) {
        debugf("smbfs_getattr [%s]: smb2_stat failed with %d\n", path, ret);
        return -ret;
    }

    if (st.smb2_type == SMB2_TYPE_DIRECTORY) {
        attr->va_type = VDIR;
    } else if (st.smb2_type == SMB2_TYPE_FILE) {
        attr->va_type = VREG;
    }

    //TODO: attr->va_mode -> It looks like high level API does not have a way to rerieve flags or any other security info
    // needs to use raw API
    attr->va_mode    = 0777;
    attr->va_nlink   = st.smb2_nlink;
    attr->va_nodeid  = st.smb2_ino;
    attr->va_atime   = to_timespec(st.smb2_atime, st.smb2_atime_nsec);
    attr->va_mtime   = to_timespec(st.smb2_mtime, st.smb2_mtime_nsec);
    attr->va_ctime   = to_timespec(st.smb2_ctime, st.smb2_ctime_nsec);
    attr->va_size    = st.smb2_size;

    return 0;
}

#define smbfs_write       ((vnop_write_t)vop_erofs)
#define smbfs_seek        ((vnop_seek_t)vop_nullop)
#define smbfs_ioctl       ((vnop_ioctl_t)vop_nullop)
#define smbfs_create      ((vnop_create_t)vop_erofs)
#define smbfs_remove      ((vnop_remove_t)vop_erofs)
#define smbfs_rename      ((vnop_rename_t)vop_erofs)
#define smbfs_mkdir       ((vnop_mkdir_t)vop_erofs)
#define smbfs_rmdir       ((vnop_rmdir_t)vop_erofs)
#define smbfs_setattr     ((vnop_setattr_t)vop_erofs)
#define smbfs_inactive    ((vnop_inactive_t)vop_nullop)
#define smbfs_truncate    ((vnop_truncate_t)vop_erofs)
#define smbfs_link        ((vnop_link_t)vop_erofs)
#define smbfs_arc         ((vnop_cache_t) nullptr)
#define smbfs_fallocate   ((vnop_fallocate_t)vop_erofs)
#define smbfs_fsync       ((vnop_fsync_t)vop_nullop)
#define smbfs_symlink     ((vnop_symlink_t)vop_erofs)
#define smbfs_readlink    ((vnop_readlink_t)vop_nullop)

struct vnops smbfs_vnops = {
    smbfs_open,       /* open */
    smbfs_close,      /* close */
    smbfs_read,       /* read */
    smbfs_write,      /* write - returns error when called */
    smbfs_seek,       /* seek */
    smbfs_ioctl,      /* ioctl */
    smbfs_fsync,      /* fsync */
    smbfs_readdir,    /* readdir */
    smbfs_lookup,     /* lookup */
    smbfs_create,     /* create - returns error when called */
    smbfs_remove,     /* remove - returns error when called */
    smbfs_rename,     /* rename - returns error when called */
    smbfs_mkdir,      /* mkdir - returns error when called */
    smbfs_rmdir,      /* rmdir - returns error when called */
    smbfs_getattr,    /* getattr */
    smbfs_setattr,    /* setattr - returns error when called */
    smbfs_inactive,   /* inactive */
    smbfs_truncate,   /* truncate - returns error when called*/
    smbfs_link,       /* link - returns error when called*/
    smbfs_arc,        /* arc */
    smbfs_fallocate,  /* fallocate - returns error when called*/
    smbfs_readlink,   /* read link */
    smbfs_symlink     /* symbolic link - returns error when called*/
};

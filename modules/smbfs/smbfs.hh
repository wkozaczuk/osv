/*
 * Copyright (C) 2018 Waldemar Kozaczuk
 * Based on NFS implementation by Benoit Canet
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef __INCLUDE_SMBFS_H__
#define __INCLUDE_SMBFS_H__

#include <string.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include <osv/mutex.h>
#include <osv/vnode.h>
#include <osv/mount.h>
#include <osv/dentry.h>
#include <osv/prex.h>

#include "smb2/smb2.h"
#include "smb2/libsmb2.h"

extern struct vfsops smbfs_vfsops;
extern struct vnops smbfs_vnops;

namespace smbfs {
    class mount_context {
    public:
        mount_context(const char *url);
        struct smb2_context *smbfs() {
            return _smbfs.get();
        };
        // Due to my particular hatred of exceptions the is_valid() method
        // can be used to now if something failed in the constructor.
        bool is_valid(int &err_no) {
            err_no = _errno;
            return _valid;
        };
        std::string server() {
            return std::string(_url->server);
        }
        std::string share() {
            return std::string(_url->path);
        }
        bool same_url(std::string url) {
            return url == _raw_url;
        }
    private:
        bool _valid;
        int _errno;
        std::string _raw_url;
        std::unique_ptr<struct smb2_context,
                void (*)(struct smb2_context *)> _smbfs;
        std::unique_ptr<struct smb2_url, void (*)(struct smb2_url *)> _url;
    };

    struct mount_point *get_mount_point(struct mount *mp);

    struct mount_context *get_mount_context(struct mount *mp, int &err_no);
}

#endif

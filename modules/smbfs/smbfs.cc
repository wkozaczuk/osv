/*
 * Copyright (C) 2018 Waldemar Kozaczuk
 * Based on NFS implementation by Benoit Canet
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/debug.hh>

#include "smbfs.hh"

namespace smbfs {

mount_context::mount_context(const char *url)
        : _valid(false),
          _errno(0),
          _raw_url(url),
          _smbfs(nullptr, smb2_destroy_context),
          _url(nullptr, smb2_destroy_url) {

    /* Create SMB context */
    _smbfs.reset(smb2_init_context());
    if (!_smbfs) {
        debug("mount_context(): failed to create SMB2/3 context\n");
        _errno = ENOMEM;
        return;
    }

    // parse the url while taking care of freeing it when needed
    _url.reset(smb2_parse_url(_smbfs.get(), url));
    if (!_url) {
        debug(std::string("mount_context(): failed to parse URL: ") +
              std::string(url) + ":" +
              smb2_get_error(_smbfs.get()) + "\n");
        _errno = EINVAL;
        return;
    }

    smb2_set_security_mode(_smbfs.get(), SMB2_NEGOTIATE_SIGNING_ENABLED);

    _errno = -smb2_connect_share(_smbfs.get(), _url->server, _url->share, _url->user);
    if (_errno) {
        debug(std::string("mount_context(): failed to mount share:g: ") +
              smb2_get_error(_smbfs.get()) + "\n");
        return;
    }

    _valid = true;
}

// __thread is not used for the following because it would give the error
// non-local variable ‘_lock’ declared ‘__thread’ has a non-trivial destructor
thread_local mutex _lock;
thread_local std::unordered_map <std::string,
std::unique_ptr<mount_context>> _map;

struct mount_context *get_mount_context(struct mount *mp, int &err_no) {
    auto m_path = static_cast<const char *>(mp->m_path);
    std::string mount_point(m_path);
    err_no = 0; //TODO Check when err_no is set as not zero

    SCOPE_LOCK(_lock);

    // if not mounted at all mount it
    if (!_map.count(mount_point)) {
        _map[mount_point] =
                std::unique_ptr<mount_context>(new mount_context(mp->m_special));
    }

    // if we remounted with a different url change the mount point
    if (!_map[mount_point]->same_url(mp->m_special)) {
        _map.erase(mount_point);
        _map[mount_point] =
                std::unique_ptr<mount_context>(new mount_context(mp->m_special));
    }

    // clear the mess if the mount point is invalid
    if (!_map[mount_point]->is_valid(err_no)) {
        _map.erase(mount_point);
        return nullptr;
    }

    return _map[mount_point].get();
}
}

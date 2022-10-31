#include <iostream>
#include <cassert>
#include <stdexcept>
#include <cstring>

#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>

#include "mounts.h"

// ----------------------------------------------------------------------

static const char* const _PATH_PROC_MOUNTINFO = "/proc/self/mountinfo";
static const char* const _PATH_PROC_MOUNTS    = "/proc/mounts";

bool utils::operator<(const MountPoint &x, const MountPoint &y)
{
    return x.source() < y.source() or x.target() < y.target();
}

void utils::MountPoint::print() const
{
    std::cout << "source: " << source() << ", "
              << "target: " << target() << ", "
              << "type: "   << type()   << "\n";
}

void utils::MountPoint::print_verbose() const
{
    std::cout << "src          : "
              << source() << "\n"

              << "src path     : "
              << mnt_fs_get_srcpath(entry_) << "\n"

              << "target       : "
              << target() << "\n"

              << "id           : "
              << mnt_fs_get_id(entry_) << "\n"

              // XXX: always 0?
              << "size         : "
              << mnt_fs_get_size(entry_) << "\n"

              // XXX: always 0?
              << "used size    : "
              << mnt_fs_get_usedsize(entry_) << "\n"

              << "parent id    : "
              << mnt_fs_get_parent_id(entry_) << "\n"

              << "device number: "
              << mnt_fs_get_devno(entry_) << "\n"

              // XXX: calling process' ID
              << "task id      : "
              << mnt_fs_get_tid(entry_) << "\n"

              << "swap area    : "
              << isSwapFs() << "\n"

              << "netfs        : "
              << isNetFs() << "\n"

              << "pseudo fs    : "
              << isPseudoFs() << "\n"

              << "fstype       : "
              << type() << "\n"

              << "options      : "
              << mnt_fs_get_options(entry_) << "\n"

              << "optional fields  : "
              << mnt_fs_get_optional_fields(entry_) << "\n"

              << "fs options   : "
              << mnt_fs_get_fs_options(entry_) << "\n"

              << "vfs options  : "
              << mnt_fs_get_vfs_options(entry_) << "\n"

              << "user options : "
              << str(mnt_fs_get_user_options(entry_)) << "\n"

              << "attributes   : "
              << str(mnt_fs_get_attributes(entry_)) << "\n"

              << "root         : "
              << mnt_fs_get_root(entry_) << "\n"

              << "bindsrc      : "
              << str(mnt_fs_get_bindsrc(entry_)) << "\n"

              << "swaptype     : "
              << str(mnt_fs_get_swaptype(entry_)) << "\n"

              << "comment      : "
              << str(mnt_fs_get_comment(entry_)) << "\n"

              << "\n";
}

std::string utils::MountPoint::str(const char *s) const
{
    return std::string(s ? s : "nil");
}

std::string utils::MountPoint::isSwapFs() const
{
    return mnt_fs_is_swaparea(entry_) ? "yes" : "no";
}

std::string utils::MountPoint::isNetFs() const
{
    return mnt_fs_is_netfs(entry_) ? "yes" : "no";
}

std::string utils::MountPoint::isPseudoFs() const
{
    return mnt_fs_is_pseudofs(entry_) ? "yes" : "no";
}

std::string utils::MountPoint::source() const
{
    const char *s = mnt_fs_get_srcpath(entry_);
    return s ? std::string(s) : "<nil>";
}

std::string utils::MountPoint::target() const
{
    const char *t = mnt_fs_get_target(entry_);
    return t ? std::string(t) : "<nil>";
}

std::string utils::MountPoint::type() const
{
    const char *t = mnt_fs_get_fstype(entry_);
    return t ? std::string(t) : "<nil>";
}

// ----------------------------------------------------------------------

static int parser_error_callback(struct libmnt_table *tb
                                 __attribute__ ((__unused__)),
                                 const char *filename,
                                 int line)
{
    throw std::runtime_error(std::string(filename) +
                             ": parse error at line " +
                             std::to_string(line));
}

void utils::MountPointsHelper::init(bool scan)
{
    mnt_init_debug(0);

    mount_table_ = mnt_new_table();

    if (!mount_table_)
    {
        throw std::runtime_error("mnt_new_table() returned null.");
    }

    mnt_table_set_parser_errcb(mount_table_, parser_error_callback);

    const char * const proc_mount_path =
        access(_PATH_PROC_MOUNTINFO, R_OK) == 0 ?
        _PATH_PROC_MOUNTINFO :
        _PATH_PROC_MOUNTS;

    if (mnt_table_parse_file(mount_table_, proc_mount_path) != 0)
    {
        throw std::runtime_error("mnt_table_parse_file() failed.");
    }

    libmnt_fs *entry = mnt_table_find_target(mount_table_,
                                             root_path_.c_str(),
                                             MNT_ITER_FORWARD);

    if (entry)
    {
        mount_points_.emplace(utils::MountPoint(entry));
    }

    if (scan)
    {
        recurse(entry);
    }
}

utils::MountPointsHelper::~MountPointsHelper()
{
    mnt_unref_table(mount_table_);
}

void utils::MountPointsHelper::reinit(std::string const & path)
{
    root_path_      = path;
    mnt_unref_table(mount_table_);
    init(false);
}

void utils::MountPointsHelper::recurse(libmnt_fs *entry)
{
    libmnt_iter *iter = mnt_new_iter(MNT_ITER_FORWARD);

    if (!iter)
    {
        throw std::runtime_error("mnt_new_iter() returned null");
    }

    libmnt_fs *child = NULL;

    while (mnt_table_next_child_fs(mount_table_, iter, entry, &child) == 0)
    {
        if (child)
        {
            mount_points_.emplace(utils::MountPoint(child));
            recurse(child);
        }
    }

    mnt_free_iter(iter);
}

std::string
utils::MountPointsHelper::get_parent_folder(std::string const & path)
{
    // See dirname(3) manpage.
    return dirname(const_cast<char *>(path.c_str()));
}

bool utils::MountPointsHelper::is_root_path(std::string const & path)
{
    char buffer[PATH_MAX];
    char *p = realpath(path.c_str(), buffer);

    if (p == nullptr)
        return false;

    return std::string(p) == "/";
}

const utils::MountPoint
utils::MountPointsHelper::get_parent_mount()
{
    auto parent = get_parent_folder(root_path_);
    reinit(parent);
    const auto mounts = get_mounts();

    // if path is root folder, and "mounts" is empty, we are in
    // trouble...
    assert(not (is_root_path(parent) and mounts.empty()));

    for (const auto m : mounts)
    {
        // When we are here, there should not be more than one item in
        // "mounts"; otherwise it is a logic error. In any case, just
        // return the first one we find.
        return m;
    }

    return get_parent_mount();
}

// ----------------------------------------------------------------------

utils::MountPointInfo utils::get_mount_point(std::string const &path)
{
    char buffer[PATH_MAX];
    const char *realp = realpath(path.data(), buffer);

    utils::MountPointsHelper mph(realp, false);

    // Return the first device from the set of mount points associated
    // with the given path.
    for (auto const m : mph.get_mounts())
    {
        return { m.source(), m.target(), m.type() };
    }

    // If MountPointsHelper::get_mounts() returned empty, find a
    // parent mount.
    auto p = mph.get_parent_mount();
    return { p.source(), p.target(), p.type() };
}

// Given a path, return associated path name.
std::string utils::get_device(std::string const &path)
{
    // auto mp = ;
    // return std::string(mp.source);
    return utils::get_mount_point(path).source;
}

// ----------------------------------------------------------------------
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

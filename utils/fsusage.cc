#include <cstring>
#include <cmath>
#include <sys/statvfs.h>

#include <limits.h>
#include <sys/types.h>
#include <sys/quota.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>

#include "utils.h"
#include "mounts.h"
#include "fsusage.h"

#include <linux/quota.h>

// ----------------------------------------------------------------------

static float percent_usage(unsigned long used, unsigned long max)
{
    if (max == 0)
        return 100.0f;

    float val = (float(used) / max) * 100.0f;

    // Limit precision to two digits.
    return roundf(val * 100.0f) / 100.0f;
}

// ----------------------------------------------------------------------

utils::fs_usage utils::get_quota_or_fs_usage(std::string const &path,
                                             std::string const &user)
{
    if (quota_enabled(path))
    {
        if (user.empty())
        {
            auto err = "No username given; can't check quota";
            utils::slog() << "[utils/fsusage] " << err;
            throw std::runtime_error(err);
        }

        return utils::get_quota_usage(path, user);
    }

    return utils::get_fs_usage(path);
}

// ----------------------------------------------------------------------

utils::fs_usage utils::get_fs_usage(std::string const &path)
{
    struct statvfs  statbuf;

    int result = statvfs(path.data(), &statbuf);

    if (result < 0)
    {
        std::stringstream err;
        err << "Error on usage of " << path << ": " << strerror(errno);

        utils::slog() << "[utils/fsusage] " << err.str();
        throw std::runtime_error(err.str());
    }

    utils::slog() << "[utils/fsusage] statvfs(" << path << ") => "
                  << "blocksize: "        << statbuf.f_bsize   << ", "
                  << "fragment size: "    << statbuf.f_frsize  << ", "
                  << "blocks on fs: "     << statbuf.f_blocks  << ", "
                  << "free blocks: "      << statbuf.f_bfree   << ", "
                  << "available blocks: " << statbuf.f_bavail  << ", "
                  << "number of inodes: " << statbuf.f_files   << ", "
                  << "free inodes: "      << statbuf.f_ffree   << ", "
                  << "available inodes: " << statbuf.f_favail  << ", "
                  << "filesystem id: "    << statbuf.f_fsid    << ", "
                  << "max path length: "  << statbuf.f_namemax;

    utils::fs_usage usage;
    usage.type = "filesystem usage";

    usage.block_size       = statbuf.f_bsize;
    usage.fragment_size    = statbuf.f_frsize;
    usage.total_blocks     = statbuf.f_blocks;
    usage.free_blocks      = statbuf.f_bfree;
    usage.available_blocks = statbuf.f_bavail;
    usage.total_size       = statbuf.f_blocks * statbuf.f_frsize;
    usage.free_size        = statbuf.f_bfree * statbuf.f_frsize;
    usage.available_size   = statbuf.f_bavail * statbuf.f_frsize;
    usage.total_inodes     = statbuf.f_files;
    usage.free_inodes      = statbuf.f_ffree;
    usage.available_inodes = statbuf.f_favail;

    auto used_blocks   = usage.total_blocks - usage.available_blocks;
    usage.blocks_usage = percent_usage(used_blocks, usage.total_blocks);
    auto used_size     = usage.total_size - usage.available_size;
    usage.size_usage   = percent_usage(used_size, usage.total_size);
    auto used_inodes   = usage.total_inodes - usage.available_inodes;
    usage.inode_usage  = percent_usage(used_inodes, usage.total_inodes);

    return usage;
}

// ----------------------------------------------------------------------

bool utils::quota_enabled(std::string const &path)
{
    auto device = utils::get_device(path);
    utils::slog() << "Checking quota on device " << device;

    // Note that "id" argument in quotactl() is ignored for Q_GETINFO
    // subcommand.  See quotactl(2) man page.
    struct dqinfo info;
    int           rc = 0;
    int           id = 0;

    rc = quotactl(QCMD(Q_GETINFO,USRQUOTA),
                  device.data(),
                  id,
                  caddr_t(&info));

    if (rc == 0 and info.dqi_valid)
    {
        utils::slog() << "User quota is enabled on device " << device;
        return info.dqi_valid;
    }
    else
    {
        utils::slog() << "quotactl(QCMD(Q_GETINFO,USRQUOTA),..) failed on "
                      << device << "; errno: " << errno << " ("
                      << strerror(errno) << ")";
    }

    rc = quotactl(QCMD(Q_GETINFO,GRPQUOTA),
                  device.data(),
                  id,
                  caddr_t(&info));

    if (rc == 0 and info.dqi_valid)
    {
        utils::slog() << "Group quota is enabled on device " << device;
        return info.dqi_valid;
    }
    else
    {
        utils::slog() << "quotactl(QCMD(Q_GETINFO,GRPQUOTA),..) failed on "
                      << device << "; errno: " << errno << " ("
                      << strerror(errno) << ")";
    }

#if defined PRJQUOTA
    rc = quotactl(QCMD(Q_GETINFO,PRJQUOTA),
                  device.data(),
                  id,
                  caddr_t(&info));

    if (rc == 0 and info.dqi_valid)
    {
        utils::slog() << "Project quota is enabled on device " << device;
        return info.dqi_valid;
    }
    else
    {
        utils::slog() << "quotactl(QCMD(Q_GETINFO,PRJQUOTA),..) failed on "
                      << device << "; errno: " << errno << " ("
                      << strerror(errno) << ")";
    }
#endif

    return false;
}

// ----------------------------------------------------------------------

static uid_t get_uid(std::string const &user)
{
    // Find buffer size needed for getpwnam_r().
    auto bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);

    if (bufsize == -1)
    {
        bufsize = 16384;
    }

    char *buf = new char[bufsize];

    if (!buf)
    {
        std::stringstream ss;
        ss << "Could not allocate " << bufsize << " bytes.";
        throw std::runtime_error(ss.str());
    }

    struct passwd  pwd;
    struct passwd *result = nullptr;

    int rc = getpwnam_r(user.data(), &pwd, buf, bufsize, &result);

    delete[] buf;

    if (result == nullptr)
    {
        std::stringstream ss;

        if (rc == 0)
        {
            ss << "Could not find user " << user;
        }
        else
        {
            ss << "Could not find user " << user
               << "; error: " << strerror(errno);
        }

        throw std::runtime_error(ss.str());
    }

    return pwd.pw_uid;
}

static utils::fs_usage get_xfs_quota_usage(std::string const &device,
                                           uid_t uid)
{
    throw std::runtime_error("XFS quota is unimplemented");

    // TODO: fill this structure.
    return utils::fs_usage{};
}

static utils::fs_usage get_generic_quota_usage(std::string const &path,
                                               std::string const &device,
                                               uid_t uid)
{
    struct dqblk dqb;

    int rc = quotactl(QCMD(Q_GETQUOTA,USRQUOTA),
                      device.data(),
                      uid,
                      caddr_t(&dqb));

    if (rc != 0)
    {
        std::stringstream ss;
        ss << "quotactl(QCMD(Q_GETQUOTA,USRQUOTA),..) failed on "
           << device << "; errno: " << errno << " ("
           << strerror(errno) << ")";
        throw std::runtime_error(ss.str());
    }

    utils::slog() << "quotactl(QCMD(Q_GETQUOTA,USRQUOTA),..) results: {"
                  << " block size : " << QIF_DQBLKSIZE << ", "
                  << " blocks hard limit : " << dqb.dqb_bhardlimit << ","
                  << " blocks soft limit : " << dqb.dqb_bsoftlimit << ", "
                  << " current space use : " << dqb.dqb_curspace   << ", "
                  << " inodes hard limit : " << dqb.dqb_ihardlimit << ", "
                  << " inodes soft limit : " << dqb.dqb_isoftlimit << ", "
                  << " current inode use : " << dqb.dqb_curinodes  << "}";

    // TODO: determine what we should be really considering -- hard
    // limit or soft limit?

    utils::fs_usage usage;
    memset(&usage, 0, sizeof(usage));

    // Of blocks, quota manpage says this:
    //
    // "By default space usage and limits are shown in kbytes (and are
    // named blocks for historical reasons)."
    //
    // I suppose it would be reasonable to assume that QIF_DQBLKSIZE
    // is what we should be using, and not filesystem block or
    // fragment sizes.
    usage.type             = "quota";
    usage.quota_limit_type = "soft";
    usage.block_size       = QIF_DQBLKSIZE;
    usage.fragment_size    = QIF_DQBLKSIZE;

    // Usage in terms of blocks.  Note that dqb.dqb_curspace is in
    // bytes, so we convert it to blocks.
    auto used_blocks       = (dqb.dqb_curspace / QIF_DQBLKSIZE);

    if (dqb.dqb_bsoftlimit != 0)
    {
        usage.total_blocks     = dqb.dqb_bsoftlimit;
        usage.free_blocks      = dqb.dqb_bsoftlimit - used_blocks;
        usage.available_blocks = dqb.dqb_bsoftlimit - used_blocks;
        usage.blocks_usage     = percent_usage(used_blocks, usage.total_blocks);

        // Usage in terms of bytes.
        usage.total_size       = dqb.dqb_bsoftlimit * QIF_DQBLKSIZE;
        usage.free_size        = usage.total_size - dqb.dqb_curspace;
        usage.available_size   = usage.total_size - dqb.dqb_curspace;
        usage.size_usage       = percent_usage(dqb.dqb_curspace, usage.total_size);
    }

    // Usage in terms of inodes.
    if (dqb.dqb_isoftlimit != 0)
    {
        usage.total_inodes     = dqb.dqb_isoftlimit;
        usage.free_inodes      = dqb.dqb_isoftlimit - dqb.dqb_curinodes;
        usage.available_inodes = dqb.dqb_isoftlimit - dqb.dqb_curinodes;
        usage.inode_usage      = percent_usage(dqb.dqb_curinodes, usage.total_inodes);
    }

    if (dqb.dqb_bsoftlimit == 0 or dqb.dqb_isoftlimit == 0)
    {
        utils::slog() << "[utils/fsusage] quota limit is set to 0 "
                      << "(dqb.dqb_bsoftlimit: " << dqb.dqb_bsoftlimit << ", "
                      << "dqb.dqb_isoftlimit: " << dqb.dqb_isoftlimit << "); "
                      << " using statvfs().";

        auto const fsusage = utils::get_fs_usage(path);

        if (dqb.dqb_bsoftlimit == 0)
        {
            usage.block_size       = fsusage.block_size;
            usage.fragment_size    = fsusage.fragment_size;
            usage.total_blocks     = fsusage.total_blocks;
            usage.free_blocks      = fsusage.free_blocks;
            usage.available_blocks = fsusage.available_blocks;
            usage.blocks_usage     = fsusage.blocks_usage;
            usage.total_size       = fsusage.total_size;
            usage.free_size        = fsusage.free_size;
            usage.available_size   = fsusage.available_size;
            usage.size_usage       = fsusage.size_usage;
        }

        if (dqb.dqb_isoftlimit == 0)
        {
            usage.total_inodes     = fsusage.total_inodes;
            usage.free_inodes      = fsusage.free_inodes;
            usage.available_inodes = fsusage.available_inodes;
            usage.inode_usage      = fsusage.inode_usage;
        }
    }

#if 0
    // We don't use grace periods for now, but we can enable them when
    // we need it.  The returned values dqi_bgrace and dqi_igrace will
    // be in seconds.

    struct dqinfo dqi;

    rc = quotactl(QCMD(Q_GETINFO,USRQUOTA),
                  device.data(),
                  uid,
                  caddr_t(&dqi));

    if (rc != 0)
    {
        std::stringstream ss;
        ss << "quotactl(QCMD(Q_GETINFO,USRQUOTA),..) failed on "
           << device << "; errno: " << errno << " ("
           << strerror(errno) << ")";
        throw std::runtime_error(ss.str());
    }

    utils::slog() << "block grace limit: " << dqi.dqi_bgrace << ", "
                  << "inode grace limit: " << dqi.dqi_igrace << ".";
#endif

    return usage;
}

// ----------------------------------------------------------------------

utils::fs_usage utils::get_quota_usage(std::string const &path,
                                       std::string const &user)
{
    auto const uid   = get_uid(user);
    auto const mount = utils::get_mount_point(path);

    utils::slog() << "Mount point for \"" << path << "\": {"
                  << "source: " << mount.source << ", "
                  << "target: " << mount.target << ", "
                  << "type: " << mount.type << "}.";

    // Based on some initial testing, special handling for XFS volumes
    // may not be necessary.  Disabling that for now.

    // if (mount.type == "xfs")
    // {
    //     return get_xfs_quota_usage(mount.source, uid);
    // }

    return get_generic_quota_usage(path, mount.source, uid);
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream &out, utils::fs_usage &usage)
{
    out << " - Usage type       : " << usage.type             << "\n"
        << " - Quota limit type : " << usage.quota_limit_type << "\n"
        << " - Block size       : " << usage.block_size       << "\n"
        << " - Fragment size    : " << usage.fragment_size    << "\n"
        << " - Total blocks     : " << usage.total_blocks     << "\n"
        << " - Free blocks      : " << usage.free_blocks      << "\n"
        << " - Available blocks : " << usage.available_blocks << "\n"
        << " - Blocks usage     : " << usage.blocks_usage     << "%\n"
        << " - Total size       : " << usage.total_size       << "\n"
        << " - Free size        : " << usage.free_size        << "\n"
        << " - Available size   : " << usage.available_size   << "\n"
        << " - Size usage       : " << usage.size_usage       << "%\n"
        << " - Total inodes     : " << usage.total_inodes     << "\n"
        << " - Free inodes      : " << usage.free_inodes      << "\n"
        << " - Available inodes : " << usage.available_inodes << "\n"
        << " - Inode usage      : " << usage.inode_usage      << "%\n\n";

    return out;
}

// ----------------------------------------------------------------------
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

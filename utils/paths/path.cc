#include <string>
#include <exception>
#include <stdexcept>
#include <cstring>
#include <iostream>

#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

#include "path.h"

void utils::Path::init()
{
    struct stat st{0};

    if (lstat(path_.c_str(), &st) != 0)
    {
        size_ = 0;
        uid_  = 0;
        gid_  = 0;
        mode_ = 0;

        switch (errno)
        {
        case ENOENT:
            type_ = PathType::DoesNotExist;
            break;
        default:
            type_ = PathType::Error;
            break;
        }

        return;
    }

    size_ = st.st_size;
    uid_  = st.st_uid;
    gid_  = st.st_gid;
    mode_ = st.st_mode;

    switch (st.st_mode & S_IFMT)
    {
    case S_IFREG:
        type_ = PathType::RegularFile;
        break;
    case S_IFDIR:
        type_ = PathType::Directory;

        // We are not really interested in the on-disk size of
        // directories here: they vary depending on filesystem
        // implementation, and we use these sizes for other
        // computations (such as estimating file transfers) anyway, so
        // assuming directory size to be 0 is a useful approximation.
        // Same approach could be applied to UNIX special files too,
        // but we have not even began to consider them...
        size_ = 0;

        break;
    case S_IFBLK:
        type_ = PathType::BlockFile;
        break;
    case S_IFCHR:
        type_ = PathType::CharacterFile;
        break;
    case S_IFIFO:
        type_ = PathType::Fifo;
        break;
    case S_IFLNK:
        type_ = PathType::SymbolicLink;
        break;
    case S_IFSOCK:
        type_ = PathType::Socket;
        break;
    default:
        type_ = PathType::Unknown;
        break;
    }
}

bool utils::Path::readable_by(std::string const & user) const
{
    passwd  pwd;
    passwd *result         = nullptr;
    char    buffer[BUFSIZ] = {0};

    auto ret = getpwnam_r(user.c_str(), &pwd, buffer, BUFSIZ, &result);

    if (result == nullptr)
    {
        if (ret == 0)
            throw std::runtime_error("no such user: " + user);

        throw std::runtime_error("getpwnam_r() failed");
    }

    return readable_by(pwd.pw_uid, pwd.pw_gid);
}

bool utils::Path::readable_by(uid_t uid, gid_t gid) const
{
    if ((uid == uid_ and (mode_ & S_IRUSR)) or
        (gid == gid_ and (mode_ & S_IRGRP)) or
        (mode_ & S_IROTH)) {
        return true;
    }

    return false;
}

bool utils::Path::ok() const
{
     return type() != PathType::Error and
         type() != PathType::DoesNotExist;
}

bool utils::Path::exists() const
{
    return type() != PathType::DoesNotExist;
}

bool utils::Path::is_directory() const
{
    return S_ISDIR(mode_);
}

bool utils::Path::is_regular_file() const
{
    return S_ISREG(mode_);
}

bool utils::Path::is_symbolic_link() const
{
    return S_ISLNK(mode_);
}

std::string utils::Path::join(std::string const &other) const
{
    return join_paths(path_, other);
}

std::string utils::join_paths(std::string const & a, std::string const & b)
{
    if (a.empty())
        return b;

    if (b.empty())
        return a;

    auto a2(a);

    const auto pathsep = "/";

    // trim trailing slashes from a
    while (a2.length() > 1 and a2.find_last_of(pathsep) == a2.length() - 1)
        a2.erase(a2.length() - 1);

    auto b2(b);

    // trim leading slashes from b
    while (b2.length() > 1 and b2.find(pathsep) == 0)
        b2.erase(b2.begin());

    // trim trailing slashes from b
    while (b2.length() > 1 and b2.find_last_of(pathsep) == b2.length() - 1)
        b2.erase(b2.length() - 1);

    if (b2 == pathsep)
        return a2;

    if (a2 == pathsep)
        return a2 + b2;

    return a2 + pathsep + b2;
}

const std::string utils::Path::canonical_name() const
{
    char buf1[PATH_MAX];
    char *realp  = realpath(path_.c_str(), buf1);

    if (!realp)
    {
        auto err = "realpath(" + path_ + ") error: " + strerror(errno);
        throw std::runtime_error(err);
    }

    if (is_directory())
        return std::string(realp) + "/";

    return std::string(realp);
}

const std::string utils::Path::base_name() const
{
    char buffer[PATH_MAX]{0};
    strncpy(buffer, path_.c_str(), path_.length());

    return basename(buffer);
}

const std::string utils::Path::dir_name() const
{
    char buffer[PATH_MAX]{0};

    auto path = canonical_name();
    strncpy(buffer, path.c_str(), path.length());

    return std::string(dirname(buffer)) + "/";
}

const utils::Path utils::Path::follow_sybolic_link() const
{
    if (not is_symbolic_link())
    {
        throw std::runtime_error("not symbolic link");
    }

    char buffer[PATH_MAX]{0};
    char *p = realpath(path_.c_str(), buffer);

    if (p == nullptr)
    {
        auto status = "realpath() on " + path_ + " failed; reason: "
            + strerror(errno);
        throw std::runtime_error(status);
    }

    return Path(p);
}


std::ostream& operator<<(std::ostream& out, const utils::PathType& type)
{
    switch (type)
    {
    case utils::PathType::Error:         out << "Error"         ; break;
    case utils::PathType::DoesNotExist:  out << "Does not exist"; break;
    case utils::PathType::RegularFile:   out << "Regular file"  ; break;
    case utils::PathType::Directory:     out << "Directory"     ; break;
    case utils::PathType::SymbolicLink:  out << "Symbolic link" ; break;
    case utils::PathType::HardLink:      out << "Hard link"     ; break;
    case utils::PathType::Fifo:          out << "FIFO"          ; break;
    case utils::PathType::CharacterFile: out << "Character file"; break;
    case utils::PathType::BlockFile:     out << "Block file"    ; break;
    case utils::PathType::Socket:        out << "Socket"        ; break;
    case utils::PathType::Unknown:       out << "Unknown"       ; break;
    }
    return out;
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

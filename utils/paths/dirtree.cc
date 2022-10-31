#include <iostream>
#include <stdexcept>
#include <cstring>
#include <sstream>

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>

#include "dirtree.h"

// ----------------------------------------------------------------------

// Using thread local storage seems to be the best way to use nftw()
// from C++.  Reason being: (1) nftw()'s callback function cannot
// access member variables, and (2) there is no simple way to set up a
// member function as callback.

static thread_local size_t g_dir_size;

static int update_size(const char        *fpath,
                       const struct stat *sb,
                       int                tflag,
                       struct FTW        *ftwbuf)
{
    if (tflag == FTW_F)
        g_dir_size += sb->st_size;

    // To tell nftw() to continue.
    return 0;
}

static size_t dir_size(const utils::Path &dir)
{
    if (not dir.is_directory())
    {
        auto status = "dir_size(): " + dir.name() + " is not a directory";
        throw std::runtime_error(status);
    }

    int nopenfd = 20;
    int flags   = 0;
    g_dir_size  = 0;

    if (nftw(dir.name().c_str(), update_size, nopenfd, flags) < 0)
    {
        auto status = "dir_size(" + dir.name() + ") failed: " + strerror(errno);
        throw std::runtime_error(status);
    }

    return g_dir_size;
}

// ----------------------------------------------------------------------

size_t utils::DirectoryTree::size() const
{
    if (root_.is_directory())
        return dir_size(root_);
    else
        return root_.size();
}

// ----------------------------------------------------------------------

std::vector<utils::Path> utils::PathSuperGroup::flatten() const
{
    std::vector<utils::Path> paths{};

    for (auto const &group : group_list_)
    {
        for (auto const &path : group)
        {
            if (path.is_regular_file())
            {
                paths.emplace_back(path);
            }
        }
    }

    return paths;
}

// ----------------------------------------------------------------------

const utils::PathSuperGroup utils::DirectoryTree::divide(size_t group_max_size,
                                                         size_t group_max_files) const
{
    PathSuperGroup supergroup;

    if (root_.is_regular_file())
    {
        PathGroup group;
        group.add(root_);
        supergroup.add(group);
        return supergroup;
    }
    else if (root_.is_directory())
    {
        PathGroup seedgroup;
        seedgroup.add(root_, 0);
        supergroup.add_dir(root_);
        divide_helper(root_, supergroup, seedgroup,
                      group_max_size, group_max_files);
        return supergroup;
    }
    else
    {
        // Ignore anything other than a regular file or directory, at
        // least until we are clear on how to handle others.
    }

    return supergroup;
}

void utils::DirectoryTree::divide_helper(utils::Path const &path,
                                         PathSuperGroup    &supergroup,
                                         PathGroup         &group,
                                         size_t             group_max_size,
                                         size_t             group_max_files) const
{
    if (not path.is_directory())
    {
        return;
    }

    auto dirsz = dir_size(path);

    if (dirsz <= group_max_size) {
        PathGroup gp;
        gp.add(path, dirsz);
        supergroup.add(gp);
        return;
    }
    else
    {
        supergroup.add_dir(path);
    }

    DIR *dirp = opendir(path.name().c_str());

    if (dirp == nullptr)
    {
        std::string e = path.name() + ": " + std::strerror(errno);
        throw std::runtime_error(e);
    }

    dirent  entry{0};
    dirent *resultp{nullptr};

    while (readdir_r(dirp, &entry, &resultp) == 0)
    {
        if (resultp == nullptr)
        {
            // deal with the remainder group, if it has contents.
            if (group.count() > 0)
            {
                supergroup.add(group);
                group.clear();
            }
            break;
        }

        // ignore "." and ".."
        if (std::string(resultp->d_name) == "." or
            std::string(resultp->d_name) == "..")
        {
            continue;
        }

        Path newpath(path.join(entry.d_name));

        if (newpath.is_directory())
        {
            divide_helper(newpath, supergroup, group,
                          group_max_size, group_max_files);
        }
        else if (newpath.is_regular_file())
        {
            if ((group.size() > 0 and group.count() > 0) and
                ((group.size() + newpath.size() > group_max_size) or
                 (group_max_files !=0 and group.count() + 1 > group_max_files)))
            {
                // Group is full; we need to restart in this case.
                supergroup.add(group);
                group.clear();
            }

            group.add(newpath);

            if (group.size() + newpath.size() == group_max_size)
            {
                supergroup.add(group);
                group.clear();
            }
        }
        else
        {
            // ignore other kind of filesystem entities for now.
        }
    }

    closedir(dirp);
}

// ----------------------------------------------------------------------

std::vector<utils::PathGroup> utils::partition(std::string const &dir,
                                               size_t const       max_groups)
{
    if (max_groups <= 0)
    {
        std::stringstream ss;
        ss << "max_groups cannot be <= 0 (max_groups=" << max_groups << ")";
        throw std::runtime_error(ss.str());
    }

    utils::DirectoryTree tree(dir);

    auto const paths = tree.divide(0).flatten();

    size_t max_group_size =
        (tree.size() / max_groups) + (tree.size() % max_groups);

    std::vector<utils::PathGroup> groups(max_groups);

    // This is O(MxN).  There must be a better way?
    for (auto const &path : paths)
    {
        for (size_t gi = 0; gi < max_groups; gi++)
        {
            auto &group = groups.at(gi);

            if (group.size() < max_group_size)
            {
                group.add(path);
                break;
            }
            else if (gi+1 == max_groups)
            {
                // Readjust maximum group size and start looping
                // again, trying to find another group.
                max_group_size += path.size();
                gi = 0;
            }
        }
    }

    return groups;
}

// ----------------------------------------------------------------------

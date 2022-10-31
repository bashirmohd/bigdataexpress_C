#include <stdexcept>
#include <cstring>
#include <algorithm>

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include "utils/utils.h"
#include "dirwalker.h"

// ----------------------------------------------------------------------

void utils::DirectoryWalker::init()
{
    recurse(root_);
}

// ----------------------------------------------------------------------

void utils::DirectoryWalker::recurse(const Path &path)
{
    if (not path.is_directory())
    {
        if (throw_exception_)
        {
            std::string e = path.name() + " is not a directory";
            throw std::runtime_error(e);
        }
        return;
    }

    DIR *dirp = opendir(path.name().c_str());

    if (dirp == nullptr)
    {
        if (throw_exception_)
        {
            std::string e = path.name() + ": " + std::strerror(errno);
            throw std::runtime_error(e);
        }
        return;
    }

    struct dirent *resultp = nullptr;

    while ((resultp = readdir(dirp)) != nullptr)
    {
        if (std::string(resultp->d_name) == "." or
            std::string(resultp->d_name) == "..")
        {
            continue;
        }

        Path newpath(path.join(resultp->d_name));

        paths_.emplace_back(newpath);

        if (newpath.is_regular_file())
        {
            size_ += newpath.size();
        }

        if (recurse_ and newpath.is_directory())
        {
            recurse(newpath);
        }
    }

    closedir(dirp);
}

// ----------------------------------------------------------------------

void utils::DirectoryWalker::sort_by_size()
{
    if (sorted_by_size_)
    {
        return;
    }

    std::sort(paths_.begin(), paths_.end(),
              [](const utils::Path &p1, const utils::Path &p2){
                  return p1.size() > p2.size();
              });

    sorted_by_size_ = true;
}

// ----------------------------------------------------------------------

std::vector<utils::PathGroup>
utils::DirectoryWalker::partition(size_t max_groups,
                                  size_t group_size)
{
    std::vector<utils::PathGroup> groups{max_groups};

    if (not sorted_by_size_)
    {
        sort_by_size();
    }

    // This is O(MxN).  There must be a better way?
    for (auto const &path : paths_)
    {
        if (not path.is_regular_file())
        {
            continue;
        }

        for (size_t gi = 0; gi < max_groups; gi++)
        {
            auto &group = groups.at(gi);

            if (group.size() < group_size)
            {
                group.add(path);
                break;
            }
        }
    }

    return groups;
}

// ----------------------------------------------------------------------

std::vector<utils::PathGroup>
utils::DirectoryWalker::partition(size_t max_groups)
{
    if (max_groups == 0)
    {
        throw std::runtime_error("max_groups cannot be 0");
    }

    size_t group_size = size() / max_groups;

    if (not sorted_by_size_)
    {
        sort_by_size();
    }

    // Readjust group size, based on initial few fils.
    size_t mean_group_count = paths_.size() / max_groups;
    size_t init_group_size  = 0;

    for (size_t i = 0; i < mean_group_count; i++)
    {
        init_group_size += paths_.at(i).size();

        if (init_group_size >= group_size)
        {
            group_size = init_group_size;
            break;
        }
    }

    std::vector<utils::PathGroup> groups{max_groups};

    // TODO: This is O(MxN).  There must be a better way?
    for (auto const &path : paths_)
    {
        if (not path.is_regular_file())
        {
            continue;
        }

        for (auto &group : groups)
        {
            if (group.size() < group_size)
            {
                group.add(path);
                break;
            }
        }
    }

    return groups;
}

// ----------------------------------------------------------------------

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

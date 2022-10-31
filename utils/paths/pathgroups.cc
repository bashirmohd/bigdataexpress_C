#include <ios>
#include <sstream>
#include <stdexcept>
#include <iostream>

#if (DEBUG == 1)
#include <cassert>
#endif

#include "pathgroups.h"

utils::PathGroupsHelper::PathGroupsHelper(const Paths &paths)
    :input_paths_(paths)
{
    init();
}

utils::PathGroupsHelper::PathGroupsHelper(const std::vector<std::string> &paths)
{
    for (auto const & p : paths)
    {
        input_paths_.push_back(Path(p));
    }

    init();
}

void utils::PathGroupsHelper::init()
{
    for (auto const & path : input_paths_)
    {
        add_sub_path(path);
    }
}

// Walk through the paths, and build a list of all the files,
// and and their size (Question: how about other metadata such
// as file type, user, group, permissions, and timestamps?)
//
// Actions:
//
// - if directory, recurse.
//
// - if symlink and directory, do not descent down.
//
// - if special file (FIFO, device) ignore.
//
// - if regular file, add to our list
//
// - if error (permission denied, I/O error, etc), ignore or
//   raise exception?
void utils::PathGroupsHelper::add_sub_path(const Path &path)
{
    try
    {
        if (path.exists())
        {
            if (path.is_regular_file())
            {
                interim_paths_.push_back(path);
            }
            else if (path.is_directory())
            {
                interim_paths_.push_back(path);

                for (auto const & subpath : DirectoryWalker(path, true))
                {
                    interim_paths_.push_back(subpath);
                }
            }
            else
            {
                // TODO: handle these.
                // std::cerr << path << "exists, "
                //           << "but not a regular file or directory; "
                //           << "skipping.\n";
            }
        }
        else
        {
            std::string e = path.name() + " not found";
            throw std::runtime_error(e);
        }
    }
    catch (...)
    {
        interim_paths_.clear();
        throw;
    }
}

utils::PathGroups& utils::PathGroupsHelper::group_into(size_t const num_groups)
{
    if (num_groups <= 0)
    {
        throw std::invalid_argument("groups should be greater than 0");
    }

    auto   total_size = find_total_size(interim_paths_);
    auto   group_size = total_size / num_groups;
    auto & groups     = group_by_size(group_size);

    // if group_by_size() returns more groups than we asked for (that
    // happens because of uneven file sizes), redistribute files in
    // the "overflow" groups among the first groups.
    if (groups.size() > num_groups)
    {
        auto nlast = groups.size() - num_groups;

        for (auto i = 0; i < nlast; ++i)
        {
            auto last = groups.back();
            groups.pop_back();

            while (not last.empty())
            {
                for (auto & g : groups)
                {
                    if (last.empty())
                        break;
                    auto & f = last.back();
                    last.pop_back();
                    g.push_back(f);
                }
            }
        }
    }

    return groups;
}

utils::PathGroups& utils::PathGroupsHelper::group_by_size(size_t const max_group_size,
                                                          size_t max_files)
{
    result_.clear();

    Paths current_group;
    size_t current_group_size = 0;

    for (auto const & p : interim_paths_)
    {
#if (DEBUG == 1)
        std::cout << "Adding " << p.name() << "...\n";
#endif
        // TODO: what about directories? what about special files?
        if (not (p.is_regular_file() or p.is_directory()))
        {
            continue;
        }

        size_t current_file_size = p.size();

        if (current_file_size > max_group_size)
        {
            // very large file; give it its own group.

#if (DEBUG == 1)
            std::cout << "Adding large file of size "
                      << current_file_size << " bytes.\n";
#endif

            Paths large{p};
            result_.push_back(large);
            continue;
        }
        else if (current_file_size + current_group_size <= max_group_size)
        {
            // current group of files is small.
            current_group.push_back(p);
            current_group_size += current_file_size;

            // This happens when we're at the end of our list.
            if ((current_group_size >= max_group_size) or
                (max_files > 0 and current_group.size() >= max_files))
            {

#if (DEBUG == 1)
                std::cout << "Adding file group of size: "
                          <<  current_group_size << " bytes, "
                          << "number of files: " << current_group.size()
                          << "(max group size: " << max_group_size << ", "
                          << "max files:" << max_files <<  ").\n";
#endif

                result_.push_back(current_group);
                current_group.clear();
                current_group_size = 0;
                continue;
            }
        }
        else
        {
            // Current group is large enough.  Save results, start a
            // new group, and continue

#if (DEBUG == 1)
            std::cout << "Adding file group of size "
                      << current_group_size << " bytes.\n";
#endif

            result_.push_back(current_group);
            current_group.clear();

            current_group.push_back(p);
            current_group_size = current_file_size;
        }
    }

    // Add the remainders also to results.
    if (current_group.size() > 0)
    {
        result_.push_back(current_group);
    }

    return result_;
}

size_t utils::find_total_size(const Paths &paths)
{
    size_t total_size = 0;

    for (auto const & path : paths)
    {
        if (path.exists() and path.is_regular_file())
        {
            total_size += path.size();
        }
    }

    return total_size;
}

void utils::PathGroupsHelper::print_input_paths() const
{
    for (auto const & path : input_paths_)
    {
        std::cout << path.name() << "\n";
    }
}

void utils::PathGroupsHelper::print_interim_paths() const
{
    for (auto const & path : interim_paths_)
    {
        std:: cout << path.name() << "\n";
    }
}

std::ostream& operator<<(std::ostream& out, const utils::Paths& ps)
{
    out << "Group total size: " << find_total_size(ps) << " bytes.\n";

// #if (DEBUG == 1)
//     // Crude pretty printing
//     out << "[group size = " << find_total_size(ps) << "\n";
//     out << "\t[\n";
//     for (auto p : ps)
//     {
//         out << "\t" << p.name() << " : " << p.size() << "\n";
//     }
//     out << "\t\n]";
//     out << "]";
// #endif

    return out;
}

std::ostream& operator<<(std::ostream& out, const utils::PathGroups& pgs)
{
    for (auto const & pg : pgs)
    {
        out << pg;
    }
    return out;
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

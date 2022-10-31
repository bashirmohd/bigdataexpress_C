#ifndef BDE_UTILS_PATH_GROUPS_H
#define BDE_UTILS_PATH_GROUPS_H

#include <vector>
#include "dirwalker.h"

namespace utils
{
    typedef std::vector<utils::Path>  Paths;
    typedef std::vector<Paths>        PathGroups;

    size_t find_total_size(const Paths &paths);

    class PathGroupsHelper
    {
    public:
        PathGroupsHelper(const Paths &paths);
        PathGroupsHelper(const std::vector<std::string> &paths);

        // group all files into num_groups
        PathGroups& group_into(size_t const num_groups);

        // group all files into sets of max size size.
        PathGroups& group_by_size(size_t const size, size_t max_files=0);

        // debugging aids.
        void print_input_paths() const;
        void print_interim_paths() const;

        // testing aids.
        const Paths& get_input_paths() { return input_paths_; };
        const Paths& get_interim_paths() { return interim_paths_; };

    private:
        // walk down the input paths and build an intermediate data
        // structure.
        void init();

        // method to handle subdirectories.
        void add_sub_path(const Path &path);

    private:
        Paths      input_paths_;
        Paths      interim_paths_;
        PathGroups result_;
    };
};

std::ostream& operator<<(std::ostream& out, const utils::Paths& ps);
std::ostream& operator<<(std::ostream& out, const utils::PathGroups& pg);

#endif

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

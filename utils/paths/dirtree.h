#ifndef BDE_UTILS_DIR_TREE_H
#define BDE_UTILS_DIR_TREE_H

#include <string>
#include <vector>
#include <set>
#include <stdexcept>

#include "path.h"

namespace utils
{
    class PathGroup {
    public:
        PathGroup() : size_(0) { }

        // for adding a regular file to this group.
        void add(const Path &p) {
            if (p.is_regular_file()) {
                path_list_.insert(p);
                size_ += p.size();
            } else {
                throw std::runtime_error(p.name() + " is not a regular file");
            }
        }

        // for adding a directory to this group.
        void add(const Path &p, size_t size) {
            if (p.is_directory()) {
                path_list_.insert(p);
                size_ += size;
            } else {
                throw std::runtime_error(p.name() + " is not a directory");
            }
        }

        // TODO: find clearer names.
        size_t size() const { return size_; }
        size_t count() const { return path_list_.size(); }
        void clear() { path_list_.clear(); size_ = 0; }
        bool empty() { return path_list_.empty(); }

        typedef std::set<Path>::const_iterator const_iterator;
        const_iterator begin() const { return path_list_.begin(); }
        const_iterator end() const  { return path_list_.end(); }

    private:
        size_t            size_;
        std::set<Path> path_list_;
    };

    class PathSuperGroup {
    public:
        PathSuperGroup() : size_(0) { }

        void add(const PathGroup &pg) {
            group_list_.push_back(pg);
            size_ += pg.size();
        }

        void add_dir(const Path &p) {
            if (p.is_directory())
                dir_list_.insert(p);
        }

        const std::set<Path> & get_dir_list() const {
            return dir_list_;
        }

        // TODO: find clearer names.
        size_t size() const { return size_; }
        size_t count() const { return group_list_.size(); }

        typedef std::vector<PathGroup>::const_iterator const_iterator;
        const_iterator begin() const { return group_list_.begin(); }
        const_iterator end() const  { return group_list_.end(); }

        // Return a flattened list of all paths from all groups under
        // this "super" group.
        std::vector<utils::Path> flatten()  const;

    private:
        size_t                 size_;
        std::vector<PathGroup> group_list_;
        std::set<Path>         dir_list_;
    };

    class DirectoryTree
    {
    public:
        DirectoryTree(const std::string &p) : root_(p) {}
        DirectoryTree(const Path &p) : root_(p) {}

        size_t size() const;

        const PathSuperGroup divide(size_t group_max_size,
                                    size_t group_max_files = 0) const;

    private:
        void divide_helper(utils::Path const &path,
                           PathSuperGroup    &supergroup,
                           PathGroup         &group,
                           size_t             group_max_size,
                           size_t             group_max_files = 0) const;

    private:
        Path   root_;
    };

    //
    // Distribute files under a directory tree into specified number
    // of groups.  The combined size of files inside each group will
    // be almost the same.
    //
    // TODO: refactor; move this inside DirectoryTree maybe?  Also
    // avoid name confusion between DirectoryTree::divide() and this
    // function.
    //
    std::vector<utils::PathGroup> partition(std::string const &dir,
                                            size_t const       max_groups);

};

#endif // BDE_UTILS_DIR_TREE_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

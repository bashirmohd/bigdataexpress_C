#ifndef BDE_UTILS_DIR_WALKER_H
#define BDE_UTILS_DIR_WALKER_H

#include <string>
#include <vector>

#include "path.h"
#include "dirtree.h"

// ----------------------------------------------------------------------

namespace utils
{
    class DirectoryWalker
    {
    public:
        DirectoryWalker(const Path &p,
                        bool recurse=false,
                        bool throw_exception=false)
            : root_(p),
              size_(0),
              sorted_by_size_(false),
              recurse_(recurse),
              throw_exception_(throw_exception) {
            init();
        };

        DirectoryWalker(const std::string &p,
                        bool recurse=false,
                        bool throw_exception=false)
            : root_(Path(p)),
              size_(0),
              sorted_by_size_(false),
              recurse_(recurse),
              throw_exception_(throw_exception){
            init();
        };

        typedef std::vector<Path>::const_iterator const_iterator;

        const_iterator begin() const { return paths_.begin(); }
        const_iterator end() const  { return paths_.end(); }

        // Return total number of files and directories we have.
        size_t count() const { return paths_.size(); }

        // This should help with testability.
        size_t count_regular_files() const {
            size_t count = 0;

            for (auto const &p : paths_) {
                if (p.is_regular_file())
                {
                    count++;
                }
            }

            return count;
        }


        // Return total size of all files.
        size_t size() const { return size_; }

        // Access paths from the flattened list by index.
        const Path& at(size_t idx) { return paths_.at(idx); }

        // Sort by size, in descending order.
        void sort_by_size();

        std::vector<utils::PathGroup> partition(size_t max_groups,
                                                size_t group_size);

        std::vector<utils::PathGroup> partition(size_t max_groups);

    private:
        void init();
        void recurse(const Path &dir);

    private:
        Path              root_;
        size_t            size_;
        bool              sorted_by_size_;
        bool              recurse_;
        bool              throw_exception_;
        std::vector<Path> paths_;
    };
};

#endif // BDE_UTILS_DIR_WALKER_H

// ----------------------------------------------------------------------

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

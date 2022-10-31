//
//  Helpers to query mounted filesystems.
//

#ifndef BDE_UTILS_MOUNT_H
#define BDE_UTILS_MOUNT_H

#include <set>
#include <string>
#include <cassert>
#include <stdexcept>

#include <libmount/libmount.h>

namespace utils
{
    // Helper class, with methods for querying a mount point.
    class MountPoint
    {
    public:
        MountPoint(libmnt_fs *entry)
            : entry_(entry) { }

        // What is mounted?
        std::string source() const;

        // Where is it mounted?
        std::string target() const;

        // What kind of filesystem is this?
        std::string type() const;

        // print source and target.
        void print() const;

        // print a lot more stuff.
        void print_verbose() const;

    private:
        std::string str(const char *s) const;
        std::string isSwapFs() const;
        std::string isNetFs() const;
        std::string isPseudoFs() const;

    private:
        libmnt_fs *entry_;
    };

    bool operator<(const MountPoint &x, const MountPoint &y);

    // Given a path, `MountPointsHelper` class will initialize a
    // storage agent.
    //
    // `MountPointsHelper` will initialize multiple storage agents if
    // `scan` is true *and* if there are further mounted filesystems
    // beneath `path`.
    class MountPointsHelper
    {
    public:
        MountPointsHelper(std::string const &path, bool scan=false)
            : root_path_(path.c_str()),
              mount_table_(nullptr) {
            init(scan);
        }

        MountPointsHelper(const char* const path, bool scan=false)
            : root_path_(path),
              mount_table_(nullptr) {
            init(scan);
        }

        ~MountPointsHelper();

        // returns list of mountpoints under this path.
        const std::set<MountPoint>& get_mounts() const {
            return mount_points_;
        }

        // "/path/to/something" may not be a mount point.  That
        // directory may be under "/path/to/" or "/path/", or even "/"
        // as the mount point.
        //
        // In order to initialize storage agent for a given folder, we
        // will still need to find the storage device where the folder
        // belongs to; hence this overcomplicated method.
        //
        // Calling this method will reinitialize the object.  That is
        // a potentially expensive operation, and since it can mutate
        // the object that we are using, we can land in trouble.
        // Careful when using this method!
        const MountPoint get_parent_mount();

        static bool is_root_path(std::string const & path);

    private:
        void init(bool scan);
        void reinit(std::string const & path);
        void recurse(libmnt_fs *fs);

        std::string get_parent_folder(std::string const & path);

    private:
        std::string           root_path_;

        std::set<MountPoint>  mount_points_;
        libmnt_table         *mount_table_;
    };

    // We can't pass utils::MountPoint type objects, because they
    // contain an opaque pointer (libmnt_fs *).
    struct MountPointInfo
    {
        std::string source;
        std::string target;
        std::string type;
    };

    // Given a path, return associated mount point.
    MountPointInfo get_mount_point(std::string const &path);

    // Special case of get_mount_point(): given a path, return
    // associated device name.
    std::string get_device(std::string const &path);
}

#endif

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

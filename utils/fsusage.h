//
//  Find filesystem usage.
//

#ifndef BDE_UTILS_FS_USAGE_H
#define BDE_UTILS_FS_USAGE_H

#include <string>

// ----------------------------------------------------------------------

namespace utils
{
    struct fs_usage
    {
        std::string   type;             // "quota" or "filesystem usage".
        std::string   quota_limit_type; // "soft" or "hard".

        unsigned long block_size;       // Filesystem block size.
        unsigned long fragment_size;    // Usually same as block size.

        unsigned long total_blocks;     // Size of fs in fragment units.
        unsigned long free_blocks;      // Number of free blocks.
        unsigned long available_blocks; // Number of available blocks.
        float         blocks_usage;     // Percentage value.

        unsigned long total_size;       // Size of the filesystem.
        unsigned long free_size;        // Size of free blocks.
        unsigned long available_size;   // Size of available blocks.
        float         size_usage;       // Percentage value.

        unsigned long total_inodes;     // Number of inodes.
        unsigned long free_inodes;      // Number of free inodes.
        unsigned long available_inodes; // Number of available inodes.
        float         inode_usage;      // Percentage value.
    };

    // General interface: try to read quota information for the given
    // path/user; if no quota is enabled, return filesystem usage.
    fs_usage get_quota_or_fs_usage(std::string const &path,
                                   std::string const &user);

    // Get usage of the filesystem associated with the given file.
    // Will throw exception on error.
    fs_usage get_fs_usage(std::string const &path);

    // Given a path, find the device associated with its parent mount
    // point, and find if quota is enabled on the device.
    bool quota_enabled(std::string const &path);

    // Given a path and username, find quota usage.
    fs_usage get_quota_usage(std::string const &path,
                             std::string const &user);
};

std::ostream& operator<<(std::ostream &out, utils::fs_usage &usage);

#endif // BDE_UTILS_FS_USAGE_H

// ----------------------------------------------------------------------
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

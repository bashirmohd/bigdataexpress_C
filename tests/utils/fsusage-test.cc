#include <iostream>
#include <utils/fsusage.h>

static void print_fs_usage(std::string const &path)
{
    try
    {
        auto usage = utils::get_fs_usage(path);

        std::cout << "Filesystem usage for " << path << ":\n"
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
    }
    catch (std::exception const &ex)
    {
        std::cerr << "Error on getting usage on " << path << ":"
                  << ex.what() << "\n";
    }
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        print_fs_usage(argv[0]);
    }
    else
    {
        for (int c = 1; c < argc; c++)
        {
            print_fs_usage(argv[c]);
        }
    }

    return 0;
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

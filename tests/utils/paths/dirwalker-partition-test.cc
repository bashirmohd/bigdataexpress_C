#include <iostream>
#include <vector>
#include <algorithm>

#include <getopt.h>

#include "utils/utils.h"
#include "utils/paths/path.h"
#include "utils/paths/dirwalker.h"

// ----------------------------------------------------------------------

static void show_usage(const char * const prog)
{
    std::cerr << "Usage: " << prog << " [args]\n\n"
              << "where [args] may be: \n\n"
              << "  -h, -?, --help, --usage : Show usage.\n"
              << "  -p, --partitions [ARG]  : Number of partitions.\n";
}

// ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    std::string dir        = ".";
    size_t      max_groups = 10;

    while (true)
    {
        static struct option long_options[] = {
            { "help",          no_argument,       0, 'h' },
            { "usage",         no_argument,       0, '?' },
            { "partitions",    required_argument, 0, 'p' },
            { 0, 0, 0, 0 }
        };

        int optind = 0;
        int c      = getopt_long(argc, argv, "h?p:",
                                 long_options, &optind);

        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
        case '?':
            show_usage(argv[0]);
            exit(0);
        case 'p':
            max_groups = std::strtol(optarg, 0, 10);
            break;
        default:
            abort();
        }
    }

    if (optind < argc)
    {
        dir = argv[optind];
    }
    else
    {
        dir = ".";
    }

    auto       tree        = utils::DirectoryWalker(dir, true);
    auto const groups      = tree.partition(max_groups);
    size_t     total_files = 0;
    size_t     total_size  = 0;

    for (auto i = 0; i < groups.size(); i++)
    {
        auto count   = groups.at(i).count();
        auto size    = groups.at(i).size();

        total_files += count;
        total_size  += size;

        std::cout << "[Group " << i << "] "
                  << "files: " << utils::format_number(count) << "\t"
                  << "size: " << utils::format_number(size) << "\n";
    }

    std::cout << "\n"
              << "Files under tree      : "
              << utils::format_number(tree.count_regular_files()) << "\n"
              << "Total files in groups : "
              << utils::format_number(total_files) << "\n"
              << "Total size            : "
              << utils::format_number(total_size) << "\n";

    if (tree.count_regular_files() != total_files)
    {
        std::cerr << "Mismatch!\n"
                  << "Total files under directory : "
                  << utils::format_number(tree.count_regular_files())
                  << "Total files in partitions   : "
                  << utils::format_number(total_files) << "\n";

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------

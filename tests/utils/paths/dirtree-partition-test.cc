#include <iostream>
#include <vector>
#include <algorithm>

#include <getopt.h>

#include "utils/utils.h"
#include "utils/paths/path.h"
#include "utils/paths/dirtree.h"

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
    std::string dir       = ".";
    size_t      max_parts = 10;

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
            max_parts = std::strtol(optarg, 0, 10);
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

    auto const groups = utils::partition(dir, max_parts);

    size_t total_files = 0;
    size_t total_size  = 0;

    for (auto i = 0; i < groups.size(); i++)
    {
        auto count = groups.at(i).count();
        auto size  = groups.at(i).size();

        total_files += count;
        total_size  += size;

        std::cout << "[Group " << i << "] "
                  << "files: " << utils::format_number(count) << "\t"
                  << "size: " << utils::format_number(size) << "\n";
    }

    std::cout << "\nTotal files: " << utils::format_number(total_files)
              << ", total size: " << utils::format_number(total_size) << "\n";

    return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------

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
              << "  -r, --runs [ARG]        : Number of runs.\n";
}

// ----------------------------------------------------------------------

bool test(std::string const dir, size_t max_groups)
{
    auto        tree        = utils::DirectoryWalker(dir, true);
    auto const &groups      = tree.partition(max_groups);
    size_t      total_files = 0;
    size_t      total_size  = 0;

    for (auto i = 0; i < groups.size(); i++)
    {
        auto count = groups.at(i).count();
        auto size  = groups.at(i).size();

        total_files += count;
        total_size  += size;

        // std::cout << "[Group " << i << "] "
        //           << "files: " << utils::format_number(count) << "\t"
        //           << "size: " << utils::format_number(size) << "\n";
    }

    std::cout << "Requested groups: " << max_groups
              << ", result groups: " << groups.size()
              << ", total grouped files: "
              << utils::format_number(total_files)
              << ", total size: "
              << utils::format_number(total_size) << " bytes\n";

    if (tree.count_regular_files() != total_files)
    {
        std::cerr << "\nMismatch!\n"
                  << "Max groups                  : " << max_groups << "\n"
                  << "Groups                      : " << groups.size() << "\n"
                  << "Total files under directory : "
                  << utils::format_number(tree.count_regular_files()) << "\n"
                  << "Total files in groups       : "
                  << utils::format_number(total_files) << "\n\n";

        return false;
    }

    std::cout << "Max groups " << max_groups << ": OK\n\n";
    return true;
}

// ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    std::string dir        = ".";
    size_t      max_runs   = 10;

    while (true)
    {
        static struct option long_options[] = {
            { "help",  no_argument,       0, 'h' },
            { "usage", no_argument,       0, '?' },
            { "runs",  required_argument, 0, 'r' },
            { 0, 0, 0, 0 }
        };

        int optind = 0;
        int c      = getopt_long(argc, argv, "h?r:",
                                 long_options, &optind);

        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
        case '?':
            show_usage(argv[0]);
            exit(0);
        case 'r':
            max_runs = std::strtol(optarg, 0, 10);
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

    for (size_t i = 1; i < max_runs; i++)
    {
        try
        {
            if (test(dir, i) == false)
            {
                std::cerr << "Test failed at run " << i << ".\n";
                return EXIT_FAILURE;
            }
        }
        catch (std::exception const &ex)
        {
            std::cerr << "Error in test: " << ex.what() << "\n";
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------

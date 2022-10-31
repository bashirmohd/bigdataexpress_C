//
// Run utils::dir_checksum_of_checksums() many times over with a
// varying number of max_threads parameters.  The code being tested is
// fairly tricky, so we can be fully confident that things work as
// expected only when we have tested it under different settings.
//

#include <iostream>

#include <getopt.h>

#include <openssl/opensslv.h>
#include <openssl/evp.h>

#include "utils/checksum.h"

// ----------------------------------------------------------------------

static std::string test(std::string const &filename,
                        std::string const &digest,
                        bool               parallel,
                        size_t             max_threads    = 1,
                        size_t             min_group_size = 1)
{
    try
    {
        auto start    = time(0);
        auto checksum = utils::dir_checksum_of_checksums(utils::Path(filename),
                                                         digest,
                                                         parallel,
                                                         max_threads,
                                                         min_group_size);
        auto end      = time(0);

        std::cout << (parallel ? "parallel " : "sequential ")
                  << digest << "(\"" << filename << "\")\t: "
                  << checksum << " (" << checksum.length() << " chars)\n"
                  << digest << " took "  << (end - start) << " seconds.\n";

        return checksum;
    }
    catch (std::exception const &ex)
    {
        return std::string("Error: ") + ex.what();
    }

    return parallel ? "parallel" : "sequential";
}

// ----------------------------------------------------------------------

static void show_usage(const char * const prog)
{
    std::cerr << "Usage: " << prog << " [args]\n\n"
              << "where [args] may be: \n\n"
              << "  -h, -?, --help, --usage : Show usage.\n"
              << "  -a, --algorithm [ARG]   : Digest algorithm (default: sha1)\n"
              << "  -t, --threads [ARG]     : Number of threads to use (default: random).\n"
              << "  -r, --runs [ARG]        : Number of runs (default: 10).\n";
}

// ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    std::string dir            = ".";
    std::string digest         = "sha1";
    size_t      max_threads    = 0;
    bool        random_threads = true;
    size_t      max_runs       = 10;
    size_t      min_group_size = 1 * 1000 * 1000 * 1000;

    while (true)
    {
        static struct option long_options[] = {
            { "help",          no_argument,       0, 'h' },
            { "usage",         no_argument,       0, '?' },
            { "algorithm",     required_argument, 0, 'a' },
            { "threads",       required_argument, 0, 't' },
            { "runs",          required_argument, 0, 'r' },
            { 0, 0, 0, 0 }
        };

        int optind = 0;
        int c      = getopt_long(argc, argv, "h?a:t:s:r:",
                                 long_options, &optind);

        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
        case '?':
            show_usage(argv[0]);
            exit(0);
        case 'a':
            digest = optarg;
            break;
        case 't':
            max_threads = std::strtol(optarg, 0, 10);
            random_threads = false;
            break;
        case 's':
            min_group_size = std::strtol(optarg, 0, 10);
            break;
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

    std::cout << "Running checksum on directory \"" << dir << "\"\n"
              << "  algorithm       : " << digest << "\n"
              << "  threads to use  : " << max_threads << "\n"
              << "  min group size  : " << min_group_size << "\n\n";

    std::cout << "Using " << OPENSSL_VERSION_TEXT << ".\n";
    OpenSSL_add_all_digests();

    std::vector<std::string> results;

    srand(time(nullptr));

    // Run checksum on given directory multiple times.
    for (auto i = 0; i < max_runs; ++i)
    {
        if (random_threads)
        {
            max_threads = 1 + (rand() % 255);
        }

        auto result = test(dir,
                           digest,
                           true,
                           max_threads,
                           min_group_size);

        results.emplace_back(result);
    }

    // Check that all results are equal.
    if (std::equal(results.begin() + 1, results.end(), results.begin()))
    {
        std::cout << "All OK.\n";
        return EXIT_SUCCESS;
    }

    std::cout << "Test failed.\n";
    for (auto i = 0; i < results.size(); i++)
    {
        std::cout << "Run " << i << "\t: " << results.at(i) << "\n";
    }

    return EXIT_FAILURE;
}

// ----------------------------------------------------------------------

#include <iostream>

#include <getopt.h>

#include <openssl/opensslv.h>
#include <openssl/evp.h>

#include "utils/checksum.h"

// ----------------------------------------------------------------------

static uint64_t rdtsc(void)
{
    uint64_t lo = 0;
    uint64_t hi = 0;

    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));

    return hi << 32 | lo;
}

// ----------------------------------------------------------------------

static std::string test(std::string const &filename,
                        std::string const &digest,
                        bool               parallel,
                        size_t             max_threads    = 1,
                        size_t             min_group_size = 1)
{
    try
    {
        auto start_t  = time(0);
        auto start    = rdtsc();
        auto checksum = utils::dir_checksum_of_checksums(utils::Path(filename),
                                                         digest,
                                                         parallel,
                                                         max_threads,
                                                         min_group_size);
        auto end      = rdtsc();
        auto end_t    = time(0);

        std::cout << (parallel ? "parallel " : "sequential ")
                  << digest << "(\"" << filename << "\")\t: "
                  << checksum << " (" << checksum.length() << " chars)\n"
                  << digest << " took " << end - start << " cycles / "
                  << (end_t - start_t) << " seconds.\n";

        return checksum;
    }
    catch (std::exception const &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
    }

    return parallel ? "parallel" : "sequential";
}

// ----------------------------------------------------------------------

static void show_usage(const char * const prog)
{
    std::cerr << "Usage: " << prog << " [args]\n\n"
              << "where [args] may be: \n\n"
              << "  -h, -?, --help, --usage : Show usage.\n"
              << "  -a, --algorithm [ARG]   : Digest algorithm (default: md5)\n"
              << "  -t, --threads [ARG]     : Max. number of threads to use.\n"
              << "  -s, --minsize [ARG]     : Min. file group size needed to create a thread.\n"
              << "  -p, --parallel          : Run only parallel tests.\n"
              << "  -l, --sequential        : Run only sequential tests.\n\n";
}

// ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    std::string dir            = ".";
    std::string digest         = "md5";
    size_t      max_threads    = 256;
    size_t      min_group_size = 1 * 1000 * 1000 * 1000;
    bool        run_sequential = true;
    bool        run_parallel   = true;

    while (true)
    {
        static struct option long_options[] = {
            { "help",          no_argument,       0, 'h' },
            { "usage",         no_argument,       0, '?' },
            { "algorithm",     required_argument, 0, 'a' },
            { "threads",       required_argument, 0, 't' },
            { "minsize",       required_argument, 0, 's' },
            { "parallel",      no_argument,       0, 'p' },
            { "sequential",    no_argument,       0, 'l' },
            { 0, 0, 0, 0 }
        };

        int optind = 0;
        int c      = getopt_long(argc, argv, "h?a:t:s:pl",
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
            break;
        case 's':
            min_group_size = std::strtol(optarg, 0, 10);
            break;
        case 'p':
            run_sequential = false;
            break;
        case 'l':
            run_parallel = false;
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
              << "  maximum threads : " << max_threads << "\n"
              << "  min group size   : " << min_group_size << "\n\n";

    std::cout << "Using " << OPENSSL_VERSION_TEXT << ".\n";
    OpenSSL_add_all_digests();

    std::string seq_result{};
    std::string par_result{};

    if (run_sequential)
    {
        std::cout << "\nRunning sequential directory checksum:\n";
        seq_result = test(dir, digest, false);
    }

    if (run_parallel)
    {
        std::cout << "\nRunning async/await directory checksum:\n";
        par_result = test(dir, digest, true, max_threads, min_group_size);
    }

    std::cout << "\n";

    if (run_parallel and run_sequential)
    {
        if (seq_result == par_result)
        {
            std::cout << "Tested OK.\n";
            return EXIT_SUCCESS ;
        }
        else
        {
            std::cout << "Test failed.\n";
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------

#include <iostream>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <openssl/opensslv.h>
#include <openssl/evp.h>

#include "utils/checksum.h"

// ----------------------------------------------------------------------

std::string test(std::string const &filename,
                 std::string const &digest)
{
    try
    {
        auto start    = time(0);
        auto checksum = utils::checksum(filename, digest);
        auto end      = time(0);

        std::cout << digest << "(" << filename << ")\t: " << checksum << "\n"
                  << digest << " took " << (end - start) << " seconds.\n";

        return checksum;
    }
    catch (std::exception const &ex)
    {
        std::cerr << "Error: " << ex.what() << ".\n";
    }

    return "";
}

// ----------------------------------------------------------------------

void print_usage(const char * const prog_name)
{
    std::cout << "Usage: \n"
              << "  " << prog_name
              << " [-a algoritm] [files]\n";
}

// ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    std::string filename = argv[0];

    // Possible digests: "sha", "sha1", "mdc2", "ripemd160", "sha224",
    // "sha256", "sha384", "sha512", "md4", "md5", "blake2b",
    // "blake2s".
    std::string algorithm = "md5";

    int c;

    while ((c = getopt (argc, argv, "h?a:")) != -1)
    {
        switch (c)
           {
           case 'a':
               algorithm = optarg;
               break;
           case '?':
           case 'h':
               print_usage(argv[0]);
               exit(1);
           default:
               break;
           }
    }

    std::cout << "Using " << OPENSSL_VERSION_TEXT << "\n";

    OpenSSL_add_all_digests();

    if (optind == argc)
    {
        test(argv[0], algorithm);
        return 0;
    }

    for (int index = optind; index < argc; index++)
    {
        test(argv[index], algorithm);
    }

    return 0;
}

// ----------------------------------------------------------------------

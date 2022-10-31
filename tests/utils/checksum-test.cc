#include <iostream>

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

void test_all(std::string const &filename)
{
    auto digests = { "sha", "sha1", "mdc2", "ripemd160",
                     "sha224", "sha256", "sha384", "sha512",
                     "md4", "md5", "blake2b", "blake2s" };

    for (auto const &digest : digests)
    {
        try
        {
            auto start = rdtsc();
            auto checksum = utils::checksum(filename, digest);
            auto end = rdtsc();

            std::cout << digest << "(" << filename << ")\t: "
                      << checksum << " (" << checksum.length() << ")\n";
            std::cout << digest << " took " << end - start << " cycles.\n";
        }
        catch (std::exception const &ex)
        {
            std::cerr << "Error: " << ex.what() << ".\n";
        }
    }
}

// ----------------------------------------------------------------------

int main(int argc, char *argv[])
{
    std::string filename = argv[0];

    if (argc > 1)
        filename = argv[1];

    std::cout << "Using " << OPENSSL_VERSION_TEXT << "\n";
    OpenSSL_add_all_digests();

    test_all(filename);

    return 0;
}

// ----------------------------------------------------------------------

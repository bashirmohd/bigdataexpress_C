#ifndef BDE_UTIL_CHECKSUM_H
#define BDE_UTIL_CHECKSUM_H

#include <string>
#include <vector>
#include <atomic>
#include <openssl/evp.h>

#include "paths/path.h"

namespace utils
{
    //
    // Given name of a regular file and a message digest algorithm,
    // this function will return the file's checksum using the
    // specified algorithm.
    //
    // These are some possibilities for "digest" argument:
    //
    //   sha, sha1, mdc2, ripemd160, sha224, sha256, sha384, sha512,
    //   md4, md5, blake2b, blake2s
    //
    // This function implemented using OpenSSL, so availabilty of
    // algorithm will depend on OpenSSL version.  BLAKE2, for example,
    // is not available in OpenSSL 1.0.2.
    //
    // TODO: disambiguate the default parameter.
    //
    std::string checksum(std::string const &path,
                         std::string const &digest = "sha1");

    std::string checksum(std::string const &path,
                         EVP_MD const      *md = EVP_sha1());

    std::string checksum_file(std::string const &path,
                              EVP_MD const      *md = EVP_sha1());

    std::string checksum_adler32(std::string const& path);

    // TODO: turns out that std::async() needs a non-overloaded
    // function name; will clean this up later.
    std::string checksum_md(std::string const &path,
                           EVP_MD const      *md = EVP_sha1());

    //
    // Given a list of checksums (or any list of strings really), sort
    // them, and compute their checksum using the specified method.
    //
    // TODO: disambiguate the default parameter.
    //
    std::string checksum_of_checksums(std::vector<std::string> &checksums,
                                      std::string const &digest = "sha1");

    std::string checksum_of_checksums(std::vector<std::string> &checksums,
                                      EVP_MD const      *md = EVP_sha1());


    // Helper functions to compute checksum of checksum of the given
    // directory's contents.
    std::string dir_checksum_of_checksums(
        utils::Path const &path,
        EVP_MD const      *md,
        bool   const       parallel       = true,
        size_t const       max_threads    = 256,
        size_t const       min_group_size = 1 * 1024 * 1024 * 1024);

    std::string dir_checksum_of_checksums(
        utils::Path const &path,
        std::string const &algorithm,
        bool   const       parallel       = true,
        size_t const       max_threads    = 256,
        size_t const       min_group_size = 1 * 1024 * 1024 * 1024);

    // An atomic counter.  A static instance of this counter will keep
    // track of the number of async tasks we launch.
    class AtomicCounter
    {
    public:
        explicit AtomicCounter(size_t c=0) {
            count.store(c);
        }

        void increment() {
            count++;
        }

        void decrement() {
            count--;
        }

        size_t get() {
            return count.load();
        }

    private:
        std::atomic_size_t count;
    };

    // Return a handle to the global counter.
    AtomicCounter& global_task_counter();
};

#endif

// ----------------------------------------------------------------------
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

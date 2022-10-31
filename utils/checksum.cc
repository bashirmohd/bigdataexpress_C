#include <stdexcept>
#include <future>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/x509v3.h>

// adler32 checksum
#include <zlib.h>

#include "utils.h"
#include "paths/dirtree.h"
#include "paths/dirwalker.h"
#include "checksum.h"

// ----------------------------------------------------------------------

// Repeated invocations of OpenSSL_add_all_digests() seem to result in
// a double free (followed by a crash), at least with OpenSSL 1.0.2k.
//
// We're using global static initialization as a quick and likely
// dirty way to make sure that we've called OpenSSL_add_all_digests()
// once, and not every time utils::checksum() is called, before
// actually using the digests.

static bool _OpenSSL_add_digests(void)
{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    OpenSSL_add_all_digests();
#endif

    return true;
}

static bool _openssl_added_digests = _OpenSSL_add_digests();

// ----------------------------------------------------------------------

std::string utils::checksum(std::string const &path, std::string const &digest)
{
    EVP_MD const *md = EVP_get_digestbyname(digest.c_str());

    if (!md)
    {
        if (digest == "adler32") return checksum_adler32(path);
        else throw std::runtime_error("Unknown message digest " + digest);
    }

    return checksum(path, md);
}

std::string utils::checksum_md(std::string const &path,
                               EVP_MD const      *md)
{
    return checksum(path, md);
}


std::string utils::checksum_file(std::string const &path,
                                 EVP_MD const      *md)
{
    return checksum(path, md);
}


std::string utils::checksum(std::string const &path, EVP_MD const *md)
{
    std::ifstream instream(path, std::ios::binary);

    if (!instream)
    {
        throw std::runtime_error("Error opening " + path);
    }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX mdctx_{0};
    EVP_MD_CTX *mdctx = &mdctx_;
#else
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
#endif

    if (!mdctx)
    {
        instream.close();
        throw std::runtime_error("EVP_MD_CTX_new() failed");
    }

    if (!EVP_DigestInit(mdctx, md))
    {
        instream.close();
#if OPENSSL_VERSION_NUMBER > 0x10100000L
        EVP_MD_CTX_free(mdctx);
#endif
        throw std::runtime_error("EVP_DigestInit_ex() failed");
    }

    while (!instream.eof())
    {
        char buf[BUFSIZ]{0};
        memset(buf, 0, BUFSIZ);

        instream.read(buf, BUFSIZ);
        size_t sz = instream.gcount();

        if (instream.bad())
        {
            instream.close();
#if OPENSSL_VERSION_NUMBER > 0x10100000L
            EVP_MD_CTX_free(mdctx);
#endif
            throw std::runtime_error("Bad stream status at: \"" + path + "\"");
        }

        if (sz > 0 and !EVP_DigestUpdate(mdctx, buf, sz))
        {
            instream.close();
#if OPENSSL_VERSION_NUMBER > 0x10100000L
            EVP_MD_CTX_free(mdctx);
#endif
            throw std::runtime_error("EVP_DigestUpdate() failed");
        }
    }

    unsigned char md_value[EVP_MAX_MD_SIZE]{0};
    unsigned int md_len = 0;

    if (!EVP_DigestFinal(mdctx, md_value, &md_len))
    {
        instream.close();
#if OPENSSL_VERSION_NUMBER > 0x10100000L
        EVP_MD_CTX_free(mdctx);
#endif
        throw ("EVP_DigestFinal() failed");
    }

    instream.close();

    char md_string[EVP_MAX_MD_SIZE*2+1]{0};

    for (auto i = 0; i < md_len; i++)
    {
        sprintf(md_string + (2*i), "%02x", md_value[i]);
    }

#if OPENSSL_VERSION_NUMBER > 0x10100000L
    EVP_MD_CTX_free(mdctx);
#endif

    return md_string;
}

std::string utils::checksum_adler32(std::string const& path)
{
    std::ifstream instream(path, std::ios::binary);

    if (!instream)
    {
        throw std::runtime_error("Error opening " + path);
    }

    auto cs = adler32(0, NULL, 0);

    while (!instream.eof())
    {
        char buf[BUFSIZ]{0};
        memset(buf, 0, BUFSIZ);

        instream.read(buf, BUFSIZ);
        size_t sz = instream.gcount();

        if (instream.bad())
        {
            instream.close();
            throw std::runtime_error("Bad stream status at: \"" + path + "\"");
        }

        if (sz > 0)
        {
            cs = adler32(cs, (uint8_t*)buf, sz);
        }
    }

    instream.close();

    // adler is a ulong type, a.k.a, uint32_t
    char cs_string[9]{0};
    sprintf(cs_string, "%08x", cs);

    return cs_string;
}

// ----------------------------------------------------------------------

std::string utils::checksum_of_checksums(std::vector<std::string> &checksums,
                                         std::string const        &digest)
{
    EVP_MD const *md = EVP_get_digestbyname(digest.c_str());

    if (!md)
    {
        throw std::runtime_error("Unknown message digest " + digest);
    }

    return checksum_of_checksums(checksums, md);
}

std::string utils::checksum_of_checksums(std::vector<std::string> &checksums,
                                         EVP_MD const             *md)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX mdctx_{0};
    EVP_MD_CTX *mdctx = &mdctx_;
#else
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
#endif

    if (!mdctx)
    {
        throw std::runtime_error("EVP_MD_CTX_new() failed");
    }

    if (!EVP_DigestInit(mdctx, md))
    {
#if OPENSSL_VERSION_NUMBER > 0x10100000L
        EVP_MD_CTX_free(mdctx);
#endif
        throw std::runtime_error("EVP_DigestInit_ex() failed");
    }

    std::sort(checksums.begin(), checksums.end());

    for (auto const c : checksums)
    {
        const char *buf = c.c_str();
        size_t      sz  = c.size();

        if (!EVP_DigestUpdate(mdctx, buf, sz))
        {
#if OPENSSL_VERSION_NUMBER > 0x10100000L
            EVP_MD_CTX_free(mdctx);
#endif
            throw std::runtime_error("EVP_DigestUpdate() failed");
        }
    }

    unsigned char md_value[EVP_MAX_MD_SIZE]{0};
    unsigned int md_len = 0;

    if (!EVP_DigestFinal(mdctx, md_value, &md_len))
    {
#if OPENSSL_VERSION_NUMBER > 0x10100000L
        EVP_MD_CTX_free(mdctx);
#endif
        throw ("EVP_DigestFinal() failed");
    }

    char md_string[EVP_MAX_MD_SIZE*2+1]{0};

    for (unsigned i = 0; i < md_len; i++)
    {
        sprintf(md_string + (2*i), "%02x", md_value[i]);
    }

#if OPENSSL_VERSION_NUMBER > 0x10100000L
    EVP_MD_CTX_free(mdctx);
#endif

    return md_string;
}

// ----------------------------------------------------------------------

// We want to globally limit the number threads we launch to compute
// file checksum; hence this counter.
static utils::AtomicCounter task_counter;

utils::AtomicCounter& utils::global_task_counter()
{
    return task_counter;
}

// ----------------------------------------------------------------------

std::string utils::dir_checksum_of_checksums(utils::Path const &path,
                                             std::string const &algorithm,
                                             bool const         parallel,
                                             size_t const       max_threads,
                                             size_t const       min_group_size)
{
    auto const *md = EVP_get_digestbyname(algorithm.c_str());

    if (!md)
    {
        throw std::runtime_error("Unknown message digest algorithm: " + algorithm);
    }

    return dir_checksum_of_checksums(path,
                                     md,
                                     parallel,
                                     max_threads,
                                     min_group_size);
}

// ----------------------------------------------------------------------

static std::vector<std::string> checksum_group(utils::PathGroup const &group,
                                               EVP_MD const           *md)
{
    if (!md)
    {
        std::stringstream ss;
        ss << __func__ << "(): Received null message digest parameter";

        utils::slog() << "[utils/checksum_group] " << ss.str();
        throw std::runtime_error(ss.str());
    }

    std::vector<std::string> checksums{};

    for (auto const & p : group)
    {
        if (p.is_regular_file())
        {
            auto checksum = utils::checksum_file(p.name(), md);
            // std::cout << __func__ << " "
            //           << p.name() << " "
            //           << checksum << "\n";
            checksums.emplace_back(checksum);
        }
    }

    return checksums;
}

// ----------------------------------------------------------------------

static std::string
sequential_directory_checksum(utils::Path const &path,
                              EVP_MD const      *md)
{
    std::vector<std::string> checksums{};

    auto tree = utils::DirectoryWalker(path, true);

    for (auto const &p : tree)
    {
        if (p.is_regular_file())
        {
            auto const checksum = utils::checksum_file(p.name(), md);
            // std::cout << __func__ << " "
            //           << p.name() << " "
            //           << checksum << "\n";
            checksums.emplace_back(checksum);
        }
    }

    return utils::checksum_of_checksums(checksums, md);
}

// ----------------------------------------------------------------------

static std::string
parallel_directory_checksum(utils::Path const &path,
                            EVP_MD const      *md,
                            size_t             max_threads,
                            size_t const       file_size_threshold)
{
    std::vector<std::string> checksums{};

    // TODO: Can we tame this monster?  Or rather: should we?
    std::vector<std::future<std::vector<std::string>>> futures{};

    auto       tree   = utils::DirectoryWalker(path, true);
    auto const groups = tree.partition(max_threads);

    std::vector<utils::PathGroup> remainders{};

    for (auto const &group : groups)
    {
        // Don't spawn a thread for groups with no files.
        if (group.count() == 0)
        {
            continue;
        }

        if (task_counter.get() < max_threads)
        {
            auto handle = std::async(std::launch::async,
                                     checksum_group,
                                     group,
                                     md);

            // std::future can't be copied, but it can be std::move()-d.
            futures.emplace_back(std::move(handle));

            task_counter.increment();

            utils::slog() << "[checksum] Running async checksum on file group"
                          << " of " << group.count() << " files, "
                          << group.size() << " bytes "
                          << "(# tasks: "
                          << task_counter.get() << ")";
        }
        else
        {
            // Handle the groups that could not be handled by an async
            // task in the parent thread.
            remainders.emplace_back(group);
        }
    }

    for (auto const &group : remainders)
    {
        if (group.count() == 0)
        {
            continue;
        }

        auto const result = checksum_group(group, md);
        checksums.insert(std::end(checksums),
                         std::begin(result),
                         std::end(result));
    }

    for (auto &f : futures)
    {
        auto result = f.get();

        checksums.insert(std::end(checksums),
                         std::begin(result),
                         std::end(result));

        task_counter.decrement();
    }

    // Check that number of regular files under the given directory
    // tree and number of checksums we've computed are the same.  This
    // should catch errors in case utils::DirectoryWalker class ever
    // gets refactored and changes behavior.
    if (tree.count_regular_files() != checksums.size())
    {
        std::stringstream ss;
        ss << "Mismatch: files: "
           << tree.count_regular_files()
           << ", checksums: " << checksums.size()
           << ", max threads: " << max_threads;

        throw std::runtime_error(ss.str());
    }

    return utils::checksum_of_checksums(checksums, md);
}

// ----------------------------------------------------------------------

std::string utils::dir_checksum_of_checksums(utils::Path const &path,
                                             EVP_MD const      *md,
                                             bool const         parallel,
                                             size_t const       max_threads,
                                             size_t const       min_group_size)
{
    if (!md)
    {
        std::stringstream ss;
        ss << __func__ << "(): Received null message digest parameter";

        utils::slog() << "[utils/checksum] " << ss.str();
        throw std::runtime_error(ss.str());
    }

    if (not path.is_directory())
    {
        std::stringstream ss;
        ss << __func__ << "(): \""
           << path.name()
           << "\" is not a directory.";

        utils::slog() << "[utils/checksum] " << ss.str();
        throw std::runtime_error(ss.str());
    }

    utils::slog() << "[checksum] Running checksum of checksum on "
                  << " directory " << path.name()
                  << " (parallel=" << parallel
                  << ", max threads="<< max_threads
                  << ", current threads=" << task_counter.get() << ")";

    std::string result;

    if (parallel)
    {
        result = parallel_directory_checksum(path,
                                             md,
                                             max_threads,
                                             min_group_size);
    }
    else
    {
        result = sequential_directory_checksum(path, md);
    }

    utils::slog() << "[checksum] checksum of checksum on "
        << " directory " << path.name() << ": " << result;

    return result;
}

// ----------------------------------------------------------------------
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

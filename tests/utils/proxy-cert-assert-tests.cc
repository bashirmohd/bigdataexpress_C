#include <iostream>
#include <thread>

#include <getopt.h>

#include "utils/proxy-cert.h"
#include "proxy-cert-test-creds.h"

#define CATCH_CONFIG_RUNNER
#include "../catch.hpp"

std::string g_common_name;
std::string g_signing_algorithm = "sha256";
int         g_count             = 10;
static bool g_verbose_flag      = false;

static void show_usage(const char * const prog)
{
    std::cout << "Usage: " << prog << " [args]\n\n"
              << "where [args] may be: \n\n"
              << "  -h, -?, --help, --usage: \t Show usage\n"
              << "  -n, --name [ARG]:        \t Common name\n"
              << "  -a, --algo [ARG]:        \t Signing algorithm\n\n";
}

static void do_work(std::string const &common_name,
                    std::string const &signing_algorithm,
                    int it)
{
    auto kc = utils::generate_keypair_and_csr(common_name, signing_algorithm);

    if (g_verbose_flag)
    {
        std::cout << "[test " << it<< "] proxy priv key:\n"
                  << kc.proxy_private_key << "\n";

        std::cout << "[test " << it << "] proxy public key:\n"
                  << kc.proxy_public_key << "\n";

        utils::print_x509_req_info(kc.signing_request);
    }

    REQUIRE(!kc.proxy_public_key.empty());
    REQUIRE(!kc.proxy_private_key.empty());

    EVP_PKEY *priv_key = utils::string_to_private_key(kc.proxy_private_key);
    std::string priv_key_string = utils::private_key_to_string(priv_key);
    EVP_PKEY_free(priv_key);

    REQUIRE(kc.proxy_private_key == priv_key_string);

    EVP_PKEY *pub_key = utils::string_to_public_key(kc.proxy_public_key);
    std::string pub_key_string = utils::public_key_to_string(pub_key);
    EVP_PKEY_free(pub_key);

    REQUIRE(kc.proxy_public_key == pub_key_string);

    auto cert = utils::sign_proxy_csr(test_issuer_key_nopass,
                                      test_issuer_cert,
                                      kc.signing_request);

    REQUIRE(!cert.empty());

    X509* x509 = utils::string_to_x509(cert);
    std::string x509_str = utils::x509_to_string(x509);
    X509_free(x509);

    REQUIRE(x509_str == cert);

    if (g_verbose_flag)
    {
        std::cout << "[test " << it << "] certificate info:\n";
        utils::print_x509_info(cert);
    }
}

TEST_CASE("Running tests")
{
    for (auto i = 0; i < g_count; ++i)
    {
        do_work(g_common_name, g_signing_algorithm, i);
    }
}

TEST_CASE("Running tests in threads")
{
    for (auto i = 0; i < g_count; ++i)
    {
        std::thread(do_work, g_common_name, g_signing_algorithm, i).join();
    }
}

int main(int argc, char *argv[])
{
    Catch::Session session;

    // Add new options to Catch's command line parser.
    using namespace Catch::clara;
    auto cli
        = session.cli()            // Get Catch's composite command line parser
        | Opt(g_common_name, "cn") // bind variable to new option, with hint string
        ["-n"]["--cn"]             // the option names it will respond to
        ("Common name")            // description string for the help output
        | Opt(g_signing_algorithm, "algo")
        ["-a"]["--algo"]
        ("Signing algorithm")
        | Opt(g_count, "count")
        ["-C"]["--count"]
        ("Iterations")
        | Opt(g_verbose_flag)
        ["-V"]["--verbose"]
        ("Run verbosely");

    // Now pass the new composite back to Catch.
    session.cli(cli);

    int rc = session.applyCommandLine( argc, argv );
    if (rc != 0)
  	return rc;

    return session.run();
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

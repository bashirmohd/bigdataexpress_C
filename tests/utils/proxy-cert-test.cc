#include <iostream>
#include <vector>
#include <thread>

#include "utils/proxy-cert.h"
#include "proxy-cert-test-creds.h"

static void do_work(int it)
{
    auto c = utils::generate_proxy_cert(test_issuer_key_nopass,
                                        test_issuer_cert,
                                        "test-common-name");

    std::cout << "[test " << it << " ] proxy priv key:\n"
              << c.proxy_private_key << "\n";

    std::cout << "[test " << it << " ] signed cert:\n"
              << c.signed_proxy_cert << "\n";

    std::cout << "[test " << it << " ] certificate info:\n";
    utils::print_x509_info(c.signed_proxy_cert);
}

static void do_silent_work(int it)
{
    auto c = utils::generate_proxy_cert(test_issuer_key_nopass,
                                        test_issuer_cert,
                                        "test-common-name");
    if (c.signed_proxy_cert.empty())
    {
        std::cout << "Error in step " << it << "; quitting.\n";
        exit(it);
    }
    else
    {
        std::cout << "Test " << it << " done.\n";
    }
}

int main(int argc, char **argv)
{
    std::cout << "Running in sequence...\n";

    for (auto i = 1; i <= 10; i++)
    {
        do_work(i);
    }

    std::cout << "Running in threads...\n";

    for (auto i = 1; i <= 10; i++)
    {
        std::thread(do_silent_work, i).join();
    }

    return 0;
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:


#include <iostream>
#include <cstdio>
#include "utils/utils.h"

int main()
{
    utils::CertSSH("------\ntest key\n------\n",
                   "------\ntest cert\n------\n");

    char *keyfile = getenv("X509_USER_KEY");
    char *certfile = getenv("X509_USER_CERT");

    std::cout << "key file:   " << keyfile << "\n"
              << "cert file:  " << certfile << "\n";

    // std::remove(keyfile);
    // std::remove(certfile);

    return 0;
}

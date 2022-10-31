#ifndef BDE_PROXY_CERT_H
#define BDE_PROXY_CERT_H

#include <string>

#include <openssl/pem.h>

// Sketching out an API for RFC 3820 proxy certificate generation.

namespace utils
{
    // ------------------------------------------------------------
    // Get proxy private key and signed proxy cert in one-shot.

    struct proxy_cert
    {
        std::string proxy_private_key; // Proxy private key, base-64 encoded.
        std::string signed_proxy_cert; // Signed proxy cert, base-64 encoded.
    };

    // Given an issuer key and cert, generate a proxy certificate.
    proxy_cert generate_proxy_cert(std::string const &issuer_key,
                                   std::string const &issuer_cert,
                                   std::string const &common_name = std::string());

    // ------------------------------------------------------------
    // The functions below basically does the same thing as above, but
    // in two steps: in step (1), you get keypair and signing request;
    // in step (2) you get a signed certificate.

    struct keypair_and_csr
    {
        std::string proxy_private_key;  // Private key, base-64 encoded.
        std::string proxy_public_key;   // Public key, base-64 encoded.
        std::string signing_request;    // Signing request, base-64 encoded.
    };

    // Create a keypair and a signing request.
    //
    // "cn" is common name; when not specified, a random number will
    // be used.
    //
    // "algo" is signature algorithm; default is sha256 with RSA
    // encryption.
    keypair_and_csr generate_keypair_and_csr(std::string const &cn = std::string(),
                                             std::string const &algo = "sha256");

    typedef std::string signed_proxy_cert;

    // Create a certificate when presented with a signing request, an
    // issuer key, and an issuer certificate.
    signed_proxy_cert sign_proxy_csr(std::string const &issuer_key,
                                     std::string const &issuer_cert,
                                     std::string const &signing_request);

    // ------------------------------------------------------------
    // Some utility functions for conversion between strings and
    // OpenSSL data structures.

    X509* string_to_x509(std::string const &str);
    std::string x509_to_string(X509 const *cert);

    X509_REQ* string_to_x509_req(std::string const &str);
    std::string x509_req_to_string(X509_REQ const *req);

    EVP_PKEY* string_to_private_key(std::string const &str);
    std::string private_key_to_string(EVP_PKEY const *key);

    EVP_PKEY* string_to_public_key(std::string const &str);
    std::string public_key_to_string(EVP_PKEY const *key);

    // Debugging aids.
    void print_x509_info(std::string const &str);
    void print_x509_info(X509 const *cert);

    void print_x509_req_info(std::string const &csr);
    void print_x509_req_info(X509_REQ const *csr);

    void print_keypair_info(EVP_PKEY const *pkey);
};

#endif // BDE_PROXY_CERT_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

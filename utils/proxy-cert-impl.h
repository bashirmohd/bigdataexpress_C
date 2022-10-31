#ifndef BDE_PROXY_CERT_IMPL_H
#define BDE_PROXY_CERT_IMPL_H

#include <memory>

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>

#include "proxy-cert-constants.h"

// ----------------------------------------------------------------------

using BIO_ptr = std::unique_ptr<BIO, decltype(&BIO_free)>;
using X509_ptr = std::unique_ptr<X509, decltype(&X509_free)>;

// ----------------------------------------------------------------------

namespace utils
{
    // Copy constructors and assignment operators should not create
    // aliases, even by mistake; so they just call abort().

    struct proxy_handle_attrs_t
    {
        proxy_handle_attrs_t()
            : key_bits(1024)
            , init_prime(RSA_F4)
            , signing_algorithm(nullptr)
            , clock_skew(5*60)
            , key_gen_callback(nullptr)
            { }

        // The size of the keys to generate for the certificate
        // request.
        int key_bits;

        // The initial prime to use for creating the key pair.
        int init_prime;

        // The signing algorithm to use for generating the proxy
        // certificate.
        const EVP_MD *signing_algorithm;

        // The clock skew (in seconds) allowed for the proxy
        // certificate.  The skew adjusts the validity time of the
        // proxy cert.
        int clock_skew;

        // The callback for the creation of the public/private key
        // pair.
        void (*key_gen_callback)(int, int, void *);
    };


    struct proxy_handle_t
    {
        proxy_handle_t(std::string cn=std::string())
            : req(X509_REQ_new())
            , proxy_key(nullptr)
            , attrs()
            , proxy_cert_info(PROXY_CERT_INFO_EXTENSION_new())
            , time_valid(12*60)
            , type(GLOBUS_GSI_CERT_UTILS_TYPE_DEFAULT)
            , common_name(cn)
            , extensions(nullptr)
            {

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
                // This is an important OpenSSL initialization step.
                // It is easy to forget to do this, but without this
                // none of the signature algorithms will be available.
                // We could probably move this step somewhere else...
                OpenSSL_add_all_algorithms();
                OpenSSL_add_all_ciphers();
                OpenSSL_add_all_digests();
#endif

                proxy_cert_info->proxyPolicy->policyLanguage =
                    OBJ_dup(OBJ_nid2obj(NID_id_ppl_inheritAll));
            }

        proxy_handle_t(proxy_handle_t const &rhs) {
            exit(1);
        }

        proxy_handle_t &operator=(proxy_handle_t const &rhs) {
            exit(1);
        }

        ~proxy_handle_t() {
            if (req) X509_REQ_free(req);
            if (proxy_key) EVP_PKEY_free(proxy_key);

            if (proxy_cert_info->pcPathLengthConstraint)
                ASN1_INTEGER_free(proxy_cert_info->pcPathLengthConstraint);

            if (proxy_cert_info)
                PROXY_CERT_INFO_EXTENSION_free(proxy_cert_info);

            if (extensions)
                sk_X509_EXTENSION_pop_free(extensions, X509_EXTENSION_free);

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
            ERR_remove_state(0);
            ERR_remove_thread_state(0);

            // libcurl still needs the SSL global state for it to work
            // correctly, so we avoid callling EVP_cleanup() here.
#endif
        }

        void set_req(X509_REQ *new_req) {
            X509_REQ_free(req);
            req = new_req;
        }

        // TODO: write copy constructor, so that we can have
        // assignments and stop leaking in the destructor.

        // The proxy request.
        X509_REQ *req;

        // The proxy private key.
        EVP_PKEY *proxy_key;

        // Proxy handle attributes.
        proxy_handle_attrs_t attrs;

        // The proxy cert info extension used in the operations.
        PROXY_CERT_INFO_EXTENSION *proxy_cert_info;

        // The number of minutes the proxy certificate is valid for.
        int time_valid;

        // The type of the generated proxy.
        cert_type_t type;

        // The common name used for draft compliant proxies. If not set a
        // random common name will be generated.
        std::string common_name;

        // The extensions to be added to the proxy certificate.
        STACK_OF(X509_EXTENSION)* extensions;
    };

    struct cred_handle_t
    {
        cred_handle_t()
            : cert(nullptr)
            , key(nullptr)
            , cert_chain(nullptr)
            , goodtill(0)
            { }

        ~cred_handle_t() {
            if (cert) X509_free(cert);
            if (key) EVP_PKEY_free(key);
            if (cert_chain) sk_X509_pop_free(cert_chain, X509_free);
        }

        cred_handle_t(cred_handle_t const &rhs) {
            abort();
        }

        cred_handle_t &operator=(cred_handle_t const &rhs) {
            abort();
        }

        void set_cert(X509 *new_cert) {
            if (cert) X509_free(cert);
            cert = new_cert;
        }

        void set_key(EVP_PKEY *new_key) {
            if (key) EVP_PKEY_free(key);
            key = new_key;
        }

        void set_cert_chain(STACK_OF(X509) *new_chain) {
            if (cert_chain)
                sk_X509_pop_free(cert_chain, X509_free);

            if (!new_chain) {
                cert_chain = nullptr;
                return;
            }

            cert_chain    = sk_X509_new_null();
            auto numcerts = sk_X509_num(new_chain);

            for (auto i = 0; i < numcerts; ++i) {
                X509 *tmp = nullptr;
                if ((tmp = X509_dup(sk_X509_value(new_chain, i))) == nullptr) {
                    throw std::runtime_error("Couldn't copy X509 cert from "
                                             " credential's cert chain");
                }
                sk_X509_insert(cert_chain, tmp, i);
            }
        }

        // The credential's signed certificate.
        X509 *cert;

        // The private key of the credential.
        EVP_PKEY *key;

        // The chain of signing certificates.
        STACK_OF(X509) *cert_chain;

        // The amount of time the credential is valid for.
        time_t goodtill;
    };

    cred_handle_t read_cert_and_key(std::string const &user_cert,
                                    std::string const &user_key);

    cred_handle_t proxy_create_signed(proxy_handle_t &p_handle,
                                      cred_handle_t  &c_handle);

    // This will create a keypair and signing request, and attach
    // those to the proxy handle.  The passed proxy handle will be
    // modified.
    void proxy_create_signing_request(proxy_handle_t &p_handle,
                                      const EVP_MD   *signing_algorithm);

    // Return value will contain signed proxy certificate; inputs are
    // the signing certificate, and issuer credentials.
    cred_handle_t proxy_sign_signing_request(proxy_handle_t &p_handle,
                                             cred_handle_t  &c_handle);

    void proxy_write_outfile(cred_handle_t const &cred_handle,
                             std::string const   &out_file_name);

    // Debugging aid.
    void proxy_make_verbose(bool b);

    // Implementation details.
    cert_type_t get_cert_type(X509 *const cert);
    cert_type_t proxy_determine_type(proxy_handle_t const &handle,
                                     cred_handle_t const  &issuer);

    void proxy_create_req(proxy_handle_t &handle);
    void proxy_inquire_req(proxy_handle_t &handle, BIO *input_bio);

    void proxy_sign_key(proxy_handle_t       &inquire_handle,
                        cred_handle_t const  &issuer_credential,
                        EVP_PKEY             *public_key,
                        X509                **signed_cert);
};

#endif // BDE_PROXY_CERT_IMPL_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

#include <stdexcept>
#include <iostream>

#include <openssl/err.h>
#include <openssl/pem.h>

#include "proxy-cert.h"
#include "proxy-cert-impl.h"

// ----------------------------------------------------------------------

static utils::cred_handle_t make_cred_handle(std::string const &issuer_key,
                                             std::string const &issuer_cert)
{
    utils::cred_handle_t c_handle;

    // Convert issuer_key from string to EVP_PKEY.
    c_handle.set_key(utils::string_to_private_key(issuer_key));

    // Convert issuer_cert from string to X509.
    c_handle.set_cert(utils::string_to_x509(issuer_cert));

    // TODO: read cert chain.  The certificates issued by cilogon
    // doesn't appear to contain the entire chain; therefore it is
    // probably safe to omit that step.
    c_handle.set_cert_chain(nullptr);

    return c_handle;
}

// ----------------------------------------------------------------------

utils::keypair_and_csr utils::generate_keypair_and_csr(std::string const &common_name,
                                                       std::string const &algorithm)
{
    proxy_handle_t p_handle(common_name);

    const EVP_MD *signing_algorithm = EVP_sha256();

    if (not algorithm.empty())
    {
        signing_algorithm = EVP_get_digestbyname(algorithm.c_str());
        if (!signing_algorithm)
        {
            throw std::runtime_error("Unknown signature algorithm: " + algorithm);
        }
    }

    proxy_create_signing_request(p_handle, signing_algorithm);

    std::string proxy_priv_key = private_key_to_string(p_handle.proxy_key);
    std::string proxy_pub_key  = public_key_to_string(p_handle.proxy_key);
    std::string proxy_csr      = x509_req_to_string(p_handle.req);

    return { proxy_priv_key, proxy_pub_key, proxy_csr };
}


utils::signed_proxy_cert utils::sign_proxy_csr(std::string const &issuer_key,
                                               std::string const &issuer_cert,
                                               std::string const &signing_request)
{
    cred_handle_t  c_handle = make_cred_handle(issuer_key, issuer_cert);

    proxy_handle_t p_handle;
    p_handle.set_req(string_to_x509_req(signing_request));

    cred_handle_t pc_handle = proxy_sign_signing_request(p_handle, c_handle);

    std::string signed_proxy_cert = x509_to_string(pc_handle.cert);

    return signed_proxy_cert;
}

utils::proxy_cert utils::generate_proxy_cert(std::string const &issuer_key,
                                             std::string const &issuer_cert,
                                             std::string const &common_name)
{
    proxy_handle_t p_handle(common_name);

    utils::cred_handle_t c_handle  = make_cred_handle(issuer_key, issuer_cert);
    utils::cred_handle_t pc_handle = utils::proxy_create_signed(p_handle, c_handle);

    std::string proxy_priv_key    = private_key_to_string(p_handle.proxy_key);
    std::string signed_proxy_cert = x509_to_string(pc_handle.cert);

    return {  proxy_priv_key, signed_proxy_cert };
}

// ----------------------------------------------------------------------

X509* utils::string_to_x509(std::string const &cert_str)
{
    BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);

    BIO_puts(bio.get(), cert_str.c_str());

    X509 *cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
    if (!cert)
    {
        throw std::runtime_error("Error reading X509 structure from string");
    }

    return cert;
}

std::string utils::x509_to_string(X509 const *cert)
{
    BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);

    if (!PEM_write_bio_X509(bio.get(), const_cast<X509*>(cert)))
    {
        throw std::runtime_error("Failed to write X509 to BIO");
    }

    BUF_MEM *bptr = nullptr;
    BIO_get_mem_ptr(bio.get(), &bptr);

    if (!bptr)
    {
        throw std::runtime_error("Failed to convert X509 structure to string");
    }

    return std::string(bptr->data, bptr->length);
}

// ----------------------------------------------------------------------

X509_REQ* utils::string_to_x509_req(std::string const &signing_request)
{
    BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);

    BIO_puts(bio.get(), signing_request.c_str());

    X509_REQ *csr = PEM_read_bio_X509_REQ(bio.get(), nullptr, nullptr, nullptr);
    if (!csr)
    {
        throw std::runtime_error("Error reading X509_REQ structure from string");
    }

    return csr;
}

std::string utils::x509_req_to_string(X509_REQ const *req)
{
    BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);

    if (!PEM_write_bio_X509_REQ(bio.get(), const_cast<X509_REQ *>(req)))
    {
        throw std::runtime_error("Failed to write X509_REQ to BIO");
    }

    BUF_MEM *bptr = nullptr;
    BIO_get_mem_ptr(bio.get(), &bptr);

    if (!bptr)
    {
        throw std::runtime_error("Failed to convert X509_REQ structure to string");
    }

    return std::string(bptr->data, bptr->length);
}

// ----------------------------------------------------------------------

// Read a private key structure from a string.
EVP_PKEY* utils::string_to_private_key(std::string const &issuer_key)
{
    BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);

    BIO_puts(bio.get(), issuer_key.c_str());

    EVP_PKEY *pkey = nullptr;

    if ((pkey = PEM_read_bio_PrivateKey(bio.get(),
                                        nullptr, nullptr, nullptr)) == nullptr)
    {
        if (ERR_GET_REASON(ERR_peek_error()) == PEM_R_BAD_PASSWORD_READ)
        {
            throw std::runtime_error("Error reading password protected private key "
                                     "from input string");
        }

        throw std::runtime_error("Error reading private key from input string");
    }

    return pkey;
}

std::string utils::private_key_to_string(EVP_PKEY const *key)
{
    BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);

    if (!PEM_write_bio_PrivateKey(bio.get(),
                                  const_cast<EVP_PKEY *>(key),
                                  0, 0, 0, 0, 0))
    {
        throw std::runtime_error("Failed to write EVP_PKEY to BIO");
    }

    BUF_MEM *bptr = nullptr;
    BIO_get_mem_ptr(bio.get(), &bptr);

    if (!bptr)
    {
        throw std::runtime_error("Failed to parse EVP_PKEY structure to private key");
    }

    return std::string(bptr->data, bptr->length);
}

// ----------------------------------------------------------------------

EVP_PKEY* utils::string_to_public_key(std::string const &issuer_key)
{
    BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);

    BIO_puts(bio.get(), issuer_key.c_str());

    EVP_PKEY *pkey = nullptr;

    if ((pkey = PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr)) == nullptr)
    {
        throw std::runtime_error("Error reading public key from input string");
    }

    return pkey;
}

std::string utils::public_key_to_string(EVP_PKEY const *key)
{
    BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);

    if (!PEM_write_bio_PUBKEY(bio.get(), const_cast<EVP_PKEY *>(key)))
    {
        throw std::runtime_error("Failed to write pubkey structure to BIO");
    }

    BUF_MEM *bptr = nullptr;
    BIO_get_mem_ptr(bio.get(), &bptr);

    if (!bptr)
    {
        throw std::runtime_error("Failed to parse pubkey structure");
    }

    return std::string(bptr->data, bptr->length);
}

// ----------------------------------------------------------------------

void utils::print_x509_info(X509 const *cert)
{
    X509_print_fp(stdout, const_cast<X509*>(cert));
}

void utils::print_x509_info(std::string const &str)
{
    X509* certp = string_to_x509(str);
    X509_print_fp(stdout, certp);
    X509_free(certp);
}

void utils::print_x509_req_info(std::string const &csr)
{
    X509_REQ* csrp = string_to_x509_req(csr);
    X509_REQ_print_fp(stdout, csrp);
    X509_REQ_free(csrp);
}

void utils::print_x509_req_info(X509_REQ const *csr)
{
    X509_REQ_print_fp(stdout, const_cast<X509_REQ*>(csr));
}

void utils::print_keypair_info(EVP_PKEY const *pkey)
{
    BIO *outbio  = BIO_new_fp(stdout, BIO_NOCLOSE);

    std::cout << "public key:\n";
    EVP_PKEY_print_public(outbio, pkey, 2, 0);

    std::cout << "key params:\n";
    EVP_PKEY_print_params(outbio, pkey, 2, 0);

    std::cout << "private key:\n";
    EVP_PKEY_print_private(outbio, pkey, 2, 0);

    BIO_free_all(outbio);
}

// ----------------------------------------------------------------------

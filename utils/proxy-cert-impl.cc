#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/bn.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bn.h>

#include <getopt.h>
#include <unistd.h>

#include <string>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "proxy-cert-impl.h"

// ------------------------------------------------------------------------

static bool g_verbose_flag = false;

void utils::proxy_make_verbose(bool choice)
{
    g_verbose_flag = choice;
}

// static bool g_verify_flag  = false;

// ------------------------------------------------------------------------

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#define X509_STORE_CTX_get_error(ctx)   (ctx->error)
#define ASN1_STRING_get0_data           ASN1_STRING_data
#define X509_getm_notBefore             X509_get_notBefore
#define X509_get0_notAfter              X509_get_notAfter
#define X509_set1_notAfter              X509_set_notAfter
#endif

// ------------------------------------------------------------------------

// Some debugging helpers.

#define PRINT_OPENSSL_ERROR() do {                                      \
        const char *e = ERR_reason_error_string(ERR_get_error());       \
        if (e)                                                          \
            std::cout << "Error: " << e << "\n";                        \
    } while (0);

static void trace_key(std::string const           &func,
                      int const                    line,
                      utils::proxy_handle_t const &p_handle)
{
    std::cout << "----- " << func << " : " << line << " ------\n";
    if (p_handle.proxy_key)
    {
        PEM_write_PrivateKey(stdout, p_handle.proxy_key, 0, 0, 0, 0, 0);
        PEM_write_PUBKEY(stdout, p_handle.proxy_key);
    }
    else
    {
        std::cout << "No key yet\n";
    }
    std::cout << "--------------------------------------------------\n";
}

static void trace_key(std::string const &func,
                      int const          line,
                      EVP_PKEY          *key)
{
    std::cout << "----- " << func << " : " << line << " ------\n";
    if (key)
    {
        PEM_write_PrivateKey(stdout, key, 0, 0, 0, 0, 0);
        PEM_write_PUBKEY(stdout, key);
    }
    else
    {
        std::cout << "Empty key\n";
    }
    std::cout << "--------------------------------------------------\n";
}

// ------------------------------------------------------------------------

static void read_cert(utils::cred_handle_t &handle, std::string const &file)
{
    if (g_verbose_flag)
        std::cout << "Reading " << file << ".\n";

    BIO_ptr bio(BIO_new_file(file.c_str(), "r"), BIO_free);

    if (!bio.get())
    {
        throw std::runtime_error("Error opening " + file);
    }

    if (!PEM_read_bio_X509(bio.get(), &handle.cert, nullptr, nullptr))
    {
        throw std::runtime_error("Error reading certificate from " + file);
    }

    if (handle.cert_chain)
    {
        handle.set_cert_chain(nullptr);
    }

    if ((handle.cert_chain = sk_X509_new_null()) == nullptr)
    {
        throw std::runtime_error("Can't initialize cert chain");
    }

    int i = 0;

    while (!BIO_eof(bio.get()))
    {
        X509 *cert = nullptr;

        if (!PEM_read_bio_X509(bio.get(), &cert, nullptr, nullptr))
        {
            ERR_clear_error();
            break;
        }

        if (!sk_X509_insert(handle.cert_chain, cert, i))
        {
            auto const *subj = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
            std::string error = "error adding cert " + std::string(subj) +
                " to issuer cert chain";

            X509_free(cert);
            throw std::runtime_error(error);
        }

        ++i;
    }

    // TODO: do what globus_i_gsi_cred_goodtill() does.
}

static void read_key(utils::cred_handle_t &handle, std::string const &file)
{
    if (g_verbose_flag)
    {
        std::cout << "Reading " << file << ".\n";
    }

    BIO_ptr bio(BIO_new_file(file.c_str(), "r"), BIO_free);

    if (!bio.get())
    {
        throw std::runtime_error("Error opening " + file);
    }

    // We can add a custom callback to provide password as the third
    // parameter here, but OpenSSL defaults are good enough, for now
    // anyway.
    if (!PEM_read_bio_PrivateKey(bio.get(), &handle.key, nullptr, nullptr))
    {
        if (ERR_GET_REASON(ERR_peek_error()) == PEM_R_BAD_PASSWORD_READ)
        {
            throw std::runtime_error("Error reading password protected private key "
                                     " from " + file);
        }

        throw std::runtime_error("Error reading private key from " + file);
    }
}

utils::cred_handle_t utils::read_cert_and_key(std::string const &user_cert,
                                              std::string const &user_key)
{
    utils::cred_handle_t c_handle;

    read_cert(c_handle, user_cert);
    read_key(c_handle, user_key);

    return c_handle;
}


// Create a proxy credential request, an unsigned certificate and the
// corresponding private key, based on the handle that is passed in.
//
// The public part of the request is written to the BIO return value.
// After the request is written, the PROXYCERTINFO extension contained
// in the handle is written to the BIO.
//
// The proxy handle is updated with the private key.
void utils::proxy_create_req(utils::proxy_handle_t &handle)
{
    int pci_NID = NID_undef;

    if (handle.proxy_key)
    {
        throw std::runtime_error("The handle's private key has "
                                 "already been initialized");
    }

    // initialize the private key.
    if ((handle.proxy_key = EVP_PKEY_new()) == nullptr)
    {
        throw std::runtime_error("Couldn't create new private "
                                 "key structure for handle");
    }

    // First, generate and setup private/public key pair.
    RSA *rsa_key = RSA_new();
    if (!rsa_key)
    {
        throw std::runtime_error("Couldn't generate RSA key pair for proxy handle");
    }

    BIGNUM *e = BN_new();
    if (!e)
    {
        throw std::runtime_error("Couldn't generate RSA key pair for proxy handle");
    }

    if (BN_add_word(e, (BN_ULONG) handle.attrs.init_prime) != 1)
    {
        throw std::runtime_error("Couldn't generate RSA key pair for proxy handle");
    }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    // BN_GENCB_new() was added in OpenSSL 1.1.0; we can't use it yet.
    BN_GENCB gencb_{0};
    BN_GENCB *gencb = &gencb_;
#else
    BN_GENCB *gencb = BN_GENCB_new();
#endif

    if (!gencb)
    {
        throw std::runtime_error("BN_GENCB_new() failed");
    }

    BN_GENCB_set_old(gencb, handle.attrs.key_gen_callback, nullptr);

    if (RSA_generate_key_ex(rsa_key, handle.attrs.key_bits, e, gencb) != 1)
    {
        throw std::runtime_error("Couldn't generate RSA key pair for proxy handle");
    }

    if (!EVP_PKEY_assign_RSA(handle.proxy_key, rsa_key))
    {
        throw std::runtime_error("Could not set private key in proxy handle");
    }

    BIO_ptr inout_bio(BIO_new(BIO_s_mem()), BIO_free);

    if (i2d_PrivateKey_bio(inout_bio.get(), handle.proxy_key) <= 0)
    {
        throw std::runtime_error("Couldn't get length of DER encoding "
                                 "for private key");
    }

    if (!X509_REQ_set_version(handle.req, 0L))
    {
        throw std::runtime_error("Could not set version of X.509 request "
                                 "in proxy handle");
    }

    if (!X509_REQ_set_pubkey(handle.req, handle.proxy_key))
    {
        throw std::runtime_error("Couldn't set public key of X.509 request "
                                 "in proxy handle");
    }

    X509_NAME *req_name = X509_NAME_new();
    if (!req_name)
    {
        throw std::runtime_error("Couldn't create a new X509_NAME "
                                 "for the proxy certificate request");
    }

    X509_NAME_ENTRY *req_name_entry =
        X509_NAME_ENTRY_create_by_NID(nullptr,
                                      NID_commonName,
                                      V_ASN1_APP_CHOOSE,
                                      (unsigned char *) "NULL SUBJECT NAME ENTRY",
                                      -1);

    if (!req_name_entry)
    {
        throw std::runtime_error("Couldn't create a new X509_NAME_ENTRY "
                                 "for the proxy certificate request");
    }

    if (!X509_NAME_add_entry(req_name,
                             req_name_entry,
                             X509_NAME_entry_count(req_name),
                             0))
    {
        throw std::runtime_error("Couldn't add the X509_NAME_ENTRY "
                                 "to the proxy certificate request's subject name");
    }

    if (req_name_entry)
    {
        X509_NAME_ENTRY_free(req_name_entry);
        req_name_entry = nullptr;
    }

    X509_REQ_set_subject_name(handle.req, req_name);
    X509_NAME_free(req_name);

    req_name = nullptr;

#if 1
    if (GLOBUS_GSI_CERT_UTILS_IS_GSI_3_PROXY(handle.type))
    {
        pci_NID = OBJ_txt2nid(PROXYCERTINFO_OLD_OID);
    }
    else if (!GLOBUS_GSI_CERT_UTILS_IS_GSI_2_PROXY(handle.type))
    {
        pci_NID = OBJ_txt2nid(PROXYCERTINFO_OID);
    }
#endif

#if 0
    // hardcoded
    pci_NID = OBJ_txt2nid(PROXYCERTINFO_OID);
#endif

    if (pci_NID != NID_undef)
    {
        ASN1_OCTET_STRING       *ext_data    = nullptr;
        X509_EXTENSION          *pci_ext     = nullptr;
        STACK_OF(X509_EXTENSION) *extensions = nullptr;
        const X509V3_EXT_METHOD *ext_method  = X509V3_EXT_get_nid(pci_NID);

        if (ext_method->i2d)
        {
            int length = ext_method->i2d(handle.proxy_cert_info, nullptr);

            if (length < 0)
            {
                throw std::runtime_error("Couldn't convert PROXYCERTINFO struct "
                                         "from internal to DER encoded form");
            }

            unsigned char *data     = new unsigned char[length]();
            unsigned char *der_data = data;

            length = ext_method->i2d(handle.proxy_cert_info, &der_data);

            if (length < 0)
            {
                delete[] data;
                throw std::runtime_error("Couldn't convert PROXYCERTINFO struct "
                                         "from internal to DER encoded form");
            }

            ext_data = ASN1_OCTET_STRING_new();

            if (!ASN1_OCTET_STRING_set(ext_data, data, length))
            {
                ASN1_OCTET_STRING_free(ext_data);
                delete[] data;
                throw std::runtime_error("Couldn't convert PROXYCERTINFO struct "
                                         "from internal to DER encoded form");
            }

            delete[] data;

            pci_ext = X509_EXTENSION_create_by_NID(nullptr,
                                                   pci_NID,
                                                   1,
                                                   ext_data);
            if (!pci_ext)
            {
                ASN1_OCTET_STRING_free(ext_data);
                throw std::runtime_error("Couldn't create PROXYCERTINFO extension");
            }

            ASN1_OCTET_STRING_free(ext_data);
        }
        else
        {
            long                db         = 0;
            X509V3_CONF_METHOD  method     = { nullptr, nullptr, nullptr, nullptr };
            char                language[80]{0};
            int                 pathlen    = 0;
            unsigned char      *policy     = nullptr;
            int                 policy_len = 0;

            OBJ_obj2txt(language, 80,
                        handle.proxy_cert_info->proxyPolicy->policyLanguage, 1);

            std::stringstream svalue;
            svalue << "language:" << language;

            if (handle.proxy_cert_info->pcPathLengthConstraint != nullptr)
            {
                pathlen =
                    ASN1_INTEGER_get(handle.proxy_cert_info->pcPathLengthConstraint);
                if (pathlen > 0)
                {
                    svalue << ",pathlen:" << pathlen;
                }
            }

            if (handle.proxy_cert_info->proxyPolicy->policy)
            {
                policy_len =
                    ASN1_STRING_length(handle.proxy_cert_info->proxyPolicy->policy);

                policy = new unsigned char[policy_len + 1]();

                std::memcpy(
                    policy,
                    ASN1_STRING_get0_data(handle.proxy_cert_info->proxyPolicy->policy),
                    policy_len);
                policy[policy_len] = '\0';

                svalue << ",policy:text:" << policy;

                delete[] policy;
            }

            X509V3_CTX ctx;
            X509V3_set_ctx(&ctx, nullptr, nullptr, nullptr, nullptr, 0L);

            ctx.db_meth = &method;
            ctx.db      = &db;
            pci_ext     = X509V3_EXT_conf_nid(nullptr,
                                              &ctx,
                                              pci_NID,
                                              (char*) svalue.str().c_str());

            if (GLOBUS_GSI_CERT_UTILS_IS_RFC_PROXY(handle.type))
            {
                X509_EXTENSION_set_critical(pci_ext, 1);
            }
        }

        extensions = sk_X509_EXTENSION_new_null();

        sk_X509_EXTENSION_push(extensions, pci_ext);

        X509_REQ_add_extensions(handle.req, extensions);

        sk_X509_EXTENSION_pop_free(extensions, X509_EXTENSION_free);
    }

    if (!X509_REQ_sign(handle.req, handle.proxy_key,
                       handle.attrs.signing_algorithm ?
                       handle.attrs.signing_algorithm : EVP_sha1()))
    {
        throw std::runtime_error("Couldn't sign the X509_REQ structure "
                                 "for later verification");
    }

    // Note: do not free the RSA key yet.

    if (e)
    {
        BN_free(e);
    }

    if (req_name)
    {
        X509_NAME_free(req_name);
    }

    if (req_name_entry)
    {
        X509_NAME_ENTRY_free(req_name_entry);
    }
}


void proxy_set_validity(X509 *new_pc,
                        X509 *issuer_cert,
                        int   skew_allowable,
                        int   time_valid)
{
    if (time_valid > ((time_t)(~0U>>1))/60)
    {
        throw std::runtime_error("Overflow in time value");
    }

    if (X509_gmtime_adj(X509_getm_notBefore(new_pc), (-skew_allowable)) == nullptr)
    {
        throw std::runtime_error("Error adjusting the allowable "
                                 "time skew for proxy");
    }

    time_t         tmp_time    = time(nullptr) + ((long) 60 * time_valid);
    ASN1_UTCTIME * pc_notAfter = nullptr;

    if (time_valid == 0 or
        X509_cmp_time(X509_get0_notAfter(issuer_cert), &tmp_time) < 0)
    {
        if ((pc_notAfter = ASN1_dup_of(ASN1_UTCTIME,
                                       i2d_ASN1_UTCTIME,
                                       d2i_ASN1_UTCTIME,
                                       X509_get0_notAfter(issuer_cert))) == nullptr)
        {
            throw std::runtime_error("Error copying issuer certificate lifetime");
        }
    }
    else
    {
        if ((pc_notAfter = ASN1_UTCTIME_new()) == nullptr)
        {
            throw std::runtime_error("Error creating new ASN1_UTCTIME "
                                     "for expiration date of proxy cert");
        }

        if (X509_gmtime_adj(pc_notAfter, ((long) 60 * time_valid)) == nullptr)
        {
            throw std::runtime_error("Error adjusting X.509 proxy cert's "
                                     "expiration time");
        }
    }

    if (!X509_set1_notAfter(new_pc, pc_notAfter))
    {
        throw std::runtime_error("Error setting X.509 proxy certificate's "
                                 "expiration");
    }

    if (g_verbose_flag)
    {
        BIO_ptr bio(BIO_new_fp(stdout, BIO_NOCLOSE), BIO_free);

        std::cout << "Not valid after: ";
        ASN1_UTCTIME_print(bio.get(), pc_notAfter);
        std::cout << "\n";

        std::cout << "Not valid before: ";
        ASN1_UTCTIME_print(bio.get(), X509_getm_notBefore(new_pc));
        std::cout << "\n";
    }

    if (pc_notAfter)
    {
        ASN1_UTCTIME_free(pc_notAfter);
    }
}


void utils::proxy_inquire_req(utils::proxy_handle_t &handle, BIO *input_bio)
{
    if (input_bio == nullptr)
    {
        throw std::runtime_error("null bio passed to proxy_inquire_req()");
    }

    if (handle.req)
    {
        X509_REQ_free(handle.req);
        handle.req = nullptr;
    }

    if (!d2i_X509_REQ_bio(input_bio, &handle.req))
    {
        throw std::runtime_error("Couldn't convert X509_REQ struct "
                                 "from DER encoded to internal form");
    }

    STACK_OF(X509_EXTENSION) *req_extensions = X509_REQ_get_extensions(handle.req);

    int pci_NID     = OBJ_txt2nid(PROXYCERTINFO_OID);
    int pci_old_NID = OBJ_txt2nid(PROXYCERTINFO_OLD_OID);
    int nid         = NID_undef;

    for (int i = 0; i < sk_X509_EXTENSION_num(req_extensions); i++)
    {
        X509_EXTENSION *extension     = sk_X509_EXTENSION_value(req_extensions,i);
        ASN1_OBJECT    *extension_oid = X509_EXTENSION_get_object(extension);
        nid                           = OBJ_obj2nid(extension_oid);

        if (nid == pci_NID || nid == pci_old_NID)
        {
            if (handle.proxy_cert_info)
            {
                PROXY_CERT_INFO_EXTENSION_free(handle.proxy_cert_info);
                handle.proxy_cert_info = nullptr;
            }

            if ((handle.proxy_cert_info =
                 (PROXY_CERT_INFO_EXTENSION*) X509V3_EXT_d2i(extension)) == nullptr)
            {
                throw std::runtime_error("Can't convert DER encoded PROXYCERTINFO "
                                         "extension to internal form");
            }

            break;
        }
    }

    if (!handle.proxy_cert_info)
    {
        PROXY_POLICY *policy = nullptr;
        if ((policy = handle.proxy_cert_info->proxyPolicy) == nullptr)
        {
            throw std::runtime_error("Can't get policy from PROXYCERTINFO extension");
        }

        ASN1_OBJECT *policy_lang = nullptr;
        if ((policy_lang = policy->policyLanguage) == nullptr)
        {
            throw std::runtime_error("Can't get policy language from "
                                     "PROXYCERTINFO extension");
        }

        int policy_nid = OBJ_obj2nid(policy_lang);

        if (nid == pci_old_NID)
        {
            if (policy_nid == OBJ_txt2nid(IMPERSONATION_PROXY_OID))
            {
                handle.type =
                    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_IMPERSONATION_PROXY;
            }
            else if (policy_nid == OBJ_txt2nid(INDEPENDENT_PROXY_OID))
            {
                handle.type =
                    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_INDEPENDENT_PROXY;
            }
            else if (policy_nid == OBJ_txt2nid(LIMITED_PROXY_OID))
            {
                handle.type =
                    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_LIMITED_PROXY;
            }
            else
            {
                handle.type =
                    GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_RESTRICTED_PROXY;
            }
        }
        else
        {
            if (policy_nid == OBJ_txt2nid(IMPERSONATION_PROXY_OID))
            {
                handle.type=
                    GLOBUS_GSI_CERT_UTILS_TYPE_RFC_IMPERSONATION_PROXY;
            }
            else if (policy_nid == OBJ_txt2nid(INDEPENDENT_PROXY_OID))
            {
                handle.type =
                    GLOBUS_GSI_CERT_UTILS_TYPE_RFC_INDEPENDENT_PROXY;
            }
            else if (policy_nid == OBJ_txt2nid(LIMITED_PROXY_OID))
            {
                handle.type =
                    GLOBUS_GSI_CERT_UTILS_TYPE_RFC_LIMITED_PROXY;
            }
            else
            {
                handle.type =
                    GLOBUS_GSI_CERT_UTILS_TYPE_RFC_RESTRICTED_PROXY;
            }
        }
    }
    else
    {
        handle.type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_PROXY;
    }

    if (req_extensions)
    {
        sk_X509_EXTENSION_pop_free(req_extensions, X509_EXTENSION_free);
    }
}

utils::cert_type_t utils::get_cert_type(X509 *const cert)
{
    X509_NAME                 *name        = nullptr;
    X509_NAME_ENTRY           *new_ne      = nullptr;
    X509_EXTENSION            *pci_ext     = nullptr;
    ASN1_STRING               *data        = nullptr;
    PROXY_CERT_INFO_EXTENSION *pci         = nullptr;
    PROXY_POLICY              *policy      = nullptr;
    ASN1_OBJECT               *policy_lang = nullptr;
    int                        policy_nid;
    int                        index       = -1;

    // Assume it is an EEC if nothing else matches.
    cert_type_t type = GLOBUS_GSI_CERT_UTILS_TYPE_EEC;

    int                critical;
    BASIC_CONSTRAINTS *x509v3_bc = nullptr;

    if ((x509v3_bc = (BASIC_CONSTRAINTS*) X509_get_ext_d2i(cert,
                                                           NID_basic_constraints,
                                                           &critical,
                                                           &index)) && x509v3_bc->ca)
    {
        type = GLOBUS_GSI_CERT_UTILS_TYPE_CA;
        if (x509v3_bc)
        {
            BASIC_CONSTRAINTS_free(x509v3_bc);
        }
        return type;
    }

    X509_NAME *subject = X509_get_subject_name(cert);

    X509_NAME_ENTRY *ne = nullptr;
    if ((ne = X509_NAME_get_entry(subject, X509_NAME_entry_count(subject)-1))
        == nullptr)
    {
        throw std::runtime_error("Can't get X509 name entry from subject");
    }

    if (!OBJ_cmp(X509_NAME_ENTRY_get_object(ne), OBJ_nid2obj(NID_commonName)))
    {
        // the name entry is of the type: common name.
        data = X509_NAME_ENTRY_get_data(ne);

        if (data->length == 5 && !memcmp(data->data,"proxy",5))
        {
            type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_PROXY;
        }
        else if (data->length == 13 && !memcmp(data->data,"limited proxy",13))
        {
            type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_LIMITED_PROXY;
        }
        else if ((index = X509_get_ext_by_NID(cert,
                                              NID_proxyCertInfo,
                                              -1)) != -1  &&
                 (pci_ext = X509_get_ext(cert, index)) &&
                 X509_EXTENSION_get_critical(pci_ext))
        {
            if ((pci = (PROXY_CERT_INFO_EXTENSION*) X509V3_EXT_d2i(pci_ext))
                == nullptr)
            {
                throw std::runtime_error("Can't convert DER encoded PROXYCERTINFO "
                                         "extension to internal form");
            }

            if ((policy = pci->proxyPolicy) == nullptr)
            {
                throw std::runtime_error("Can't get policy from PROXYCERTINFO "
                                         "extension");
            }

            if ((policy_lang = policy->policyLanguage) == nullptr)
            {
                throw std::runtime_error("Can't get policy language from "
                                         "PROXYCERTINFO extension");
            }

            policy_nid = OBJ_obj2nid(policy_lang);

            if (policy_nid == NID_id_ppl_inheritAll)
            {
                type = GLOBUS_GSI_CERT_UTILS_TYPE_RFC_IMPERSONATION_PROXY;
            }
            else if (policy_nid == NID_Independent)
            {
                type = GLOBUS_GSI_CERT_UTILS_TYPE_RFC_INDEPENDENT_PROXY;
            }
            else if (policy_nid == OBJ_txt2nid(LIMITED_PROXY_OID))
            {
                type = GLOBUS_GSI_CERT_UTILS_TYPE_RFC_LIMITED_PROXY;
            }
            else
            {
                type = GLOBUS_GSI_CERT_UTILS_TYPE_RFC_RESTRICTED_PROXY;
            }

            if (X509_get_ext_by_NID(cert, NID_proxyCertInfo, index) != -1)
            {
                throw std::runtime_error("Found more than one PCI extension");
            }
        }
        else if ((index = X509_get_ext_by_NID(cert,
                                              OBJ_txt2nid(PROXYCERTINFO_OLD_OID),
                                              -1)) != -1 &&
                 (pci_ext = X509_get_ext(cert, index)) &&
                 X509_EXTENSION_get_critical(pci_ext))
        {
            if ((pci = (PROXY_CERT_INFO_EXTENSION*) X509V3_EXT_d2i(pci_ext))
                == nullptr)
            {
                throw std::runtime_error("Can't convert DER encoded PROXYCERTINFO "
                                         "extension to internal form");
            }

            if ((policy = pci->proxyPolicy) == nullptr)
            {
                throw std::runtime_error("Can't get policy from PROXYCERTINFO "
                                         "extension");
            }

            if ((policy_lang = policy->policyLanguage) == nullptr)
            {
                throw std::runtime_error("Can't get policy language from "
                                         "PROXYCERTINFO extension");
            }

            policy_nid = OBJ_obj2nid(policy_lang);

            if (policy_nid == NID_id_ppl_inheritAll)
            {
                type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_IMPERSONATION_PROXY;
            }
            else if (policy_nid == NID_Independent)
            {
                type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_INDEPENDENT_PROXY;
            }
            else if (policy_nid == OBJ_txt2nid(LIMITED_PROXY_OID))
            {
                type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_LIMITED_PROXY;
            }
            else
            {
                type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_RESTRICTED_PROXY;
            }

            if (X509_get_ext_by_NID(cert,
                                    OBJ_txt2nid(PROXYCERTINFO_OLD_OID),
                                    index) != -1)
            {
                throw std::runtime_error("Found more than one PCI extension");
            }
        }

        if (GLOBUS_GSI_CERT_UTILS_IS_PROXY(type))
        {
            // It is some kind of proxy - now we check if the subject
            // matches the signer, by adding the proxy name entry CN
            // to the signer's subject.

            if (g_verbose_flag)
            {
                std::cout << "Subject is " << data->data;
            }

            if ((name = X509_NAME_dup(X509_get_issuer_name(cert))) == nullptr)
            {
                throw std::runtime_error("Error copying X509_NAME struct");
            }

            if ((new_ne = X509_NAME_ENTRY_create_by_NID(nullptr,
                                                        NID_commonName,
                                                        data->type,
                                                        data->data, -1)) == nullptr)
            {
                throw std::runtime_error("Error creating X509 name entry of: ");
            }

            if (!X509_NAME_add_entry(name, new_ne, X509_NAME_entry_count(name), 0))
            {
                X509_NAME_ENTRY_free(new_ne);
                new_ne = nullptr;

                throw std::runtime_error("Error adding name entry with "
                                         "value: , to subject");
            }

            if (new_ne)
            {
                X509_NAME_ENTRY_free(new_ne);
                new_ne = nullptr;
            }

            if (X509_NAME_cmp(name, subject))
            {
                // Reject this certificate.  Only the user may sign
                // the proxy.
                throw std::runtime_error("Issuer name + proxy CN entry "
                                         "does not equal subject name");
            }

            if (name)
            {
                X509_NAME_free(name);
                name = nullptr;
            }
        }
    }

exit:

    if (x509v3_bc)
    {
        BASIC_CONSTRAINTS_free(x509v3_bc);
    }

    if (new_ne)
    {
        X509_NAME_ENTRY_free(new_ne);
    }

    if (name)
    {
        X509_NAME_free(name);
    }

    if (pci)
    {
        PROXY_CERT_INFO_EXTENSION_free(pci);
    }

    return type;
}


utils::cert_type_t utils::proxy_determine_type(utils::proxy_handle_t const &handle,
                                               utils::cred_handle_t const  &issuer)
{
    cert_type_t requested_cert_type;
    cert_type_t issuer_cert_type = get_cert_type(issuer.cert);

    if ((handle.type & GLOBUS_GSI_CERT_UTILS_TYPE_FORMAT_MASK) == 0)
    {
        // Didn't specify format of the proxy.  Choose based on the
        // issuer's format, defaulting to RFC-compliant proxies if the
        // issuer is an EEC.
         switch (issuer_cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_FORMAT_MASK)
         {
         case GLOBUS_GSI_CERT_UTILS_TYPE_RFC:
         case GLOBUS_GSI_CERT_UTILS_TYPE_EEC:
             requested_cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_RFC;
             break;
         case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2:
             requested_cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2;
             break;
         case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3:
             requested_cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3;
             break;
         case GLOBUS_GSI_CERT_UTILS_TYPE_CA:
             throw std::runtime_error("CA Certificates can't sign proxies");
         default:
             throw std::runtime_error("Proxy type inconsistent with "
                                      "issuing certificate");
         }
    }
    else
    {
        // Verify that only one certificate format was selected.  Fail
        // if the format is EEC or CA, since we can't sign those as
        // proxies.
        switch (handle.type & GLOBUS_GSI_CERT_UTILS_TYPE_FORMAT_MASK)
        {
        case GLOBUS_GSI_CERT_UTILS_TYPE_RFC:
            requested_cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_RFC;
            break;
        case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2:
            requested_cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2;
            break;
        case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3:
            requested_cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3;
            break;
        case GLOBUS_GSI_CERT_UTILS_TYPE_EEC:
        case GLOBUS_GSI_CERT_UTILS_TYPE_CA:
            throw std::runtime_error("Cannot sign CA or EEC as proxy");
        default:
            throw std::runtime_error("Unable to determine proxy type");
        }
    }

    // Verify that the selected proxy type is compatible with the
    // issuer's proxy format.
    if (((requested_cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_FORMAT_MASK) !=
         (issuer_cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_FORMAT_MASK)) &&
         ! (issuer_cert_type & (GLOBUS_GSI_CERT_UTILS_TYPE_EEC |
                                GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY)))
    {
        throw std::runtime_error("Proxy type inconsistent with issuing certificate");
    }

    if ((handle.type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK) == 0 && handle.extensions)
    {
        requested_cert_type = (cert_type_t)(requested_cert_type | GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY);
    }
    else if ((handle.type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK) == 0)
    {
        // Didn't specify type of the proxy (Impersonation, Limited,
        // Independent, Restricted). Choose based on the issuer's
        // format, defaulting to impersonation proxies.
        switch (issuer_cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK)
        {
        case 0: // issuer not a proxy
        case GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY:
            requested_cert_type = (cert_type_t)(requested_cert_type | GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY);
            break;
        case GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY:
            requested_cert_type = (cert_type_t)(requested_cert_type | GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY);
            break;
        case GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY:
            requested_cert_type = (cert_type_t)(requested_cert_type | GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY);
            break;
        case GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY:
            requested_cert_type = (cert_type_t)(requested_cert_type | GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY);
            break;
        default:
            throw std::runtime_error("Unable to determine proxy type");
        }
    }
    else
    {
        // Verify that only one certificate type was selected.
        switch (handle.type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK)
        {
        case GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY:
            requested_cert_type = (cert_type_t)(requested_cert_type | GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY);
            break;
        case GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY:
            requested_cert_type = (cert_type_t)(requested_cert_type | GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY);
            break;
        case GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY:
            requested_cert_type = (cert_type_t)(requested_cert_type | GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY);
                break;
        case GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY:
            requested_cert_type = (cert_type_t)(requested_cert_type | GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY);
            break;
        default:
            throw std::runtime_error("Unable to determine proxy type");
        }
    }

    // Verify that the selected proxy type is compatible with the
    // issuer's type.  Impersonation or EEC can sign anything,
    // otherwise, the type must be the same.
    if (((requested_cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK) !=
         (issuer_cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK)) &&
         ! (issuer_cert_type & (GLOBUS_GSI_CERT_UTILS_TYPE_EEC |
                                GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY |
                                GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY)))
    {
        throw std::runtime_error("Proxy type inconsistent with issuing certificate");
    }

     // Finally, verify that we're not trying to use a restricted or
     // independent proxy with the legacy proxy format.
    if (GLOBUS_GSI_CERT_UTILS_IS_GSI_2_PROXY(requested_cert_type) &&
        (requested_cert_type & (GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY|GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY)))
    {
        throw std::runtime_error("Invalid legacy proxy type");
    }
exit:
    return requested_cert_type;
}

static void check_certificate(X509* certificate)
{
    EVP_PKEY *pkey = X509_get_pubkey(certificate);

    switch (X509_verify(certificate, pkey))
    {
    case 1:
        std::cout << "X509_verify(): valid signature (1)\n";
        break;
    case 0:
        std::cout << "X509_verify(): signature check failed (0)\n";
        break;
    case -1:
        std::cout << "X509_verify(): signature cannot be checked (-1)\n";
        break;
    default:
        std::cout << "X509_verify(): impossible error (?)\n";
        break;
    };

    EVP_PKEY_free(pkey);
}

static void check_certificate_validaty(X509* certificate)
{
    X509_STORE *store = X509_STORE_new();
    X509_STORE_add_cert(store, certificate);

    X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    X509_STORE_CTX_init(ctx, store, certificate, nullptr);

    if (X509_verify_cert(ctx) == 1)
    {
        std::cout << "Certificate verified.\n";
    }
    else
    {
        std::cout << "X509_verify_cert(): Error verifying certificate: "
                  << X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx))
                  << "\n";
    }

    X509_STORE_free(store);
    X509_STORE_CTX_free(ctx);
}

static void proxy_set_subject(X509              *new_pc,
                              X509              *issuer_cert,
                              std::string const &common_name)
{
    if (!new_pc or !issuer_cert)
    {
        throw std::runtime_error("null input to proxy_set_subject()");
    }

    if (g_verbose_flag)
    {
        std::cout << "Setting CN: " << common_name << ".\n";
    }

    X509_NAME       *pc_name       = nullptr;
    X509_NAME_ENTRY *pc_name_entry = nullptr;

    if ((pc_name = X509_NAME_dup(X509_get_subject_name(issuer_cert))) == nullptr)
    {
        throw std::runtime_error("Error copying subject name of proxy cert");
    }

    if ((pc_name_entry =
         X509_NAME_ENTRY_create_by_NID(&pc_name_entry,
                                       NID_commonName,
                                       V_ASN1_APP_CHOOSE,
                                       (unsigned char *) common_name.c_str(),
                                       -1)) == nullptr)
    {
        throw std::runtime_error("Error creating NAME ENTRY of type common name");
    }

    if (!X509_NAME_add_entry(pc_name,
                             pc_name_entry,
                             X509_NAME_entry_count(pc_name), 0) ||
        !X509_set_subject_name(new_pc, pc_name))
    {
        throw std::runtime_error("Error setting common name of subject "
                                 "in proxy cert");
    }

    if (pc_name_entry)
    {
        X509_NAME_ENTRY_free(pc_name_entry);
    }

    if (pc_name)
    {
        X509_NAME_free(pc_name);
    }
}

void utils::proxy_sign_key(utils::proxy_handle_t       &inquire_handle,
                           utils::cred_handle_t const  &issuer_credential,
                           EVP_PKEY                    *public_key,
                           X509                       **signed_cert)
{
    if (!public_key)
    {
        throw std::runtime_error("proxy_sign_key(): null public key");
    }

    if (!signed_cert)
    {
        throw std::runtime_error("proxy_sign_key(): null cert");
    }

    *signed_cert = nullptr;

    X509 *issuer_cert = X509_dup(issuer_credential.cert);
    if (!issuer_cert)
    {
        // TODO: clearer error message.
        throw std::runtime_error("proxy_sign_key(): could not get issuer cert");
    }

    if ((*signed_cert = X509_new()) == nullptr)
    {
        throw std::runtime_error("proxy_sign_key(): could not initialize new X509");
    }

    cert_type_t proxy_type = proxy_determine_type(inquire_handle, issuer_credential);

    if ((inquire_handle.type & GLOBUS_GSI_CERT_UTILS_TYPE_FORMAT_MASK) == 0)
    {
        // TODO.
        throw std::runtime_error("---TODO---");
    }

    int pci_NID = NID_undef;

    if (GLOBUS_GSI_CERT_UTILS_IS_GSI_3_PROXY(proxy_type))
    {
        pci_NID = OBJ_txt2nid(PROXYCERTINFO_OLD_OID);
    }
    else if (GLOBUS_GSI_CERT_UTILS_IS_RFC_PROXY(proxy_type))
    {
        pci_NID = OBJ_txt2nid(PROXYCERTINFO_OID);
    }

    ASN1_INTEGER *serial_number = nullptr;
    std::string   common_name;

    if (pci_NID != NID_undef)
    {
        const EVP_MD            *sha1       = EVP_sha1();
        const X509V3_EXT_METHOD *ext_method = X509V3_EXT_get_nid(pci_NID);

        unsigned char md[SHA_DIGEST_LENGTH] = {0};
        unsigned int len                    = 0;

        ASN1_digest((i2d_of_void *) i2d_PUBKEY, sha1, (char *) public_key, md, &len);
        long sub_hash = md[0] + (md[1] + (md[2] + (md[3] >> 1) * 256) * 256) * 256;

        if (!inquire_handle.common_name.empty())
        {
            common_name = inquire_handle.common_name;
        }
        else
        {
            common_name = std::to_string(sub_hash);
        }

        serial_number = ASN1_INTEGER_new();
        ASN1_INTEGER_set(serial_number, sub_hash);

        if (ext_method->i2d)
        {
            // TODO: maybe implement this?  Unsure we will in practice
            // need this though.
            throw std::runtime_error(std::string(__func__) + ":" +
                                     std::to_string(__LINE__) + " -- " +
                                     "unimplemented");
        }
        else
        {
            long           db         = 0;
            char language[80]         = {0};
            int            pathlen    = 0;
            unsigned char *policy     = nullptr;
            int            policy_len = 0;
            char          *tmp        = nullptr;

            OBJ_obj2txt(language, 80,
               inquire_handle.proxy_cert_info->proxyPolicy->policyLanguage, 1);

            if (g_verbose_flag)
            {
                std::cout << "Language: " << language << ".\n";
            }

            // TODO: use C++-ism.
            char lang_value[160] = {0};
            if (sprintf(lang_value, "language:%s", language) < 10)
            {
                throw std::runtime_error("proxy_sign_key(): sprintf() failed");
            }

            if (inquire_handle.proxy_cert_info->pcPathLengthConstraint)
            {
                // TODO: implement this.
                throw std::runtime_error("proxy_sign_key(): unhandled "
                                         "pcPathLengthConstraint");
            }

            if (inquire_handle.proxy_cert_info->proxyPolicy->policy)
            {
                // TODO: implement this.
                throw std::runtime_error("proxy_sign_key(): unhandled proxyPolicy");
            }


            X509V3_CTX         ctx;
            X509V3_CONF_METHOD method = { nullptr, nullptr, nullptr, nullptr };

            X509V3_set_ctx(&ctx, nullptr, nullptr, nullptr, nullptr, 0L);
            ctx.db_meth = &method;
            ctx.db      = &db;

            X509_EXTENSION *pci_ext =
                X509V3_EXT_conf_nid(NULL, &ctx, pci_NID, lang_value);

            // TODO: RFC proxy; remove GLOBUS_GSI_* stuff
            if (((proxy_type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK) != 0) &&
                ((proxy_type & GLOBUS_GSI_CERT_UTILS_TYPE_RFC) != 0))
            {
                X509_EXTENSION_set_critical(pci_ext, 1);
            }

            if (!X509_add_ext(*signed_cert, pci_ext, 0))
            {
                throw std::runtime_error("Couldn't add X.509 extension "
                                         "to new proxy cert");
            }

            if (pci_ext)
            {
                X509_EXTENSION_free(pci_ext);
            }
        }
    }
    else if (proxy_type == GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_LIMITED_PROXY)
    {
        throw std::runtime_error("proxy_sign_key(): unhandled proxy type "
                                 "GSI_2_LIMITED_PROXY");

        // common_name = LIMITED_PROXY_NAME;
        // serial_number = X509_get_serialNumber(issuer_cert);
    }
    else
    {
        throw std::runtime_error("proxy_sign_key(): unhandled proxy type " +
                                 proxy_type);

        // common_name = PROXY_NAME;
        // serial_number = X509_get_serialNumber(issuer_cert);
    }

    int position = -1;
    if ((position = X509_get_ext_by_NID(issuer_cert, NID_key_usage, -1)) > -1)
    {
        X509_EXTENSION *extension = nullptr;
        if (!(extension = X509_get_ext(issuer_cert, position)))
        {
            throw std::runtime_error("Couldn't get keyUsage extension "
                                     "from issuer cert");
        }

        ASN1_BIT_STRING *usage = nullptr;
        if (!(usage = static_cast<ASN1_BIT_STRING *>(
                  X509_get_ext_d2i(issuer_cert, NID_key_usage, nullptr, nullptr))))
        {
            throw std::runtime_error("Couldn't convert keyUsage struct from"
                                     " DER encoded form to internal form");
        }

        ASN1_BIT_STRING_set_bit(usage, 1, 0); // Non Repudiation.
        ASN1_BIT_STRING_set_bit(usage, 5, 0); // Certificate Sign.

        int ku_DER_length = i2d_ASN1_BIT_STRING(usage, nullptr);

        if (ku_DER_length < 0)
        {
            ASN1_BIT_STRING_free(usage);
            throw std::runtime_error("Couldn't convert keyUsage struct from "
                                     "internal to DER encoded form");
        }

        // TODO: use new?
        unsigned char *ku_DER = (unsigned char *) malloc(ku_DER_length);

        if (!ku_DER)
        {
            ASN1_BIT_STRING_free(usage);
            throw std::runtime_error("proxy_sign_key(): malloc() failed");
        }

        unsigned char *mod_ku_DER = ku_DER;
        ku_DER_length             = i2d_ASN1_BIT_STRING(usage, &mod_ku_DER);

        if (ku_DER_length < 0)
        {
            ASN1_BIT_STRING_free(usage);
            throw std::runtime_error("Couldn't convert keyUsage from internal "
                                     "to DER encoded form");
        }

        ASN1_BIT_STRING_free(usage);
        ASN1_OCTET_STRING *ku_DER_string = ASN1_OCTET_STRING_new();

        if (ku_DER_string == nullptr)
        {
            free(ku_DER);
            throw std::runtime_error("Couldn't creat new ASN.1 octet string "
                                     "for the DER encoding of the keyUsage");
        }

        ku_DER_string->data   = ku_DER;
        ku_DER_string->length = ku_DER_length;

        extension = X509_EXTENSION_create_by_NID(nullptr,
                                                 NID_key_usage,
                                                 1, ku_DER_string);
        ASN1_OCTET_STRING_free(ku_DER_string);

        if (extension == nullptr)
        {
            throw std::runtime_error("Couldn't create new keyUsage extension");
        }

        if (!X509_add_ext(*signed_cert, extension, 0))
        {
            X509_EXTENSION_free(extension);
            throw std::runtime_error("Couldn't add X.509 keyUsage extension "
                                     "to new proxy cert");
        }

        X509_EXTENSION_free(extension);
    }

    if ((position = X509_get_ext_by_NID(issuer_cert, NID_ext_key_usage, -1)) > -1)
    {
        X509_EXTENSION *extension = nullptr;

        if (!(extension = X509_get_ext(issuer_cert, position)))
        {
            throw std::runtime_error("Couldn't get extendedKeyUsage extension "
                                     "form issuer cert");
        }

        if (!(extension = X509_EXTENSION_dup(extension)))
        {
            throw std::runtime_error("Couldn't copy extendedKeyUsage extension");
        }

        if (!X509_add_ext(*signed_cert, extension, 0))
        {
            throw std::runtime_error("Couldn't add X.509 extendedKeyUsage "
                                     "extension to new proxy cert");
        }

        X509_EXTENSION_free(extension);
    }

    //  Add any extensions added to the handle
    if (inquire_handle.extensions != nullptr)
    {
        auto max = sk_X509_EXTENSION_num(inquire_handle.extensions);
        for (auto index = 0;  index < max; ++index)
        {
            X509_EXTENSION *ext =
                sk_X509_EXTENSION_value(inquire_handle.extensions,
                                        index);

            if (!X509_add_ext(*signed_cert, ext, -1)) // at end.
            {
                throw std::runtime_error("Couldn't add X.509 extension to "
                                         "new proxy cert");
            }
        }
    }

    proxy_set_subject(*signed_cert, issuer_cert, common_name);

    if (!X509_set_issuer_name(*signed_cert, X509_get_subject_name(issuer_cert)))
    {
        throw std::runtime_error("Error setting issuer's subject of X509");
    }

    if (!X509_set_version(*signed_cert, 2))
    {
        throw std::runtime_error("Error setting version number of X509");
    }

    if (!X509_set_serialNumber(*signed_cert, serial_number))
    {
        throw std::runtime_error("Error setting serial number of X509");
    }

    proxy_set_validity(*signed_cert, issuer_cert,
                       inquire_handle.attrs.clock_skew,
                       inquire_handle.time_valid);

    if (!X509_set_pubkey(*signed_cert, public_key))
    {
        throw std::runtime_error("Couldn't set pubkey of X.509 cert");
    }

    // Sign the new certificate: equivalent of
    // globus_gsi_cred_get_key()

    if (!issuer_credential.key)
    {
        throw std::runtime_error("null key in issuer credential");
    }

    BIO_ptr pk_mem_bio(BIO_new(BIO_s_mem()), BIO_free);

    if (i2d_PrivateKey_bio(pk_mem_bio.get(), issuer_credential.key) <= 0)
    {
        throw std::runtime_error("Couldn't convert private key to DER encoded form");
    }

    EVP_PKEY *issuer_priv_key = nullptr;

    if (!d2i_PrivateKey_bio(pk_mem_bio.get(), &issuer_priv_key))
    {
        throw std::runtime_error("Could not get issuer key");
    }

    // TODO: check requested signing algorithm.
    const EVP_MD *issuer_digest = EVP_get_digestbynid(
        X509_get_signature_nid(issuer_cert));

    if (!issuer_digest)
    {
        throw std::runtime_error("Error getting issuer digest");
    }

    if (!X509_sign(*signed_cert, issuer_priv_key, issuer_digest))
    {
        throw std::runtime_error("Error signing proxy cert");
    }

    if (g_verbose_flag)
    {
        std::cout << "Signed certificate:\n";
        X509_print_fp(stdout, *signed_cert);
        PEM_write_X509(stdout, *signed_cert);
    }

    X509_free(issuer_cert);
    EVP_PKEY_free(issuer_priv_key);

    if (pci_NID != NID_undef)
    {
        // if (pci_DER_string)
        // {
        //     ASN1_OCTET_STRING_free(pci_DER_string);
        // }
        // else if (pci_DER)
        // {
        //     free(pci_DER);
        // }

        if (serial_number)
        {
            ASN1_INTEGER_free(serial_number);
        }
    }

    if (g_verbose_flag)
    {
        check_certificate(*signed_cert);
        check_certificate_validaty(*signed_cert);
    }
}

// This function signs the public part of a proxy credential request,
// i.e. the unsigned certificate, previously read by
// proxy_inquire_req() using the supplied issuer_credential.
//
// This operation will add a ProxyCertInfo extension to the proxy
// certificate if values contained in the extension are specified in
// the handle.  The resulting signed certificate is written to the
// output_bio.
static void proxy_sign_req(utils::proxy_handle_t      &inquire_handle,
                           utils::cred_handle_t const &cred_handle,
                           BIO                        *output_bio)
{
    // TODO: globus_gsi_proxy_sign_req() for reference.
    // gsi/proxy/proxy_core/source/library/globus_gsi_proxy.c +1059

    if (!output_bio)
    {
        // TODO: better error message.
        throw std::runtime_error("proxy_sign_req(): null BIO input");
    }

    EVP_PKEY *req_pubkey = X509_REQ_get_pubkey(inquire_handle.req);
    if (!req_pubkey)
    {
        throw std::runtime_error("X509_REQ_get_pubkey() failed");
    }

    if (!X509_REQ_verify(inquire_handle.req, req_pubkey))
    {
        throw std::runtime_error("X509_REQ_verify() failed");
    }

    X509 *new_pc = nullptr;
    proxy_sign_key(inquire_handle, cred_handle, req_pubkey, &new_pc);

    // Write out the X509 certificate in DER encoded format to the
    // BIO.
    if (!i2d_X509_bio(output_bio, new_pc))
    {
        throw std::runtime_error("Could not convert X.509 proxy cert from "
                                 "DER encoded to internal form");
    }

    if (new_pc)
    {
        X509_free(new_pc);
    }

    if (req_pubkey)
    {
        EVP_PKEY_free(req_pubkey);
    }
}

static X509* proxy_get_certificate(BIO *bio)
{
    if (!bio)
    {
        throw std::runtime_error("proxy_get_certificate(): null BIO input");
    }

    // Get the signed proxy cert from the BIO.
    X509_ptr signed_cert(d2i_X509_bio(bio, nullptr), X509_free);

    if(!signed_cert.get())
    {
        throw std::runtime_error("proxy_get_certificate(): Couldn't convert "
                                 "X.509 proxy cert from DER to internal form");
    }

    auto *dup_cert = X509_dup(signed_cert.get());
    if (!dup_cert)
    {
        throw std::runtime_error("Could not make copy of X509 cert");
    }

    return dup_cert;
}

static EVP_PKEY* proxy_get_private_key(EVP_PKEY *key)
{
    if (!key)
    {
        throw std::runtime_error("Null key input to proxy_get_private_key()");
    }

    BIO_ptr inout_bio(BIO_new(BIO_s_mem()), BIO_free);

    if (i2d_PrivateKey_bio(inout_bio.get(), key) <= 0)
    {
        throw std::runtime_error("Couldn't get length of DER encoding "
                                 "for private key");
    }

    auto *dup_key = d2i_PrivateKey_bio(inout_bio.get(), nullptr);
    if (!dup_key)
    {
        throw std::runtime_error("Error getting private key from proxy handle");
    }

    return dup_key;
}

static STACK_OF(X509*) proxy_get_cert_chain(BIO *bio)
{
    auto *cert_chain = sk_X509_new_null();

    if (!cert_chain)
    {
        throw std::runtime_error("Couldn't create new stack for cert chains");
    }

    while (!BIO_eof(bio))
    {
        X509 *tmp = nullptr;

        if ((tmp = d2i_X509_bio(bio, &tmp)) == nullptr)
        {
            throw std::runtime_error("Can't read DER encoded X.509 cert from BIO");
        }

        sk_X509_push(cert_chain, tmp);
    }

    return cert_chain;
}

static utils::cred_handle_t proxy_assemble_cred(utils::proxy_handle_t &proxy_handle,
                                                BIO                   *input_bio)
{
    if (!input_bio)
    {
        throw std::runtime_error("proxy_assemble_cred(): null input");
    }

    utils::cred_handle_t cred_handle;
    cred_handle.set_cert(proxy_get_certificate(input_bio));

    // TODO: reset validity.
    //
    // See globus_i_gsi_cred_goodtill() in
    // gsi/credential/source/library/globus_gsi_cred_handle.c

    // Copy key to credential handle.
    if (proxy_handle.proxy_key)
    {
        cred_handle.set_key(proxy_get_private_key(proxy_handle.proxy_key));
    }

    auto *cert_chain = proxy_get_cert_chain(input_bio);
    cred_handle.set_cert_chain(cert_chain);
    sk_X509_pop_free(cert_chain, X509_free);

    return cred_handle;
}

static void get_cert_chain(utils::cred_handle_t &issuer,
                           STACK_OF(X509) **issuer_cert_chain)
{
    // See globus_gsi_cred_get_cert_chain() for reference.
    // gsi/credential/source/library/globus_gsi_cred_handle.c +924

    if (!issuer_cert_chain)
    {
        throw std::runtime_error("Failure reading cert chain: null input");
    }

    if (!issuer.cert_chain)
    {
        *issuer_cert_chain = nullptr;
        return;
    }

    *issuer_cert_chain = sk_X509_new_null();

    for (int i = 0; i < sk_X509_num(issuer.cert_chain); ++i)
    {
        X509 *tmp_cert = X509_dup(sk_X509_value(issuer.cert_chain, i));
        if (tmp_cert == nullptr)
        {
            throw std::runtime_error("Error copying cert from cred's cert chain");
        }

        sk_X509_push(*issuer_cert_chain, tmp_cert);
    }
}

// Update proxy handle's "req" field.
void utils::proxy_create_signing_request(utils::proxy_handle_t &p_handle,
                                         const EVP_MD          *signing_algorithm)
{
    if (!signing_algorithm)
    {
        throw std::runtime_error("Signing algorithm is not known.");
    }

    p_handle.attrs.signing_algorithm = signing_algorithm;
    proxy_create_req(p_handle);
}

// The return value will contain the signed certifcate.
utils::cred_handle_t utils::proxy_sign_signing_request(utils::proxy_handle_t &p_handle,
                                                       utils::cred_handle_t  &c_handle)
{
    X509 *issuer_cert = X509_dup(c_handle.cert);
    if (!issuer_cert)
    {
        throw std::runtime_error("X509_dup() failed");
    }

    BIO_ptr rw_mem_bio(BIO_new(BIO_s_mem()), BIO_free);

    // Write the request to the BIO.
    if (!i2d_X509_REQ_bio(rw_mem_bio.get(), p_handle.req))
    {
        throw std::runtime_error("Couldn't convert X509 request from internal "
                                 "to DER encoded form");
    }

    utils::proxy_handle_t inquire_handle;
    inquire_handle.attrs = p_handle.attrs;

    proxy_inquire_req(inquire_handle, rw_mem_bio.get());

    inquire_handle.type        = proxy_determine_type(p_handle, c_handle);
    inquire_handle.common_name = p_handle.common_name;
    inquire_handle.time_valid  = p_handle.time_valid;
    inquire_handle.extensions  = p_handle.extensions;

    proxy_sign_req(inquire_handle, c_handle, rw_mem_bio.get());

    if (!i2d_X509_bio(rw_mem_bio.get(), issuer_cert))
    {
        throw std::runtime_error("Couldn't write issuer cert to mem bio");
    }

    X509_free(issuer_cert);
    issuer_cert = nullptr;

    STACK_OF(X509) *issuer_cert_chain = nullptr;
    get_cert_chain(c_handle, &issuer_cert_chain);

    for (int i = 0; i < sk_X509_num(issuer_cert_chain); ++i)
    {
        X509 *chain_cert = sk_X509_value(issuer_cert_chain, i);

        if (!i2d_X509_bio(rw_mem_bio.get(), chain_cert))
        {
            throw std::runtime_error("Couldn't write cert from "
                                     "cert chain to mem bio");
        }
    }

    sk_X509_pop_free(issuer_cert_chain, X509_free);
    issuer_cert_chain = nullptr;

    // TODO: See
    // gsi/proxy/proxy_core/source/library/globus_gsi_proxy_handle.c
    // +205 for reference of globus_gsi_proxy_handle_destroy().

    // globus_gsi_proxy_handle_destroy(inquire_handle);
    // inquire_handle = nullptr;

    return proxy_assemble_cred(p_handle, rw_mem_bio.get());
}

static const EVP_MD * proxy_get_signing_algorithm(utils::proxy_handle_t      &p_handle,
                                                  utils::cred_handle_t const &c_handle)
{
    if (!p_handle.attrs.signing_algorithm)
    {
        int           nid           = X509_get_signature_nid(c_handle.cert);
        const EVP_MD *issuer_digest = EVP_get_digestbynid(nid);

        if (!issuer_digest)
        {
            throw std::runtime_error("Certifcates's signature algorithm is "
                                     "not supported by OpenSSL");
        }

        p_handle.attrs.signing_algorithm = issuer_digest;
    }

    return p_handle.attrs.signing_algorithm;
}

// Create signed proxy certificate.
utils::cred_handle_t utils::proxy_create_signed(utils::proxy_handle_t &p_handle,
                                                utils::cred_handle_t  &c_handle)
{
    const EVP_MD *signing_algorithm = proxy_get_signing_algorithm(p_handle, c_handle);
    proxy_create_signing_request(p_handle, signing_algorithm);

    return proxy_sign_signing_request(p_handle, c_handle);
}

void utils::proxy_write_outfile(cred_handle_t const &cred_handle,
                                std::string const   &out_file_name)
{
    // chmod(proxy_filename, S_IRWXU);

    FILE *temp_fp = fopen(out_file_name.c_str(), "w");

    if (!temp_fp)
    {
        throw std::runtime_error("Error opening " + out_file_name);
    }

    BIO_ptr proxy_bio(BIO_new_fp(temp_fp, BIO_CLOSE), BIO_free);

    if (!proxy_bio.get())
    {
        fclose(temp_fp);
        throw std::runtime_error("Error creating BIO for " + out_file_name);
    }

    if (!PEM_write_bio_X509(proxy_bio.get(), cred_handle.cert))
    {
        throw std::runtime_error("proxy_write_outfile(): Can't write "
                                 "PEM formatted X509 cert to BIO stream");
    }

    if (g_verbose_flag)
    {
        std::cout << "Wrote cert to " << out_file_name << ".\n";
    }

    if (cred_handle.key)
    {
        if (!PEM_write_bio_PrivateKey(proxy_bio.get(), cred_handle.key,
                                      NULL, NULL, 0, NULL, NULL))
        {
            throw std::runtime_error("proxy_write_outfile(): Can't write "
                                     "PEM formatted private key to BIO stream");

        }

        if (g_verbose_flag)
        {
            std::cout << "Wrote key to " << out_file_name << ".\n";
        }
    }
    else if (g_verbose_flag)
    {
        std::cout << "Private key not present; omitting from "
                  << out_file_name << ".\n";
    }

    for (int i = 0; i < sk_X509_num(cred_handle.cert_chain); ++i)
    {
        if (!PEM_write_bio_X509(proxy_bio.get(),
                                sk_X509_value(cred_handle.cert_chain, i)))
        {
            throw std::runtime_error("proxy_write_outfile(): Can't write "
                                     "PEM formatted X509 cert in cert chain "
                                     "to BIO stream");
        }
        else
        {
            if (g_verbose_flag)
            {
                std::cout << "Wrote cert chain item #" << i << " to "
                          << out_file_name << "\n";
            }
        }
    }

    if (g_verbose_flag)
    {
        std::cout << "Done.\n";
    }
    else
    {
        std::cout << "Wrote proxy certificate to " << out_file_name << ".\n";
    }
}

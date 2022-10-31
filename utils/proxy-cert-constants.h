#ifndef BDE_PROXY_CERT_CONSTANTS_H
#define BDE_PROXY_CERT_CONSTANTS_H

namespace utils
{

    static const char * const PROXYCERTINFO_OLD_OID   = "1.3.6.1.4.1.3536.1.222";
    static const char * const PROXYCERTINFO_OID       = "1.3.6.1.5.5.7.1.14";
    static const char * const PROXYCERTINFO_SN        = "PROXYCERTINFO";
    static const char * const PROXYCERTINFO_LN        = "Proxy Certificate Info Extension";
    static const char * const PROXYCERTINFO_OLD_SN    = "OLD_PROXYCERTINFO";
    static const char * const PROXYCERTINFO_OLD_LN    = "Proxy Certificate Info Extension (old OID)";

    static const char * const ANY_LANGUAGE_OID        = "1.3.6.1.5.5.7.21.0";
    static const char * const ANY_LANGUAGE_SN         = "ANY_LANGUAGE";
    static const char * const ANY_LANGUAGE_LN         = "Any Language";

    static const char * const IMPERSONATION_PROXY_OID = "1.3.6.1.5.5.7.21.1";
    static const char * const IMPERSONATION_PROXY_SN  = "IMPERSONATION_PROXY";
    static const char * const IMPERSONATION_PROXY_LN  = "GSI impersonation proxy";

    static const char * const INDEPENDENT_PROXY_OID   = "1.3.6.1.5.5.7.21.2";
    static const char * const INDEPENDENT_PROXY_SN    = "INDEPENDENT_PROXY";
    static const char * const INDEPENDENT_PROXY_LN    = "GSI independent proxy";

    static const char * const LIMITED_PROXY_OID       = "1.3.6.1.4.1.3536.1.1.1.9";
    static const char * const LIMITED_PROXY_SN        = "LIMITED_PROXY";
    static const char * const LIMITED_PROXY_LN        = "GSI limited proxy";

    enum cert_type_t
    {
        // Default proxy type.
        GLOBUS_GSI_CERT_UTILS_TYPE_DEFAULT  = 0,

        // A end entity certificate.
        GLOBUS_GSI_CERT_UTILS_TYPE_EEC      = (1 << 0),

        // A CA certificate.
        GLOBUS_GSI_CERT_UTILS_TYPE_CA       = (1 << 1),

        // Legacy Proxy Format.
        GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2    = (1 << 2),

        // X.509 Proxy Certificate Profile (draft) Proxy Format.
        GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3    = (1 << 3),

        // X.509 Proxy Certificate Profile Compliant Proxy Format.
        GLOBUS_GSI_CERT_UTILS_TYPE_RFC      = (1 << 4),

        // Proxy certificate formats mask.
        GLOBUS_GSI_CERT_UTILS_TYPE_FORMAT_MASK =
        (GLOBUS_GSI_CERT_UTILS_TYPE_EEC |
         GLOBUS_GSI_CERT_UTILS_TYPE_CA |
         GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2 |
         GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3 |
         GLOBUS_GSI_CERT_UTILS_TYPE_RFC),

        // Impersonation proxy type.
        GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY
        = (1 << 5),

        // Limited proxy type.
        GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY
        = (1 << 6),

        // Restricted proxy type.
        GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY
        = (1 << 7),

        // Independent proxy type.
        GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY
        = (1 << 8),

        // Proxy types mask.
        GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK =
        (GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY |
         GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY |
         GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY |
         GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY),

        // A X.509 Proxy Certificate Profile (pre-RFC) compliant
        // impersonation proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_IMPERSONATION_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3 |
         GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY),

        // A X.509 Proxy Certificate Profile (pre-RFC) compliant
        // independent proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_INDEPENDENT_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3 |
         GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY),

        // A X.509 Proxy Certificate Profile (pre-RFC) compliant
        // limited proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_LIMITED_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3 |
         GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY),

        // A X.509 Proxy Certificate Profile (pre-RFC) compliant
        // restricted proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_RESTRICTED_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3 |
         GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY),

        // A legacy Globus impersonation proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2 |
         GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY),

        // A legacy Globus limited impersonation proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_LIMITED_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2 |
         GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY),

        // A X.509 Proxy Certificate Profile RFC compliant
        // impersonation proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_RFC_IMPERSONATION_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_RFC |
         GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY),

        // A X.509 Proxy Certificate Profile RFC compliant independent
        // proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_RFC_INDEPENDENT_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_RFC |
         GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY),

        // A X.509 Proxy Certificate Profile RFC compliant limited
        // proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_RFC_LIMITED_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_RFC |
         GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY),

        // A X.509 Proxy Certificate Profile RFC compliant restricted
        // proxy.
        GLOBUS_GSI_CERT_UTILS_TYPE_RFC_RESTRICTED_PROXY =
        (GLOBUS_GSI_CERT_UTILS_TYPE_RFC |
         GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY)
    };

#define GLOBUS_GSI_CERT_UTILS_IS_PROXY(cert_type)               \
    ((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK) != 0)

#define GLOBUS_GSI_CERT_UTILS_IS_RFC_PROXY(cert_type)                   \
    (((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK) != 0) &&      \
     ((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_RFC) != 0))

#define GLOBUS_GSI_CERT_UTILS_IS_GSI_3_PROXY(cert_type)                 \
    (((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK) != 0) &&      \
     ((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3) != 0))

#define GLOBUS_GSI_CERT_UTILS_IS_GSI_2_PROXY(cert_type)                 \
    (((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_PROXY_MASK) != 0) &&      \
    ((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2) != 0))

#define GLOBUS_GSI_CERT_UTILS_IS_INDEPENDENT_PROXY(cert_type)           \
    ((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_INDEPENDENT_PROXY) != 0)

#define GLOBUS_GSI_CERT_UTILS_IS_RESTRICTED_PROXY(cert_type)            \
    ((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_RESTRICTED_PROXY) != 0)

#define GLOBUS_GSI_CERT_UTILS_IS_LIMITED_PROXY(cert_type)               \
    ((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_LIMITED_PROXY) != 0)

#define GLOBUS_GSI_CERT_UTILS_IS_IMPERSONATION_PROXY(cert_type)         \
    ((cert_type & GLOBUS_GSI_CERT_UTILS_TYPE_IMPERSONATION_PROXY) != 0)

};

#endif // BDE_PROXY_CERT_CONSTANTS_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

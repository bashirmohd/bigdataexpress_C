#include <iostream>

#include <getopt.h>
#include <unistd.h>
#include <libgen.h>

#include "utils/proxy-cert-impl.h"

static bool g_verbose_flag = true;
static bool g_verify_flag  = false;

static void show_usage(const char * const prog)
{
    // TODO: Add expanded usage help.
    std::cerr << "Usage: " << prog << " [args]\n\n"
              << "where [args] may be: \n\n"
              << "  -h, -?, --help, --usage: \t Show usage\n"
              << "  -V, --version:           \t Print version\n"
              << "  -v, --verbose:           \t Print more output\n"
              << "  -k, --verify:            \t Verify\n"
              << "  -t, --valid [ARG]:       \t Validity\n"
              << "  -m, --hours [ARG]:       \t Validity\n"
              << "  -c, --cert [ARG]:        \t Certificate file\n"
              << "  -e, --key [ARG]:         \t Key file\n"
              << "  -d, --certdir [ARG]:     \t Key/cert directory\n"
              << "  -o, --out [ARG]:         \t Output filename\n"
              << "  -b, --bits [ARG]:        \t Bits\n\n";
}

static void show_version(const char * const prog)
{
    auto version = "0.1";
    std::cerr << prog << " version " << version << std::endl;
}

int main(int argc, char **argv)
{
    auto const DEFAULT_USER_CERT = std::string("usercert.pem");
    auto const DEFAULT_USER_KEY  = std::string("userkey.pem");
    auto const DEFAULT_CERT_DIR  = std::string(getenv("HOME")) + "/" + ".globus";

    std::string user_cert{DEFAULT_CERT_DIR + "/" + DEFAULT_USER_CERT};
    std::string user_key{DEFAULT_CERT_DIR + "/" + DEFAULT_USER_KEY};
    std::string cert_dir{DEFAULT_CERT_DIR};

    std::string out_dir = "/tmp";
    int         bits    = 1024;

    while (true)
    {
        static struct option long_options[] = {
            { "help",    no_argument,       0, 'h' },
            { "usage",   no_argument,       0, '?' },
            { "version", no_argument,       0, 'V' },
            { "verbose", no_argument,       0, 'v' },
            { "verify",  no_argument,       0, 'k' },
            { "valid",   required_argument, 0, 't' },
            { "hours",   required_argument, 0, 'm' }, /* TODO */
            { "cert",    required_argument, 0, 'c' },
            { "key",     required_argument, 0, 'e' },
            { "certdir", required_argument, 0, 'd' },
            { "out",     required_argument, 0, 'o' },
            { "bits",    required_argument, 0, 'b' },
            { 0, 0, 0, 0 }
        };

        int optind = 0;
        int c      = getopt_long(argc, argv, "h?Vvkt:m:c:e:d:o:b:",
                                 long_options, &optind);

        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
        case '?':
            show_usage(argv[0]);
            exit(0);
        case 'V':
            show_version(basename(argv[0]));
            exit(0);
        case 'v':
            g_verbose_flag = true;
            utils::proxy_make_verbose(g_verbose_flag);
            break;
        case 'k':
            g_verify_flag = true;
            break;
        case 'c':
            user_cert = optarg;
            break;
        case 'e':
            user_key = optarg;
            break;
        case 'd':
            cert_dir = optarg;
            break;
        case 'o':
            out_dir = optarg;
            break;
        case 'b':
            bits = std::strtol(optarg, 0, 10);
            break;
        default:
            abort();
        }
    }

    if (g_verbose_flag)
    {
        std::cerr << "user_cert : " << user_cert << std::endl;
        std::cerr << "user_key  : " << user_key  << std::endl;
        std::cerr << "cert_dir  : " << cert_dir  << std::endl;
        std::cerr << "bits      : " << bits      << std::endl;
    }

    // The bogus failures when running "ctest" or "make test" and
    // cert/key are not available are kind of annoying; moreover this
    // is test better run directly from the commandline when we need
    // to check things.
    if (access(user_cert.c_str(), F_OK) != 0)
    {
        std::cerr << "Can't open user cert\n";
        return 0;
    }

    if (access(user_key.c_str(), F_OK) != 0)
    {
        std::cerr << "Can't open user key\n";
        return 0;
    }

    utils::proxy_handle_t p_handle;
    utils::cred_handle_t  c_handle = utils::read_cert_and_key(user_cert, user_key);

    // sign the proxy certificate.
    utils::cred_handle_t pc_handle = utils::proxy_create_signed(p_handle, c_handle);

    // TODO: read PKCS12 files.
    // TODO: verify.

    utils::proxy_write_outfile(pc_handle, "./outfile.pem");

    return 0;
}

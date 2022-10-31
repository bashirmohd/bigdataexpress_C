#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <sys/file.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <memory>

#include "utils/utils.h"
#include "bde_version.h"
#include "conf.h"
#include "main.h"

#include <argp.h>

// ----------------------------------------------------------------------

// banner() and git_version() is useful when printing program version.

static const std::string banner(void)
{
    return std::string("\n") +
        R"( ___ ___  ___     _                _    )" + "\n" +
        R"(| _ )   \| __|   /_\  __ _ ___ _ _| |_  )" + "\n" +
        R"(| _ \ |) | _|   / _ \/ _` / -_) ' \  _| )" + "\n" +
        R"(|___/___/|___| /_/ \_\__, \___|_||_\__| )" + "\n" +
        R"(                     |___/              )" + "\n" +
        R"(    https://bigdataexpress.fnal.gov     )" + "\n\n";
}

static const std::string git_version(void)
{
    return std::string("") +

#if defined(BDE_VERSION)
        "Version             : " + BDE_VERSION + "\n" +
#endif
#if defined(BDE_GIT_BRANCH)
        "Git branch          : " + BDE_GIT_BRANCH + "\n" +
#endif
#if defined(BDE_GIT_COMMIT_HASH)
        "Last commit hash    : " + BDE_GIT_COMMIT_HASH + "\n" +
#endif
#if defined(BDE_GIT_COMMIT_AUTHOR)
        "Last commit author  : " + BDE_GIT_COMMIT_AUTHOR + "\n" +
#endif
#if defined(BDE_GIT_COMMIT_SUBJECT)
        "Last commit subject : " + BDE_GIT_COMMIT_SUBJECT + "\n" +
#endif
#if defined(BDE_GIT_COMMIT_DATE)
        "Last commit date    : " + BDE_GIT_COMMIT_DATE + "\n" +
#endif
        "Build date          : " + __DATE__ + " " + __TIME__ + "\n";
}

// ----------------------------------------------------------------------

// Command line parsing using Argp.  See GNU libc manual:
// https://www.gnu.org/software/libc/manual/html_node/Argp.html

const char *argp_program_bug_address = "<sajith@fnal.gov>";

static struct argp_option options[] =
{
    { "verbose",   'v', 0,      0, "Produce verbose output"           },
    { "version",   'V', 0,      0, "Print program version"            },
    { "daemonize", 'd', 0,      0, "Run in background after startup"  },
    { "config",    'c', "FILE", 0, "Path to configuration file"       },
    { "log",       'l', "FILE", 0, "Log to file (default: stdout)"    },
    { "lock",      'L', "FILE", 0, "Path to lock FILE "               },
    { 0 }
};

struct arguments
{
    char        *args[2];
    bool         verbose;
    bool         daemonize;
    std::string  config_file;
    std::string  log_file;
    std::string  lock_file;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    auto *args = static_cast<struct arguments *>(state->input);

    switch (key)
    {
    case 'v':
        args->verbose = true;
        break;

    case 'V':
        std::cout << banner()
                  << git_version() << "\n";
        exit(0);

    case 'c':
        args->config_file = arg;
        break;

    case 'd':
        args->daemonize = true;
        break;

    case 'l':
        args->log_file = arg;
        break;

    case 'L':
        args->lock_file = arg;
        break;

    case ARGP_KEY_ARG:
        args->args[state->arg_num] = arg;
        break;

    case ARGP_KEY_END:
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

// ----------------------------------------------------------------------

std::unique_ptr<Main> main_module;

int main(int argc, char **argv)
{
    static const char  *desc     = "Big Data Express Agent program.\n"
        "\v"
        "By default, the agent program will attempt to read its "
        "configuration from a file named \"agent.json\" in "
        "current directory.  By default, it will also try to acquire "
        "a lock on the file \"/var/run/lock/bdeagent.lock\".\n"
        "\v"
        "Please see the project wiki for documentation.";

    static const char  *args_doc = nullptr;
    static struct argp  argp     = { options, parse_opt, args_doc, desc };

    struct arguments args;

    // some defaults.
    args.verbose     = true;
    args.daemonize   = false;
    args.config_file = "agent.json";

    if (argp_parse(&argp, argc, argv, 0, 0, &args))
    {
        argp_help(&argp, stdout, ARGP_HELP_SEE, argv[0]);
        exit(EXIT_FAILURE);
    }

    if (args.config_file.empty())
    {
        args.config_file = args.daemonize ? "/etc/bde/agent.json" : "agent.json";
    }

    if (args.log_file.empty() and args.daemonize)
    {
        args.log_file = "/tmp/bdeagent.log";
    }

    if (access(args.config_file.c_str(), R_OK) == -1)
    {
        std::cerr << "Error: can't open configuration file "
                  << "\'" << args.config_file << "\' "
                  << "(" << strerror(errno) << ")\n"
                  << "Quitting.\n";
        exit(EXIT_FAILURE);
    }

    std::cout << banner()
              << "-- Running " << utils::version() << ".\n"
              << "-- Starting BDE Agent as a "
              << (args.daemonize ? "background" : "foreground") << " process.\n"
              << "-- Reading configuration from " << args.config_file << ".\n"
              << "-- Logging to "
              << (args.log_file.empty() ? "console" : args.log_file) << ".\n\n";

    if (args.lock_file.empty())
    {
        args.lock_file = "/var/run/lock/bdeagent.lock";
    }

    int lockfd = open(args.lock_file.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);

    if (lockfd < 0)
    {
        std::cerr << "Can't create lock file "
                  <<"\"" << args.lock_file << "\": "
                  << strerror(errno) << ".\n";
        exit(EXIT_FAILURE);
    }

    if (flock(lockfd, LOCK_EX | LOCK_NB) != 0)
    {
        std::cerr << "Can't get a lock on "
                  << "\"" << args.lock_file << "\": "
                  << strerror(errno) << ".\n\n"
                  << "Is another instance of bdeagent running?\n\n";
        exit(EXIT_FAILURE);
    }

    std::ofstream logstream;

    if (not args.log_file.empty())
    {
        logstream.open(args.log_file, std::ofstream::app);

        if (logstream.fail())
        {
            std::cerr << "Error: can't open log file "
                      <<"\'" << args.log_file << "\'.\n"
                      << "Quitting.\n";
            exit(EXIT_FAILURE);
        }

        utils::service::set_console(logstream);
    }

    if (args.daemonize)
    {
        std::cout << "-- Daemonizing...\n\n";
        if (daemon(0, 0) != 0)
        {
            std::cerr << "Error: daemon() failed: "
                      << strerror(errno) << "\n"
                      << "Quitting.\n";
            exit(EXIT_FAILURE);
        }
    }

    // Ignore SIGALRM, apparently raised by libcurl resolver.
    signal(SIGALRM, SIG_IGN);

    // random seed
    std::srand(time(NULL));

    std::string error;

    try
    {
        Conf conf(args.config_file.c_str());
        main_module.reset(new Main(conf));
        main_module->run(args.daemonize);
    }
    catch (std::exception const & ex)
    {
        error = "[BDE Agent] Error: " +
            std::string(ex.what()) + ". Exiting.";
    }
    catch (...)
    {
        error = "[BDE Agent] Error: unknown error. Exiting.";
    }

    if (not error.empty())
    {
        utils::slog() << error;

        if (not args.daemonize)
        {
            // Also log error on the console.
            std::cerr << error << "\n\n";
        }

        logstream.close();
        exit(EXIT_FAILURE);
    }

    logstream.close();
}

// ----------------------------------------------------------------------

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:


#include <cstdio>
#include <iostream>
#include <stdlib.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <memory>
#include <getopt.h>

#include <signal.h>
#include <unistd.h>

#include "main.h"
#include "utils/utils.h"

using namespace utils;

std::unique_ptr<Main> main_module;

void print_usage(const char * const prog_name)
{
    std::cout << "Usage: \n"
              << "  " << prog_name << " "
              << "[-i|-d|-v|-h] [-c config file] [-l log file]\n"
              << "  -i to run in foreground.\n"
              << "  -d to run as daemon in background.\n"
              << "  -v print version and exit.\n"
              << "  -h print this message and exit.\n";
}

int main(int argc, char ** argv)
{
    bool daemon_mode = false;

    std::string config_file;
    std::string log_file;
    std::string cmd_path;

    int c;

    while ((c = getopt (argc, argv, "idc:l:v?h")) != -1)
    {
        switch (c)
           {
           case 'i':
               daemon_mode = false;
               break;
           case 'd':
               daemon_mode = true;
               break;
           case 'c':
               config_file = optarg;
               break;
           case 'l':
               log_file    = optarg;
               break;
           case 'v':
               std::cout << utils::version() << ".\n";
               exit(0);
           case '?':
           case 'h':
               print_usage(argv[0]);
               exit(1);
           default:
               break;
           }
    }

    if (config_file.empty())
    {
        config_file = daemon_mode ? "/etc/bdeserver.json" : "bdeserver.json";
    }

    if (log_file.empty() and daemon_mode)
    {
        log_file = "/var/lib/bdeserver/log/bdeagent.log";
    }

#if 0
    if (access(config_file.c_str(), R_OK) == -1)
    {
        std::cerr << "Error: can't open configuration file "
                  << "\'" << config_file << "\' "
                  << "(" << strerror(errno) << ")\n"
                  << "Quitting.\n";
        exit(EXIT_FAILURE);
    }
#endif

    std::cout //<< banner()
              << "-- Running " << utils::version() << ".\n"
              << "-- Starting BDE Server as a "
              << (daemon_mode ? "background" : "foreground") << " process.\n"
              << "-- Reading configuration from " << config_file << ".\n"
              << "-- Logging to "
              << (log_file.empty() ? "console" : log_file) << ".\n\n";

    std::ofstream logstream;

    if (not log_file.empty())
    {
        logstream.open(log_file, std::ofstream::app);

        if (logstream.fail())
        {
            std::cerr << "Error: can't open log file "
                      <<"\'" << log_file << "\'.\n"
                      << "Quitting.\n";
            exit(EXIT_FAILURE);
        }

        utils::service::set_console(logstream);
    }

    if (daemon_mode)
    {
        std::cout << "-- Daemonizing...\n";

        if (daemon(0, 0) != 0)
        {
            std::cerr << "Error: daemon() failed: "
                      << strerror(errno) << "\n"
                      << "Quitting.\n";
            exit(EXIT_FAILURE);
        }

        int pid_file_handle = open("/var/lib/bdeserver/run/bdeserver.pid", O_RDWR | O_CREAT, 0600);

        if (pid_file_handle == -1)
        {
            slog(s_error) << "could not open PID file, exiting";
            exit(EXIT_FAILURE);
        }

        char str[10];
        sprintf(str, "%d\n", getpid());
        write(pid_file_handle, str, strlen(str));
        close(pid_file_handle);

        cmd_path = "/var/lib/bdeserver/";
    }

    // Ignore SIGALRM, apparently raised by libcurl resolver.
    signal(SIGALRM, SIG_IGN);

    // configuration
    Conf conf(config_file.c_str());

    // init main module
    bool again = false;

    do
    {
        conf["bde"]["cmd_path"] = cmd_path;
        main_module.reset(new Main(conf));

        try
        {
            main_module->run();
            again = false;
        }
        catch(std::exception & e)
        {
            utils::slog() << "exception catched:\n" << e.what();
            main_module.reset();
            again = false;
        }
        catch(...)
        {
            utils::slog() << "unknown exception catched. trying again\n";
            main_module.reset();
            again = false;
        }

    } while(again);

    logstream.close();
    return 0;
}

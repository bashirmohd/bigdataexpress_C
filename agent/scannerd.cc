#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <memory>

#include "utils/utils.h"
#include "conf.h"
#include "scanner.h"

std::unique_ptr<Scanner> main_module;

void print_usage(const char * const prog_name)
{
    std::cout << "Usage: \n"
              << "  " << prog_name
              << "[-i|-d] [-c config file] [-l log file]\n"
              << "  -i to run in foreground\n"
              << "  -d to run as daemon in background\n";
}

int main(int argc, char **argv)
{
    bool        daemon_mode = false;

    std::string config_file;
    std::string log_file;

    int c;

    while ((c = getopt (argc, argv, "idc:l:?:h")) != -1)
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
        config_file = daemon_mode ? "/etc/agent.json" : "agent.json";
    }

    if (log_file.empty() and daemon_mode)
    {
        log_file = "/var/lib/bdeagent/log/bdeagent.log";
    }

    if (access(config_file.c_str(), R_OK) == -1)
    {
        std::cerr << "Error: can't open configuration file "
                  << "\'" << config_file << "\' "
                  << "(" << strerror(errno) << ")\n"
                  << "Quitting.\n";
        exit(EXIT_FAILURE);
    }

    const std::string banner = std::string("\n") +
        R"( ___ ___  ___     _                _    )" + "\n" +
        R"(| _ )   \| __|   /_\  __ _ ___ _ _| |_  )" + "\n" +
        R"(| _ \ |) | _|   / _ \/ _` / -_) ' \  _| )" + "\n" +
        R"(|___/___/|___| /_/ \_\__, \___|_||_\__| )" + "\n" +
        R"(                     |___/              )" + "\n" +
        R"(    https://bigdataexpress.fnal.gov     )" + "\n\n";

    std::cout << banner
              << "-- Starting SSA Scanner as a "
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
        std::cout << "-- Daemonizing...\n\n";
        if (daemon(0, 0) != 0)
        {
            std::cerr << "Error: daemon() failed: "
                      << strerror(errno) << "\n"
                      << "Quitting.\n";
            exit(EXIT_FAILURE);
        }
    }

    std::string error;

    try
    {
        Conf conf(config_file.c_str());
        main_module.reset(new Scanner(conf));
        main_module->run();
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

        if (not daemon_mode)
        {
            // Also log error on the console.
            std::cerr << error << "\n\n";
        }

        logstream.close();
        exit(EXIT_FAILURE);
    }

    logstream.close();
}

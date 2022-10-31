//
// This DTN Agent test uses the "agent/main" framework; which will
// initialize AgentManager which will initialize things specified in
// the configuration file.
//

#include <iostream>
#include <memory>
#include <getopt.h>
#include <signal.h>

#include "agent/main.h"

static std::unique_ptr<Main> main_module;

void usage(const char * const progname)
{
    std::cout << "Usage: " << progname << " [-c config] [-r]\n"
              << " `-r` argument wil tell the program to keep running "
              << "and wait for commands.\n";
}

void signalHandler(int signum)
{
    std::cout << "Caught signal " << signum << ", exiting.\n";
    main_module.reset();
    exit(1);
}

int main(int argc, char **argv)
{
    auto configFile = "dtnagent.sample.conf";
    auto runFlag = false;

    int c;

    while ((c = getopt(argc, argv, "c:hr")) != -1)
    {
        switch (c)
           {
           case 'c':
               configFile = optarg;
               break;
           case 'h':
               usage(argv[0]);
               exit(1);
           case 'r':
               runFlag = true;
               break;
           default:
               break;
           }
    }

    if (access(configFile, R_OK) == -1)
    {
        std::cerr << "Can't open configuration file \'"
                  << std::string(configFile) << "\' "
                  << "(error: " << strerror(errno) << ")\n";
        usage(argv[0]);
        exit(1);
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signalHandler;

    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGHUP, &action, nullptr);

    Conf conf(configFile);

    main_module.reset(new Main(conf));

    if (runFlag)
    {
        main_module->run();
    }

    return 0;
}

//
// This is to test DTNAgent class in isolation -- just to make sure it
// can parse configuration correctly.
//

#include <iostream>
#include <memory>
#include <getopt.h>
#include <signal.h>

// make sa_family_t available with old glibc
#include <sys/socket.h>

#include <agent/dtnagent.h>

static std::unique_ptr<DTNAgent> agent;

void usage(const char * const progname)
{
    std::cout << "Usage: " << progname << " [-c config]" << std::endl;
}

int main(int argc, char **argv)
{
    auto configFile = "dtnagent.sample.conf";
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

    Conf conf(configFile);
    auto agent_conf = conf["modules"]["m1"];

    try
    {
        agent.reset(new DTNAgent(agent_conf));
        // XXX: Below code will likely crash.
        // agent->check_configuration(agent_conf);
    }
    catch (std::exception const & ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        exit(1);
    }

    std::cout << *agent << std::endl;
    return 0;
}

#include <iostream>

#include <unistd.h>
#include <string>

#include "conf.h"
#include "sitestore.h"
#include "utils/utils.h"

using namespace utils;

void print_usage(const char * const name)
{
    std::cout << "Usage: \n"
              << "  " << name << " "
              << "[-c|-h] [-c config file]\n"
              << "  default log file is bdeserver.json or /etc/bdeserver.json.\n"
              << "  if the log file is not provided, use the default connection mongo://localhost:27017/bde.\n";
}
 

int main(int argc, char ** argv)
{
    int c;
    std::string conf_file;

    while ( (c = getopt(argc, argv, "c:?h")) != -1 )
    {
        switch(c)
        {
        case 'c':
            conf_file = optarg;
            break;
        case '?':
        case 'h':
            print_usage(argv[0]);
            exit(1);
        default:
            break;
        }
    }

    if (conf_file.empty())
    {
        conf_file = "bdeserver.json";
    }

    if (access(conf_file.c_str(), R_OK) == -1)
    {
        conf_file = "/etc/bdeserver.json";

        if (access(conf_file.c_str(), R_OK) == -1)
        {
            conf_file = "";
        }
    }

    std::string host = "localhost";
    std::string db = "bde";
    int port = 27017;

    if (!conf_file.empty())
    {
        Conf conf(conf_file.c_str());

        host = conf.get<std::string>("bde.store.host");
        db   = conf.get<std::string>("bde.store.db");
        port = conf.get<int>("bde.store.port");
    }

    slog() << "using db " << host << ":" << port << "/" << db;

    SiteStore store(host, port, db);

    slog() << "clearing database...";

    store.delete_all("rawjob");
    store.delete_all("sjob");
    store.delete_all("block");

    slog() << "database reset done";

}

#include <iostream>
#include <thread>

#include <unistd.h>

#include "utils/process.h"

void usage(char const *prog)
{
    std::cout << "Usage: " <<  prog << "\n";
}

int main(int argc, char *argv[])
{
    int timeout = 2;
    int c;

    while ((c = getopt(argc, argv, "t:h?")) != -1)
    {
        switch (c)
        {
        case 't':
            timeout = std::stoi(optarg);
            break;
        case 'h':
        case '?':
            usage(argv[0]);
            exit(1);
        default:
            break;
        }
    }

    std::string prog("/usr/local/mdtmftp+/1.0.0/sbin/mdtm-ftp-server");

    std::vector<std::string> argvec {
        "-data-interface", "131.225.2.29",
            "-password-file", "/opt/fnal/bde/config/mdtmftp+/password",
            "-p", "6001",
            "-c", "/opt/fnal/bde/config/mdtmftp+/server.conf",
            "-l", "/tmp/mdtmftp.log",
            "-log-level", "all" };

    std::cout << "Running " << prog << "\n";

    if (timeout > 0)
    {
        // run a detached thread that will terminate on exit.
        std::thread t([&prog, &argvec](){ utils::supervise_process(prog, argvec); });
        t.detach();
        std::cout << "Exiting after timeout (" << timeout << "s)\n";
        sleep(timeout);
        exit(0);
    }
    else
    {
        // run in the same thread.
        utils::supervise_process(prog, argvec);
    }

    exit(0);
}

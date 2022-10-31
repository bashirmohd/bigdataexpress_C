#include <iostream>
#include <thread>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils/process.h"

static void release(void)
{
    std::cout << "Exit handler running.\n";

    // Do not call killpg() when running within "ctest" or "make
    // test".  That will crash the test suite!
    if (!getenv("CTEST_INTERACTIVE_DEBUG_MODE"))
    {
        if (killpg(0, SIGTERM) < 0)
        {
            perror("killpg()");
        }
    }
}

static void sig_handler(int sig)
{
    std::cout << "Supervisor (pid:" << getpid() << ") "
              << __func__ << " caught " << sig << "\n";

    release();

    // if (killpg(0, SIGTERM) < 0)
    // {
    //     perror("killpg()");
    // }
    // else
    // {
    //     std::cout << "Supervisor (pid:" << getpid() << ") "
    //               << "sent SIGTERM to process group.\n";
    // }

    // int status = 0;
    // wait(&status);

    _exit(0);
}

static void usage(char const *prog)
{
    std::cout << "Usage: " <<  prog << " [-t timeout] prog args\n";
}

int main(int argc, char *argv[])
{
    const char  *prog    = "/usr/bin/sleep";
    char       **args    = nullptr;
    int          timeout = 3;

    int c;

    // if (setpgid(0, 0) < 0)
    // {
    //     perror("setpgid()");
    // }

    // if (setpgrp() < 0)
    // {
    //     perror("setpgrp()");
    // }

    std::cout << "Supervisor process group: " << getpgrp() << ".\n";

    atexit(release);

    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGHUP, sig_handler);

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

    if (optind < argc)
    {
        prog = argv[optind];
        args = argv+optind+1;
    }
    else
    {
        const char *a2[] = { "1", nullptr};
        args = const_cast<char **>(a2);
    }

    std::vector<std::string> argvec;

    for (int i = 0; args[i] != 0; ++i)
    {
        argvec.emplace_back(args[i]);
    }

    std::cout << "Running " << prog << "\n";

    std::thread t([&prog, &argvec](){ utils::supervise_process(prog, argvec); });

    if (timeout > 0)
    {
        t.detach();
        sleep(timeout);
        std::cout << "Quitting after timeout (" << timeout << "s)\n";
    }
    else
    {
        t.join();
    }

    for (auto const &a : argvec)
    {
        delete &a;
    }

    exit(0);
}

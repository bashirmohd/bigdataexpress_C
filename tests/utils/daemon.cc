#include <iostream>

#include <string>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/prctl.h>

static void sig_handler(int sig)
{
    exit(0);
}

int main(int argc, char *argv[])
{
    std::cout << "Daemon process group: " << getpgrp() << ".\n";

    signal(SIGTERM , sig_handler);
    signal(SIGINT  , sig_handler);
    signal(SIGHUP  , sig_handler);

    if (daemon(0,0) < 0)
    {
        perror("daemon");
        exit(1);
    }

    // If we want to receive parent death signals, set
    // PR_SET_PDEATHSIG to some signal.  This is one way for the
    // children to exit along with the parent.
    // prctl(PR_SET_PDEATHSIG, SIGTERM);

    // Rename ourselves in the process table, just because.
    prctl(PR_SET_NAME, (char *) "upsimba");

    while (true)
    {
        sleep(1);
    }

    return 0;
}


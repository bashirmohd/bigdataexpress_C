#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

static void sig_term_handler(int sig)
{
    std::cout << "[nodaemon] received signal " << sig << ".\n";
    exit(sig);
}

static void sig_alarm_handler(int sig)
{
    std::cout << "[nodaemon] received signal " << sig << ".\n";
    exit(sig);
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, sig_term_handler);
    signal(SIGALRM, sig_alarm_handler);

    alarm(1);

    while (true)
    {
        sleep(1);
    }

    return 0;
}

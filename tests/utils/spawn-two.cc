#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <iostream>
#include <stdexcept>
#include <thread>

static bool run_process(const char * const command)
{
    std::cout << __func__ << " running: \"" << command << "\"\n";

    FILE *pipe = popen(command, "re");

    if (not pipe)
    {
        auto err = "Error running \"" + std::string(command) + "\":" + strerror(errno);
        return false;
    }

    char buffer[BUFSIZ]{0};
    std::string result_text;

    while (not feof(pipe))
    {
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            result_text += buffer;
        }
    }

    auto pstat = pclose(pipe);

    if (pstat < 0)
    {
        auto status = "pclose() error: " + std::string(strerror(errno));
        std::cerr << "[run_process] exit code: " << pstat
                  << ", status: " << status;
        std::cerr << "[run_process] process read buffer:" << result_text;
        return false;
    }

    auto result = WEXITSTATUS(pstat);

    std::cout << "-- \"" << command << "\" exit code: " << result << "\n"
              << "-- \"" << command << "\" stdout: \n" << result_text << "\n";

    return result == 0 ? true : false;
}

int main(int argc, char *argv[])
{
    #if 1
    auto cmd1 = "for i in `seq 1 3` ; do sleep $i ; echo $i ; done";
    auto cmd2 = "for i in `seq 1 2` ; do sleep $i ; echo $i ; done";
    #else
    auto cmd1 = "/usr/bin/iostat 192.168.2.40:/nfs/share";
    auto cmd2 = "/usr/bin/ping -c 2 -w 2 192.2.2.1";
    #endif

    auto t1 = std::thread([cmd1](){ run_process(cmd1); });
    auto t2 = std::thread([cmd2](){ run_process(cmd2); });

    t1.join();
    t2.join();

    return 0;
}

// Local Variables:
// compile-command: "g++ -Wall -std=c++11 -pthread spawn-two.cc -o spawn-two"
// End:

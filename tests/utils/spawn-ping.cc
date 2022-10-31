#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <iostream>
#include <stdexcept>

bool run_process(std::string const & command, bool throw_exception=false)
{
    std::cout << __func__ << " running command: " << command << "\n";

    // popen() manual does not say anything about standard error - it
    // looks like we must implement capturing standard error in terms
    // of pipe(), exec(), etc and do the plumbing ourselves. This one
    // simple tick however captures standard error in a cheap manner!
    auto real_cmd = command + " 2>&1";

    FILE *pipe = popen(real_cmd.c_str(), "re");

    if (not pipe)
    {
        auto err = "Error running \"" + command + "\":" + strerror(errno);

        if (throw_exception)
        {
            throw std::runtime_error(err);
        }

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

    if (result != 0 and throw_exception)
    {
        throw std::runtime_error(result_text);
    }

    if (result == 0 and result_text.empty())
    {
        result_text = "OK";
    }

    std::cout << "-- result code: " << result << "\n"
              << "-- program output: \n" << result_text << "\n";

    return result;
}

int main(int argc, char *argv[])
{
    auto count = 5;

    if (argc > 1) {
        count = std::stoi(argv[1]);
    }

    std::cout << "count: " << count << "\n";

    auto cmd = "ping -w 2 -c 2 192.2.2.1";


    for (auto i = 0; i < count; i++)
    {
        std::cout << "[" << (i+1) << "] "
                  << (run_process(cmd) ? "success" : "failure") << "\n";
        std::cout << "--------------------------------------------------\n";
    }

    return 0;
}

// Local Variables:
// compile-command: "g++ -Wall -std=c++11 spawn-ping.cc -o spawn-ping"
// End:

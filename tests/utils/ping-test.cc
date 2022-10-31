#include <iostream>

#include <signal.h>
#include <string.h>
#include <sys/prctl.h>

#include "utils/process.h"

static utils::ProcessResult test_ping(std::string const &destination,
                                      size_t const       deadline,
                                      size_t const       count)
{
    return utils::run_ping(destination, deadline, count);
}

int main(int argc, char **argv)
{
    std::string destination = "192.2.2.1";
    size_t      deadline    = 2;
    size_t      count       = 2;

    if (argc > 1)
    {
        destination = argv[1];
    }

    auto result = test_ping(destination, deadline, count);

    std::cout << "\n\n--------------------------------------------------\n"
              << "Process exit code: "  << result.code << "\n"
              << "Process text: \n" << result.text << "\n";

    return 0;
}

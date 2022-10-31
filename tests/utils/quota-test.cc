#include <iostream>
#include <sstream>

#include "utils/fsusage.h"

int main(int argc, char **argv)
{
    const char *path = ".";

    if (argc > 1)
    {
        path = argv[1];
    }

    std::cout << "Test #1: checking if quota is enabled for path \""
              << path << "\"\n";

    if (utils::quota_enabled(path))
    {
        std::cout << "Quota is enabled on " << path << "\n\n";
    }
    else
    {
        std::cout << "Quota is not enabled on " << path << "\n\n";
    }

    try
    {
        auto user  = getenv("USER");
        std::cout << "Test #2: checking quota for user \"" << user << "\" "
                  << "on path \"" << path << "\"\n";
        auto usage = utils::get_quota_usage(path, user);
        std::cout << "\nUsage:\n" << usage << "\n";
    }
    catch(std::exception const &ex)
    {
        std::cout << "Error reading quota: " << ex.what() << "\n";
    }
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

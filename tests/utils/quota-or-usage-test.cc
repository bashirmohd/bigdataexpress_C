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

    try
    {
        auto user  = getenv("USER");

        std::cout << "Checking quota/filesystem for user \"" << user << "\" "
                  << "on path \"" << path << "\"\n";

        auto usage = utils::get_quota_or_fs_usage(path, user);

        std::cout << "\nFilesystem usage on \""
                  << path << "\" by user " << user << ":\n"
                  << usage;
    }
    catch(std::exception const &ex)
    {
        std::cout << "Error reading quota: " << ex.what() << "\n";
    }

    return 0;
}

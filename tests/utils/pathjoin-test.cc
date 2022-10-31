#include <iostream>
#include "utils/paths/path.h"

using namespace utils;

int main(int argc, char *argv[])
{
    // these test straightforward path joins.
    std::cout << join_paths("/", "/") << "\n";
    std::cout << join_paths(join_paths
                            (join_paths("/", "usr"), "share"),
                            "man") << "\n";
    std::cout << join_paths(join_paths("usr", "share"),
                            "man") << "\n";

    // these test edge cases.
    std::cout << join_paths(join_paths("/", "/"),
                            join_paths("/", "/")) << "\n";
    std::cout << join_paths(join_paths
                            (join_paths("/", "/"),
                             join_paths("/", "/")),
                            join_paths
                            (join_paths("/", "/"),
                             join_paths("/", "/"))) << "\n";

    std::cout << join_paths("////", "//usr//") << "\n";
    std::cout << join_paths(join_paths("/", "usr//"),
                            "share///") << "\n";

    std::cout << join_paths(join_paths
                            (join_paths("/", "usr//"), "share///"),
                            "//man//") << "\n";

    std::cout << join_paths(join_paths
                            (join_paths
                             (join_paths("/", "usr//"),
                              "share///"),
                             "//man//"),
                            "man1") << "\n";
    std::cout << join_paths(join_paths
                            (join_paths
                             (join_paths
                              (join_paths("/", "usr//"),
                               "share///"),
                              "//man//"),
                             "man1"), "man.1.gz") << "\n";

    return 0;
}

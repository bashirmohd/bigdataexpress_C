#include <iostream>
#include <utils/paths/dirwalker.h>

// Under normal circumstances, this should produce an output exactly
// like "find <path>".

int main(int argc, char *argv[])
{
    const char * dir = ".";

    if (argc >= 2)
    {
        dir = argv[1];
    }

    for (auto const &path : utils::DirectoryWalker(dir, true))
    {
        std::cout << path.name() << "\n";
    }

    return 0;
}

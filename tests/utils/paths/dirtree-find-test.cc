#include <iostream>
#include <utils/paths/dirtree.h>

/*
 * Under normal circumstances, this should produce an output exactly
 * like "find <path>", modulo any UNIX special files in the <path>.
 * If that doesn't happen, we have a bug.
 */

int main(int argc, char *argv[])
{
    const char * dir = ".";

    if (argc >= 2)
    {
        dir = argv[1];
    }

    for (auto const & group : utils::DirectoryTree(dir).divide(0))
    {
        for (auto const & p : group)
        {
            std::cout << p.name() << "\n";
        }
    }

    return 0;
}

#include <iostream>
#include <utils/paths/dirwalker.h>

// A test to make sure that utils::DirectoryWalker::sort_by_size()
// works as intended.

int main(int argc, char *argv[])
{
    const char * dir = ".";

    if (argc >= 2)
    {
        dir = argv[1];
    }

    auto walker = utils::DirectoryWalker(dir, true);

    std::cout << "Before sorting:\n";

    for (auto const &path : walker)
    {
        std::cout << path.size() << "\t" << path.name() << "\n";
    }

    walker.sort_by_size();

    std::cout << "\n\nAfter sorting:\n";

    for (auto const &path : walker)
    {
        std::cout << path.size() << "\t" << path.name() << "\n";
    }

    return 0;
}

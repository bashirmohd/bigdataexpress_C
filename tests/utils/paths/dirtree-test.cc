#include <iostream>
#include <utils/paths/dirtree.h>

int main(int argc, char *argv[])
{
    const char * dir = ".";
    size_t       sz  = 1024 * 1024;

    if (argc == 2)
    {
        dir = argv[1];
    }

    if (argc >= 3)
    {
        dir = argv[1];
        sz  = atol(argv[2]);
    }

    std::cout << "Dividing \'" << dir << "\' to groups of size "<< sz << std::endl;

    utils::DirectoryTree tree(dir);

    std::cout << "Aggregate size of files in \'" << dir << "\': "
              << tree.size() << std::endl;

    auto groups = tree.divide(sz);
    std::cout << "We have " << groups.count() << " groups" << std::endl;

    for (auto const & group : groups)
    {
        std::cout << "-- group size: " << group.size() << std::endl;

        for (auto const & p : group)
        {
            std::cout << "\t "<< p.name()
                      << " (size: " << p.size() << ")" << std::endl;
        }
    }

    return 0;
}

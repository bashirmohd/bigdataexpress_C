#include <iostream>
#include <iomanip>
#include <utils/paths/dirtree.h>

int main(int argc, char *argv[])
{
    const char * path = ".";

    if (argc > 1)
    {
        path = argv[1];
    }

    auto sz  = 1024;

    if (argc > 2)
    {
        sz = atoi(argv[2]);
    }

    std::cout << "Making groups of " << sz << " bytes.\n";
    auto res = utils::DirectoryTree(path).divide(sz);

    int i = 0;
    size_t count = 0;
    for (auto const & g : res)
    {
        std::cout << "group "  << std::setw(4) << i++ << "\t"
                  << "size: "  << g.size()     << "\t\t"
                  << "count: " << g.count()    << "\n";

        count += g.count();

        for (auto const & p : g)
        {
            std::cout << "\t-- " << p.name()
                      << " (" << p.type() << ", "
                      << p.size() << " bytes)\n";
        }

        // property:
        // if (g.size() > sz)
        //     assert (g.count() == 1);
    }

    std::cout << "total size : " <<  res.size() << "\n";
    std::cout << "total count: " << count << "\n";

    return 0;
}

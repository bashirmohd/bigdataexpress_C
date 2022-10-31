#include <algorithm>
#include <iostream>
#include <utils/paths/dirtree.h>

int main(int argc, char *argv[])
{
    const char * dir = ".";

    if (argc >= 2)
    {
        dir = argv[1];
    }

    auto const &tree = utils::DirectoryTree(dir).divide(0);
    std::vector<utils::Path> paths{};

    for (auto const &group : tree)
    {
        for (auto const &p : group)
        {
            paths.push_back(std::move(p));
        }
    }

    std::sort(paths.begin(), paths.end(),
              [](const utils::Path &p1, const utils::Path &p2){
                  return p1.size() > p2.size();
              });

    for (auto const &p : paths)
    {
        std::cout << p.size() << "\t -- " << p.name() << "\n";
    }

    return 0;
}

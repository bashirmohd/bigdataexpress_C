#include <iostream>

#include <utils/grid-mapfile.h>

int main(int argc, char **argv)
{
    const char *mapfile = "./grid-mapfile-read";

    setenv("GRIDMAP", mapfile, 1);

    std::cout << "Env var GRIDMAP is set to " << getenv("GRIDMAP") << "\n";

    auto m = utils::grid_mapfile_get_entries();

    std::cout << "Read " << m.path << ".\n";

    for (auto const &v : m.system_entries)
    {
        std::cout << "[system] Read DN: "
                  << v.first << ", LN: " << v.second << "\n";
    }

    for (auto const &v : m.bde_entries)
    {
        std::cout << "[bde] Read DN: "
                  << v.first << ", LN: " << v.second << "\n";
    }
}

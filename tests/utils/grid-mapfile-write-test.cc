#include <iostream>

#include <utils/grid-mapfile.h>

int main(int argc, char **argv)
{
    const char *mapfile = "./grid-mapfile-write";

    setenv("GRIDMAP", mapfile, 1);

    std::cout << "Env var GRIDMAP is set to " << getenv("GRIDMAP") << "\n";

    utils::gridmap_entries newmap = {
        {"/test/", "user"},
        {"test", "user"},
        {"/DC=org/DC=cilogon/C=US/O=Fermilab/OU=People/CN=User/CN=UID:user", "user"}
    };

    std::cout << "Will add entries: \n";
    for (auto const &m : newmap)
    {
        std::cout << "  " << m.first << " -- " << m.second << "\n";
    }
    std::cout << "\n";

    utils::grid_mapfile_push_entries(newmap);

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

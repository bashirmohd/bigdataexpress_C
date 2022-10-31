#ifndef BDE_GRID_MAPFILE_H
#define BDE_GRID_MAPFILE_H

#include <map>
#include <string>

// Ways to manipulate /etc/grid-security/grid-mapfile.  Respects the
// GRIDMAP enironment variable, so we might not always be working on
// that particular file.

namespace utils
{
    typedef std::multimap<std::string, std::string> gridmap_entries;

    struct grid_map {
        std::string     path;
        gridmap_entries system_entries;
        gridmap_entries bde_entries;
    };

    grid_map grid_mapfile_get_entries();

    void grid_mapfile_push_entries(gridmap_entries const &entries);
};

#endif

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

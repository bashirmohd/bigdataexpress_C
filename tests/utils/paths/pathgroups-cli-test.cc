#include <iostream>
#include <algorithm>
#include <getopt.h>

#include <utils/paths/pathgroups.h>

// ----------------------------------------------------------------------

static const std::string TEST_PATH = "/usr/share/doc";

static void usage(const char * const progname)
{
    std::cout << "Usage: "
              << progname
              << " [-s size] OR [-n number] [paths]\n";
}

static void show_input_paths(utils::Paths const &paths)
{
    std::cout << "input paths:\n";
    for (auto const & path : paths)
        std::cout << " - " << path.name() << "\n";
    std::cout << "\n";
}

static void show_groups(utils::PathGroups const & groups)
{
    int i = 1;

    std::cout << "Total of " << groups.size() << " group(s):\n";
    for (auto const & gp : groups)
    {
        std::cout << " - Group " << i++ << ": \t"
                  << gp.size() << " files \t"
                  << find_total_size(gp) << " bytes.\n";
    }
}

int main(int argc, char **argv)
{
    size_t ngroups    = 0;
    size_t group_size = 0;

    int c;

    while ((c = getopt(argc, argv, "s:n:?")) != -1)
    {
        switch (c)
           {
           case 's':
               group_size = std::stoll(optarg);
               break;
           case 'n':
               ngroups = std::stoll(optarg);
               break;
           case '?':
               usage(argv[0]);
               exit(1);
           default:
               break;
           }
    }

    if (ngroups == 0 and group_size == 0)
    {
        group_size = 10 * 1024 * 1024;
    }

    utils::Paths paths;

    if (optind < argc)
    {
         for (auto i = optind; i < argc; ++i)
        {
            paths.push_back(utils::Path(argv[i]));
        }
    }
    else
    {
        paths.push_back(utils::Path(TEST_PATH));
    }

    show_input_paths(paths);

    if (ngroups != 0)
    {
        std::cout << "Dividing input paths into "
                  << ngroups << " groups.\n\n";
        show_groups(utils::PathGroupsHelper(paths).group_into(ngroups));
    }

    if (group_size != 0)
    {
        std::cout << "Dividing input paths into groups of "
                  << group_size << " bytes.\n\n";
        show_groups(utils::PathGroupsHelper(paths).group_by_size(group_size));
    }

    return 0;
}

// ----------------------------------------------------------------------
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

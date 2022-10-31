#include <iostream>
#include <stdexcept>
#include <getopt.h>

#include "utils/mounts.h"

// TODO: if a path is given, and nothing is mounted on it, find its
// parent.

// TODO: handle multiple inputs.

int main(int argc, char **argv)
{
    int c;

    bool verbose_flag = false;
    bool recurse_flag = false;

    while ((c = getopt (argc, argv, "rv")) != -1)
    {
        switch (c)
           {
           case 'r':
               recurse_flag = true;
               break;
           case 'v':
               verbose_flag = true;
               break;
           default:
               break;
           }
    }

    const char *rootPath = "/";

    if (argv[optind])
    {
        rootPath = argv[optind];
    }

    try
    {
        utils::MountPointsHelper helper(rootPath, recurse_flag);
        auto mounts = helper.get_mounts();

        if (mounts.empty())
        {
            const auto & mount = helper.get_parent_mount();

            if (verbose_flag)
                mount.print_verbose();
            else
                mount.print();
        }
        else
        {
            for (auto & mount : mounts)
            {
                if (verbose_flag)
                    mount.print_verbose();
                else
                    mount.print();
            }
        }
    }
    catch (std::runtime_error & ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
    }
    catch (...)
    {
        throw;
    }
}

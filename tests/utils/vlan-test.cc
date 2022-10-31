#include <iostream>
#include <cstring>

#include "utils/vlan.h"

void usage(const char *progname)
{
    std::cerr << "Usage: " << progname << " [args]\n"
              << "  where args are: \n"
              << "    add <iface> <id>\n"
              << "    rm <name>\n";
}

int main(int argc, char **argv)
{
    // Don't bother running this from "ctest" or "make test"; this is
    // for directly testing vlan operations from command line.
    if (getenv("CTEST_INTERACTIVE_DEBUG_MODE") != 0)
        return 0;

    if (argc < 2) {
        usage(argv[0]);
        exit(1);
    }

    const char *cmd = argv[1];

    if ((strncmp(cmd, "add", strlen("add")) == 0) and argc == 4)
    {
        const char *iface   = argv[2];
        int         vlan_id = strtol(argv[3], nullptr, 10);

        try	{
            auto name = utils::add_vlan(iface, vlan_id);
            std::cout << "success: created " << name << ".\n";
        } catch (std::exception &ex) {
            std::cerr << "Exception: " << ex.what() << "\n";
        }
    }
    else if ((strncmp(cmd, "rm", strlen("rm")) == 0) and argc == 3)
    {
        const char *ifname = argv[2];

        try {
            utils::delete_vlan(ifname);
            std::cout << "success\n";
        } catch (std::exception &ex) {
            std::cerr << "Exception: " << ex.what() << "\n";
        }
    }
    else
    {
        usage(argv[0]);
        exit(1);
    }

    return 0;
}

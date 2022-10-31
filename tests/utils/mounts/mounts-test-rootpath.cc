#include <iostream>
#include <stdexcept>
#include <getopt.h>

#include "utils/mounts.h"

static std::string q(std::string const &p)
{
    return "\"" + p + "\"";
}

void test_path(std::string const &p, bool expected)
{
    auto result = utils::MountPointsHelper::is_root_path(p);
    std::cout << q(p) << "\t "
              << (result == expected ? "OK" : "NOT OK") << "\n";
}

int main(int argc, char **argv)
{
    test_path("/"     , true);
    test_path("///"   , true);
    test_path(" ///"  , false);
    test_path(" /// " , false);
    test_path(" /// " , false);
    test_path("/usr"  , false);

    return 0;
}

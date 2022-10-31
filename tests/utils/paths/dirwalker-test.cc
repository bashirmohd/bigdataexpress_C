#define CATCH_CONFIG_MAIN
#include "../../catch.hpp"

#include <utils/paths/dirwalker.h>

using namespace utils;

TEST_CASE("directory walker: invalid directory")
{
    // "" is an invalid path; should throw an exception.
    REQUIRE_THROWS(new DirectoryWalker("", true, true));
}

TEST_CASE("directory walker: current directory")
{
    // current directory should have more than 0 members.
    DirectoryWalker dirs(".");
    REQUIRE(dirs.count() > 0);
    REQUIRE(dirs.begin() != dirs.end());
}

TEST_CASE("directory walker: iterate over current directory")
{
    // current directory should have more than 0 members.
    DirectoryWalker dirs(".");
    REQUIRE(dirs.begin() != dirs.end());
}

TEST_CASE("directory walker: sort test")
{
    // current directory should have more than 0 members.
    DirectoryWalker dirs(".", true);
    REQUIRE(dirs.begin() != dirs.end());

    size_t count1 = dirs.count();
    dirs.sort_by_size();
    size_t count2 = dirs.count();

    REQUIRE(dirs.begin() != dirs.end());
    REQUIRE(count1 == count2);

    if (dirs.count() > 2)
    {
        REQUIRE(dirs.at(0).size() >= dirs.at(1).size());
    }

    if (dirs.count() > 3)
    {
        REQUIRE(dirs.at(0).size() >= dirs.at(2).size());
        REQUIRE(dirs.at(1).size() >= dirs.at(2).size());
    }
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

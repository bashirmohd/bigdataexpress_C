#define CATCH_CONFIG_MAIN
#include "../../catch.hpp"

#include <vector>

#include <utils/paths/dirwalker.h>
#include <utils/paths/pathgroups.h>

static const std::string TEST_PATH = "/usr/share/doc";

using namespace utils;

TEST_CASE("PathGroupsHelper: constructor")
{
    Paths paths{Path(".")};
    REQUIRE_NOTHROW(new PathGroupsHelper(paths));
    REQUIRE_NOTHROW(new PathGroupsHelper(paths));

    REQUIRE_NOTHROW(new PathGroupsHelper({"."}));
    REQUIRE_NOTHROW(new PathGroupsHelper({".", "/bin", "/sbin"}));
}

TEST_CASE("PathGroupsHelper: constructor errors")
{
    // Invalid paths should throw an exception.
    REQUIRE_THROWS(new PathGroupsHelper({""}));
    REQUIRE_THROWS(new PathGroupsHelper({".", "/bin", "", "/sbin"}));
}

TEST_CASE("PathGroupsHelper: size")
{
    PathGroupsHelper g1({"."});
    PathGroupsHelper g2({"."});

    REQUIRE(g1.get_input_paths().size() != 0);
    REQUIRE(g2.get_input_paths().size() != 0);

    REQUIRE(g1.get_input_paths() == g2.get_input_paths());
}

TEST_CASE("PathGroupsHelper: assignments")
{
    PathGroupsHelper g1({"."});
    PathGroupsHelper g2 = g1;

    REQUIRE(g1.get_input_paths().size() != 0);
    REQUIRE(g2.get_input_paths().size() != 0);

    REQUIRE(g1.get_input_paths() == g2.get_input_paths());
}

TEST_CASE("PathGroupsHelper: group_into")
{
    PathGroupsHelper g1({TEST_PATH});

    // invalid inputs should cause exception
    REQUIRE_THROWS(g1.group_into(0));

    for (size_t i = 1; i <= 24; ++i)
    {
        size_t group_size = g1.group_into(i).size();
        CHECK(group_size == i);

        // TODO: is this the right test?
        // CHECK(group_size <= i);
    }

    // TODO: take variations due to large files into account.
}

TEST_CASE("PathGroupsHelper: group_by_size")
{
    PathGroupsHelper g1({TEST_PATH});

    size_t incr = 1024 * 1024;

    for (size_t group_size = incr; group_size <= 100 * incr;
         group_size += incr)
    {
        PathGroups pgs = g1.group_by_size(group_size);

        REQUIRE(pgs.size() > 0);

        for (size_t i = 0; i < pgs.size(); ++i)
        {
            auto current_group = pgs.at(i);
            size_t current_group_size = find_total_size(current_group);

            if (current_group.size() > 1)
            {
                CHECK(current_group_size <= group_size);
            }

            if (current_group_size > group_size)
            {
                CHECK(current_group.size() == 1);
            }
        }
    }
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

#define CATCH_CONFIG_MAIN
#include "../../catch.hpp"

#include <utils/paths/path.h>

using namespace utils;

TEST_CASE("Path: constructor")
{
    Path p1("/usr");
    REQUIRE(p1.name() == "/usr");
}

TEST_CASE("Path: constructor errors")
{
    // invalid path should cause an exception to be thrown.
    REQUIRE(not Path("").exists());
}

TEST_CASE("Path: equality operators")
{
    Path p1("/sbin");
    Path p2("/bin");

    REQUIRE(p1 != p2);

    Path p4("/sbin");
    REQUIRE(p1 == p4);

    Path p5("/bin");
    REQUIRE(p2 == p5);
}

TEST_CASE("Path: exists")
{
    Path p1("/");
    REQUIRE(p1.exists());

    Path p2("/bin/sh");
    REQUIRE(p2.exists());
}

TEST_CASE("Path: is_directory")
{
    Path p1("/");
    REQUIRE(p1.is_directory());

    Path p2("/bin/sh");
    REQUIRE(not p2.is_directory());
}

TEST_CASE("Path: is_regular_file")
{
    Path p1("/");
    REQUIRE(not p1.is_regular_file());

    Path p2("/bin/sh");
    REQUIRE(not p2.is_regular_file());

    Path p3("/bin/bash");
    REQUIRE(p3.is_regular_file());
}

TEST_CASE("Path: is_symbolic_link")
{
    Path p1("/");
    REQUIRE(not p1.is_symbolic_link());
}

TEST_CASE("Path: file_size")
{
    Path p1("/bin/sh");
    REQUIRE(p1.size() > 0);

    // TODO: how to handle file_size() on directories?
    // boost::filesystem throws an exception...
    Path p2("/");
    REQUIRE(p2.size() == 0);
}

TEST_CASE("Path: name")
{
    Path p1("path-test");
    REQUIRE(p1.name().find("path-test") == 0);
}

TEST_CASE("Path: base_name")
{
    Path p1("path-test");
    REQUIRE(p1.base_name().find("path-test") == 0);
    REQUIRE(*p1.base_name().begin() != '/');
}

TEST_CASE("Path: dir_name")
{
    Path p1("path-test");

    REQUIRE(Path(p1.dir_name()).is_directory());
    REQUIRE(p1.dir_name() != p1.name());
    REQUIRE(p1.dir_name() != p1.base_name());
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

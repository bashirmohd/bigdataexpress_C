//
// Property tests for utils::string2size().
//

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include <iostream>

#include "utils/utils.h"

// ------------------------------------------------------------------------

// 1 is 1 bytes.
// 12 is 12 bytes.
// 1024 is 1024 bytes.
// 10000 is 10000 bytes.
TEST_CASE("utils::string2size() - plain numbers")
{
    REQUIRE(utils::string2size("1")     == 1);
    REQUIRE(utils::string2size("12")    == 12);
    REQUIRE(utils::string2size("1024")  == 1024);
    REQUIRE(utils::string2size("10000") == 10000);

}

// 1kB is 1000 bytes.
// 1KB is 1000 bytes.
// kb is 0 bytes.
// kB is 0 bytes.
// 128kB is 128000 bytes.
TEST_CASE("utils::string2size() - KB suffix")
{
    REQUIRE(utils::string2size("1kB")   == 1000);
    REQUIRE(utils::string2size("1KB")   == 1000);
    REQUIRE(utils::string2size("kb")    == 0);
    REQUIRE(utils::string2size("kB")    == 0);
    REQUIRE(utils::string2size("128kB") == 128000);
}

// 1MB is 1000000 bytes.
// 128MB is 128000000 bytes.
TEST_CASE("utils::string2size() - MB suffix")
{
    REQUIRE(utils::string2size("1MB")   == 1000000);
    REQUIRE(utils::string2size("128MB") == 128000000);
}

// 1GB is 1000000000 bytes.
// GB is 0 bytes.
// G is 0 bytes.
// 256GB is 256000000000 bytes.
TEST_CASE("utils::string2size() - GB suffix")
{
    REQUIRE(utils::string2size("1GB")   == 1000000000);
    REQUIRE(utils::string2size("GB")    == 0);
    REQUIRE(utils::string2size("G")     == 0);
    REQUIRE(utils::string2size("256GB") == 256000000000);
}

// 1TB is 1000000000000 bytes.
// 1T is 1000000000000 bytes.
TEST_CASE("utils::string2size() - TB suffix")
{
    REQUIRE(utils::string2size("1TB") == 1000000000000);
    REQUIRE(utils::string2size("1T")  == 1000000000000);
}

// 1P is 1000000000000000 bytes.
// 1PB is 1000000000000000 bytes.
TEST_CASE("utils::string2size() - PB suffix")
{
    REQUIRE(utils::string2size("1P")  == 1000000000000000);
    REQUIRE(utils::string2size("1PB") == 1000000000000000);
}

// EB, ZT, YB etc are unsupported, because we can't represent them without bignums.
TEST_CASE("utils::string2size() - unsupported prefixes")
{
    REQUIRE_THROWS(utils::string2size("1EB"));
    REQUIRE_THROWS(utils::string2size("1E"));
    REQUIRE_THROWS(utils::string2size("1Z"));
    REQUIRE_THROWS(utils::string2size("1ZB"));
    REQUIRE_THROWS(utils::string2size("1Y"));
    REQUIRE_THROWS(utils::string2size("1YB"));
}

// Strings that are not numbers will be evaluated to 0.
TEST_CASE("utils::string2size() - just strings")
{
    REQUIRE(utils::string2size("notasize") == 0);
    REQUIRE(utils::string2size("fortytwo") == 0);
    REQUIRE(utils::string2size("42")       == 42);
}

// ------------------------------------------------------------------------

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

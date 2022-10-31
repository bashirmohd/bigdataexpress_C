#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include <vector>
#include <string>
#include <iostream>

#include "utils/checksum.h"

TEST_CASE("utils::checksum_of_checksums()")
{
    std::vector<std::string> list =
        { "can", "we", "have", "some", "checksum", "please" };

    auto result = utils::checksum_of_checksums(list, "sha1");
    auto expected = "c1e1533d855c4c610c6e65acc8ac354156d14a8b";

    REQUIRE_NOTHROW(result == expected);
    REQUIRE_THROWS(utils::checksum_of_checksums(list, "nosuchalgorithm") == "");
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

#include <iostream>
#include <algorithm>
#include <climits>
#include <cfloat>

#include "utils/network.h"
#include "utils/tsdb.h"

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

bool test_insert_00(utils::TimeSeriesDB &tsdb,
                    std::string const   &measurement)
{
    utils::TimeSeriesMeasurement<1, float> measure(tsdb,
                                                   measurement,
                                                   {"test-col-name"},
                                                   {"test-tag-name"});

    auto r1 = measure.insert(0.0f,    {"test-tag-val"});
    auto r2 = measure.insert(1.1f,    {"test-tag-val"});
    auto r3 = measure.insert(-1.1f,   {"test-tag-val"});
    auto r4 = measure.insert(FLT_MAX, {"test-tag-val"});
    auto r5 = measure.insert(FLT_MIN, {"test-tag-val"});

    return (r1 and r2 and r3 and r4 and r5);
}

bool test_insert_01(utils::TimeSeriesDB &tsdb,
                    std::string const   &host,
                    std::string const   &measurement,
                    std::string const   &id,
                    std::string const   &iface)
{
    std::array<std::string, 3> tags   = {"host", "id", "interface"};
    std::array<std::string, 9> fields =
        { "interval"   ,
          "rx-bytes"   ,
          "rx-packets" ,
          "rx-dropped" ,
          "rx-errors"  ,
          "tx-bytes"   ,
          "tx-packets" ,
          "tx-dropped" ,
          "tx-errors"  };

    utils::TimeSeriesMeasurement<3,
                                 size_t,
                                 size_t,
                                 size_t,
                                 size_t,
                                 size_t,
                                 size_t,
                                 size_t,
                                 size_t,
                                 size_t> nwdata(tsdb, measurement, fields, tags);

    return nwdata.insert(10, 1, 2, 3, 4, 5, 6, 7, 8, {host, id, iface});
}

bool test_insert_02(utils::TimeSeriesDB &tsdb,
                    std::string const   &measurement,
                    std::string const   &id)
{
    std::array<std::string, 12> fields = { "boolf",
                                           "stringf",
                                           "cstringf",
                                           "intf",
                                           "uintf",
                                           "longf",
                                           "ulongf",
                                           "longlongf",
                                           "ulonglongf",
                                           "floatf",
                                           "doublef",
                                           "ldoublef" };

    utils::TimeSeriesMeasurement<2,
                                 bool,
                                 std::string,
                                 const char *,
                                 int,
                                 unsigned int,
                                 long,
                                 unsigned long,
                                 long long,
                                 unsigned long long,
                                 float,
                                 double,
                                 long double
                                 >
        measure(tsdb, measurement, fields, {"tag-name", "id"});

    auto r1 = measure.insert(true,
                             "teststring",
                             "cteststring",
                             INT_MAX,
                             UINT_MAX,
                             LONG_MAX,
                             ULONG_MAX,
                             LLONG_MAX,
                             ULLONG_MAX,
                             FLT_MAX,
                             DBL_MAX,
                             0, // LDBL_MAX will fail.
                             {"tag-value", id});

    auto r2 = measure.insert(true,
                             "teststring",
                             "cteststring",
                             INT_MIN,
                             0,
                             LONG_MIN,
                             0,
                             LLONG_MIN,
                             0,
                             FLT_MIN,
                             DBL_MIN,
                             LDBL_MIN,
                             {"tag-value", id});

    return (r1 and r2);
}

TEST_CASE("tsdb", "simple insertion test")
{
    auto host = "yosemite.fnal.gov";
    auto port = 8086;
    auto db   = "bde_test";

    utils::TimeSeriesDB tsdb(host, port, db);

    // Proceed *only if* we are using test database.
    if (not (host == "yosemite.fnal.gov" and db == "bde_test"))
    {
        return;
    }

    // Below we try to remove inserted values from test database, but
    // turns out that partial deletes are troublesome with time series
    // databases -- which means, they don't necessarily work the way
    // we expect them to work when we have a "where" clause.  It
    // doesn't matter whether we use "drop series" or "delete"
    // statements.

    // Maybe we can set a retention policy for test database?
    std::string namep = "test_db_policy";
    auto dropp = "DROP RETENTION POLICY " + namep +" ON " + db;
    auto retp  = "CREATE RETENTION POLICY " + namep + " ON " + db +
        " DURATION 1h REPLICATION 1 DEFAULT";

    REQUIRE(tsdb.raw_query(dropp).first == 200);
    REQUIRE(tsdb.raw_query(retp).first  == 200);

    {
        std::string measure = "simple";
        auto        wherec  = "\"" + measure + "\"";
        auto        deleteq = "DELETE FROM " + wherec;
        auto        selectq = "SELECT * FROM " +  wherec;

        REQUIRE(tsdb.raw_query(deleteq).first == 200);
        REQUIRE(tsdb.raw_query(selectq).first == 200);
        REQUIRE(test_insert_00(tsdb, measure) == true);
        REQUIRE(tsdb.raw_query(selectq).first == 200);
        REQUIRE(tsdb.raw_query(deleteq).first == 200);
    }

    {
        std::string measure = "network";
        auto        id      = "test-85b3f150-664e-11e7-8c5c-002590fd1b24";
        auto        iface   = "test-iface";
        auto        wherec  = "\"" + measure + "\" WHERE id=\"" + id + "\"";
        // auto        deleteq = "DROP SERIES FROM " + wherec;
        auto        deleteq = "DELETE FROM " + wherec;
        auto        selectq = "SELECT * FROM " +  wherec;

        REQUIRE(tsdb.raw_query(deleteq).first                  == 200);
        REQUIRE(tsdb.raw_query(selectq).first                  == 200);
        REQUIRE(test_insert_01(tsdb, host, measure, id, iface) == true);
        REQUIRE(tsdb.raw_query(selectq).first                  == 200);
        REQUIRE(tsdb.raw_query(deleteq).first                  == 200);
    }

    {
        std::string measure = "test";
        auto        id      = "test-ff241d44-5552-458e-8f11-e7e2e048fad1";
        auto        wherec  = "\"" + measure + "\" WHERE id=\"" + id + "\"";
        auto        deleteq = "DELETE FROM " + wherec;
        auto        selectq = "SELECT * FROM " +  wherec;

        REQUIRE(tsdb.raw_query(deleteq).first     == 200);
        REQUIRE(tsdb.raw_query(selectq).first     == 200);
        REQUIRE(test_insert_02(tsdb, measure, id) == true);
        REQUIRE(tsdb.raw_query(selectq).first     == 200);
        REQUIRE(tsdb.raw_query(deleteq).first     == 200);
    }
}

TEST_CASE("escape", "utils::escape() test")
{
    // std::cout << utils::escape("nothing-to-escape") << "\n"
    //           << utils::escape("has space") << "\n"
    //           << utils::escape("has\ttab") << "\n"
    //           << utils::escape("has=equal=sign") << "\n";

    REQUIRE(utils::escape("nothing-to-escape") == "nothing-to-escape");
    REQUIRE(utils::escape("has space") == "has\\ space");
    REQUIRE(utils::escape("has\ttab") == "has\\\ttab");
    REQUIRE(utils::escape("has=equal=sign") == "has\\=equal\\=sign");
}

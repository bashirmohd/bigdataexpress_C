#ifndef BDE_UTILS_TSDB_H
#define BDE_UTILS_TSDB_H

// DTN Agents need to record some time series data regarding DTNs
// (network, CPU, memory usage...), and InfluxDB happens to be our
// current choice of time series database.
//
// https://www.influxdata.com/time-series-platform/influxdb/
//
// InfluxDB exposes an HTTP API for writing and querying time series
// data.  See their documentation for details:
//
// https://docs.influxdata.com/influxdb/v1.2/guides/writing_data/
// https://docs.influxdata.com/influxdb/v1.2/guides/querying_data/
//
// The APIs below provide a nifty wrapper (nifty enough for our
// purpose anyway!) over InfluxDB's HTTP API, implemented using
// curlcpp.

// Usage is as follows, which should be pretty self-explanatory:
//
//     utils::TimeSeriesDB tsdb("host", 8086, "db");
//
//     auto const num_tags = 2;
//     utils::TimeSeriesMeasurement<num_tags, float, bool> m(tsdb,
//                                                 "series-name",
//                                                 {"col-name-1", "col-name-2"},
//                                                 {"tag-name-1", "tag-name-2"});
//     m.insert(3.14, true, {"tag-val1", "tag-val2"});
//

#include <list>
#include <string>
#include <utils/json/json.h>

#include <iostream>

#include "baseconf.h"
#include "tsdb-helpers.h"

namespace utils
{
    class TimeSeriesDB {
    public:
        TimeSeriesDB(std::string const &host,
                     int port,
                     std::string const &dbname,
                     std::string const &username=std::string(),
                     std::string const &password=std::string())
            : host_(host)
            , port_(port)
            , dbname_(dbname)
            , username_(username)
            , password_(password)
            , base_endpoint_(host_ + ":" + std::to_string(port_))
            , write_endpoint_(base_endpoint_ + "/write" + "?&db=" + dbname_)
            , query_endpoint_(base_endpoint_ + "/query" + "?&db=" + dbname_) { };

        TimeSeriesDB(BaseConf const & conf)
            : host_(conf.get<std::string>("agent.tsdb.host"))
            , port_(conf.get<int>("agent.tsdb.port"))
            , dbname_(conf.get<std::string>("agent.tsdb.db"))
            , base_endpoint_(host_ + ":" + std::to_string(port_))
            , write_endpoint_(base_endpoint_ + "/write" + "?&db=" + dbname_)
            , query_endpoint_(base_endpoint_ + "/query" + "?&db=" + dbname_) { };

        bool                        run_post(std::string const &query) const;
        std::pair<int, Json::Value> raw_query(std::string const &query) const;

    private:
        std::string const host_;
        int         const port_;
        std::string const dbname_;
        std::string const username_;
        std::string const password_;
        std::string const base_endpoint_;
        std::string const write_endpoint_;
        std::string const query_endpoint_;
    };

    template <size_t N>
    using FieldNames = std::array<std::string, N>;

    template <size_t N>
    using TagNames = std::array<std::string, N>;

    template <size_t N>
    using TagValues = std::array<std::string, N>;

    template<size_t NUM_TAGS, typename... Fields>
    class TimeSeriesMeasurement {

        using row_t = std::tuple<Fields...>;
        static constexpr auto NUM_FIELDS = std::tuple_size<row_t>::value;

    public:
        TimeSeriesMeasurement(TimeSeriesDB const           &db,
                              std::string const            &measurement,
                              FieldNames<NUM_FIELDS> const &field_names,
                              TagNames<NUM_TAGS> const     &tag_names)
            : db_(db)
            , measurement_(measurement)
            , field_names_(field_names)
            , tag_names_(tag_names) { }

        bool insert(Fields const ... field_values,
                    TagValues<NUM_TAGS> const &tag_values) {

            auto query   = measurement_;
            auto tag_str = tag_set_builder<NUM_TAGS>::build(tag_names_, tag_values);

            if (not tag_str.empty()) {
                query += tag_str;
            }

            // Construct value fields of the query.
            std::string val_str{};
            auto row = std::make_tuple(field_values...);

            field_set_builder<std::tuple<Fields...>,
                              NUM_FIELDS, NUM_FIELDS>::build(val_str,
                                                             row,
                                                             field_names_);

            if (val_str.empty()) {
                return false;
            }

            query += " " + val_str;

            // std::cout << __func__ << " q: " << query << "\n";
            // return true;

            return db_.run_post(query);
        }

    private:
        TimeSeriesDB const     &db_;
        std::string const       measurement_;
        FieldNames<NUM_FIELDS>  field_names_;
        TagNames<NUM_TAGS>      tag_names_;
    };
};

#endif // BDE_UTILS_TSDB_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

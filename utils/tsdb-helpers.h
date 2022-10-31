#ifndef BDE_UTILS_TSDB_HELPERS_H
#define BDE_UTILS_TSDB_HELPERS_H

// Code here help us massage time series inputs to some "stringy"
// shape we want them to be, so that they can be submitted to InfluxDB
// via its HTTP API.

namespace utils
{
    template<typename T>
    static std::string format(T const &v) {
        return std::to_string(v);
    }

    template<>
    std::string format(bool const &v) {
        return v ? "true" : "false";
    }

    template<>
    std::string format(std::string const &v) {
        return "\"" + v + "\"";
    }

    template<>
    std::string format(const char * const &v) {
        return "\"" + std::string(v) + "\"";
    }

    std::string escape(std::string const &str);

    template <typename Tuple, size_t TupleSize, size_t FieldsSize>
    struct field_set_builder {
        static void build(std::string &str,
                          Tuple const &tup,
                          std::array<std::string, FieldsSize> const &fs) {

            field_set_builder<Tuple, TupleSize-1, FieldsSize>::build(str, tup, fs);

            auto key = fs.at(TupleSize-1);
            auto val = format(std::get<TupleSize-1>(tup));

            if (!key.empty() and !val.empty()) {
                str += "," + key + "=" + val;
            }
        }
    };

    template <typename Tuple, size_t FieldsSize>
    struct field_set_builder<Tuple, 1, FieldsSize> {
        static void build(std::string &str,
                          Tuple const &tup,
                          std::array<std::string, FieldsSize> const &fs) {
            auto key = fs.at(0);
            auto val = format(std::get<0>(tup));

            if (!key.empty() and !val.empty()) {
                str += escape(key) + "=" + val;
            }
        }
    };

    template <size_t NumTags>
    struct tag_set_builder {
        static std::string build(std::array<std::string, NumTags> const &tag_names,
                                 std::array<std::string, NumTags> const &tag_values) {
            std::string str{};

            for (auto i = 0; i < NumTags; ++i) {
                auto key = tag_names.at(i);
                auto val = tag_values.at(i);

                if (!key.empty() and !val.empty()) {
                    str += "," + escape(key) + "=" + escape(val);
                }
            }

            return str;
        }
    };
}

#endif // BDE_UTILS_TSDB_HELPERS_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

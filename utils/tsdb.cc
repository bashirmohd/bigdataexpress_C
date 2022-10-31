#include <ostream>
#include <curl_easy.h>

#include "tsdb.h"
#include "utils/utils.h"

// extra logging from this module.
static const bool verbose = false;
static       bool printed_error = false;

bool utils::TimeSeriesDB::run_post(std::string const &query) const
{
    if (query.empty())
    {
        throw std::runtime_error("empty query");
    }

    std::ostringstream                 str;
    curl::curl_ios<std::ostringstream> writer(str);
    curl::curl_easy                    easy(writer);

    easy.add<CURLOPT_URL>(write_endpoint_.c_str());
    easy.add<CURLOPT_POSTFIELDS>(query.c_str());
    easy.add<CURLOPT_POSTFIELDSIZE_LARGE>(query.length());
    easy.add<CURLOPT_TCP_KEEPALIVE>(1L);
    easy.add<CURLOPT_NOSIGNAL>(1);

    if (not username_.empty())
    {
        std::string auth = username_ + ":" + password_;
        easy.add<CURLOPT_USERPWD>(auth.c_str());
    }

    // easy.add<CURLOPT_VERBOSE>(1);
    // easy.add<CURLOPT_NOPROGRESS>(1L);

    try
    {
        easy.perform();
        printed_error = false; // reset error flag.
    }
    catch (curl::curl_easy_exception const &error)
    {
#if (DEBUG==1)
        // Helpful when testing: print the error stack, and clear it
        // once printed.
        error.print_traceback();
        error.clear_traceback();
#endif

        if (!printed_error) {
            utils::slog() << "[InfluxDB] Curl error: " << error.what();
            printed_error = true;
        }

        return false;
    }

    auto status = easy.get_info<CURLINFO_RESPONSE_CODE>().get();

    if (verbose)
    {
        utils::slog() << "[InfluxDB] sent {" << query << "} to {"
                      << write_endpoint_ << "}; received status: "
                      << status << ", message: " << str.str();
    }

    if (status >= 200 and status < 300)
    {
        return true;
    }

    // log failures.
    utils::slog() << "[InfluxDB] sent {" << query << "} to {"
                  << write_endpoint_ << "}; received status: "
                  << status << ", message: " << str.str();

    return false;
}


// This method is just to make sure that things work as they should --
// such as, to be called from test cases and the such.
std::pair<int, Json::Value>
utils::TimeSeriesDB::raw_query(std::string const &query) const
{
    std::ostringstream                 str;
    curl::curl_ios<std::ostringstream> writer(str);
    curl::curl_easy                    easy(writer);

    // make a local non-const copy of query string.
    auto q(query);
    easy.escape(q);

    std::string url = query_endpoint_ + "&q=" + q ;

    easy.add<CURLOPT_URL>(url.c_str());

    if (not username_.empty())
    {
        std::string auth = username_ + ":" + password_;
        easy.add<CURLOPT_USERPWD>(auth.c_str());
    }

    try
    {
        easy.perform();
    }
    catch (curl::curl_easy_exception const &error)
    {
#if (DEBUG==1)
        error.print_traceback(); // helpful when testing.
        error.clear_traceback();
#endif

        throw error;
    }

    Json::Value  res;
    Json::Reader reader;

    reader.parse(str.str(), res);

    auto status = easy.get_info<CURLINFO_RESPONSE_CODE>().get();

    if (verbose)
    {
        utils::slog() << "[InfluxDB] sent {" << query << "} to {"
                      << query_endpoint_ << "}; received status: " << status;
    }

    return std::pair<int, Json::Value>(status, res);
}

std::string utils::escape(std::string const &str)
{
    std::stringstream ss;

    for (auto const &c : str)
    {
        if (c == ',' or c == '=' or isspace(c))
        {
            ss << '\\' << c;
        }
        else
        {
            ss << c;
        }
    }

    return ss.str();
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:


#include "portal.h"
#include "utils/utils.h"

#include "curl_exception.h"

using namespace utils;

Portal::Portal()
  : ss ()
  , ios (ss)
  , easy (ios)
  , writer ()
  , reader ()
{
    // use json
    //header.add("Content-Type:application/json");
    //header.add("Content-Type:application/x-www-form-urlencoded");
    //easy.add<CURLOPT_HTTPHEADER>(header.get());

    // do not verify server certifiate cause we self-signed it
    //easy.add<CURLOPT_SSL_VERIFYPEER>(0);
    //easy.add<CURLOPT_SSL_VERIFYHOST>(0);
}

Portal::~Portal()
{
}

std::pair<long, Json::Value> Portal::perform()
{
    easy.add<CURLOPT_SSL_VERIFYPEER>(0);
    easy.add<CURLOPT_SSL_VERIFYHOST>(0);

    easy.add<CURLOPT_WRITEFUNCTION>(ios.get_function());
    easy.add<CURLOPT_WRITEDATA>(static_cast<void*>(ios.get_stream()));

    try
    {
        easy.perform();
    }
    catch (curl::curl_easy_exception const & err)
    {
        curl::curlcpp_traceback errors = err.get_traceback();
        err.print_traceback();

        ss.str("");
        easy.reset();

        return std::make_pair(0, Json::Value());
    }

    Json::Value res;
    bool    r = reader.parse(ss.str(), res);
    auto code = easy.get_info<CURLINFO_RESPONSE_CODE>();

    ss.str("");
    easy.reset();

    return std::make_pair(code.get(), res);
}

std::pair<long, Json::Value> Portal::post_form(
        std::string const & url, std::string const & params)
{
    curl::curl_header header;
    header.add("Content-Type:application/x-www-form-urlencoded");
    easy.add<CURLOPT_HTTPHEADER>(header.get());

    easy.add<CURLOPT_URL>(url.c_str());
    easy.add<CURLOPT_POSTFIELDS>(params.c_str());

    return perform();
}
std::pair<long, Json::Value> Portal::post(
        std::string const & url, Json::Value const & params)
{
    curl::curl_header header;
    header.add("Content-Type:application/json");
    easy.add<CURLOPT_HTTPHEADER>(header.get());

    std::string s = writer.write(params) + " ";

    easy.add<CURLOPT_URL>(url.c_str());
    easy.add<CURLOPT_POSTFIELDS>(s.c_str());

    return perform();
}

std::pair<long, Json::Value> Portal::post(
        std::string const & url, 
        std::string const & acc_token,
        std::string const & refresh_token,
        Json::Value const & params )
{
    curl::curl_header header;
    header.add("Content-Type:application/json");
    header.add("Authorization: bearer " + acc_token);
    header.add("Refresh: " + refresh_token);

    easy.add<CURLOPT_HTTPHEADER>(header.get());

    std::string s = writer.write(params) + " ";

    easy.add<CURLOPT_URL>(url.c_str());
    easy.add<CURLOPT_POSTFIELDS>(s.c_str());

    return perform();
}

std::pair<long, Json::Value> Portal::put(
        std::string const & url, 
        std::string const & acc_token,
        std::string const & refresh_token )
{
    curl::curl_header header;
    header.add("Content-Type:application/json");
    header.add("Authorization: bearer " + acc_token);
    header.add("Refresh: " + refresh_token);

    easy.add<CURLOPT_HTTPHEADER>(header.get());

    easy.add<CURLOPT_URL>(url.c_str());
    easy.add<CURLOPT_CUSTOMREQUEST>("PUT");

    return perform();
}

std::pair<long, Json::Value> Portal::del(
        std::string const & url, 
        std::string const & acc_token,
        std::string const & refresh_token )
{
    curl::curl_header header;
    header.add("Content-Type:application/json");
    header.add("Authorization: bearer " + acc_token);
    header.add("Refresh: " + refresh_token);

    easy.add<CURLOPT_HTTPHEADER>(header.get());

    easy.add<CURLOPT_URL>(url.c_str());
    easy.add<CURLOPT_CUSTOMREQUEST>("DELETE");

    return perform();
}



#include "wan.h"

#include "utils/utils.h"
#include "utils/json/json.h"

#include <thread>

using namespace utils;

WAN::WAN(Json::Value const & wan, Portal & portal)
: token()
, p(portal)
, auth_url(wan["sense"]["auth"].asString())
, base_url(wan["sense"]["host"].asString())
, username(wan["sense"]["username"].asString())
, password(wan["sense"]["password"].asString())
, client_id(wan["sense"]["client_id"].asString())
, client_secret(wan["sense"]["client_secret"].asString())
, configured(!auth_url.empty() && !base_url.empty())
{
    slog() 
        << "WAN configured with: \n"
        << "        auth url:  " << auth_url << "\n"
        << "        base url:  " << base_url << "\n";
}

void WAN::get_token()
{
    std::string params;
    params += "grant_type=password";
    params += "&client_id=" + client_id;
    params += "&username=" + username;
    params += "&password=" + password;
    params += "&client_secret=" + client_secret;

    auto res = p.post_form( auth_url, params );

    token.acc = res.second["access_token"].asString();
    token.ref = res.second["refresh_token"].asString();
}

wan_create_res_t WAN::create(
        std::string const & src_stp,
        std::string const & dst_stp,
        std::string const & src_vlan_s,
        std::string const & dst_vlan_s )
{
    get_token();

    Json::Value i1;
    i1["service_type"] = "Multi-Path P2P VLAN";
    i1["service_alias"] = "SENSE.Demo.X2";

    i1["connections"][0]["name"] = "connection 1";
    i1["connections"][0]["terminals"][0]["uri"]    = src_stp;
    i1["connections"][0]["terminals"][0]["label"]  = src_vlan_s;
    i1["connections"][0]["terminals"][1]["uri"]    = dst_stp;
    i1["connections"][0]["terminals"][1]["label"]  = dst_vlan_s;
    i1["connections"][0]["bandwidth"]["qos_class"] = "guaranteedCapped";

    i1["queries"][0]["ask"] = "maximum-bandwidth";
    i1["queries"][0]["options"][0]["name"] = "connection 1";

    i1["queries"][1]["ask"] = "extract-result-values";
    i1["queries"][1]["options"][0]["sparql"] = "SELECT DISTINCT ?port ?vlan WHERE {?port nml:hasBidirectionalPort ?vlanPort. ?vlanPort nml:hasLabel ?vlanLabel. ?vlanLabel nml:value ?vlan. FILTER NOT EXISTS {?port a mrs:SwitchingSubnet.}}";

    slog() << "wan create: " << i1;

    auto r1 = p.post( base_url, token.acc, token.ref, i1 );
    slog() << "wan create: " << r1.first << ", " << r1.second;

    // res object
    wan_create_res_t res{r1.second["service_uuid"].asString(), 0, 0, 0};

    // loop through queries
    for (auto const & query : r1.second["queries"])
    {
        if (query["asked"].asString() == "maximum-bandwidth")
        {
            try
            {
                res.max_bw = std::stod(query["results"][0]["bandwidth"].asString());
            }
            catch (...)
            {
                slog(s_error) << "invalid return max_bw";
                res.max_bw = 0.0;
            }
        }
        else if (query["asked"].asString() == "extract-result-values")
        {
            for (auto const & r : query["results"])
            {
                if (r["port"].asString() == src_stp) 
                {
                    try 
                    { 
                        res.src_vlan = std::stoi(r["vlan"].asString());
                    } 
                    catch (...) 
                    {
                        slog(s_error) << "invalid return vlan id";
                        res.src_vlan = 0;
                    }
                }
                else if (r["port"].asString() == dst_stp)
                {
                    try 
                    { 
                        res.dst_vlan = std::stoi(r["vlan"].asString());
                    } 
                    catch (...) 
                    {
                        slog(s_error) << "invalid return vlan id";
                        res.dst_vlan = 0;
                    }
                }
            }
        }
    }

    return res;
}

bool WAN::reserve(
        std::string const & uuid,
        std::string const & src_stp,
        std::string const & dst_stp,
        int src_vlan,
        int dst_vlan,
        double bandwidth )
{
    get_token();

    std::string src_vlan_s = (src_vlan == 0) ? "any" : std::to_string(src_vlan);
    std::string dst_vlan_s = (dst_vlan == 0) ? "any" : std::to_string(dst_vlan);

    Json::Value i2;
    i2["service_type"] = "Multi-Path P2P VLAN";
    i2["service_alias"] = "SENSE.Demo.X2";

    i2["connections"][0]["name"] = "connection 1";
    i2["connections"][0]["terminals"][0]["uri"]    = src_stp;
    i2["connections"][0]["terminals"][0]["label"]  = src_vlan_s;
    i2["connections"][0]["terminals"][1]["uri"]    = dst_stp;
    i2["connections"][0]["terminals"][1]["label"]  = dst_vlan_s;

    i2["connections"][0]["bandwidth"]["qos_class"] = "guaranteedCapped";
    i2["connections"][0]["bandwidth"]["capacity"]  = "10000";

    slog() << "wan reserve: " << i2;

    auto r2 = p.post( base_url + "/" + uuid + "/reserve", token.acc, token.ref, i2 );
    slog() << "wan reserve: " << r2.first << ", " << r2.second;

    return r2.first == 201;
}

bool WAN::commit(std::string const & uuid)
{
    get_token();
    auto r3 = p.put( base_url + "/" + uuid + "/commit", token.acc, token.ref );
    slog() << "wan commit: " << r3.first << ", " << r3.second;
    return r3.first == 200;
}

bool WAN::release(std::string const & uuid, int max_retry)
{
    int retry = 0;

    while(++retry < max_retry)
    {
        get_token();
        auto r4 = p.put( base_url + "/" + uuid + "/release", token.acc, token.ref );

        slog() << "wan release: " << r4.first << ", " << r4.second;
        if (r4.first == 200) return true;

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    slog() << "failed..."; 
    return false;
}

bool WAN::terminate(std::string const & uuid, int max_retry)
{
    int retry = 0;

    while(++retry < max_retry)
    {
        get_token();
        auto r5 = p.put( base_url + "/" + uuid + "/terminate", token.acc, token.ref );

        slog() << "wan terminate: " << r5.first << ", " << r5.second;
        if (r5.first == 200) return true;

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    slog() << "failed..."; 
    return false;
}

bool WAN::del(std::string const & uuid, int max_retry)
{
    int retry = 0;

    while(++retry < max_retry)
    {
        get_token();
        auto r6 = p.del( base_url + "/" + uuid, token.acc, token.ref );

        slog() << "wan delete: " << r6.first << ", " << r6.second;
        if (r6.first == 200) return true;

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    slog() << "failed..."; 
    return false;
}



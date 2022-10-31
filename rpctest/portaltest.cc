
#include "utils/utils.h"
#include "utils/portal.h"

#include "../server/wan.h"

#include <unistd.h>
#include <thread>

using namespace utils;

void call_raw_portal()
{
    Portal p;

    std::string params;
    params += "grant_type=password";
    params += "&client_id=StackV";
    params += "&username=fermilab";
    params += "&password=SENSE-SC18";
    params += "&client_secret=ae53fbea-8812-4c13-918f-0065a1550b7c";

    slog() << "---------------------- token --------------------------";
    auto res = p.post_form(
            "https://k152.maxgigapop.net:8543/auth/realms/StackV/protocol/openid-connect/token", 
            params );

    slog() << res.first << ", " << res.second;

    std::string acc = res.second["access_token"].asString();
    std::string ref = res.second["refresh_token"].asString();

    slog() << "---------------------- create --------------------------";

    std::string base_url = 
            "https://179-132.research.maxgigapop.net:8443/StackV-web/restapi/sense/service";

    Json::Value i1;
    i1["service_type"] = "Multi-Path P2P VLAN";
    i1["service_alias"] = "SENSE.Demo.X2";

    i1["connections"][0]["name"] = "connection 1";
    i1["connections"][0]["terminals"][0]["uri"] = "urn:ogf:network:es.net:2013::wash-cr5:6_1_1:+";
    i1["connections"][0]["terminals"][0]["label"] = "3618";
    i1["connections"][0]["terminals"][1]["uri"] = "urn:ogf:network:es.net:2013::chic-cr5:3_2_1:+";
    i1["connections"][0]["terminals"][1]["label"] = "3618";
    i1["connections"][0]["bandwidth"]["qos_class"] = "guaranteedCapped";

    i1["queries"][0]["ask"] = "maximum-bandwidth";
    i1["queries"][0]["options"][0]["name"] = "connection 1";

    auto r1 = p.post( base_url, acc, ref, i1 );
    slog() << r1.first << ", " << r1.second;

    std::string uuid = r1.second["service_uuid"].asString();

    std::this_thread::sleep_for(std::chrono::seconds(10));

    slog() << "---------------------- reserve --------------------------";

    Json::Value i2;
    i2["service_type"] = "Multi-Path P2P VLAN";
    i2["service_alias"] = "SENSE.Demo.X2";
    i2["connections"][0]["name"] = "connection 1";
    i2["connections"][0]["terminals"][0]["uri"] = "urn:ogf:network:es.net:2013::wash-cr5:6_1_1:+";
    i2["connections"][0]["terminals"][0]["label"] = "3618";
    i2["connections"][0]["terminals"][1]["uri"] = "urn:ogf:network:es.net:2013::chic-cr5:3_2_1:+";
    i2["connections"][0]["terminals"][1]["label"] = "3618";
    i2["connections"][0]["bandwidth"]["qos_class"] = "guaranteedCapped";
    i2["connections"][0]["bandwidth"]["capacity"] = "10000";

    auto r2 = p.post( base_url + "/" + uuid + "/reserve", acc, ref, i2 );
    slog() << r2.first << ", " << r2.second;

    std::this_thread::sleep_for(std::chrono::seconds(10));

    slog() << "---------------------- commit --------------------------";

    auto r3 = p.put( base_url + "/" + uuid + "/commit", acc, ref );
    slog() << r3.first << ", " << r3.second;

    std::this_thread::sleep_for(std::chrono::seconds(10));

    slog() << "---------------------- release --------------------------";

    auto r4 = p.put( base_url + "/" + uuid + "/release", acc, ref );
    slog() << r4.first << ", " << r4.second;

    std::this_thread::sleep_for(std::chrono::seconds(30));

    slog() << "---------------------- terminate --------------------------";

    auto r5 = p.put( base_url + "/" + uuid + "/terminate", acc, ref );
    slog() << r5.first << ", " << r5.second;

    std::this_thread::sleep_for(std::chrono::seconds(20));

    slog() << "---------------------- delete --------------------------";

    auto r6 = p.del( base_url + "/" + uuid, acc, ref );
    slog() << r6.first << ", " << r6.second;
}

void call_wan(WAN & wan)
{
#if 0
    std::string src_stp = "urn:ogf:network:es.net:2013::wash-cr5:6_1_1:+";
    std::string dst_stp = "urn:ogf:network:es.net:2013::chic-cr5:3_2_1:+";

    int src_vlan = 3618;
    int dst_vlan = 3618;

    auto uuid = wan.create(src_stp, dst_stp, src_vlan, dst_vlan);

    slog() << "uuid = " << uuid;

    if (uuid.empty()) return;

    std::this_thread::sleep_for(std::chrono::seconds(10));

    auto r2 = wan.reserve(uuid, src_stp, dst_stp, src_vlan, dst_vlan, 10000);
    if (!r2) { wan.del(uuid); return; }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    auto r3 = wan.commit(uuid);
    if (!r3) { wan.del(uuid); return; }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    auto r4 = wan.release(uuid);
    if (!r4) { return; }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    auto r5 = wan.terminate(uuid);
    if (!r5) { return; }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    auto r6 = wan.del(uuid);
    if (!r6) { return; }
#endif

    slog() << "disabled";
    return;
}

void call_wan(WAN & wan, int step, std::string const & uuid)
{
    std::string src_stp = "urn:ogf:network:es.net:2013::wash-cr5:6_1_1:+";
    std::string dst_stp = "urn:ogf:network:es.net:2013::chic-cr5:3_2_1:+";

    std::string src_vlan_s = "any";//3618;
    std::string dst_vlan_s = "any";//3618;

    int src_vlan = 3618;
    int dst_vlan = 3618;

    wan_create_res_t cres;

    switch(step)
    {
        case 1:
            cres = wan.create(src_stp, dst_stp, src_vlan_s, dst_vlan_s);

            slog() << "uuid = " << cres.uuid
                << ", src_vlan_id = " << cres.src_vlan
                << ", dst_vlan_id = " << cres.dst_vlan
                << ", max_bw = " << cres.max_bw
                ;

            break;

        case 2:
            slog() << wan.reserve(uuid, src_stp, dst_stp, src_vlan, dst_vlan, 10000);
            break;

        case 3:
            slog() << wan.commit(uuid);
            break;

        case 4:
            slog() << wan.release(uuid);
            break;

        case 5:
            slog() << wan.terminate(uuid);
            break;

        case 6:
            slog() << wan.del(uuid);
            break;

        default:
            break;
    }
}

void print_usage(std::string const & app)
{
    slog() << app << " -a action [uuid]";
    slog() << "available actions: create, reserve, commit, release, terminate, delete";
}

int main(int argc, char ** argv)
{
    std::string action;
    int c;

    while ( (c=getopt(argc, argv, "a:")) != -1 )
    {
        switch(c)
        {
            case 'a':
                action = optarg;
                break;
            case '?':
                print_usage(argv[0]);
                exit(1);
            default:
                break;
        }
    }

    slog() << action;

    int step = -1;

    if (action.empty())             step = 0;
    else if (action == "create")    step = 1;
    else if (action == "reserve")   step = 2;
    else if (action == "commit")    step = 3;
    else if (action == "release")   step = 4;
    else if (action == "terminate") step = 5;
    else if (action == "delete")    step = 6;
    else
    {
        std::cout << "invalid action!\n";
        print_usage(argv[0]);
        exit(1);
    }

    std::string uuid;

    if (step > 1 && optind == argc)
    {
        std::cout << "no uuid provided!\n";
        print_usage(argv[0]);
        exit(1);
    }
    else if (step > 1)
    {
        uuid = argv[optind];
    }

    Json::Value sense;
    sense["sense"]["auth"] = "https://k152.maxgigapop.net:8543/auth/realms/StackV/protocol/openid-connect/token";
    sense["sense"]["host"] = "https://179-132.research.maxgigapop.net:8443/StackV-web/restapi/sense/service";
    sense["sense"]["username"] = "fermilab";
    sense["sense"]["password"] = "SENSE-SC18";
    sense["sense"]["client_id"] = "StackV";
    sense["sense"]["client_secret"] =  "ae53fbea-8812-4c13-918f-0065a1550b7c";

    Portal p;

    WAN wan(sense, p);

    if (step == 0)
    {
        call_wan(wan);
    }
    else
    {
        call_wan(wan, step, uuid);
    }
}


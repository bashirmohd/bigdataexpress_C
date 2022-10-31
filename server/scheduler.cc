
#include "scheduler.h"
#include "utils/utils.h"
#include "utils/proxy-cert.h"

#include <iostream>

namespace
{
    void mdtmftp()
    {
    }
}

using namespace std;
using namespace utils;

Scheduler::Scheduler(Conf const & conf, SiteStore & store, RpcClient & rpc)
: io()
, sig(io, SIGINT, SIGTERM)
, evs()
, strands()

//, ld_type(conf.get<string>("bde.agents.ld.type"))
//, ld_queue(conf.get<string>("bde.agents.ld.queue", "mdtm-listen"))
, ld_auth(conf.get<string>("bde.agents.ld.authentication", "password") == "password" ? ld_auth_t::password : ld_auth_t::cert)
, ld_auth_usr(conf.get<string>("bde.agents.ld.auth_username", ""))
, ld_auth_pwd(conf.get<string>("bde.agents.ld.auth_password", ""))
, ld_file(conf.get<bool>("bde.agents.ld.write_command_files", false))
, ld_checksum(conf.get<bool>("bde.agents.ld.checksum_enable", false))
, ld_checksum_alg(conf.get<string>("bde.agents.ld.checksum_algorithm", "sha1"))
, ld_group_size_str(conf.get<string>("bde.agents.ld.group_size", "20GB"))
, ld_group_size(20000000000ull)

, cmd_path(conf.get<string>("bde.cmd_path", ""))

, net_conf(conf["bde"]["network"])

, inactive_timeout_ms(300000)

, store(store)
, rpc(rpc)
, portal()
, wan(conf["bde"]["network"]["wan_dynamic"], portal)
, jm()
, rm(jm, store)
, tsdb("localhost", 8086, "bde")
, tsrate(tsdb, "job", {"rate"}, {"group", "id"})
, tsjobs(tsdb, "jobs", {"active_be", "active_rt", "waiting", "error"}, {})
, tsschedule(tsdb, "schedule", {"scheduled"}, {})
, ts_site_txrx(tsdb, "site_txrx", {"tx_bytes", "rx_bytes"}, {"id"})
, site_txrx()
, work_threads(8)
, exceptions()
{
    sig.async_wait([this](asio::error_code const & ec, int signal) {
        utils::slog() << "signal captured.";
        io.stop();
    });

    if (ld_checksum_alg != "md5" && 
        ld_checksum_alg != "sha" &&
        ld_checksum_alg != "sha1" &&
        ld_checksum_alg != "sha224" &&
        ld_checksum_alg != "sha256" &&
        ld_checksum_alg != "sha384" &&
        ld_checksum_alg != "sha512")
    {
        throw std::runtime_error("unsupported checksum algorithm: " + ld_checksum_alg);
    }

    if (ld_group_size_str.size() < 2)
    {
        throw std::runtime_error("invalid group_size: " + ld_group_size_str);
    }
    else
    {
        std::string s_num = ld_group_size_str.substr(0, ld_group_size_str.size()-2);
        std::string s_unit = ld_group_size_str.substr(ld_group_size_str.size()-2, 2);

        uint64_t num = 1;
        uint64_t unit = 1;

        num = stoll(s_num);

        switch(s_unit[0])
        {
            case 'K': case 'k': unit = 1024ull; break;
            case 'M': case 'm': unit = 1024ull*1024ull; break;
            case 'G': case 'g': unit = 1024ull*1024ull*1024ull; break;
            case 'T': case 't': unit = 1024ull*1024ull*1024ull*1024ull; break;
            default: throw std::runtime_error("invalid unit: " + s_unit);
        }

        ld_group_size = num * unit;
    }

    slog() << "Scheduler uses following configurations:\n"
        //<< "\tLaunching Daemon type:            " << ld_type << "\n"
        //<< "\tLaunching Daemon queue name:      " << ld_queue << "\n"
        << "\tLaunching Daemon auth method:     " << (ld_auth == ld_auth_t::password ? "password" : "certificate") << "\n"
        << "\tLaunching Daemon write cmd:       " << (ld_file ? "true" : "false") << "\n"
        << "\tChecksum verification:            " << (ld_checksum ? "true" : "false") << "\n"
        << "\tChecksum algorithm:               " << ld_checksum_alg << "\n"
        << "\tGroup size:                       " << ld_group_size << " (" << ld_group_size_str << ")\n"
        << "\tNetwork configurations:           " << net_conf << "\n";
}

Scheduler::~Scheduler()
{
}

void Scheduler::run()
{
    // init the site tx rx
    Json::Value sites = store.get_sites();
    for (auto const & site : sites) 
    {
        int64_t tx_bytes = site["tx_bytes"].asInt64();
        int64_t rx_bytes = site["rx_bytes"].asInt64();

        site_txrx[site["_id"]["$oid"].asString()] = { tx_bytes, rx_bytes };
        slog() << "site " << site["abbrev"].asString() << ": tx_bytes = " << tx_bytes << ", rx_bytes = " << rx_bytes;
    }

    // schedule event
    add_event(std::bind(&Scheduler::schedule, this), chrono::milliseconds(4000));
    add_event(std::bind(&Scheduler::poll_rate, this), chrono::milliseconds(5000));

    // start the service
    for (auto & t : work_threads)
    {
        t = std::thread([this](){ 
            try
            {
                io.run();
            }
            catch(...)
            {
                exceptions.push(std::current_exception());
                io.stop();
            }
        });
    }

    // main thread
    //io.run();

    // wait for join
    for (auto & t : work_threads)
        t.join();

    if (!exceptions.empty())
    {
        auto ep = exceptions.front();
        exceptions.pop();
        std::rethrow_exception(ep);
    }

    // exiting
    //store.delete_all("stream");
}

void Scheduler::add_event(
        std::function<std::chrono::milliseconds()> const & handler,
        std::chrono::milliseconds const & delay )
{
    // locatioin of the new event handler
    int n = evs.size();

    // save the event handler
    evs.emplace_back( make_pair(
        asio::steady_timer(io, delay),
        [this, n, handler](asio::error_code const & ec) {
            auto intv = handler();
            auto & ev = evs[n];
            ev.first.expires_from_now(intv);
            ev.first.async_wait(ev.second);
        }
    ));

    // start wait
    evs[n].first.async_wait(evs[n].second);
}

void Scheduler::dispatch( std::function<void()> const & handler, std::string const & tag )
{
    if (tag.empty())
    {
        io.post(handler);
    }
    else
    {
        auto res = strands.emplace(tag, asio::io_service::strand(io));
        res.first->second.post(handler);
    }
}

void Scheduler::resource_construct(
        bool online, 
        std::string const & cid, 
        std::string const & queue,
        Json::Value const & modules)
{
    dispatch([this, online, cid, queue, modules](){ 
        slog() << "agent: " << cid << ", online: " << online; 

        if (online) rm.add_modules(cid, queue, modules);
        else        rm.del_modules(cid, modules);

    }, "resource");
}

void Scheduler::add_raw_job(std::string const & id, res_cb_t const & cb)
{
    dispatch([this, id, cb](){
        bootstrap_transfer_job(id);
    });

    cb(Json::Value());
}

Json::Value Scheduler::find_best_effort_dtn_pair(
        std::string const & src_storage_id,
        std::string const & dst_storage_id,
        Json::Value const & src_site_info,
        Json::Value const & dst_site_info )
{
    auto src_url = src_site_info["url"].asString();
    auto dst_url = dst_site_info["url"].asString();

    auto src_abbrev = src_site_info["abbrev"].asString();
    auto dst_abbrev = dst_site_info["abbrev"].asString();

    // query for the avaiable best effort dtns from both source and dst sites
    Json::Value params;

    params["cmd"] = "get_available_best_effort_dtn";
    params["storage"] = src_storage_id;
    params["remote"] = dst_abbrev;

    auto r_src = portal.post(std::string("https://") + src_url + "/bde/command", params);

    slog() << "get_available_best_effort_dtn from source site " << src_abbrev;
    slog() << r_src.second;

    params["storage"] = dst_storage_id;
    params["remote"] = src_abbrev;

    auto r_dst = portal.post(std::string("https://") + dst_url + "/bde/command", params);

    slog() << "get_available_best_effort_dtn from destination site " << dst_abbrev;
    slog() << r_dst.second;

    // the final return value
    Json::Value res;

    // check the portal responses
    if (r_src.first != 200)
    {
        res["code"] = 100;
        res["error"] = "source site portal unreachable";
        return res;
    }
    else if (r_dst.first != 200)
    {
        res["code"] = 101;
        res["error"] = "destination site portal unreachable";
        return res;
    }
    else
    {
        res["code"] = 0;
        res["error"] = "success";
    }

#if 0
    // find DTN pair following the rule:
    //   (src_sdn: SDN support at source site)
    //   (dst_sdn: SDN support at destination site)
    //
    // 1. src_sdn = false or dst_sdn = false: 
    //         pick public_dtns from both source and dst.
    //         if any of the public_dtns list is empty, report error.
    // 2. src_sdn = true and dst_sdn = true: 
    //         try pick both from private dtns, if not available, pick both from publics
    //         if no private pair or public pair available, report error.

    // case 1: no sdn at either site
    if (r_src.second["sdn"].asBool() == false || r_dst.second["sdn"].asBool() == false)
    {
        if (r_src.second["public_dtns"].empty() || r_dst.second["public_dtns"].empty())
        {
            // failed to find a pair of public DTNs
            slog() << "unable to find public DTN pairs.";

            res["code"] = 200;
            res["error"] = "unable to find a public DTN pair for the job";
        }
        else
        {
            res["src_dtn"] = r_src.second["public_dtns"][0];
            res["dst_dtn"] = r_dst.second["public_dtns"][0];

            res["src_storage"] = r_src.second["storage"];
            res["src_storage"] = r_dst.second["storage"];

            res["sdn_setup"] = false;
        }
    }
    // case 2: both have sdn
    else 
    {
        // 2.1 find private pair
        if (!r_src.second["private_dtns"].empty() && !r_dst.second["private_dtns"].empty())
        {
            res["src_dtn"] = r_src.second["private_dtns"][0];
            res["dst_dtn"] = r_dst.second["private_dtns"][0];

            res["src_storage"] = r_src.second["storage"];
            res["src_storage"] = r_dst.second["storage"];

            res["sdn_setup"] = true;
        }
        // 2.2 or the public pair
        else if (!r_src.second["public_dtns"].empty() && !r_dst.second["public_dtns"].empty())
        {
            res["src_dtn"] = r_src.second["public_dtns"][0];
            res["dst_dtn"] = r_dst.second["public_dtns"][0];

            res["src_storage"] = r_src.second["storage"];
            res["src_storage"] = r_dst.second["storage"];

            res["sdn_setup"] = false;
        }
        // 2.3 still no pair -- error
        else
        {
            // failed to find a public pair of DTNs
            slog() << "unable to find either private or public DTN pairs for SDN supported sites.";

            res["code"] = 201;
            res["error"] = "unable to find either private or public DTN pairs for SDN enabled sites";
        }
    }
#endif

    // new rule:
    //
    // 1. private dtn pair first
    // 2. if not available, try the public pair
    // 3. both not available, then fail

    // private pair
    if (!r_src.second["private_dtns"].empty() && !r_dst.second["private_dtns"].empty())
    {
        res["src_dtn"] = r_src.second["private_dtns"][0];
        res["dst_dtn"] = r_dst.second["private_dtns"][0];

        res["src_storage"] = r_src.second["storage"];
        res["src_storage"] = r_dst.second["storage"];

        res["private"] = true;
    }
    // or the public pair
    else if (!r_src.second["public_dtns"].empty() && !r_dst.second["public_dtns"].empty())
    {
        res["src_dtn"] = r_src.second["public_dtns"][0];
        res["dst_dtn"] = r_dst.second["public_dtns"][0];

        res["src_storage"] = r_src.second["storage"];
        res["src_storage"] = r_dst.second["storage"];

        res["private"] = false;
    }
    // 2.3 still no pair -- error
    else
    {
        // failed to find a public pair of DTNs
        slog() << "unable to find either private or public DTN pairs";

        res["code"] = 201;
        res["error"] = "unable to find either private or public DTN pairs";
    }

    // fill in the site url for source and dest
    if (res["code"].asInt() == 0)
    {
        res["src_dtn"]["site"] = src_site_info;
        res["dst_dtn"]["site"] = dst_site_info;
    }

    return res;
}

Json::Value Scheduler::setup_wan_path(
        Json::Value const & src_dtn,
        Json::Value const & dst_dtn,
        double bandwidth,
        std::string const & wan_src_vlan_s,
        std::string const & wan_dst_vlan_s )
{
    Json::Value res;

    res["code"]  = 0;
    res["error"] = "";

    std::string src_stp = src_dtn["stp"]["name"].asString();
    std::string dst_stp = dst_dtn["stp"]["name"].asString();

    // create
    auto cres = wan.create( src_stp, dst_stp,
            wan_src_vlan_s, wan_dst_vlan_s );

    auto const & uuid = cres.uuid;

    slog() << "WAN path created, uuid = " << uuid
        << ", src vlan_id = " << cres.src_vlan
        << ", dst vlan_id = " << cres.dst_vlan
        << ", maximum bandwidth = " << cres.max_bw
        ;

    // creation failure
    if (uuid.empty())
    {
        slog() << "invalid uuid, exiting...";

        res["wan_uuid"] = "";
        res["code"] = 300;
        res["error"] = "WAN path creation failure";

        return res;
    }

    // store the uuid
    res["wan_uuid"] = uuid;
    res["wan_src_vlan"] = cres.src_vlan;
    res["wan_dst_vlan"] = cres.dst_vlan;
    res["wan_max_bw"] = cres.max_bw;

    // reserve
    slog() << "WAN path reserving...";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    bool r1 = wan.reserve( uuid, src_stp, dst_stp,
            cres.src_vlan, cres.dst_vlan, bandwidth );

    // reserve failure
    if (!r1)
    {
        slog() << "WAN reservation failure, exiting...";

        wan.del(uuid);

        res["wan_uuid"] = "";
        res["code"] = 301;
        res["error"] = "WAN path reservation faiure";

        return res;
    }

    // commit
    slog() << "WAN path commiting...";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    bool r2 = wan.commit(uuid);

    // commit failure
    if (!r2)
    {
        slog() << "WAN commit failure, exiting...";

        wan.del(uuid);

        res["wan_uuid"] = "";
        res["code"] = 302;
        res["error"] = "WAN path commit faiure";

        return res;
    }

    slog() << "done.";

    return res;


#if 0
    bool wan_selected = false;

    auto sip = src_dtn["data_ip"].asString();
    auto dip = dst_dtn["data_ip"].asString();

    if (sip == "10.36.19.16" && dip == "10.36.19.11")
    {
        wan_selected = true;
        res["wan"]["intent1"] = "intent-1-3619.json";
        res["wan"]["intent2"] = "intent-2-3619.json";
    }
    else if (sip == "10.36.19.16" && dip == "10.36.18.15")
    {
        wan_selected = true;
        res["wan"]["intent1"] = "intent-1-3618.json";
        res["wan"]["intent2"] = "intent-2-3618.json";
    }
    else
    {
        wan_selected = false;
    }

    if (wan_enable && wan_selected)
    {
        res["wan_setup"] = true;

        auto uuid = wan.create(res["wan"]["intent1"].asString());
        slog() << "WAN path created, uuid = " << uuid;

        if (uuid.empty())
        {
            res["wan"]["uuid"] = "";

            // invalid uuid
            slog() << "invalid uuid, exiting...";

            res["code"] = 300;
            res["error"] = "WAN path creation failure";
        }
        else
        {
            // store the uuid
            res["wan"]["uuid"] = uuid;

            // following steps
            slog() << "WAN path reserving...";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            wan.reserve(uuid, res["wan"]["intent2"].asString());
            slog() << "done.";

            slog() << "WAN path commiting...";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            wan.commit(uuid);
            slog() << "done.";
        }
    }
#endif

    return res;
}

Json::Value Scheduler::prepare_flow(
        Json::Value const & rjob,
        Json::Value const & src_dtn,
        Json::Value const & dst_dtn,
        Json::Value const & src_storage,
        Json::Value const & dst_storage,
        double max_rate )
{
    // prepare the flow object, and mdtm launching command object
    Json::Value flow;
    Json::Value rpc_json;

    auto flow_task = utils::guid();

    flow["state"] = (int)flow_state::scheduling;
    flow["type"]  = "best_effort";
    flow["task"]  = flow_task;

    flow["srcstorage"] = src_storage;
    flow["dststorage"] = dst_storage;

    flow["srcdtn"] = src_dtn;
    flow["dstdtn"] = dst_dtn;

    flow["max_rate"] = max_rate;
    flow["current_rate"] = 0.0;
    flow["avg_rate"] = 0.0;
    flow["tx_bytes"] = 0;

    flow["start"] = (Json::Value::Int64)(std::chrono::duration_cast<std::chrono::milliseconds>( 
            std::chrono::system_clock::now().time_since_epoch()).count());

    // hand over the block to MDTM launching daemon
    slog() << "preparing mdtm launching command";

    rpc_json["cmd"] = "launch";
    rpc_json["target"] = "launcher-agent";
    rpc_json["params"]["task"] = flow["task"];

    rpc_json["params"]["ip_src"] = src_dtn["ip"]; //sjob["srcdtn_addr"];
    rpc_json["params"]["ip_dst"] = dst_dtn["ip"]; //sjob["dstdtn_addr"];

    rpc_json["params"]["port_src"] = src_dtn["dtp_ctrl_port"];
    rpc_json["params"]["port_dst"] = dst_dtn["dtp_ctrl_port"];

    rpc_json["params"]["port_range_src"]["min"] = 5000;
    rpc_json["params"]["port_range_src"]["max"] = 6000;
    rpc_json["params"]["port_range_dst"]["min"] = 5000;
    rpc_json["params"]["port_range_dst"]["max"] = 6000;

    // the launch command saved in the flow shouldn't contain any files
    rpc_json["params"]["files"] = Json::arrayValue;

    // authentication
    if (ld_auth == ld_auth_t::password)
    {
        // username and password authentication
        rpc_json["params"]["usr_src"] = ld_auth_usr;
        rpc_json["params"]["pwd_src"] = ld_auth_pwd;

        rpc_json["params"]["usr_dst"] = ld_auth_usr;
        rpc_json["params"]["pwd_dst"] = ld_auth_pwd;

        rpc_json["params"]["cert_src"] = "temp_src_cert";
        rpc_json["params"]["cert_dst"] = "temp_dst_cert";

        rpc_json["params"]["key_src"] = "temp_src_key";
        rpc_json["params"]["key_dst"] = "temp_dst_key";

        rpc_json["params"]["url_scheme_src"] = 0;
        rpc_json["params"]["url_scheme_dst"] = 0;
    }
    else if (ld_auth == ld_auth_t::cert)
    {
        // proxy certificate authentication
        std::string cred_src = rjob["src_certs"]["proxy_cert"].asString();
        cred_src += rjob["src_certs"]["proxy_key"].asString();
        cred_src += rjob["src_certs"]["user_cert"].asString();

        std::string cred_dst = rjob["dst_certs"]["proxy_cert"].asString();
        cred_dst += rjob["dst_certs"]["proxy_key"].asString();
        cred_dst += rjob["dst_certs"]["user_cert"].asString();

        rpc_json["params"]["proxy_credential_src"] = cred_src;
        rpc_json["params"]["proxy_credential_dst"] = cred_dst;

        rpc_json["params"]["url_scheme_src"] = 1;
        rpc_json["params"]["url_scheme_dst"] = 1;
    }

    flow["launching"] = rpc_json;

    return flow;
}

void Scheduler::schedule_clean_up(
        std::string const & rid,
        std::string const & sid,
        std::string const & error,
        //bool sdn_setup, 
        //bool wan_setup,
        Json::Value const & src_dtn,
        Json::Value const & dst_dtn )
{
    //log
    slog() << "scheduling error: " << error;
    store.update_sjob_stage(sid, "teardown.general", sjob_stage::working);

    // path teardown
    if (!src_dtn.empty() && !dst_dtn.empty())
    {
        int res_sdn = teardown_sdn_path(sid, src_dtn, dst_dtn);
        int res_wan = teardown_wan_path(sid, src_dtn["wan_uuid"].asString());

        auto stage = (res_sdn || res_wan) ? sjob_stage::error : sjob_stage::success;
        store.update_sjob_stage(sid, "teardown.general", stage);
    }
    else
    {
        store.update_sjob_stage(sid, "teardown.general", sjob_stage::success);
    }

    // update state
    store.update_sjob_state(sid, sjob_state::error);
    store.update_rawjob_state(rid, rawjob_state::error);

    // update error message
    store.update_collection_field("sjob", sid, "error", error);
    store.update_collection_field("rawjob", rid, "error", error);
}

std::chrono::milliseconds Scheduler::schedule()
{
    //slog() << "start job scheduling";

    // get the first job of type reserved and doesnt have base path started, sorted by the submitted time
    //
    // create_stream (job, base_rate, inf_time);
    //
    // {
    //   auto block = store.get_a_block(job);
    //   storage, dtn, virtual_ip, vlan_id, port, time=inf, state=init
    //   sid = store.insert_stream(storage, dtn, ip, vlan, port, time)
    //   rpc.call(local_dtn, sid, ip, vlan)
    //   rpc.call(remote_dtn, sid, ip, vlan)
    //   rpc.call(local_dtn, ping_remote_dtn)
    //   rpc.call(mdtm, sid, block)
    // }
    //
    // return;
    //
    // for (storage: all_storages)
    // {
    //     {reserved_load, measured_load} = rpc.call(sys_load, storage);
    //
    //     if (reserved_load < 0.8)
    //     {
    //         rank all reserve jobs
    //         select job wit hightest rank
    //
    //         do
    //         {
    //             res = create_stream(job, min(max_rate, available_rate), time_slot);
    //             if (res == false) job = job->next;
    //             else break;
    //         } while(true);
    //
    //         return;
    //     }
    //
    //     if (measured_load < 0.8)
    //     {
    //         rank all besteffort jobs
    //         select job with hightest rank
    //
    //         do
    //         {
    //             res = create_link(job, time_slot);
    //             if (res) break;
    //             job = job->next;
    //         } while (true);
    //     }
    // }
    //
    //     


    // schedule for the best effort jobs
    auto be_sjobs = store.get_active_best_effort_sjobs();
    //slog() << "best effort jobs " << be_sjobs;

    for (auto const & sjob : be_sjobs)
    {
        // get the number of max flows
        int max_flows = 1;

        // now we limit the max flow to 1 for each sjob
        // if (sjob.isMember("max_flows") && sjob["max_flows"].asInt()>=1) max_flows = sjob["max_flows"].asInt();

        // reach the limit?
        if (sjob["flows"].size() >= max_flows) continue;

        // we will now schedule a new flow for this sjob
        // first find the first idle block
        for (auto const & block : sjob["blocks"])
        {
            if (block["state"].asInt() != (int)block_state::waiting) continue;

            // now this is the job we are going to dispatch with the current block
            slog() << "best effort job to be dispatched: " << sjob["_id"]["$oid"] << ", block id = " << block["id"];

            // sjob id and rawjob id
            std::string sid = sjob["_id"]["$oid"].asString();
            std::string rid = sjob["rawjob"]["$oid"].asString();

            // first retrieve the rawjob that this sjob belongs to
            auto rjob = store.get_rawjob_from_id(rid);

            // site urls
            auto src_site_info = store.get_site_info(sjob["srcsite"]["$oid"].asString());
            auto dst_site_info = store.get_site_info(sjob["dstsite"]["$oid"].asString());

            //-----------------------------------------------------------------------------------
            // query for the avaiable best effort dtns from both source and dst sites
            //   res_dtn: {
            //     code: int, error: str, 
            //     private: bool,
            //     src_dtn: { ..., sdn_setup: bool, site: { } },
            //     dst_dtn: { ..., sdn_setup: bool, site: { } },
            //     src_storage: { ... },
            //     dst_storage: { ... }
            //   }
            store.update_sjob_state(sid, sjob_state::dtn_matching);
            store.update_sjob_stage(sid, "bootstrap.query", sjob_stage::success);
            store.update_sjob_stage(sid, "bootstrap.match", sjob_stage::working);

            auto res_dtn = find_best_effort_dtn_pair(
                    sjob["srcstorage"].asString(), 
                    sjob["dststorage"].asString(),
                    src_site_info, dst_site_info );

            slog() << "find_best_effort_dtn_pair: " << res_dtn;

            // DTN pairing error
            if (res_dtn["code"].asInt())
            {
                store.update_sjob_stage(sid, "bootstrap.match", sjob_stage::error);
                store.update_sjob_stage(sid, "bootstrap.general", sjob_stage::error);

                schedule_clean_up(rid, sid, res_dtn["error"].asString());
                break;
            }
            else
            {
                store.update_sjob_stage(sid, "bootstrap.match", sjob_stage::success);
                store.update_sjob_stage(sid, "bootstrap.general", sjob_stage::success);
            }

            // returned DTN info
            Json::Value src_dtn = std::move(res_dtn["src_dtn"]);
            Json::Value dst_dtn = std::move(res_dtn["dst_dtn"]);

            // storage info
            Json::Value src_storage = std::move(res_dtn["src_storage"]);
            Json::Value dst_storage = std::move(res_dtn["dst_storage"]);

            // start network handling
            store.update_sjob_stage(sid, "network.general", sjob_stage::working);

            // unified network handling, only for private dtn pairs
            //   res_net: {
            //     code: int, error: str,
            //     wan_uuid: str,
            //     src_sdn_path_id: int,
            //     dst_sdn_path_id: int
            //   }
            if (res_dtn["private"].asBool())
            {
                store.update_sjob_stage(sid, "network.planning", sjob_stage::working);

                auto res_net = network_handling(sid, src_dtn, dst_dtn);

                slog() << "network handling: " << res_net;

                src_dtn["wan_uuid"] = res_net["wan_uuid"];
                dst_dtn["wan_uuid"] = res_net["wan_uuid"];

                src_dtn["path_id"] = res_net["src_sdn_path_id"];
                dst_dtn["path_id"] = res_net["dst_sdn_path_id"];

                // network hanlding error
                if (res_net["code"].asInt())
                {
                    store.update_sjob_stage(sid, "network.general", sjob_stage::error);
                    schedule_clean_up(rid, sid, res_net["error"].asString(), src_dtn, dst_dtn);
                    break;
                }
            }
            else
            {
                slog() << "public DTN pair, no network handling needed";

                store.update_sjob_stage(sid, "network.planning", sjob_stage::success);
                store.update_sjob_stage(sid, "network.public", sjob_stage::success);
                store.update_sjob_stage(sid, "teardown.public", sjob_stage::pending);

                src_dtn["wan_uuid"] = "";
                dst_dtn["wan_uuid"] = "";

                src_dtn["path_id"] = -1;
                dst_dtn["path_id"] = -1;
            }

            //-----------------------------------------------------------------------------------
            // sleep before verifing the path
            store.update_sjob_state(sid, sjob_state::path_verification);
            store.update_sjob_stage(sid, "network.ping", sjob_stage::working);
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // verify
            auto verify_res = verify_path(src_dtn, dst_dtn);

            if (verify_res["code"].asInt() != 0)
            {
                store.update_sjob_stage(sid, "network.ping", sjob_stage::error);
                store.update_sjob_stage(sid, "network.general", sjob_stage::error);

                schedule_clean_up( rid, sid, verify_res["error"].asString(), 
                        /*sdn_setup, wan_setup, */src_dtn, dst_dtn );

                break;
            }
            else
            {
                store.update_sjob_stage(sid, "network.ping", sjob_stage::success);
                store.update_sjob_stage(sid, "network.general", sjob_stage::success);
            }

            //-----------------------------------------------------------------------------------
            // prepare the flow object
            Json::Value flow = prepare_flow(rjob, src_dtn, dst_dtn, src_storage, dst_storage, 10000);
            flow["block"] = block["id"];

            // insert the flow
            store.insert_flow(sid, flow);

            //-----------------------------------------------------------------------------------
            // mdtm launching command object
            Json::Value rpc_json = flow["launching"];
            Json::Value block_files = store.get_block_files(block["id"].asString());
            rpc_json["params"]["files"] = block_files["files"];
            rpc_json["params"]["mode"]  = block_files["mode"];

            // write launch daemon command files
            if (ld_file)
            {
                std::ofstream of(cmd_path + "cmd1.json");
                Json::StyledWriter writer;
                of << writer.write(rpc_json);
                of.close();
            }

            // log
            slog() << "flow scheduled";

            //-----------------------------------------------------------------------------------
            // and finally calls the MDTM to launch the transfer
            store.update_sjob_state(sid, sjob_state::launching);

            store.update_sjob_stage(sid, "transfer.general", sjob_stage::working);
            store.update_sjob_stage(sid, "transfer.launch", sjob_stage::working);

            auto res = rpc.call(rm.launcher().queue(), rpc_json, 20);

            //slog() << "mdtm launch command called, cmd: " << rpc_json;
            slog() << "mdtm launch res: " << res;

            // update sjob state and rawjob state
            if (res.isMember("error"))
            {
                slog() << "mdtm launch error " << res["error"];

                store.update_sjob_stage(sid, "transfer.launch", sjob_stage::error);
                store.update_sjob_stage(sid, "transfer.general", sjob_stage::error);

                schedule_clean_up( rid, sid, std::string("mdtm launch error: ") + res["error"].asString(),
                        /*sdn_setup, wan_setup, */src_dtn, dst_dtn );
            }
            else
            {
                using std::chrono::duration_cast;
                using std::chrono::milliseconds;
                using std::chrono::system_clock;

                // update the block state
                store.update_sjob_block_state(block["id"].asString(), block_state::transferring);

                // update the flow status
                store.update_sjob_flow_state(flow["task"].asString(), flow_state::setting);

                // last flow active
                store.update_sjob_flow_field( flow["task"].asString(), "last_active",
                        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() );

                // sjob and rawjob
                // only update the rawjob to transferring. sjob state will be updated
                // once the mdtm-client reports back with mld_state = 1
                // store.update_sjob_state(sid, sjob_state::transferring);
                store.update_rawjob_state(rid, rawjob_state::transferring);
            }

            // break the loop: we only schedule one flow at a time
            break;
        }

    }

    //slog() << "job scheduling done";
    return std::chrono::milliseconds(5*1000);
}

Json::Value Scheduler::setup_sdn_path(
        std::string const & sid,
        Json::Value const & src_dtn, 
        Json::Value const & dst_dtn,
        double rate, 
        bool pre_ping, 
        int oscar_src, 
        int oscar_dst )
{
    std::string src_url = src_dtn["site"]["url"].asString();
    std::string dst_url = dst_dtn["site"]["url"].asString();

    bool src = src_dtn["sdn_setup"].asBool();
    bool dst = dst_dtn["sdn_setup"].asBool();

    int src_vlan = src ? src_dtn["data_vlan_id"].asInt() : 0;
    int dst_vlan = dst ? dst_dtn["data_vlan_id"].asInt() : 0;

    // return object
    Json::Value res;
    res["src_path_id"] = -1;
    res["dst_path_id"] = -1;
    res["code"] = 0;
    res["error"] = "";

    // Pre-SDN setup
    // send pre-pings first so that the SDN switch can see the source ip
    if (pre_ping)
    {
        Json::Value ping_rpc;

        ping_rpc["cmd"] = "dtn_icmp_ping";
        ping_rpc["params"]["cmd"] = "dtn_send_icmp_ping";

        ping_rpc["dtn"] = src_dtn["id"];
        ping_rpc["params"]["params"]["destination"] = dst_dtn["data_ip"];

        auto r_src = portal.post(std::string("https://") + src_url + "/bde/command", ping_rpc);
        slog() << "DTN icmp ping from src dtn: " << r_src.second;

        ping_rpc["dtn"] = dst_dtn["id"];
        ping_rpc["params"]["params"]["destination"] = src_dtn["data_ip"];

        auto r_dst = portal.post(std::string("https://") + dst_url + "/bde/command", ping_rpc);
        slog() << "DTN icmp ping from dst dtn: " << r_dst.second;

#if 0
        // path is already there
        if (r_src.second["code"].asInt() == 0 /*&& r_dst.second["code"].asInt() == 0*/)
        {
            slog() << "use an existing path";
            return res;
        }
#endif
    }

    // now to setup the path by calling the SDN agent

    int src_path_retry = 0;
    int dst_path_retry = 0;

    Json::Value sdn_rpc;
    sdn_rpc["cmd"]                            = "sdn_reserve_request";
    sdn_rpc["params"]["cmd"]                  = "reserve_request";
    sdn_rpc["params"]["start"]                = "direct";
    sdn_rpc["params"]["end"]                  = "2020-12-30 16:35";  // TODO: end time is hardcoded

    sdn_rpc["params"]["sdn_setup"]            = src_dtn["sdn_setup"];

    sdn_rpc["params"]["dtns"]["trafficType"]  = "1";
    sdn_rpc["params"]["dtns"]["vlanId"]       = std::to_string(src_vlan);
    sdn_rpc["params"]["dtns"]["oscarsVlanId"] = std::to_string(oscar_src);
    sdn_rpc["params"]["dtns"]["routeType"]    = "h2g";
    sdn_rpc["params"]["dtns"]["rate"]         = std::to_string(rate);

    sdn_rpc["params"]["dtns"]["srcId"]  = src_dtn["id"];
    sdn_rpc["params"]["dtns"]["dstId"]  = dst_dtn["id"];

    sdn_rpc["params"]["dtns"]["srcIp"]  = src_dtn["data_ip"];
    sdn_rpc["params"]["dtns"]["dstIp"]  = dst_dtn["data_ip"];

    sdn_rpc["params"]["dtns"]["srcMac"] = src_dtn["data_mac"];
    sdn_rpc["params"]["dtns"]["dstMac"] = dst_dtn["data_mac"];

    while (true)
    {
        if (src) store.update_sjob_stage(sid, "network.lan_src", sjob_stage::working);

        auto r_src = portal.post(std::string("https://") + src_url + "/bde/command", sdn_rpc);
        slog() << "SDN reserve for source site " << src_url << ": " << r_src.second;

        if (r_src.second["code"].asInt() == 0 && r_src.second["sdn"].isMember("InstalledPathId"))
        {
            res["src_path_id"] = r_src.second["sdn"]["InstalledPathId"].asInt();
            slog() << "SDN path created at source with path id " << res["src_path_id"];

            if (src) 
            {
                store.update_sjob_stage(sid, "network.lan_src", sjob_stage::success);
                store.update_sjob_stage(sid, "teardown.lan_src", sjob_stage::pending);
            }

            break;
        }
        else
        {
            res["src_path_id"] = -1;
            ++src_path_retry;

            slog() << "SDN path creation failed at source site: " << r_src.second;

            if (src_path_retry < 3)
            {
                // sleep 2s before next retry
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            else
            {
                store.update_sjob_stage(sid, "network.lan_src", sjob_stage::error);

                res["code"] = -1;
                res["error"] = std::string("Error at SDN source site")
                    + ". Code: " + r_src.second["sdn"]["ErrorCode"].asString()
                    + ". Desc: " + r_src.second["sdn"]["Description"].asString();

                return res;
            }
        }
    }

    // path at destination
    sdn_rpc["params"]["sdn_setup"]            = dst_dtn["sdn_setup"];

    sdn_rpc["params"]["dtns"]["trafficType"]  = "1";
    sdn_rpc["params"]["dtns"]["vlanId"]       = std::to_string(dst_vlan);
    sdn_rpc["params"]["dtns"]["oscarsVlanId"] = std::to_string(oscar_dst);
    sdn_rpc["params"]["dtns"]["routeType"]    = "h2g";
    sdn_rpc["params"]["dtns"]["rate"]         = std::to_string(rate);

    sdn_rpc["params"]["dtns"]["srcId"]  = dst_dtn["id"];
    sdn_rpc["params"]["dtns"]["dstId"]  = src_dtn["id"];

    sdn_rpc["params"]["dtns"]["srcIp"]  = dst_dtn["data_ip"];
    sdn_rpc["params"]["dtns"]["dstIp"]  = src_dtn["data_ip"];

    sdn_rpc["params"]["dtns"]["srcMac"] = dst_dtn["data_mac"];
    sdn_rpc["params"]["dtns"]["dstMac"] = src_dtn["data_mac"];

    while (true)
    {
        if (dst) store.update_sjob_stage(sid, "network.lan_dst", sjob_stage::working);

        auto r_dst = portal.post(std::string("https://") + dst_url + "/bde/command", sdn_rpc);
        slog() << "SDN reserve for destination site " << dst_url << ": " << r_dst.second;

        if (r_dst.second["code"].asInt() == 0 && r_dst.second["sdn"].isMember("InstalledPathId"))
        {
            res["dst_path_id"] = r_dst.second["sdn"]["InstalledPathId"].asInt();
            slog() << "SDN path created at destination with path id " << res["dst_path_id"];

            if (dst) 
            {
                store.update_sjob_stage(sid, "network.lan_dst", sjob_stage::success);
                store.update_sjob_stage(sid, "teardown.lan_dst", sjob_stage::pending);
            }

            break;
        }
        else
        {
            res["dst_path_id"] = -1;
            ++dst_path_retry;

            slog() << "SDN path creation failed at destination site: " << r_dst.second;

            if (dst_path_retry < 3)
            {
                // sleep 2s before next retry
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            else
            {
                store.update_sjob_stage(sid, "network.lan_dst", sjob_stage::error);

                res["code"] = -2;
                res["error"] = std::string("Error at SDN destination site")
                    + ". Code: " + r_dst.second["sdn"]["ErrorCode"].asString()
                    + ". Desc: " + r_dst.second["sdn"]["Description"].asString();

                return res;
            }
        }
    }

    res["code"] = 0;
    return res;
}

Json::Value Scheduler::verify_path(
        Json::Value const & src_dtn, 
        Json::Value const & dst_dtn )
{
    std::string src_url = src_dtn["site"]["url"].asString();
    std::string dst_url = dst_dtn["site"]["url"].asString();

    Json::Value res;
    res["code"] = 0;

    Json::Value ping_rpc;
    ping_rpc["cmd"] = "dtn_icmp_ping";
    ping_rpc["dtn"] = src_dtn["id"];
    ping_rpc["params"]["cmd"] = "dtn_send_icmp_ping";
    ping_rpc["params"]["params"]["destination"] = dst_dtn["data_ip"];

    int retry = 0;
    int max_retry = src_dtn["wan_uuid"].asString().empty() ? 5 : 30;

    while (true)
    {
        auto r_src = portal.post(std::string("https://") + src_url + "/bde/command", ping_rpc);
        slog() << "DTN icmp ping from src dtn (" 
            << retry + 1 << "/" << max_retry << "): "
            << r_src.second;

        // path verification failed
        if (r_src.second["code"].asInt() == 0)
        {
            return res;
        }
        else
        {
            ++retry;
            slog() << "Path verification error";

            if (retry < max_retry)
            {
                // sleep 2s before next retry
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
            else
            {
                res["code"] = r_src.second["code"];
                res["error"] = std::string("path verification error: ") + r_src.second["error"].asString();

                return res;
            }
        }
    }
}

int Scheduler::teardown_sdn_path(
        std::string const & sid,
        Json::Value const & src_dtn, 
        Json::Value const & dst_dtn )
{
    slog() << "sdn_release_request";

    store.update_sjob_state(sid, sjob_state::sdn_teardown);

    std::string src_url = src_dtn["site"]["url"].asString();
    std::string dst_url = dst_dtn["site"]["url"].asString();

    // SDN/LAN release path
    Json::Value sdn_release;
    sdn_release["cmd"] = "sdn_release_request";

    // release at source site
    if (src_dtn["path_id"].asInt() != -1)
    {
        if(src_dtn["sdn_setup"].asBool()) store.update_sjob_stage(sid, "teardown.lan_src", sjob_stage::working);

        sdn_release["params"]["installedPathId"] = src_dtn["path_id"];
        sdn_release["params"]["dtn"] = src_dtn["id"];
        sdn_release["params"]["ip"] = dst_dtn["data_ip"];

        auto r_src = portal.post(std::string("https://") + src_url + "/bde/command", sdn_release);

        slog() << "sdn path release at src: " << sdn_release;
        slog() << "sdn path released at src: " << r_src.second;

        if(src_dtn["sdn_setup"].asBool()) store.update_sjob_stage(sid, "teardown.lan_src", sjob_stage::success);
    }
    else
    {
        slog() << "skip sdn path release at src";
    }

    // release at dst site
    if (dst_dtn["path_id"].asInt() != -1)
    {
        if(dst_dtn["sdn_setup"].asBool()) store.update_sjob_stage(sid, "teardown.lan_dst", sjob_stage::working);

        sdn_release["params"]["installedPathId"] = dst_dtn["path_id"];
        sdn_release["params"]["dtn"] = dst_dtn["id"];
        sdn_release["params"]["ip"] = src_dtn["data_ip"];

        auto r_dst = portal.post(std::string("https://") + dst_url + "/bde/command", sdn_release);

        slog() << "sdn path release at dst: " << sdn_release;
        slog() << "sdn path released at dst: " << r_dst.second;

        if(dst_dtn["sdn_setup"].asBool()) store.update_sjob_stage(sid, "teardown.lan_dst", sjob_stage::success);
    }
    else
    {
        slog() << "skip sdn path release at dst";
    }

    return 0;
}

int Scheduler::teardown_wan_path(
        std::string const & sid,
        std::string const & uuid )
{
    // do noting for empty uuid
    if (uuid.empty()) return 0;

    store.update_sjob_state(sid, sjob_state::wan_teardown);
    store.update_sjob_stage(sid, "teardown.wan_dynamic", sjob_stage::working);

    // step 4: release
    slog() << "WAN path releasing... uuid = " << uuid;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    if (!wan.release(uuid))
    {
        slog() << "ERROR: wan release failed.";
        store.update_sjob_stage(sid, "teardown.wan_dynamic", sjob_stage::error);
        return 1;
    }

    // step 5:terminate 
    slog() << "WAN path terminating...";
    std::this_thread::sleep_for(std::chrono::seconds(30));

    if (!wan.terminate(uuid))
    {
        slog() << "ERROR: wan terminate failed.";
        store.update_sjob_stage(sid, "teardown.wan_dynamic", sjob_stage::error);
        return 2;
    }

    // step 6: delete
    slog() << "WAN path deleting...";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (!wan.del(uuid))
    {
        slog() << "ERROR: wan delete failed.";
        store.update_sjob_stage(sid, "teardown.wan_dynamic", sjob_stage::error);
        return 3;
    }

    store.update_sjob_stage(sid, "teardown.wan_dynamic", sjob_stage::success);
    return 0;
}

bool Scheduler::needs_sdn_setup(std::string const & dtn)
{
    auto const & lan_dtns = net_conf["lan"]["dtns"];

    for (auto const & d : lan_dtns)
        if (d.asString() == dtn) return true;

    return false;
}

Json::Value Scheduler::find_dtn_stp(std::string const & dtn)
{
    for (auto const & stp : net_conf["wan_dynamic"]["stps"])
    {
        for(auto const & d : stp["dtns"])
        {
            if (d.asString() == dtn) return stp;
        }
    }

    return Json::objectValue;
}

Json::Value Scheduler::network_handling(
        std::string const & sid,
        Json::Value const & src_dtn, 
        Json::Value const & dst_dtn )
{
    // look up the static WAN configuration
    //   if found, obtain vlan id, and wan_setup = false
    //   else look up the dynamic WAN configuration
    //       if found, wan_setup = true, setup wan, obtain vlan
    //       else return error
    //
    // look up lan (sdn) configuration at source
    //   if found, src_sdn_setup = true, setup sdn with vlanid
    //   else src_sdn_setup = false
    //
    // look up lan (sdn) configurtion at dest
    //   if found, dst_sdn_setup = true, setup sdn with vlanid
    //   else dst_sdn_setup = false
    //
    // any error during the wan or lan setup process
    // clean up and return error

    Json::Value res;
    res["code"] = 0;
    res["error"] = "success";

    res["wan_uuid"] = "";
    res["src_sdn_path_id"] = -1;
    res["dst_sdn_path_id"] = -1;

    bool found_static = false;
    bool found_dynamic = false;

    double max_bw = 0.0;

    int wan_src_vlan = 0;
    int wan_dst_vlan = 0;

    std::string wan_src_vlan_s;
    std::string wan_dst_vlan_s;

    auto src_site = src_dtn["site"]["abbrev"].asString();
    auto dst_site = dst_dtn["site"]["abbrev"].asString();

    // static path
    for(auto const & p : src_dtn["wan_static"]["paths"])
    {
        if (p["remote_site"].asString() == dst_site)
        {
            found_static = true;

            auto const & c = p["circuits"][0];
            max_bw = c["bandwidth"].asDouble();
            wan_src_vlan = c["local_vlan_id"].asInt();
            wan_dst_vlan = c["remote_vlan_id"].asInt();

            break;
        }
    }

    // dynamic path
    if (!found_static)
    {
        bool has_src_stp = !src_dtn["stp"].empty();
        bool has_dst_stp = !dst_dtn["stp"].empty();

        slog() << "has_src_stp: " << has_src_stp;
        slog() << "has_dst_stp: " << has_dst_stp;

        if (has_src_stp && has_dst_stp)
        {
            bool src_sdn = src_dtn["sdn_setup"].asBool();
            int  src_dtn_vlan = src_dtn["data_vlan_id"].asInt();

            bool dst_sdn = dst_dtn["sdn_setup"].asBool();
            int  dst_dtn_vlan = dst_dtn["data_vlan_id"].asInt();


            //slog() << "src_sdn: " << src_sdn;
            //slog() << "dst_sdn: " << dst_sdn;

            found_dynamic = true;
            max_bw = 10000.0;

            if (src_sdn)
            {
                if (src_dtn["stp"].isMember("local_vlan_constraints") &&
                        !src_dtn["stp"]["local_vlan_constraints"].asString().empty() ) 
                {
                    wan_src_vlan_s = src_dtn["stp"]["local_vlan_constraints"].asString();
                }
                else
                {
                    wan_src_vlan_s = "any";
                }
            }
            else
            {
                wan_src_vlan_s = std::to_string(src_dtn_vlan);
            }

            if (dst_sdn)
            {
                if (dst_dtn["stp"].isMember("local_vlan_constraints") &&
                        !src_dtn["stp"]["local_vlan_constraints"].asString().empty() ) 
                {
                    wan_dst_vlan_s = dst_dtn["stp"]["local_vlan_constraints"].asString();
                }
                else
                {
                    wan_dst_vlan_s = "any";
                }
            }
            else
            {
                wan_dst_vlan_s = std::to_string(dst_dtn_vlan);
            }

#if 0
            if (src_sdn && dst_sdn)
            {
                // both go through AmoebaNet, let SENSE decide the vlan id
                wan_local_vlan = 0;
                wan_remote_vlan = 0;
                max_bw = 10000.0;
                found_dynamic = true;
            }
            else if (src_sdn)
            {
                // must follow the dst vlan id
                wan_local_vlan = dst_vlan;
                wan_remote_vlan = dst_vlan;
                max_bw = 10000.0;
                found_dynamic = true;
            }
            else if (dst_sdn)
            {
                // must follow the src vlan id
                wan_local_vlan = src_vlan;
                wan_remote_vlan = src_vlan;
                max_bw = 10000.0;
                found_dynamic = true;
            }
            else
            {
                // src vlan must be the same as the dst vlan
                if (src_vlan == dst_vlan)
                {
                    wan_local_vlan = src_vlan;
                    wan_remote_vlan = src_vlan;
                    max_bw = 10000.0;
                    found_dynamic = true;
                }
                else
                {
                    found_dynamic = false;
                }
            }
#endif
        }
        else
        {
            found_dynamic = false;
        }
    }

    // must have at least one available
    if (found_static)
    {
        slog() << "use static OSCAR WAN path"
            << ", src_vlan_id = " << wan_src_vlan
            << ", dst_vlan_id = " << wan_dst_vlan
            ;

        // stages
        store.update_sjob_stage(sid, "network.planning", sjob_stage::success);
        store.update_sjob_stage(sid, "network.wan_static", sjob_stage::success);
        store.update_sjob_stage(sid, "network.ping", sjob_stage::pending);

        if (src_dtn["sdn_setup"].asBool()) 
            store.update_sjob_stage(sid, "network.lan_src", sjob_stage::pending);

        if (dst_dtn["sdn_setup"].asBool()) 
            store.update_sjob_stage(sid, "network.lan_dst", sjob_stage::pending);
    }
    else if (found_dynamic)
    {
        slog() << "setting up dynamic SENSE WAN path";

        store.update_sjob_state(sid, sjob_state::wan_setup);

        store.update_sjob_stage(sid, "network.planning", sjob_stage::success);
        store.update_sjob_stage(sid, "network.wan_dynamic", sjob_stage::working);
        store.update_sjob_stage(sid, "network.ping", sjob_stage::pending);

        if (src_dtn["sdn_setup"].asBool()) 
            store.update_sjob_stage(sid, "network.lan_src", sjob_stage::pending);

        if (dst_dtn["sdn_setup"].asBool()) 
            store.update_sjob_stage(sid, "network.lan_dst", sjob_stage::pending);

        auto res_wan = setup_wan_path(
                src_dtn, dst_dtn, 10000 /* max_bw */,
                wan_src_vlan_s, wan_dst_vlan_s);

        // error setting up the wan path
        if (res_wan["code"].asInt())
        {
            store.update_sjob_stage(sid, "network.wan_dynamic", sjob_stage::error);

            res["wan_uuid"] = "";
            res["code"] = res_wan["code"];
            res["error"] = res_wan["error"];

            return res;
        }
        else
        {
            store.update_sjob_stage(sid, "network.wan_dynamic", sjob_stage::success);
            store.update_sjob_stage(sid, "teardown.wan_dynamic", sjob_stage::pending);

            res["wan_uuid"] = res_wan["wan_uuid"];
            res["wan_src_vlan"] = res_wan["wan_src_vlan"];
            res["wan_dst_vlan"] = res_wan["wan_dst_vlan"];
            res["wan_max_bw"] = res_wan["wan_max_bw"];

            wan_src_vlan = res_wan["wan_src_vlan"].asInt();
            wan_dst_vlan = res_wan["wan_dst_vlan"].asInt();
        }
    }
    else
    {
        slog() << "unable to find valid static or dynamic WAN path";

        store.update_sjob_stage(sid, "network.planning", sjob_stage::error);

        res["code"] = 101;
        res["error"] = "unable to find valid static or dynamic WAN path.";

        return res;
    }

    // sdn setup
    store.update_sjob_state(sid, sjob_state::sdn_setup);

    auto res_sdn = setup_sdn_path( sid,
            src_dtn, dst_dtn, 
            max_bw, false /*pre-ping*/,
            wan_src_vlan, wan_dst_vlan );

    // record the path_id regardless of the success or not
    res["src_sdn_path_id"] = res_sdn["src_path_id"];
    res["dst_sdn_path_id"] = res_sdn["dst_path_id"];

    // log
    slog() << "res_sdn = " << res_sdn;

    // error
    if (res_sdn["code"].asInt())
    {
        res["code"] = res_sdn["code"];
        res["error"] = res_sdn["error"];

        return res;
    }
   
    return res;
}


std::chrono::milliseconds Scheduler::poll_rate()
{
    // number of waiting jobs and error jobs
    Json::Value rjobs_w = store.get_rawjobs_from_state(rawjob_state::waiting);
    int rawjobs_waiting = rjobs_w.size();
    
    Json::Value rjobs_e = store.get_rawjobs_from_state(rawjob_state::error);
    int rawjobs_error   = rjobs_e.size();

    // get rawjobs that are transferring
    Json::Value rjobs   = store.get_rawjobs_from_state(rawjob_state::transferring);
    int rawjobs_active_be = rjobs.size();  // best effort jobs
    int rawjobs_active_rt = 0;             // realtime jobs

    // insert job statistics
    tsjobs.insert(rawjobs_active_be, rawjobs_active_rt, rawjobs_waiting, rawjobs_error, {});

    // wait time before next poll
    auto delay = std::chrono::milliseconds(5000);

    // not active jobs
    if (rawjobs_active_be + rawjobs_active_rt == 0)
    {
        return delay;
    }

    // has the launcher agent?
    if (!rm.has_launcher())
    {
        slog() << "no available launcher agent, skipping the rate poll";
        return delay;
    }

    // get_rate rpc object
    Json::Value rate_rpc = Json::objectValue;
    rate_rpc["cmd"] = "mld_rate";
    rate_rpc["target"] = "launcher-agent";
    rate_rpc["params"]["task"] = "";

    // go through every rawjob
    for (auto const & rjob : rjobs)
    {
        double raw_rate = 0.0;  // aggregated rawjob rate
        int64_t raw_tx_bytes = 0;

        std::string rjob_id = rjob["_id"]["$oid"].asString();
        Json::Value sjobs = store.get_sjobs_from_rawjob_id(rjob_id);

        for (auto const & sjob : sjobs)
        {
            double sjob_rate = 0.0;
            int64_t sjob_tx_bytes = 0;
            std::string sjob_id = sjob["_id"]["$oid"].asString();

            for (auto const & flow : sjob["flows"])
            {
                flow_state fs = (flow_state)flow["state"].asInt();
                std::string task = flow["task"].asString();

                // do not sample the done state
                if (fs == flow_state::done || fs == flow_state::error) continue;

                // query the rate if the status is setting or transferring
                if (fs == flow_state::setting || fs == flow_state::transferring)
                {
                    // query from launching daemon
                    rate_rpc["params"]["task"] = task;
                    auto rate = rpc.call(rm.launcher().queue(), rate_rpc);

                    slog() << "rate for " << task << ": " << rate;

                    // timed out?
                    if (rate.isMember("timed_out") && rate["timed_out"].asBool())
                        continue;

                    double flow_rate = 0.0;
                    int64_t tx_bytes = 0;

                    int code = rate["code"].asInt();

                    if (code == 1)
                    {
                        // unknown task
                        // probably due to the restart of the mdtm-client, need to mark the
                        // teardown path
                        teardown_sdn_path(sjob_id, flow["srcdtn"], flow["dstdtn"]);
                        teardown_wan_path(sjob_id, flow["srcdtn"]["wan_uuid"].asString());

                        // task as failed
                        store.update_sjob_flow_state(task, flow_state::error);
                        store.update_sjob_block_state(flow["block"].asString(), block_state::waiting);

                        // update sjob and rawjob
                        store.update_sjob_state(sjob_id, sjob_state::error);
                        store.update_collection_field("sjob", sjob_id, "error", "invalid task at launching agent");

                        store.update_rawjob_state(rjob_id, rawjob_state::error);
                        store.update_collection_field("rawjob", rjob_id, "error", "invalid task at launching agent");


                        // stop here
                        return delay;
                    }
                    else if (code == 2)
                    {
                        // task is still good, just the rate is unknown
                        // for now, save the rate as 0
                        rate["rate"] = 0.0;
                    }

                    if (rate["code"].asInt() != 0) continue;

                    // flow rate and tx bytes
                    flow_rate = rate["rate"].asDouble() * 1024 * 1024;
                    tx_bytes = rate["transferred"].asInt64();

                    // accumulate to the sjob_rate and raw_rate
                    sjob_rate += flow_rate;
                    raw_rate += flow_rate;

                    // and insert into tsdb for the flow
                    tsrate.insert(flow_rate, {"flow", task});

                    // how much has been transferred of this block since last poll
                    std::string bid = flow["block"].asString();
                    Json::Value const & blocks = sjob["blocks"];
                    int64_t last_tx_bytes = 0;

                    for (auto const & block : blocks)
                    {
                        if (block["id"].asString() == bid)
                        {
                            last_tx_bytes = block["tx_bytes"].asInt64();
                            break;
                        }
                    }

                    // transferred for this block and this flow
                    tx_bytes = tx_bytes - last_tx_bytes;
                    slog() << "transferred bytes since last poll: " << tx_bytes;

                    // accumulate the tx bytes for sjob and rawjob
                    sjob_tx_bytes += tx_bytes;
                    raw_tx_bytes += tx_bytes;

                    // average rate
                    using std::chrono::duration_cast;
                    using std::chrono::milliseconds;
                    using std::chrono::system_clock;

                    auto now_epoch = system_clock::now().time_since_epoch();

                    int64_t milli_start = flow["start"].asInt64();
                    int64_t milli_now   = duration_cast<milliseconds>(now_epoch).count();

                    double  avg_rate    = (flow["tx_bytes"].asInt64() + tx_bytes) * 1000.0 
                                          / (milli_now - milli_start);

                    // any transferred bytes?
                    if (tx_bytes > 0)
                    {
                        store.update_sjob_flow_field(task, "last_active", milli_now);
                    }
                    else
                    {
                        int64_t last_active = flow["last_active"].asInt64();

                        if (milli_now - last_active > inactive_timeout_ms)
                        {
                            // task timed out. no rate or activity for an extended period of time
                            store.update_sjob_flow_state(task, flow_state::error);
                            store.update_sjob_block_state(bid, block_state::waiting);

                            // teardown path
                            teardown_sdn_path(sjob_id, flow["srcdtn"], flow["dstdtn"]);
                            teardown_wan_path(sjob_id, flow["srcdtn"]["wan_uuid"].asString());

                            // update sjob and rawjob
                            store.update_sjob_state(sjob_id, sjob_state::error);
                            store.update_collection_field("sjob", sjob_id, "error", 
                                    "job failed due to no transferring progress for an extended period of time");

                            store.update_rawjob_state(rjob_id, rawjob_state::error);
                            store.update_collection_field("rawjob", rjob_id, "error", 
                                    "job failed due to no transferring progress for an extended period of time");
                        }
                    }

                    // update the flow rate
                    store.update_sjob_flow_field(task, "current_rate", flow_rate);
                    store.update_sjob_flow_field(task, "avg_rate", avg_rate);

                    // increment the transferred bytes
                    store.increment_sjob_block_field (bid,  "tx_bytes", tx_bytes);
                    store.increment_sjob_flow_field  (task, "tx_bytes", tx_bytes);
                }
                else
                {
                    // insert 0 into tsdb
                    tsrate.insert(0.0, {"flow", task});
                }
            }

            // insert sjob_rate into tsdb
            tsrate.insert(sjob_rate, {"sjob", sjob_id});

            // increment the transferred bytes
            store.increment_sjob_field(sjob_id, "tx_bytes", sjob_tx_bytes);
        }

        // insert raw-rate into tsdb
        tsrate.insert(raw_rate, {"rawjob", rjob_id});

        // increment the transferred bytes
        store.increment_rawjob_field(rjob_id, "tx_bytes", raw_tx_bytes);

        // increment the tx bytes of the source site and rx bytes of the dst site
        auto src_site = store.increment_site_field(rjob["srcsite"]["$oid"].asString(), "tx_bytes", raw_tx_bytes);
        auto dst_site = store.increment_site_field(rjob["dstsite"]["$oid"].asString(), "rx_bytes", raw_tx_bytes);

        site_txrx[rjob["srcsite"]["$oid"].asString()].first  += raw_tx_bytes;
        site_txrx[rjob["dstsite"]["$oid"].asString()].second += raw_tx_bytes;
    }

    for (auto const & txrx : site_txrx)
        ts_site_txrx.insert(txrx.second.first, txrx.second.second, { txrx.first });

    return delay;
}


void Scheduler::get_storage_list(res_cb_t const & cb)
{
    //dispatch([this, cb]() { cb(store.get_storage_list()); });

    dispatch([this, cb]() { cb(rm.get_active_storage_list()); });
}

void Scheduler::get_dtn_list(res_cb_t const & cb)
{
    dispatch([this, cb]() { cb(rm.get_active_dtn_list()); });
}

void Scheduler::get_dtn_topology(res_cb_t const& cb)
{
    dispatch([this, cb]() { cb(rm.get_active_dtn_topology()); });
}

void Scheduler::publish_dtn_user_mapping(res_cb_t const & cb)
{
    dispatch([this, cb]() { 
        Json::Value res = Json::objectValue;
        res["code"] = 0;

        // prepare the output array
        Json::Value per_dtn = Json::objectValue;

        // retrieve the dtn list
        // auto dtns = store.get_dtn_list();
        auto dtns = rm.get_active_dtn_list();

        for (auto const & dtn : dtns)
        {
            auto dtnid = dtn["id"].asString();

            per_dtn[dtnid] = Json::objectValue;
            per_dtn[dtnid]["cmd"] = "dtn_push_gridmap_entries";
            per_dtn[dtnid]["target"] = dtnid;
            per_dtn[dtnid]["params"]["map"] = Json::arrayValue;
        }

        // fill in the cn_dtn_user mapping data
        auto cn_dtn = store.get_cn_dtn_user_mapping();

        for (auto const & entry : cn_dtn)
        {
            auto dtnid = entry["dtnid"].asString();
            if (!per_dtn.isMember(dtnid)) continue;

            Json::Value map = Json::objectValue;
            map["dn"] = entry["cn"];
            map["ln"] = entry["username"];
            per_dtn[dtnid]["params"]["map"].append(map);
        }

        slog() << per_dtn;

        // send to each dtn
        for (auto const & entry : per_dtn)
        {
            auto dtnid = entry["target"].asString();
            auto queue = rm.graph().get_dtn_node(dtnid).queue();

            auto dtn_res = rpc.call(queue, entry);
            slog() << dtn_res;
        }

        return cb(res);
    });
}

Json::Value Scheduler::probe_rate(Json::Value const & params)
{
    auto s = params["storage"].asString();
    auto r = params["rate"].asDouble();

    int res = rm.probe_path_rate(s, r);

    Json::Value v;
    v["code"] = res;

    return v;
}

Json::Value Scheduler::query_path_dtns(Json::Value const & params)
{
    auto s = params["storage"].asString();
    auto r = params["rate"].asDouble();

    auto res = rm.query_path_dtns(s, r);

    Json::Value v;
    v["code"] = 0;
    v["dtns"] = Json::arrayValue;

    for(auto const & dr : res)
    {
        Json::Value dtn;
        dtn["host"] = dr.first;
        dtn["rate"] = dr.second;

        v["dtns"].append(dtn);
    }

    return v;
}

void Scheduler::get_dtn_info(Json::Value const & params, res_cb_t const & cb)
{
    dispatch([this, params, cb]() {
        auto dtn_id = params["dtn"].asString();
        if (!rm.graph().has_dtn_node(dtn_id)) cb(Json::objectValue);

        Json::Value res;
        auto const dtn = rm.graph().get_dtn_node(dtn_id);
        res["dtn"] = dtn.properties();

        auto path = params["path"].asString();

        if (!path.empty())
        {
            Json::Value rpc_disk;
            rpc_disk["cmd"] = "dtn_get_disk_usage";
            rpc_disk["target"] = dtn.rid();
            rpc_disk["params"]["path"] = path;

            auto r = rpc.call(dtn.queue(), rpc_disk);
            if (r["code"].asInt() != 0)
            {
                slog(s_error) << "error getting the dtn disk usage: " << r;
                res["error"] = r["error"];
                res["code"] = 101;
                return cb(res);
            }

            res["disk"] = r["result"];
        }

        cb(res);
    });
}

void Scheduler::get_total_size(Json::Value const & params, res_cb_t const & cb)
{
    dispatch([this, params, cb]() {
        slog() << params;
        Json::Value res = Json::objectValue;
        res["sizes"] = Json::arrayValue;

        // storage -> map of (file -> id)
        std::map<std::string, std::map<std::string, std::string>> storage_file_map;

        for (auto const & file : params)
        {
            std::string fileid = file.asString();
            auto fs = split(fileid, '|');

            // sanity check
            if (fs.size() != 4)
            {
                slog(s_error) << "scheduler::get_total_size(): unknown src path";
                res["error"] = "invalid parameters";
                return cb(res);
            }

            std::string site    = fs[0];
            std::string storage = fs[1] + '|' + fs[2];
            std::string path    = fs[3];

            // TODO: check that site is the local site

            // groups the files according to their storages
            storage_file_map[storage].insert(std::make_pair(path, fileid));

            // call dtn to expand the files
            Json::Value expand = Json::objectValue;
            expand["cmd"] = "dtn_expand";

            auto & snode = rm.graph().get_storage_node(storage);
            auto & dnode = snode.get_one_dtn_node();

            expand["target"] = dnode.rid();
            expand["params"]["files"] = path;

            // rpc call to dtn.expand
            auto filesize = rpc.call(dnode.queue(), expand);

            //slog() << "expand: " << filesize;

            // check error
            if (filesize["code"].asInt() != 0)
            {
                slog(s_error) << "error expanding the files: " << filesize;
                res["error"] = "error at expanding the files";
                return cb(res);
            }

            // get the size
            Json::Value v = Json::objectValue;
            v["id"]     = fileid;
            v["bytes"]  = filesize["total_size"].asDouble();
            v["number"] = filesize["files"].size();
            res["sizes"].append(v);
        }

        return cb(res);
    });
}

void Scheduler::get_file_list(Json::Value const & params, res_cb_t const & cb)
{
    dispatch([this, params, cb]() {

        auto dtn_id   = params["dtn"].asString();
        auto dtn_path = params["path"].asString();

        //auto user = params["user"].asString();
        auto site = params["site"].asString();
        auto cn   = params["cn"].asString();
        auto checksum = params["checksum"].asBool();

        slog() << "get_file_list: " 
               << dtn_id << ":" << dtn_path 
               << " (" << cn << "), checksum = " << checksum;

        std::string dtn_addr;
        Json::Value res;

        if (rm.graph().has_dtn_node(dtn_id))
        {
            // get the mapped user on this dtn
            auto username = store.get_user_from_cn(cn, dtn_id);
            slog() << "username: " << username;

            // TODO: skip empty username for now
            if (username.empty()) username = "rucio";

            if (username.empty())
            {
                // did not find the username
                slog() << "invalid username";
                res["entries"] = Json::arrayValue;
            }
            else
            {
                // get dtn details
                auto const & dtn = rm.graph().get_dtn_node(dtn_id);
                dtn_addr = dtn.ctrl_ip();

                // local DTN, get file list using direct DTN rpc call
                Json::Value p;
                p["target"] = dtn_id;
                p["cmd"] = "dtn_list";
                p["params"]["path"] = dtn_path;
                p["params"]["checksum"] = checksum;
                // TODO: skip empty username for now
                //p["params"]["user"] = username;

                res = rpc.call(dtn.queue(), p);
                slog() << "dtn res = " << res["code"];

                if (!res.isMember("code") || res["code"].asInt()!=0)
                    res["entries"] = Json::arrayValue;
            }
        }
        else
        {
            // did not find the dtn in local site
            slog() << "invalid dtn";
            res["entries"] = Json::arrayValue;
            res["code"] = 400;
            res["error"] = "invalid dtn";

#if 0
            // remote dtn login to get file list --- no longer supported
            Json::Value p;
            p["cmd"] = "get_dtn_info";
            p["dtn"] = params["dtn"];

            slog() << "get_file_list: waiting for get_dtn_info() ...";

            auto site_info = store.get_site_info(site);
            auto r = portal.post(std::string("https://") + site_info["url"].asString() + "/bde/command", p);

            if (r.first != 200) return cb(Json::arrayValue);
            dtn_addr = r.second["ctrl_interface"]["ip"].asString();

            slog() << "get_file_list: dtn_ip = " << dtn_addr;
            slog() << "get_file_list: ssh ls ...";

            auto keys = store.get_user_keys(user, site);
            if ( keys.isMember("error") )
            {
                slog(s_error) << "error at getting user keys and certs: " << keys["error"] << "\n";
                return cb(Json::arrayValue);
            }

            // remote DTN, get file list using GSISSH
            std::string cmd = std::string("\"ls ") + dtn_path + " -l \" | awk 'NR>1 {print $1, $5, $9}'";
            utils::CertSSH cssh(keys["keypem"].asString(), keys["cert"].asString());

            auto rssh = cssh.exec(dtn_addr, cmd);

            slog() << "get_file_list: ssh returned";

            res["entries"] = Json::arrayValue;
            auto lines = utils::split(rssh, '\n');

            for (auto const & line : lines)
            {
                auto toks = utils::split(line, ' ');
                if (toks.size() != 3) continue;

                Json::Value entry = Json::arrayValue;
                entry[0] = toks[2];
                entry[1] = (Json::Value::Int64)std::stoll(toks[1]);
                entry[2] = (toks[0][0] == 'd');

                res["entries"].append(std::move(entry));
            }
#endif
        }

        //slog() << "res = " << res;
        return cb(res);
    });
}

void Scheduler::create_folder(Json::Value const & params, res_cb_t const & cb)
{
    dispatch([this, params, cb]() {

        auto dtn_id   = params["dtn"].asString();
        auto dtn_path = params["path"].asString();

        auto user = params["user"].asString();
        auto site = params["site"].asString();
        auto cn   = params["cn"].asString();

        slog() << "create_folder: " << dtn_id << ":" << dtn_path << " (" << cn << ")";

        std::string dtn_addr;
        Json::Value res;

        if (rm.graph().has_dtn_node(dtn_id))
        {
            // get the mapped user on this dtn (could be empty)
            auto username = store.get_user_from_cn(cn, dtn_id);
            slog() << "username: " << username;

            // get dtn details
            auto const & dtn = rm.graph().get_dtn_node(dtn_id);
            dtn_addr = dtn.ctrl_ip();

            // local DTN, get file list using direct DTN rpc call
            Json::Value p;
            p["target"] = dtn_id;
            p["cmd"] = "dtn_mkdir";
            p["params"]["path"] = dtn_path;
            p["params"]["user"] = username;

            res = rpc.call(dtn.queue(), p);
            slog() << "dtn res = " << res["code"];
        }
        else
        {
            slog() << "invalid dtn";
            res["code"] = 2;
            res["error"] = "invalid dtn";
        }

        //slog() << "res = " << res;
        return cb(res);
    });
}

void Scheduler::expand_and_group(Json::Value const & params, res_cb_t const & cb)
{
    dispatch([this, params, cb]() {
        slog() << "expand and group: " << params;

        Json::Value req = params["params"];
        req["cmd"] = "dtn_expand_and_group_v2";
        auto q = rm.get_node_queue_name(req["target"].asString());
        Json::Value res = rpc.call(q, req, 600);

        return cb(res);
    });
}

void Scheduler::verify_checksum(Json::Value const & params, res_cb_t const & cb)
{
    dispatch([this, params, cb]() {
        slog() << "verify checksum: " << params;

        Json::Value req;
        req["cmd"] = "dtn_verify_checksums";
        req["target"] = params["params"]["dtn"];
        req["params"]["algorithm"] = params["params"]["algorithm"];
        req["params"]["checksums"] = params["params"]["checksums"];

        auto q = rm.get_node_queue_name(req["target"].asString());
        Json::Value res = rpc.call(q, req, 600);

        return cb(res);
    });
}

Json::Value Scheduler::get_available_best_effort_dtn(Json::Value const & params)
{
    Json::Value rpc_json;
    Json::Value r;

    // remote site abbrev for this dtn query
    auto remote_site = params["remote"].asString();

    // is the source storage busy?
    auto src_storage = params["storage"].asString();
    auto src_storage_q = rm.get_node_queue_name(src_storage);
    rpc_json["cmd"] = "ls_get_rate";
    rpc_json["target"] = src_storage;
    rpc_json["params"] = Json::objectValue;
    auto res = rpc.call(src_storage_q, rpc_json);

    r["storage"]["id"] = src_storage;
    r["storage"]["rd_rate"] = res["rate_read"].asDouble() * 1e6;
    r["storage"]["wr_rate"] = res["rate_write"].asDouble() * 1e6;

    // TODO: use the real load
    r["storage"]["load"] = 0.1;

    // get all the dtns connected to the storage
    auto dtns = rm.get_storage_connected_dtns(src_storage);
    r["private_dtns"] = Json::arrayValue;
    r["public_dtns"] = Json::arrayValue;

    // get the status on all dtns
    for (auto dtn : dtns)
    {
        auto const & dtn_id = dtn->rid();
        auto const & dtn_q = dtn->queue();

        rpc_json["cmd"] = "dtn_status";
        rpc_json["target"] = dtn_id;
        rpc_json["params"]["interface"] = dtn->properties()["ctrl_interface"]["name"];

        auto res = rpc.call(dtn_q, rpc_json);

        Json::Value d;
        d["id"]      = dtn->rid();
        d["name"]    = dtn->name();
        d["ip"]      = dtn->properties()["ctrl_interface"]["ip"];
        d["data_ip"] = dtn->properties()["data_interfaces"][0]["ip"];
        d["data_mac"]= dtn->properties()["data_interfaces"][0]["mac"];
        d["cap"]     = dtn->properties()["data_interfaces"][0]["rate"];

        // data transfer program control port, aka, the mdtm-server port
        d["dtp_ctrl_port"] = dtn->properties()["data_transfer_program"]["port"];

        d["rx_rate"] = res["status"]["rate"]["rx-bytes"];
        d["tx_rate"] = res["status"]["rate"]["tx-bytes"];
        d["load"]    = (d["rx_rate"].asDouble() + d["tx_rate"].asDouble()) / (d["cap"].asDouble() * 1e3);

        // needs sdn setup or not
        if (needs_sdn_setup(dtn->name()))
        {
            d["sdn_setup"] = true;
            d["data_vlan_id"] = dtn->properties()["data_interfaces"][0]["vlan_id"].asInt();
        }
        else
        {
            d["sdn_setup"] = false;
            d["data_vlan_id"] = dtn->properties()["data_interfaces"][0]["vlan_id"].asInt();
        }

        // static wan portion of the net_conf
        d["wan_static"] = net_conf["wan_static"];

        // is this dtn connected to any sense stp?
        d["stp"] = find_dtn_stp(dtn->name());

        // private interface or public interface
        if (utils::is_private_ip(d["data_ip"].asString()))
        {
            r["private_dtns"].append(std::move(d));
        }
        else
        {
            r["public_dtns"].append(std::move(d));
        }
    }

    return r;
}

Json::Value Scheduler::sdn_reserve_request(Json::Value const & params)
{
    //slog() << "sdn params: " << params;
    slog() << "SDN reserve request...";
    Json::Value res;
    res["code"] = 0;

    // local dtn id
    auto dtn_id = params["params"]["dtns"]["srcId"].asString();

    // dtn online?
    if (!rm.graph().has_dtn_node(dtn_id))
    {
        slog() << "requested DTN " << dtn_id << " is offline";
        res["code"] = -3;
        res["error"] = "requested DTN offline";
        return res;
    }

    // get dtn node
    auto const & local_dtn = rm.graph().get_dtn_node(dtn_id);

    // call to add ARP entry
    Json::Value rpc_arp;
    rpc_arp["cmd"] = "dtn_add_neighbor";
    rpc_arp["target"] = local_dtn.rid();
    rpc_arp["params"]["ip"] = params["params"]["dtns"]["dstIp"];
    rpc_arp["params"]["mac"] = params["params"]["dtns"]["dstMac"];

    slog() << "Adding ARP entry ... " << rpc_arp;
    res["arp"] = rpc.call(local_dtn.queue(), rpc_arp, 10);
    slog() << "res: " << res["arp"];

    // call to add route
    Json::Value rpc_route;
    rpc_route["cmd"] = "dtn_add_route";
    rpc_route["target"] = local_dtn.rid();
    rpc_route["params"]["destination"] = params["params"]["dtns"]["dstIp"];
    rpc_route["params"]["via"] = params["params"]["dtns"]["srcIp"];

    slog() << "Adding route ... " << rpc_route;
    res["route"] = rpc.call(local_dtn.queue(), rpc_route, 10);
    slog() << "res: " << res["route"];

    // call to sdn reserve
    if (params["params"]["sdn_setup"].asBool())
    {
        // sdn agent online?
        if (!rm.has_sdn())
        {
            slog() << "SDN agent is offline...";
            res["code"] = -2;
            res["error"] = "SDN agent offline";
            return res;
        }

        // call to setup SDN path
        slog() << "SDN reserve ... " << params["params"];
        res["sdn"] = rpc.call(rm.sdn().queue(), params["params"], 10);
    }
    else
    {
        slog() << "Skiping SDN reserve.";
        res["sdn"]["InstalledPathId"] = 0;
    }

    slog() << "res: " << res;
    return res;
}

Json::Value Scheduler::sdn_release_request(Json::Value const & params)
{
    slog() << "calling SDN release..." << params["params"];

    // rpc object
    Json::Value req;
    Json::Value res;

    // release SDN path
    if (params["params"]["installedPathId"].asInt() > 0)
    {
        // sdn online
        if (!rm.has_sdn())
        {
            slog(s_error) << "SDN agent offline";
            res["code"] = -2;
            res["error"] = "SDN agent offline";
            return res;
        }

        req["cmd"] = "release_request";
        req["installedPathId"] = params["params"]["installedPathId"];

        res["sdn"] = rpc.call(rm.sdn().queue(), req, 10);
        slog() << "release SDN path: " << res["sdn"];
    }

    // get dtn
    std::string dtn_id = params["params"]["dtn"].asString();

    if (!rm.graph().has_dtn_node(dtn_id))
    {
        slog(s_error) << "invalid dtn id: " << dtn_id;
        return res;
    }

    auto const & dtn = rm.graph().get_dtn_node(dtn_id);
    std::string dtn_queue = dtn.queue();

    // delete route
    req["cmd"] = "dtn_delete_route";
    req["target"] = dtn_id;
    req["params"] = Json::objectValue;
    req["params"]["destination"] = params["params"]["ip"];

    res["route"] = rpc.call(dtn_queue, req, 10);
    slog() << "delete route: " << res["route"];

    // delete ARP entry
    req["cmd"] = "dtn_delete_neighbor";
    req["target"] = dtn_id;
    req["params"] = Json::objectValue;
    req["params"]["ip"] = params["params"]["ip"];

    res["arp"] = rpc.call(dtn_queue, req, 10);
    slog() << "delete arp entry: " << res["arp"];

    return res;
}

Json::Value Scheduler::dtn_icmp_ping(Json::Value const & params)
{
    auto dtn_id = params["dtn"].asString();
    auto const & dtn = rm.graph().get_dtn_node(dtn_id);

    // add target
    Json::Value p = params["params"];
    p["target"] = dtn_id;

    // rpc call
    slog() << "calling DTN icmp ping..." << p;
    Json::Value res = rpc.call(dtn.queue(), p, 10);
    return res;
}

void Scheduler::mld_status(std::string const & task, Json::Value const & params)
{
    dispatch([this, task, params]() {

        slog() << "status report for task: " << task << ", " << params;

        //std::string task = params["task"].asString();
        int state = params["state"].asInt();

        // get the sjob (which contains the flow and blocks)
        Json::Value sjob = store.get_sjob_from_task_id(task);
        std::string sid  = sjob["_id"]["$oid"].asString();
        std::string rid  = sjob["rawjob"]["$oid"].asString();

        // valid task id?
        if (sjob.isMember("error"))
        {
            slog(s_error) << "error at finding the sjob for mld_status (" << state << "): " << sjob["error"];
            return;
        }

        // find the flow
        Json::Value flow;
        std::string blockid;

        for (auto const & f : sjob["flows"])
        {
            if (f["task"].asString() == task)
            {
                flow = f;
                blockid = f["block"].asString();
                break;
            }
        }

        // valid flow and block id?
        if (flow.isNull() || blockid.empty())
        {
            slog(s_error) << "error at mld_status (3): cannot find the assicated block from flow task id";
            return;
        }

        // tear down the path or not
        auto teardown = [this](std::string const & sid, Json::Value const & flow) {
            slog() << "teardown_path";
            store.update_sjob_stage(sid, "teardown.general", sjob_stage::working);

            int res_sdn = teardown_sdn_path(sid, flow["srcdtn"], flow["dstdtn"]);
            int res_wan = teardown_wan_path(sid, flow["srcdtn"]["wan_uuid"].asString());

            auto stage = (res_sdn || res_wan) ? sjob_stage::error : sjob_stage::success;
            store.update_sjob_stage(sid, "teardown.general", stage);
        };

        auto job_fail = [this](std::string const & err, std::string const & sid, std::string const & rid) {
            slog() << "job failed";
            store.update_sjob_stage(sid, "transfer.general", sjob_stage::error);
            store.update_sjob_state(sid, sjob_state::error);
            store.update_rawjob_state(rid, rawjob_state::error);
            store.update_collection_field("sjob", sid, "error", err);
            store.update_collection_field("rawjob", rid, "error", err);
        };

        // report state
        if (state == 1)
        {
            slog() << "block " << blockid << " of flow " << task << " has started transfer";

            // the flow is in normal running state
            store.update_sjob_flow_state(task, flow_state::transferring);

            // sjob state
            store.update_sjob_state(sid, sjob_state::transferring);
            store.update_sjob_stage(sid, "transfer.launch", sjob_stage::success);
            store.update_sjob_stage(sid, "transfer.transfer", sjob_stage::working);

            // set the start time for the block
            store.update_sjob_block_field(
                blockid, 
                "start", 
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()
            );
        }
        else if (state == 2)
        {
            int ec = params["error_code"].asInt();
            slog() << "MDTM-client has reported an error(" << ec << ") when transferring block " << blockid << " of flow " << task;

            store.update_sjob_stage(sid, "transfer.launch", sjob_stage::success);
            store.update_sjob_stage(sid, "transfer.transfer", sjob_stage::error);

            // the transfer is in error
            store.update_sjob_flow_state(task, flow_state::error);

            // reset the block state so it can be re-send later
            store.update_sjob_block_state(blockid, block_state::waiting);

            // update sjob and rawjob
            std::string err = std::string("mdtm error code: ") + to_string(ec) + "</br> error message: ";

            if (ec == 201) err += "no port available";
            else if (ec == 202) err += "authorization failed";
            else if (ec == 203) err += "error in reading proxy credential";
            else if (ec == 204) err += "certificate expired";
            else if (ec == 205) err += "operation aborted, maybe caused by connection failures";
            else if (ec == 206) err += "end of file reached";
            else err += "unknown error";

            // job fail and teardown path
            teardown(sid, flow);
            job_fail(err, sid, rid);

        }
        else if (state == 3)
        {
            // re-transmit the same block or need to find a new block
            bool find_new_block = true;

            bool finished = true;     // all blocks of the sjob finished transfer
            bool all_assigned = true; // all blocks either finished, or is under transferring

            // task is finished
            slog() << "block " << blockid << " of flow " << task << " has finished transfer";

            // stages
            store.update_sjob_stage(sid, "transfer.launch", sjob_stage::success);
            store.update_sjob_stage(sid, "transfer.transfer", sjob_stage::success);

            // put the flow in waiting state
            store.update_sjob_flow_state(task, flow_state::waiting);

            // find and mark the finished block
            for (auto const & b : sjob["blocks"])
            {
                if (b["id"].asString() == blockid)
                {
                    int64_t milli_end   = utils::epoch_ms();
                    int64_t milli_start = b["start"].asInt64();

                    int64_t size = b["size"].asInt64();
                    int64_t txbs = b["tx_bytes"].asInt64();
                    double  rate = size * 1000.0 / (milli_end - milli_start);

                    store.update_sjob_block_field(blockid, "avg_rate", rate);

                    slog() << "average rate for the block: " << rate;
                    slog() << "tx bytes: " << size - txbs;

                    store.increment_sjob_block_field(blockid, "tx_bytes", size - txbs);
                    store.increment_sjob_flow_field (task,    "tx_bytes", size - txbs);
                    store.increment_sjob_field      (sid,     "tx_bytes", size - txbs);
                    store.increment_rawjob_field    (rid,     "tx_bytes", size - txbs);

                    // increment the tx bytes of the source site and rx bytes of the dst site
                    auto src_site = store.increment_site_field(
                            sjob["srcsite"]["$oid"].asString(), "tx_bytes", size - txbs);

                    auto dst_site = store.increment_site_field(
                            sjob["dstsite"]["$oid"].asString(), "rx_bytes", size - txbs);

                    site_txrx[sjob["srcsite"]["$oid"].asString()].first  += size - txbs;
                    site_txrx[sjob["dstsite"]["$oid"].asString()].second += size - txbs;

                    ts_site_txrx.insert(
                            src_site["tx_bytes"].asInt64(), 
                            src_site["rx_bytes"].asInt64(), 
                            {src_site["_id"]["$oid"].asString()});

                    ts_site_txrx.insert(
                            dst_site["tx_bytes"].asInt64(), 
                            dst_site["rx_bytes"].asInt64(), 
                            {dst_site["_id"]["$oid"].asString()});

                    break;
                }
            }

            // verify the checksums of the block
            auto block_files = store.get_block_files(blockid);

            // has checksum entry in the block?
            bool has_checksum = block_files.isMember("checksum");

            // do or not do the checksum verification
            if (!ld_checksum)
            {
                slog() << "do not verify the checksum by bde configuration.";
            }
            else if (!has_checksum)
            {
                slog() << "do not verify the checksum for folder creation blocks.";
            }
            else
            {
                // sjob state
                store.update_sjob_state(sid, sjob_state::checksum_verification);
                store.update_sjob_stage(sid, "transfer.checksum", sjob_stage::working);

                // checksum verify result
                std::pair<long, Json::Value> r_vc;

                // checksum verification
                Json::Value vc;
                vc["cmd"] = "verify_checksum";
                vc["params"]["dtn"] = flow["dstdtn"]["id"];
                vc["params"]["algorithm"] = block_files["checksum"]["algorithm"];
                vc["params"]["checksums"] = block_files["checksum"]["checksums"];

                auto dst_site_info = store.get_site_info(sjob["dstsite"]["$oid"].asString());
                r_vc = portal.post(std::string("https://") + dst_site_info["url"].asString() + "/bde/command", vc);

                if ( r_vc.first != 200 || r_vc.second["code"] != 0 )
                {
                    // checksum verification failed
                    slog() << "checksum verification failed: " << r_vc.second;
                    store.update_sjob_stage(sid, "transfer.checksum", sjob_stage::error);

                    // retrogress the transferred bytes
                    for (auto const & b : sjob["blocks"])
                    {
                        if (b["id"].asString() == blockid)
                        {
                            int64_t size = b["size"].asInt64();

                            store.increment_sjob_block_field(blockid, "tx_bytes", -size);
                            store.increment_sjob_flow_field (task,    "tx_bytes", -size);
                            store.increment_sjob_field      (sid,     "tx_bytes", -size);
                            store.increment_rawjob_field    (rid,     "tx_bytes", -size);

                            break;
                        }
                    }

                    // update sjob retry count
                    store.increment_sjob_field(sid, "transfer_retry", 1);

                    // retry count before update
                    if (sjob["transfer_retry"].asInt() >= 2)
                    {
                        slog() << "exceeded maximum transfer retry count. job failing";

                        // report job fail and bail
                        teardown(sid, flow);
                        job_fail("exceed maximum transfer retry, job failed", sid, rid);

                        return;
                    }
                    else
                    {
                        // retransfer the same block
                        find_new_block = false;

                        finished = false;
                        all_assigned = false;
                    }
                }
                else
                {
                    slog() << "checksum verification succeeded";
                    store.update_sjob_stage(sid, "transfer.checksum", sjob_stage::success);
                }
            }

            // need to find a new block?
            if (find_new_block)
            {
                // either no need to do checksum verification or verification succeeded
                // mark the current block as finished
                store.update_sjob_block_state(blockid, block_state::finished);

                // epoch now
                int64_t milli_now = utils::epoch_ms();

                // set the end time for the block
                store.update_sjob_block_field(blockid, "end", milli_now);

                // TODO: flow has marked as expired?
                // if yes, tear down and abandon this flow

                // find next available block
                for (auto const & b : sjob["blocks"])
                {
                    // is this the block we just done transferring?
                    if (b["id"] == blockid) continue;

                    // block state
                    int bstate = b["state"].asInt();

                    // block finished?
                    if (bstate == (int)block_state::finished) continue;

                    // at least one block is waiting or transferring so the job is not finished
                    finished = false;

                    // block being transferred?
                    if (bstate == (int)block_state::transferring) continue;

                    // found a block waiting
                    all_assigned = false;

                    // this is the block we are looking for
                    blockid = b["id"].asString();
                    block_files = store.get_block_files(blockid);

                    break;
                }

            }  // find_new_block

            // no more available blocks
            if (all_assigned)
            {
                // no longer need this flow, mark it as "waiting to be teared"
                // TODO: note that once a do-er sees this state it will tell the SDN controller
                // to tear down the flow, and put the flow into "tearting" state
                store.update_sjob_flow_state(task, flow_state::waiting_to_be_teared);
                slog() << "all blocks as been assigned or finished transfer. this flow is no longer needed.";

                if (finished)
                {
                    // tranfser on this flow is finished
                    store.update_sjob_stage(sid, "transfer.general", sjob_stage::success);
                }

                // teardown -- release the path
                teardown(sid, flow);

                // this sjob is finished
                if (finished)
                {
                    slog() << "sjob(" << sid << ") has finished";

                    // update the sjob state
                    store.update_sjob_state(sid, sjob_state::finished);

                    // query and update the rawjob state
                    Json::Value sjobs = store.get_sjobs_from_rawjob_id(rid);

                    bool rawjob_finished = true;
                    for (auto const & sjob : sjobs)
                    {
                        if (sjob["state"].asInt() != (int)sjob_state::finished)
                        {
                            rawjob_finished = false;
                            break;
                        }
                    }
    
                    if (rawjob_finished)
                    {
                        store.update_rawjob_state(rid, rawjob_state::finished);
                        slog() << "rawjob(" << rid << ") has finished";
                    }
                }
                else
                {
                    // potentially increase the flow size
                    if (sjob["max_flows"].asInt() < max_concurrent_flows_per_job)
                    {
                        //store.increment_sjob_field(sid, "max_flows", 1);
                        store.update_collection_field("sjob", sid, 
                                "max_flows", max_concurrent_flows_per_job);
                    }
                }
            }
            else
            {
                // transfer the found block
                slog() << "prepare to launch block(" << blockid << ") on the same flow";

                store.update_sjob_stage(sid, "transfer.launch", sjob_stage::working);
                store.update_sjob_stage(sid, "transfer.transfer", sjob_stage::pending);
                if (ld_checksum && find_new_block) store.update_sjob_stage(sid, "transfer.checksum", sjob_stage::pending);

                // prepare the launch command
                Json::Value launch_cmd = flow["launching"];

                // only upate the files and mode of the launching command
                launch_cmd["params"]["files"] = block_files["files"];
                launch_cmd["params"]["mode"]  = block_files["mode"];

                // write launch daemon command files
                if (ld_file)
                {
                    static int x = 2;
                    std::string fname = cmd_path + "cmd" + std::to_string(x) + ".json";
                    x++;

                    std::ofstream of(fname);
                    Json::StyledWriter writer;
                    of << writer.write(launch_cmd);
                    of.close();
                }

                // launcher available?
                if (!rm.has_launcher())
                {
                    slog() << "no available launcher agent, unable to finish the transfer";
                    store.update_sjob_stage(sid, "transfer.launch", sjob_stage::error);

                    // report job fail and bail
                    teardown(sid, flow);
                    job_fail("launcher agent not avaialbe, job failed", sid, rid);

                    return;
                }
                else
                {
                    // sjob state
                    store.update_sjob_state(sid, sjob_state::launching);

                    // and calls the MDTM to launch the transfer
                    auto res = rpc.call(rm.launcher().queue(), launch_cmd, 20);

                    //slog() << "mdtm launch command called, cmd: " << rpc_json;
                    slog() << "mdtm launch res: " << res;

                    // mark the block as tranferring
                    store.update_sjob_block_state(blockid, block_state::transferring);

                    // update the flow information
                    store.update_sjob_flow_field(task, "block", blockid);

                    // update the flow state
                    store.update_sjob_flow_state(task, flow_state::setting);

                    // update sjob state
                    // store.update_sjob_state(sid, sjob_state::transferring);
                    // slog() << "updated sjob state";

                    // update the rawjob state
                    store.update_rawjob_state(rid, rawjob_state::transferring);
                    slog() << "updated rawjob state";
                }

            } // all_assigned

        } // end status 3

    });
}

void Scheduler::bootstrap_transfer_job(std::string const & id)
{
    // now we can start working on the job
    slog(s_info) << "processing raw job id = " << id;

    // get the job from database
    auto job = store.get_rawjob_from_id(id);

    // check error
    if (job.isMember("error"))
    {
        slog(s_error) << "scheduler: error at getting raw job (id: " << id << ")";
        slog(s_error) << job["error"];
        return;
    }

#if 1
    // first check basic components
    if (!rm.has_launcher())
    {
        slog(s_error) << "laucher agent not available, unable to bootstrap the transfer job";
        store.update_rawjob_state(id, rawjob_state::error);
        store.update_collection_field("rawjob", id, "error", "no active launcher agent available");
        return;
    }
#endif

    // TODO: force it to be the besteffort job
    job["type"] = "best_effort";

    // if this is a dataset job
    bool ds = job.get("dataset", false).asBool();

    // rawjob
    slog() << "bootstrapping rawjob: " << job;

#if 1
    // create proxy certificate for both the source and dest site
    if (!create_raw_job_proxy_cert(job))
    {
        slog(s_error) << "scheduler::create_raw_job_proxy_certificate() error";

        store.update_rawjob_state(id, rawjob_state::error);
        store.update_collection_field("rawjob", id, "error", job["error"].asString());

        return;
    }
#endif

    // parse the destination path
    if (!parse_raw_job_dst(job))
    {
        slog(s_error) << "scheduler::parse_raw_job_dst() error";

        store.update_rawjob_state(id, rawjob_state::error);
        store.update_collection_field("rawjob", id, "error", job["error"].asString());

        return;
    }

    // parse the source files path
    bool parse_src_res = false;
    if (!ds) parse_src_res = parse_raw_job_src(job);
    else     parse_src_res = parse_raw_job_src_ds(job);

    if (!parse_src_res)
    {
        slog(s_error) << "scheduler::parse_raw_job_src() error";

        store.update_rawjob_state(id, rawjob_state::error);
        store.update_collection_field("rawjob", id, "error", job["error"].asString());

        return;
    }

    // check destination disk usage
    if ( !job["dst"]["dist"].empty() && 
            job["size"].asInt64() > job["dst"]["disk"]["size"]["available"].asInt64() * 0.95)
    {
        slog(s_error) << "scheduler::bootstrap() insufficient destination disk size";

        store.update_rawjob_state(id, rawjob_state::error);
        store.update_collection_field("rawjob", id, "error", "insufficient disk space");

        return;
    }

    // per source storage job
    Json::Value sjobs = create_per_storage_job(job);
    if (sjobs.size() < 1)
    {
        slog(s_error) << "scheduler::create_per_storage_job() error";

        store.update_rawjob_state(id, rawjob_state::error);
        store.update_collection_field("rawjob", id, "error", "create_per_storage_job() failed");

        return;
    }

    // site infos
    Json::Value ssite = store.get_site_info(sjobs[0]["srcsite"]["$oid"].asString());
    Json::Value dsite = store.get_site_info(sjobs[0]["dstsite"]["$oid"].asString());

    //slog() << "sjobs = " << sjobs;

    // delete already existing sjob entries
    // note: mostly for testing. in production if sjob for the given raw job id
    // already exists, we might have some serious problems
    auto deleted = store.del_sjobs(job["_id"]["$oid"].asString());
    slog() << deleted << " sjobs removed from db";

    // divide each subjob into small transfer blocks
    for ( auto & sjob : sjobs )
    {
        sjob["state"] = (int)sjob_state::bootstrap;  // initial job state
        sjob["tx_bytes"] = 0;                        // transferred bytes for the sjob

        sjob["max_flows"] = 1;              // the first block has to be transferred sequentially
        sjob["flows"] = Json::arrayValue;   // dtn pairs doing the transfer

        sjob["transfer_retry"] = 0;         // number of current transfer retries
        sjob["path_retry"] = 0;             // number of current path creation/verification retries

        // initial job stages
        sjob["stage"]["bootstrap"]["general"] = (int)sjob_stage::working;
        sjob["stage"]["bootstrap"]["query"]   = (int)sjob_stage::working;
        sjob["stage"]["bootstrap"]["match"]   = (int)sjob_stage::pending;

        sjob["stage"]["network"]["general"]   = (int)sjob_stage::pending;
        sjob["stage"]["network"]["planning"]  = (int)sjob_stage::pending;

        sjob["stage"]["transfer"]["general"]  = (int)sjob_stage::pending;
        sjob["stage"]["transfer"]["launch"]   = (int)sjob_stage::pending;
        sjob["stage"]["transfer"]["transfer"] = (int)sjob_stage::pending;
        if (ld_checksum) sjob["stage"]["transfer"]["checksum"] = (int)sjob_stage::pending;

        sjob["stage"]["teardown"]["general"]  = (int)sjob_stage::pending;

        // store the sjob into database
        auto sjob_id = store.insert("sjob", sjob);
    }

    bool reschedule_now = false;

    if (job["type"].asString() == "reserved")
    {
        // admission base rate
        // admission returns: 1 -- available now, 2 -- available in future, 3 -- not available
        //
        // if (admission() == 3) 
        // {
        //     set raw_job as denied in database;
        //     return;
        // }
        //
        // if (admission() == 1) 
        // {
        //     immediate_reschedule = true;
        // }
    }
    else if (job["type"].asString() == "best_effort")
    {
        // wait for periodic scheduling
    }
    else
    {
        throw std::runtime_error("unknown job type");
    }

    // insert job into database
    // store.insert()

    if (reschedule_now)
    {
        // reschedule now
        // dispatch()
    }

    return;

#if 0
    slog() << "scheduling for base jobs";

    // admission control for base jobs
    for ( auto & sjob : sjobs )
    {
        // base job
        admission(sjob, ssite, dsite, false);
    }

    slog() << "scheduling for extra jobs";

    // admission control for extra jobs
    for ( auto & sjob : sjobs )
    {
        sjob["rate"] = sjob["rate"].asDouble() * 5.0;
        admission(sjob, ssite, dsite, true);
    }
#endif

    // write
    //Json::StyledWriter writer;
    //std::cout << writer.write(ssite) << "\n";
}

bool Scheduler::create_raw_job_proxy_cert(Json::Value & job)
{
    std::string rid     = job["_id"]["$oid"].asString();
    std::string user    = job["user"]["$oid"].asString();
    std::string srcsite = job["srcsite"]["$oid"].asString();
    std::string dstsite = job["dstsite"]["$oid"].asString();

    // retrieve the user cilogon certificate from database
    auto src_keys = store.get_user_keys(user, srcsite);

    if (src_keys.isMember("error"))
    {
        job["error"] = "error at getting source site user certificate from db: " + src_keys["error"].asString();
        slog(s_error) << job["error"];
        return false;
    }

    // cilogon cert for the dest site
    auto dst_keys = store.get_user_keys(user, dstsite);

    if (dst_keys.isMember("error"))
    {
        job["error"] = "error at getting source site user certificate from db: " + dst_keys["error"].asString();
        slog(s_error) << job["error"];
        return false;
    }

    // generate proxy certificate
    auto src_proxy = utils::generate_proxy_cert(src_keys["keypem"].asString(), src_keys["cert"].asString());
    auto dst_proxy = utils::generate_proxy_cert(dst_keys["keypem"].asString(), dst_keys["cert"].asString());

    // add the generated proxy certs and original user certs into the raw_job entry
    Json::Value src_certs;
    src_certs["user_key"]   = src_keys["keypem"];
    src_certs["user_cert"]  = src_keys["cert"];
    src_certs["proxy_key"]  = src_proxy.proxy_private_key;
    src_certs["proxy_cert"] = src_proxy.signed_proxy_cert;

    Json::Value dst_certs;
    dst_certs["user_key"]   = dst_keys["keypem"];
    dst_certs["user_cert"]  = dst_keys["cert"];
    dst_certs["proxy_key"]  = dst_proxy.proxy_private_key;
    dst_certs["proxy_cert"] = dst_proxy.signed_proxy_cert;

    Json::FastWriter writer;

    bsoncxx::document::value v_src = bsoncxx::from_json(writer.write(src_certs));
    store.update_collection_field("rawjob", rid, "src_certs", v_src);

    bsoncxx::document::value v_dst = bsoncxx::from_json(writer.write(dst_certs));
    store.update_collection_field("rawjob", rid, "dst_certs", v_dst);

    return true;
}

bool Scheduler::parse_raw_job_dst(Json::Value & job)
{
    std::string dstsite = job["dstsite"]["$oid"].asString();
    std::string dstfile = job["dstfile"].asString();

    auto dsts = split(dstfile, '|');

    // sanity check for the dst path
    if (dsts.size() != 4)
    {
        job["error"] = "scheduler::parse_raw_job_dst(): unknown dst path";
        slog(s_error) << "scheduler::parse_raw_job_dst(): unknown dst path";
        return false;
    }

    // check whether the destination path comes form the same destination site
    if (dstsite != dsts[0])
    {
        job["error"] = "scheduler::parse_raw_job_dst(): dst path is not consistent with dst site.";
        slog(s_error) << "scheduler::parse_raw_job_dst(): dst path is not consistent with dst site.";
        return false;
    }

    // get remote DTN info
    std::string dtn_addr;
    Json::Value p;
    p["cmd"]     = "get_dtn_info";
    p["dtn"]     = dsts[1];
    p["storage"] = dsts[1] + '|' + dsts[2];
    p["path"]    = dsts[3];

    auto site_info = store.get_site_info(dstsite);
    auto url = std::string("https://") + site_info["url"].asString() + "/bde/command";
    auto r = portal.post(url, p);

    if (r.first != 200)
    {
        job["error"] = "error get_dtn_info().";
        utils::slog(s_error) << "Command " << p << " failed. "
                             << "Code: " << r.first << ", msg: " << r.second;
        return false;
    }

    slog() << "get_dtn_info: " << r.second;

    // write to job object
    job["dst"]["site"]["$oid"] = dstsite;
    job["dst"]["storage"]  = dsts[1] + '|' + dsts[2];
    job["dst"]["dtn"]      = dsts[1];
    job["dst"]["dtn_addr"] = r.second["dtn"]["ctrl_interface"]["ip"];
    job["dst"]["path"]     = dsts[3];
    job["dst"]["disk"]     = r.second["disk"];

    job.removeMember("dstfile");
    job.removeMember("dstsite");

    slog() << "dst site: " << job["dst"]["site"]["$oid"] 
        << ", storage: "   << job["dst"]["storage"] 
        << ", dtn: "       << job["dst"]["dtn"] 
        << ", dtn_addr: "  << job["dst"]["dtn_addr"] 
        << ", path: "      << job["dst"]["path"];

    //store.insert("temp", job);

    return true;
}

bool Scheduler::parse_raw_job_src(Json::Value & job)
{
    std::string user    = job["user"]["$oid"].asString();
    std::string srcsite = job["srcsite"]["$oid"].asString();
    std::string srcfile = job["srcfile"].asString();

    // get site info (url)
    auto site_info = store.get_site_info(srcsite);

    // split the source files
    auto srcfiles = split(srcfile, ',');

    Json::Value smap;
    Json::Value dtninfo;

    for (auto & src : srcfiles)
    {
        // srcs[0] = site
        // srcs[1] = storage
        // srcs[2] = dtn
        // srcs[3] = path

        auto srcs = split(src, '|');

        // sanity check
        if (srcs.size() != 4)
        {
            job["error"] = "scheduler::parse_raw_job_src(): unknown src path";
            slog(s_error) << "scheduler::parse_raw_job_src(): unknown src path";
            return false;
        }

        std::string site = srcs[0];
        std::string storage = srcs[1] + '|' + srcs[2];
        std::string dtn = srcs[1];
        std::string path = srcs[3];

        // check whether it is from the same source site
        if (srcsite != site)
        {
            job["error"] = "scheduler::parse_raw_job_src(): src path is not consisitent with the src site.";
            slog(s_error) << "scheduler::parse_raw_job_src(): src path is not consisitent with the src site.";
            return false;
        }

        // get dtn info from rest api call
        if ( !dtninfo.isMember(dtn) )
        {
            Json::Value p;
            p["cmd"] = "get_dtn_info";
            p["dtn"] = dtn;

            auto r = portal.post(std::string("https://") + site_info["url"].asString() + "/bde/command", p);

            if (r.first != 200) 
            {
                job["error"] = "error get_dtn_info()";
                return false;
            }

            dtninfo[dtn] = r.second["dtn"];
        }

        // init the object
        if ( !smap.isMember(storage) )
        {
            smap[storage] = Json::objectValue;
            smap[storage]["srcsite"]["$oid"] = srcsite;
            smap[storage]["srcstorage"] = storage;
            smap[storage]["srcdtn"] = dtn;
            //smap[storage]["srcdtn_addr"] = rm.graph().get_dtn_node(dtn).ctrl_ip();
            smap[storage]["srcdtn_addr"] = dtninfo[dtn]["ctrl_interface"]["ip"];
            smap[storage]["srcpath"] = Json::arrayValue;
        }

        // append the path
        smap[storage]["srcpath"].append(path);
    }

    slog() << "src_dtn_info: " << dtninfo;

    // expected job duration
    auto begin = job["submittedat"].asInt64();
    auto   end = job["deadline"].asInt64();
    //auto  time = end - begin;
    auto  time = end * 1000;

#if 0
    // get user key and cert
    auto keys = store.get_user_keys(user, srcsite);
    utils::CertSSH cssh(keys["keypem"].asString(), keys["cert"].asString());
#endif

    // prepare the src doc object
    Json::Value srcdoc(Json::arrayValue);

    // call dtn to expand full file list
    Json::Value expand = Json::objectValue;
    Json::Value rpc_req;

    // total size
    double total_size = 0;

    for ( auto & s : smap )
    {
        //expand["cmd"] = "dtn_expand_and_group_v2";
        expand["target"] = s["srcdtn"];
        expand["params"]["dst_path"] = job["dst"]["path"];
        expand["params"]["group_size"] = (Json::UInt64)ld_group_size;
        expand["params"]["max_files"] = 2000;  // optional, max number of files in each block
        expand["params"]["src_path"] = s["srcpath"];
        expand["params"]["result_in_db"] = false;  // TODO: set to false for now. needs to figure out the local site problem first
        expand["params"]["checksum"]["compute"] = ld_checksum;
        expand["params"]["checksum"]["algorithm"] = ld_checksum_alg;

        // expand through rest api calls
        rpc_req["cmd"] = "file_expand_and_group";
        rpc_req["params"] = expand;

        auto url = std::string("https://" + site_info["url"].asString() + "/bde/command");
        auto rpc_res = portal.post(url, rpc_req);
        slog() << "expand and group rest call res = " << rpc_res.first;

        if (rpc_res.first != 200) 
        {
            job["error"] = "error expand_and_group rest call";
            return false;
        }

        // result files
        auto & files = rpc_res.second;

        // failed to expand the file list
        if (files["error"].asString() != "OK" || files["code"].asInt() != 0) 
        {
            job["error"] = "error expand_and_group response code";
            slog() << files;
            return false;
        }

        // get the expanded size
        size_t size = files["total_size"].asUInt64();

        // accumulate the size
        total_size += size;

        // record the size
        s["size"] = (Json::UInt64)size;

        // get the necessary bandwidth
        s["baserate"] = size / (time/1000);

        // prepare for blocks and files
        s["blocks"] = Json::arrayValue;
        int num_files = 0;

        for (auto & group : files["files"]["groups"])
        {
            // mdtm transfer mode:
            //   0. https://cdcvs.fnal.gov/redmine/projects/bigdata-express/wiki/MDTM_Launching_Daemon
            //   1. for folder creation (parent_folder_list is true), always mode 1
            //   2. for actual transfers (parent_folder_list is false or not exist), the mode
            //      value depends on the number of files in this group. If it is a single file,
            //      use mode 0, otherwise use mode 1.
            if ( group.isMember("parent_folder_list") 
                    && group["parent_folder_list"].asBool() )
            {
                group["mode"] = 1;
            }
            else
            {
                group["mode"] = (group["files"].size() == 1) ? 0 : 1;
            }

            // insert into block table
            std::string blockid = store.insert("block", group);

            // block metadata
            Json::Value block = Json::objectValue;
            block["id"] = blockid;
            block["state"] = 0;
            block["size"] = group["size"];
            block["files"] = group["files"].size();
            block["tx_bytes"] = 0;

            // insert block metadata into sjob
            s["blocks"].append(std::move(block));

            // number of files in the block
            num_files += group["files"].size();
        }

        // total number of files
        s["files"] = num_files;

        // append the object
        srcdoc.append(std::move(s));
    }

    job["src"] = srcdoc;
    job.removeMember("srcfile");
    job.removeMember("srcsite");

#if 0
    for (auto & s : srcdoc)
    {
        slog() << "src site: " << s["srcsite"]["$oid"] 
            << ", storage: " << s["srcstorage"] 
            << ", dtn: " << s["srcdtn"] 
            << ", dtn_addr: " << s["srcdtn_addr"];

        slog() << "path(s): ";

        for (auto & f : s["srcpath"])
        {
            slog() << "    " << f;
        }

        slog() << "files: ";

        for (auto & f : s["files"])
        {
            slog() << "    " << f["name"] << ",\t" << f["size"];
        }

        slog() << "    total size = " << s["size"] << " KB, expected transfer rate = " << s["baserate"] << " Bytes/sec";
    }
#endif

    job["size"] = total_size;
    store.update_rawjob_size(job["_id"]["$oid"].asString(), total_size);

    slog() << "after parse src: " << job;

    //slog(s_info) << srcdoc;

    return true;
}

bool Scheduler::parse_raw_job_src_ds(Json::Value & job)
{
    std::string user    = job["user"]["$oid"].asString();
    std::string srcsite = job["srcsite"]["$oid"].asString();

    // get site info (url)
    auto site_info = store.get_site_info(srcsite);

    // files
    Json::Value const& files = job["files"];

    Json::Value smap;
    Json::Value dtninfo;

    for (auto & file : files)
    {
        // file[0] = src
        // file[1] = dst
        // file[2] = size
        // file[3] = 0

        // srcs[0] = site
        // srcs[1] = storage
        // srcs[2] = dtn
        // srcs[3] = path
        std::string src = file[0].asString();
        auto srcs = split(src, '|');

        std::string dst = file[1].asString();
        auto dsts = split(dst, '|');

        // sanity check
        if (srcs.size() != 4)
        {
            job["error"] = "scheduler::parse_raw_job_src(): unknown src path";
            slog(s_error) << "scheduler::parse_raw_job_src(): unknown src path";
            return false;
        }

        std::string site = srcs[0];
        std::string storage = srcs[1] + '|' + srcs[2];
        std::string dtn = srcs[1];
        std::string path = srcs[3];

        // check whether it is from the same source site
        if (srcsite != site)
        {
            job["error"] = "scheduler::parse_raw_job_src(): src path is not consisitent with the src site.";
            slog(s_error) << "scheduler::parse_raw_job_src(): src path is not consisitent with the src site.";
            return false;
        }

        // get dtn info from rest api call
        if ( !dtninfo.isMember(dtn) )
        {
            Json::Value p;
            p["cmd"] = "get_dtn_info";
            p["dtn"] = dtn;

            auto r = portal.post(std::string("https://") + site_info["url"].asString() + "/bde/command", p);

            if (r.first != 200) 
            {
                job["error"] = "error get_dtn_info()";
                return false;
            }

            dtninfo[dtn] = r.second["dtn"];
        }

        // init the object
        if ( !smap.isMember(storage) )
        {
            smap[storage] = Json::objectValue;
            smap[storage]["srcsite"]["$oid"] = srcsite;
            smap[storage]["srcstorage"] = storage;
            smap[storage]["srcdtn"] = dtn;
            //smap[storage]["srcdtn_addr"] = rm.graph().get_dtn_node(dtn).ctrl_ip();
            smap[storage]["srcdtn_addr"] = dtninfo[dtn]["ctrl_interface"]["ip"];
            smap[storage]["srcpath"] = Json::arrayValue;
        }

        // extract the src path and file
        std::string spath;
        std::string sfile;

        std::string dpath = dsts[3];
        if (dpath.back() != '/') dpath += '/';

        auto found = path.find_last_of("/\\");
        if (found == std::string::npos)
        {
            // only contains filename
            sfile = path;
        }
        else
        {
            spath = path.substr(0, found+1);
            sfile = path.substr(found+1);
        }

        // prepare an entry
        // ["/DATA/10G/01/", "f01.txt", "/data2/10G/01/", 1000]
        Json::Value entry = Json::objectValue;
        entry["spath"] = spath;  // src path
        entry["sfile"] = sfile;  // src file
        entry["dpath"] = dpath;  // dst path
        entry["size"]  = file[2];  // file size

        // append the path entry
        smap[storage]["srcpath"].append(entry);
    }

    slog() << "src_dtn_info: " << dtninfo;

    // expected job duration
    auto begin = job["submittedat"].asInt64();
    auto   end = job["deadline"].asInt64();
    //auto  time = end - begin;
    auto  time = end * 1000;

    // prepare the src doc object
    Json::Value srcdoc(Json::arrayValue);

    // call dtn to expand full file list
    Json::Value expand = Json::objectValue;
    Json::Value rpc_req;

    // total size
    double total_size = 0;

    for ( auto & s : smap )
    {
        Json::Value group;
        group["mode"] = 1;
        group["files"] = Json::arrayValue;

        size_t size = 0;
        for (auto const& e : s["srcpath"]) 
        {
            size += e["size"].asUInt64();

            Json::Value file1 = Json::arrayValue;
            Json::Value file2 = Json::arrayValue;

            // create folder
            file1[0] = e["spath"];
            file1[1] = e["dpath"];
            file1[2] = 0;
            file1[3] = 0;
            group["files"].append(file1);

            // copy file
            file2[0] = e["spath"].asString() + e["sfile"].asString();
            file2[1] = e["dpath"].asString() + e["sfile"].asString();
            file2[2] = e["size"];
            file2[3] = 0;
            group["files"].append(file2);
        }

        // a single group
        group["size"] = (Json::UInt64)size;

        // insert into block table
        std::string blockid = store.insert("block", group);

        // block metadata
        Json::Value block = Json::objectValue;
        block["id"] = blockid;
        block["state"] = 0;
        block["size"] = (Json::UInt64)size;
        block["files"] = group["files"].size();
        block["tx_bytes"] = 0;

        // insert block metadata into sjob
        s["blocks"][0] = std::move(block);

        // record the size
        s["size"] = (Json::UInt64)size;

        // get the necessary bandwidth
        s["baserate"] = size / (time/1000);

        // total number of files
        s["files"] = group["files"].size()/2;

        // accumulate the size
        total_size += size;

        // append the object
        srcdoc.append(std::move(s));
    }

    job["src"] = srcdoc;
    job.removeMember("srcfile");
    job.removeMember("srcsite");

    job["size"] = total_size;
    store.update_rawjob_size(job["_id"]["$oid"].asString(), total_size);

    slog() << "after parse src: " << job;

    //slog(s_info) << srcdoc;

    return true;
}

Json::Value Scheduler::create_per_storage_job(Json::Value & job)
{
    for ( auto & s : job["src"] )
    {
        s["rawjob"]     = job["_id"];
        s["type"]       = job["type"];
        s["submittedat"]=  job["submittedat"];
        s["priority"]   = job["priority"];
        s["dstsite"]    = job["dst"]["site"];
        s["dststorage"] = job["dst"]["storage"];
        s["dstdtn"]     = job["dst"]["dtn"];
        s["dstdtn_addr"]= job["dst"]["dtn_addr"];
        s["dstpath"]    = job["dst"]["path"];
        s["rate"]       = s["baserate"];
    }

    return job["src"];
}

void Scheduler::admission(Json::Value const & sjob, Json::Value const & ssite, Json::Value const & dsite, bool extra)
{
    std::string surl = ssite["url"].asString();
    std::string durl = dsite["url"].asString();

    // set the rate for test
    //sjob["rate"] = 50.0;

    slog(s_info) << "dst site url: " << durl;
    slog(s_info) << "src site url: " << surl;

    Json::Value params;

    // try lock src and destination
    params["cmd"] = "trylock";
    auto res = portal.post(std::string("https://") + surl + "/bde/command", params);
    //slog(s_info) << res.first << ", " << res.second;
    slog() << "trylock the source site ... " << res.second["locked"];

    res = portal.post(std::string("https://") + durl + "/bde/command", params);
    //slog(s_info) << res.first << ", " << res.second;
    slog() << "trylock the destination site ... " << res.second["locked"];

    // query src dtns
    // -- 1. yes, available right away
    // -- 2. no, but can be available after killing a job
    // -- 3. no, and will not be available even after killing other Re jobs
    // -- 4. no, requested rate is larger than the site capacity
    params["cmd"] = "probe_rate";
    params["storage"] = sjob["srcstorage"];
    params["rate"] = sjob["rate"];
    slog() << "sending admission request to source with " << (extra ? "EXTRA" : "BASE") << " rate r = " << sjob["rate"].asDouble()/1e6 << " MB/sec ...";
    res = portal.post(std::string("https://") + surl + "/bde/command", params);
    slog(s_info) << "probe_rate_source: http_code = " << res.first << ", api_code = " << res.second["code"];

    // not accepted
    if (res.second["code"] >= 3)
    {
        slog() << "source site admission failed";

        params["cmd"] = "unlock";
        portal.post(std::string("https://") + durl + "/bde/command", params);
        slog() << "unlock the destination site ... true.";

        portal.post(std::string("https://") + surl + "/bde/command", params);
        slog() << "unlock the source site ... true.";
        return;
    }

    // query dst dtns
    // -- 1. yes -- 2. no -- 3. no -- 4. no
    params["cmd"] = "probe_rate";
    params["storage"] = sjob["dststorage"];
    params["rate"] = sjob["rate"];
    slog() << "sending admission request to destination with " << (extra ? "EXTRA" : "BASE") << " rate r = " << sjob["rate"].asDouble()/1e6 << " MB/sec ...";
    res = portal.post(std::string("https://") + durl + "/bde/command", params);
    slog(s_info) << "probe_rate_dest: http_code = " << res.first << ", api_code = " << res.second["code"];

    // not accepted
    if (res.second["code"] >= 3)
    {
        slog() << "destination site admission failed";

        params["cmd"] = "unlock";
        portal.post(std::string("https://") + durl + "/bde/command", params);
        slog() << "unlock the destination site ... true.";

        portal.post(std::string("https://") + surl + "/bde/command", params);
        slog() << "unlock the source site ... true.";
        return;
    }

    slog() << "admission succeed on both source and destination.";
    
    // find dtn list on source site
    params["cmd"] = "query_path_dtns";
    params["storage"] = sjob["srcstorage"];
    params["rate"] = sjob["rate"];
    slog() << "sending dtn provisioning request to source site ...";
    auto res1 = portal.post(std::string("https://") + surl + "/bde/command", params);
    slog() << "dtn provisioning source: http_code = " << res1.first << ", api_code = " << res1.second["code"] << ", dtn_list: ";
    for(auto const & d : res1.second["dtns"])
        slog() << "    dtn: " << d["host"] << " @ rate = " << d["rate"].asDouble()/1e6 << " MB/sec";
    
    // find dtn list on destination site
    params["cmd"] = "query_path_dtns";
    params["storage"] = sjob["dststorage"];
    params["rate"] = sjob["rate"];
    slog() << "sending dtn provisioning request to destination site ...";
    auto res2 = portal.post(std::string("https://") + durl + "/bde/command", params);
    slog() << "dtn provisioning destination: http_code = " << res2.first << ", api_code = " << res2.second["code"] << ", dtn_list: ";
    for(auto const & d : res2.second["dtns"])
        slog() << "    dtn: " << d["host"] << " @ rate = " << d["rate"].asDouble()/1e6 << " MB/sec";

    // match src dtns with dst dtns
    // returns one or multiple { src-dtn -> dst-dtn (bandwidth) } paths
    // store the path into the json object
    std::vector<std::pair<std::string, double>> src_dtns;
    std::vector<std::pair<std::string, double>> dst_dtns;

    for (auto const & dr : res1.second["dtns"])
    {
        src_dtns.emplace_back(std::make_pair(dr["host"].asString(), dr["rate"].asDouble()));
    }

    for (auto const & dr : res2.second["dtns"])
    {
        dst_dtns.emplace_back(std::make_pair(dr["host"].asString(), dr["rate"].asDouble()));
    }

    // match
    auto dtn_pairs = rm.match_path_dtns(src_dtns, dst_dtns);

    auto raw_job_id  = sjob["rawjob"]["$oid"].asString();
    auto src_storage = sjob["srcstorage"].asString();
    auto dst_storage = sjob["dststorage"].asString();

    auto raw_job = store.get_rawjob_from_id(raw_job_id);
    auto user = raw_job["user"]["$oid"].asString();
    auto site = raw_job["srcsite"]["$oid"].asString();
    auto keys = store.get_user_keys(user, site);

    int zz = 0;

    for (auto const & dp : dtn_pairs)
    {
        rm.create_job(raw_job_id,
                src_storage, dst_storage,
                std::get<0>(dp), std::get<1>(dp),
                std::get<2>(dp), extra);

        slog(s_debug) << "job created: from storage (" << src_storage 
            << "), via dtn (" << std::get<0>(dp) << " -> " << std::get<1>(dp) 
            << "), @ rate = " << std::get<2>(dp)/1e6 << " MB/s";

        if (extra == false && zz == 0)
        {
            // only do job size < 200 MB
            if (sjob["size"].asDouble() < 200 * 1024 * 1024)
            {
                std::thread t([keys, sjob]() {
                    utils::CertSSH cssh(keys["keypem"].asString(), keys["cert"].asString());
                    std::string src = "gsiftp://" + sjob["srcdtn"].asString() + ":5001" + sjob["srcpath"][0].asString();
                    std::string dst = "gsiftp://" + sjob["dstdtn"].asString() + ":5001" + sjob["dstpath"].asString();
                    std::string cmd = "/usr/local/mdtmftp/1.0.1/bin/mdtm-ftp-client -vb -p 4 " + src + " " + dst;

                    slog() << cssh.exec(cmd);
                });

                t.detach();

                slog() << "mdtmftp job dispatched";

            }
            else
            {
                slog() << "Skipping the large job for now";
            }
        }

        ++zz;
    }


    // unlock both sites
    params["cmd"] = "unlock";
    portal.post(std::string("https://") + durl + "/bde/command", params);
    slog() << "unlock the destination site ... true.";

    portal.post(std::string("https://") + surl + "/bde/command", params);
    slog() << "unlock the source site ... true.";
    return;
}


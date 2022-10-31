#include "resourcemanager.h"

using namespace std;
using namespace utils;

ResourceManager::ResourceManager(JobManager & jm, SiteStore & store)
: jm(jm)
, rg(jm)
, store(store)
, construct_mtx()
{
    rg.reset();
    rg.add_gateway();
}

ResourceManager::~ResourceManager()
{
}

void ResourceManager::construct()
{
    // mutex lock to avoid concurrent construction of the graph
    std::lock_guard<std::mutex> lock(construct_mtx);

    // reset the resource graph
    rg.reset();

    // gateway
    rg.add_gateway();

    // storage nodes
    auto storages = store.get_storage_list();
    for (auto const & s : storages)
    {
        auto & node = rg.add_storage_node(
                s["id"].asString(), 
                s["name"].asString(), 
                StorageType::internal, 
                s["io_capacity"].asDouble() );

        // set properties
        node.set_queue( s["queue_name"].asString() );
        node.set_root_folder( s["root_folder"].asString() );
        node.set_properties( s );

        // log
        slog() << "storage: " << node.rid() << " (" << node.name() << ") added.";
    }

    slog() << storages.size() << " storage nodes created";

    // dtn nodes
    auto dtns = store.get_dtn_list();
    for (auto const & d : dtns)
    {
        // dtn object
        auto & node = rg.add_dtn_node(d["id"].asString(), d["name"].asString());

        // control (management) ip
        node.set_ctrl_ip(d["ctrl_interface"]["ip"].asString());
        node.set_queue(d["queue_name"].asString());
        node.set_properties(d);

        // dtn to gateway edges
        rg.add_edge_to_gateway(node.rid(), d["data_interfaces"][0]["rate"].asDouble());

        // log
        slog() << "dtn: " << node.rid() << " (" << node.name() << ") added.";
    }

    slog() << dtns.size() << " dtn nodes created";

    // storage to dtn edges
    auto sdmaps = store.get_storage_dtn_map();
    for (auto const & sd : sdmaps)
    {
        try
        {
            auto s = sd["storage"].asString();
            auto d = sd["dtn"].asString();

            rg.add_edge(s, d, 4e10);
            rg.get_storage_node(s).set_one_dtn_node(rg.get_dtn_node(d));

            slog() << "storage: " << sd["storage"] << " -> DTN: " << sd["dtn"] << " map added.";
        }
        catch(...)
        {
            slog() << "Fail to add storage to dtn map: " << sd["storage"] << " -> " << sd["dtn"];
        }
    }

    slog() << sdmaps.size() << " storage to dtn map created";
}

void ResourceManager::add_modules(std::string const & cid, std::string const & queue, Json::Value const & modules)
{
    // iteration 1: add modules
    for (auto const & m : modules)
    {
        //slog() << m;

        auto   id = m["id"].asString();
        auto type = m["type"].asString();
        auto name = m["name"].asString();

        if (type == "DTN")
        {
            // insert node
            auto & node = rg.add_dtn_node(id, name);

            // control (management) ip
            node.set_ctrl_ip(m["ctrl_interface"]["ip"].asString());
            node.set_queue(queue);
            node.set_properties(m);

            // dtn to gateway edges
            rg.add_edge_to_gateway(id, m["data_interfaces"][0]["rate"].asDouble());

            // log
            slog() << "dtn: " << id << " (" << name << ") added.";
        }
        else if (type == "LocalStorage")
        {
            // insert node
            auto & node = rg.add_storage_node(id, name, StorageType::internal, 1000000);

            // set properties
            node.set_queue( queue );
            node.set_root_folder( m["root_folder"].asString() );
            node.set_properties( m );

            // log
            slog() << "storage: " << id << " (" << name << ") added.";
        }
        else if (type == "Launcher")
        {
            if (rg.has_launcher())
            {
                slog(s_error) << "already has a launcher agent registered, will delete the current launcher";
                rg.del_launcher();
            }

            // add the new module
            rg.add_launcher(id, queue);
        }
        else if (type == "SDN")
        {
            if (rg.has_sdn())
            {
                slog(s_error) << "already has an SDN agent registered, will delete the current SDN agent";
                rg.del_sdn();
            }

            // add the new module
            rg.add_sdn(id, queue);
        }
        else
        {
            slog(s_error) << "unknown module type: " << type;
        }
    }

    // iteration 2: add dtn - stroage links
    for (auto const & m : modules)
    {
        auto   id = m["id"].asString();
        auto type = m["type"].asString();
        auto name = m["name"].asString();

        if (type == "DTN")
        {
            for (auto const & s : m["storages"])
            {
                auto   sid = s["id"].asString();
                auto stype = s["type"].asString();
                auto sname = s["name"].asString();

                double rate = (stype == "local") ? 4e10 : 4e10;

                try
                {
                    rg.add_edge(sid, id, rate);
                    rg.get_storage_node(sid).set_one_dtn_node(rg.get_dtn_node(id));

                    slog() << "storage (" << sname << ") -> dtn (" << name << ") map added.";
                }
                catch (...)
                {
                    slog() << "Fail to add storage to dtn map: " << sname << " -> " << name;
                }
            }
        }
    }

    // print
    rg.print();


    get_active_dtn_topology();
}

void ResourceManager::del_modules(std::string const & cid, Json::Value const & modules)
{
    for (auto const & m : modules)
    {
        auto   id = m["id"].asString();
        auto type = m["type"].asString();
        auto name = m["name"].asString();

        // delete the node
        rg.del_node(id);

        // log
        slog() << type << ": " << id << "(" << name << ") has been deleted";
    }

    // print
    rg.print();
}


std::string const &
ResourceManager::get_node_queue_name(std::string const & nid)
{
    return rg.get_node(nid).queue();
}

Json::Value
ResourceManager::get_active_storage_list()
{
    Json::Value res = Json::arrayValue;
    auto storages = rg.get_storage_nodes();

    for (auto const & s_pair : storages)
    {
        // get the storage node
        auto const & s = s_pair.second;

        // this is an isolated storage
        if (!s.has_out_edges()) continue;

        // get its props
        Json::Value js = s.properties();

        // find the first connected DTN ctrl ip
        auto const & dtn = dynamic_cast<DTNNode const &>(s.get_out_edges()[0]->get_dst_node());
        js["dtn"] = dtn.ctrl_ip();

        res.append(js);
    }

    slog() << "get_active_storage_list(): " << res;
    return res;
}

Json::Value
ResourceManager::get_active_dtn_list()
{
    Json::Value res = Json::arrayValue;
    auto dtns = rg.get_dtn_nodes();

    for (auto const & d_pair : dtns)
    {
        // get the dtn node
        auto const & d = d_pair.second;

        // this is an isolated dtn
        if (!d.has_out_edges()) continue;

        // get its props
        Json::Value jd = d.properties();

        // push to the array
        res.append(jd);
    }

    slog() << "get_active_dtn_list(): " << res;
    return res;
}

Json::Value
ResourceManager::get_active_dtn_topology()
{
    Json::Value res = Json::objectValue;
    res["dtns"] = Json::arrayValue;

    for (auto const & d_pair : rg.get_dtn_nodes())
    {
        // get the dtn node
        auto const & d = d_pair.second;

        // this is an isolated dtn
        if (!d.has_out_edges()) continue;

        // get its props
        Json::Value const& jd = d.properties();
        Json::Value dtn = Json::objectValue;

        dtn["id"] = jd["id"];
        dtn["label"] = jd["name"];
        dtn["ls"] = Json::arrayValue;

        for(auto const& e : d.get_in_edges())
        {
            auto const& s = e->get_src_node();

            Json::Value const& js = s.properties();
            Json::Value ls = Json::objectValue;

            ls["device"] = js["name"];
            ls["roots"] = js["root_folders"];
            ls["id"] = js["id"];

            dtn["ls"].append(ls);
        }

        // push to the array
        res["dtns"].append(dtn);
    }

    slog() << "get_active_dtn_topology(): " << res;
    return res;
}

int ResourceManager::probe_path_rate(std::string const & storage, double rate, bool logging)
{
    if (logging)
    {
        slog() << "start probe_path_rate (admission control)";
    }

    // find the storage node
    auto & snode = rg.get_storage_node(storage);

    // storage node object
    double cap = snode.capacity();
    double sre = snode.sum_re();
    double srb = snode.sum_rb();

    int res_storage = 0;

    if (logging)
    {
        slog() << "examing storage (" << storage << ") capacity:";
        slog() << "    requested rate = " << rate/1e6 << " MB/sec, cap = " << cap/1e6 << " MB/sec, srb = " << srb/1e6 << " MB/sec, sre = " << sre/1e6 << " MB/sec";
    }

    // test the capacity of storage node
    if (cap < rate)
    {
        if (logging) slog() << "    insufficient storage capacity, admission failed";
        return 4;
    }
    else if (cap - srb - sre > rate)
    {
        if (logging) slog() << "    admission for storage bw accepted.";
        res_storage = 1;
    }
    else if (cap - srb > rate)
    {
        if (logging) slog() << "    admission for storage bw accepted on the condition of removing extra jobs.";
        res_storage = 2;
    }
    else
    {
        if (logging) slog() << "    insufficient storage bw, admission failed";
        return 3;
    }

    //if (logging)
    //slog(s_debug) << "res_storage = " << res_storage;

    // now we probe the (storage - dtn) links
    auto & sd_edges = snode.get_out_edges();
    double avail_edge_rate = 0;
    double avail_edge_rate_extra = 0;

    if (logging)
    slog() << "total " << sd_edges.size() << " dtns attached to the requested storage";

    for( auto & p_sd_edge : sd_edges )
    {
        // de-reference the pointer
        auto & sd_edge = *p_sd_edge;

        // basic properties
        double sd_cap = sd_edge.capacity();
        double sd_sre = sd_edge.sum_re();
        double sd_srb = sd_edge.sum_rb();

        if (logging)
        {
        slog() << "    examing links from storage via dtn (" << sd_edge.get_dst_node().rid() << ")";
        slog(s_debug) << "        dtn imbound bandwidth capacity: " << sd_cap/1e6 << " MB/sec, sum_re: " << sd_sre/1e6 << " MB/sec, sum_rb: " << sd_srb/1e6 << " MB/sec";
        }

        // rate > cap?
        //if (rate > sd_cap) return 4;

        // available rate
        double sd_rate = sd_cap - sd_sre - sd_srb;
        double sd_rate_extra = sd_cap - sd_srb;

        //if (logging)
        //slog() << "            available bw = " << sd_rate/1e6 << " MB/sec, available bw (extra) = " << sd_rate_extra/1e6 << " MB/sec";

        // further look into dtn - gateway links
        auto & dtn = sd_edge.get_dst_node();
        auto & dg_edges = dtn.get_out_edges();

        if (dg_edges.size() != 1) 
            throw std::runtime_error("dtn to gateway edge != 1");

        auto & dg_edge = *(dg_edges[0]);
        double dg_cap = dg_edge.capacity();
        double dg_sre = dg_edge.sum_re();
        double dg_srb = dg_edge.sum_rb();

        if (logging)
        slog() << "        dtn outbound bandwidth capacity: " << dg_cap/1e6 << " MB/sec, sum_re: " << dg_sre/1e6 << " MB/sec, sum_rb: " << dg_srb/1e6 << " MB/sec";

        // rate > cap?
        //if (rate > dg_cap) return 4;

        double dg_rate = dg_cap - dg_sre - dg_srb;
        double dg_rate_extra = dg_cap - dg_srb;

        //if (logging)
        //slog() << "            available bw = " << dg_rate/1e6 << " MB/sec, available bw (extra) = " << dg_rate_extra/1e6 << " MB/sec";

        // get the overall path rate
        double rate = sd_rate < dg_rate ? sd_rate : dg_rate;
        double rate_extra = sd_rate_extra < dg_rate_extra ? sd_rate_extra : dg_rate_extra;

        if (rate < 1000) rate = 0;
        if (rate_extra < 1000) rate_extra = 0;

        if (logging)
        slog() << "        >> overall available_bw = " << rate/1e6 << " MB/sec, available_bw_extra = " << rate_extra/1e6 << " MB/sec";

        // accumulate the path rate
        avail_edge_rate += rate;
        avail_edge_rate_extra += rate_extra;
    }

    if (logging)
    slog(s_debug) << "    storage to gateway combined edge_rate = " << avail_edge_rate/1e6 << " MB/sec, edge_rate_extra = " << avail_edge_rate_extra/1e6 << " MB/sec";

    int res_edges = 0;

    if (avail_edge_rate > rate)
    {
        res_edges = 1;
        if (logging) slog() << "    admission for network path accepted.";
    }
    else if (avail_edge_rate_extra > rate)
    {
        res_edges = 2;
        if (logging) slog() << "    admission for network path accepted on the condition of removing extra jobs.";
    }
    else
    {
        if (logging) slog() << "    insufficient network path bw.";
        return 3;
    }

    // final query results
    if (res_storage == 1 && res_edges == 1)
    {
        if (logging) slog() << "admission succeed.";
        return 1;
    }
    else
    {
        if (logging) slog() << "admission succeed on the condition of removing extra jobs.";
        return 2;
    }
}

std::vector<std::pair<std::string, double>> ResourceManager::query_path_dtns(std::string const & storageid, double rate)
{
    slog(s_debug) << "start dtn provisioning:";

    // get storage node
    StorageNode & snode = rg.get_storage_node(storageid);

    slog() << "checking available bandwidth (calling probe_path_rate()).";

    int i = 0;

    // mark Re jobs to make space for new job
    while (int r = probe_path_rate(storageid, rate, false) != 1)
    {
        // not enough bandwidth
        if (r == 3) return std::vector<std::pair<std::string, double>>();

        // shouldnt happen, but just to make sure that the storage node has running jobs
        if (snode.get_job_size() == 0) return std::vector<std::pair<std::string, double>>();

        if (i == 0) slog() << "    not available bandwidth. Looking to suspend the largest extra job:";

        // find the largest job
        BaseJob & largest_job = snode.find_largest_re_job();

        // mark the job to be stopped
        largest_job.set_state(JobState::marked_for_prealloc);
        store.update_stream_state(largest_job.id(), (int)JobState::marked_for_prealloc);

        slog() << "        job: " << largest_job.id() << " has been suspended.";

        ++i;

        if ( i > 100 ) slog() << "something is not right in making spaces for new job";
    }

    slog() << "site has enough bandwidth.";

    // now working on find the list of dtns that will accomodate the rate
    double remaining_rate = rate;
    std::vector<std::pair<std::string, double>> dtn_rate_v;

    auto & sd_edges = snode.get_out_edges();

    slog() << "loop through " << sd_edges.size() << " dtns to find the list of available dtns that will accomodate the reuqested rate";

    for (auto & p_sd_edge : sd_edges)
    {
        auto & sd_edge = *p_sd_edge;

        auto & dtn = sd_edge.get_dst_node();
        double sd_rate = sd_edge.available_rate();

        auto & dg_edges = dtn.get_out_edges();
        double dg_rate = dg_edges[0]->available_rate();

        double edge_rate = std::min(sd_rate, dg_rate);

        if (edge_rate < 1000)
        {
            slog() << "    dtn (" << dtn.rid() << ") is fully occupied. Looking for next DTN.";
            continue;
        }

        if (edge_rate > remaining_rate)
        {
            dtn_rate_v.emplace_back(dtn.rid(), remaining_rate);
            slog() << "    dtn (" << dtn.rid() << ") link rate = " << edge_rate/1e6 << " MB/sec, provisioned rate = " << remaining_rate/1e6 << " MB/sec, remaining request rate = 0 MB/sec";
            remaining_rate = 0;
            break;
        }
        else
        {
            dtn_rate_v.emplace_back(dtn.rid(), edge_rate);
            remaining_rate -= edge_rate;
            slog() << "    dtn (" << dtn.rid() << ") link rate = " << edge_rate/1e6 << " MB/sec, provisioned rate = " << edge_rate/1e6 << " MB/sec, remaining request rate = " << remaining_rate/1e6 << " MB/sec";
        }
    }

    return dtn_rate_v;
}

std::vector<std::tuple<std::string, std::string, double>>
    ResourceManager::match_path_dtns(
        std::vector<std::pair<std::string, double>> & src_dtns, 
        std::vector<std::pair<std::string, double>> & dst_dtns )
{

    slog(s_debug) << "match_path_dtns";

    int nsrc = src_dtns.size();
    int ndst = dst_dtns.size();

    int isrc = 0;
    int idst = 0;

    bool done = false;

    vector<tuple<string, string, double>> dtn_pairs;

    while (!done)
    {
        auto & s_dr = src_dtns[isrc];
        auto & d_dr = dst_dtns[idst];

        double s_rate = s_dr.second;
        double d_rate = d_dr.second;

        double rate = 0.0;

        if (s_rate == d_rate)
        {
            rate = d_rate;

            s_dr.second -= s_rate;
            d_dr.second -= d_rate;

            ++isrc;
            ++idst;
        }
        if (s_rate > d_rate)
        {
            rate = d_rate;

            s_dr.second -= d_rate;
            d_dr.second -= d_rate;

            ++idst;
        }
        else
        {
            rate = s_rate;

            s_dr.second -= s_rate;
            d_dr.second -= s_rate;

            ++isrc;
        }

        // push to result
        dtn_pairs.emplace_back(make_tuple(s_dr.first, d_dr.first, rate));

        // done?
        if (isrc >= nsrc && idst >= ndst)
        {
            done = true;
        }
        else if ( (isrc >= nsrc && idst < ndst) || (isrc < nsrc && idst >= ndst) )
        {
            throw std::runtime_error("something wrong in dtn matching");
        }
    }

    return dtn_pairs;
}

void ResourceManager::create_job(std::string const & rawjob_id,
        std::string const & src_storage, std::string const & dst_storage,
        std::string const & src_dtn, std::string const & dst_dtn,
        double rate, bool extra)
{
    JobGroup jg = extra ? JobGroup::extra : JobGroup::base;

    auto & job = jm.add_basic_job(utils::guid(), jg, JobState::running, rate);

    auto & snode = rg.get_storage_node(src_storage);
    auto & dnode = rg.get_dtn_node(src_dtn);
    auto & gnode = rg.get_general_node("gateway");

    snode.add_job(job);
    dnode.add_job(job);
    gnode.add_job(job);

    rg.get_edge(src_storage, src_dtn).add_job(job);
    rg.get_edge(src_dtn, "gateway").add_job(job);

    // insert into database
    Json::Value j(Json::objectValue);
    j["rawjob"]["$oid"] = rawjob_id;
    j["guid"] = job.id();
    j["srcstorage"] = src_storage;
    j["dststorage"] = dst_storage;
    j["srcdtn"] = src_dtn;
    j["dstdtn"] = dst_dtn;
    j["rate"] = rate;
    j["type"] = (int)jg;
    j["state"] = (int)JobState::running;

    store.insert("stream", j);
}

std::vector<DTNNode *> ResourceManager::get_storage_connected_dtns(std::string const & storage_id)
{
    if (!rg.has_storage_node(storage_id))
        throw std::runtime_error("invalid storage id while trying to get the connected dtns");

    StorageNode & snode = rg.get_storage_node(storage_id);
    auto const & edges = snode.get_out_edges();

    std::vector<DTNNode *> dtns;

    for (auto const & edge : edges)
    {
        RNode & dtn = edge->get_dst_node();
        dtns.push_back(dynamic_cast<DTNNode *>(&dtn));
    }

    return dtns;
}









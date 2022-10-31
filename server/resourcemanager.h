#ifndef BDE_SERVER_RESOURCE_MANANGER_H
#define BDE_SERVER_RESOURCE_MANANGER_H

#include "resourcegraph.h"
#include "jobmanager.h"
#include "sitestore.h"

#include <mutex>
#include <tuple>

class ResourceManager
{
public:

    ResourceManager(JobManager & jm, SiteStore & store);
    ~ResourceManager();

    RGraph const & graph() const
    { return rg; }

    RGraph       & graph()
    { return rg; }

    RNode const & sdn() const
    { return rg.get_sdn(); }

    RNode const & launcher() const
    { return rg.get_launcher(); }

    bool has_sdn() const
    { return rg.has_sdn(); }

    bool has_launcher() const
    { return rg.has_launcher(); }

    void construct();

    void add_modules(std::string const & cid, std::string const & queue, Json::Value const & modules);
    void del_modules(std::string const & cid, Json::Value const & modules);

    Json::Value
        get_active_storage_list();

    Json::Value
        get_active_dtn_list();

    Json::Value
        get_active_dtn_topology();

    int 
        probe_path_rate(std::string const & storage, double rate, bool logging = true);

    std::string const &
        get_node_queue_name(std::string const & nid);

    std::vector<std::pair<std::string, double>> 
        query_path_dtns(std::string const & storage, double rate);

    std::vector<std::tuple<std::string, std::string, double>>
        match_path_dtns( std::vector<std::pair<std::string, double>> & src_dtns, 
                         std::vector<std::pair<std::string, double>> & dst_dtns );

    void create_job(std::string const & rawjob_id,
            std::string const & src_storage, std::string const & dst_storage,
            std::string const & src_dtn, std::string const & dst_dtn, 
            double rate, bool extra);

    std::vector<DTNNode *> 
        get_storage_connected_dtns(std::string const & storage_id);

private:

    JobManager & jm;
    RGraph rg;
    SiteStore & store;

    std::mutex construct_mtx;
};

#endif

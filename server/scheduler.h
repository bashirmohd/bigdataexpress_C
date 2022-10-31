#ifndef BDE_SERVER_SCHEDULER_H
#define BDE_SERVER_SCHEDULER_H

#include <thread>
#include <mutex>
#include <condition_variable>

#include <list>
#include <string>
#include <queue>

#include "typedefs.h"
#include "jobmanager.h"
#include "resourcemanager.h"
#include "sitestore.h"
#include "wan.h"
#include "conf.h"

#include "utils/portal.h"
#include "utils/rpcclient.h"
#include "utils/tsdb.h"

#include <chrono>
#include <asio.hpp>
#include <asio/steady_timer.hpp>

class Scheduler
{
public:

    using res_cb_t = std::function<void(Json::Value const &)>;

    // ctor and dtor
    Scheduler(Conf const & conf, SiteStore & store, RpcClient & rpc);
    ~Scheduler();

    void run();

    void add_event(
            std::function<std::chrono::milliseconds()> const & handler,
            std::chrono::milliseconds const & delay );

    void dispatch(
            std::function<void()> const & handler, 
            std::string const & tag = "");

    // public methods

    // periodic schedule
    std::chrono::milliseconds schedule();
    std::chrono::milliseconds poll_rate();

    // tear down WAN path
    int teardown_wan_path(
            std::string const & sid,
            std::string const & uuid);

    // tear down SDN path
    int teardown_sdn_path(
            std::string const & sid,
            Json::Value const & src_dtn, 
            Json::Value const & dst_dtn );

    // setup SDN path
    Json::Value setup_sdn_path(
            std::string const & sid,
            Json::Value const & src_dtn, 
            Json::Value const & dst_dtn,
            double rate, /*Mb*/
            bool pre_ping,
            int oscar_src,
            int oscar_dst );

    // verify network path
    Json::Value verify_path(
            Json::Value const & src_dtn, 
            Json::Value const & dst_dtn );

    void resource_construct() 
    { rm.construct(); }

    void resource_construct(
            bool online,
            std::string const & cid,
            std::string const & queue,
            Json::Value const & modules );

    void get_storage_list                            ( res_cb_t const & cb);
    void get_dtn_list                                ( res_cb_t const & cb);
    void get_dtn_topology                            ( res_cb_t const & cb);
    void publish_dtn_user_mapping                    ( res_cb_t const & cb);
    void add_raw_job      (std::string const & id,     res_cb_t const & cb);
    void get_file_list    (Json::Value const & params, res_cb_t const & cb);
    void create_folder    (Json::Value const & params, res_cb_t const & cb);
    void get_dtn_info     (Json::Value const & params, res_cb_t const & cb);
    void get_total_size   (Json::Value const & params, res_cb_t const & cb);
    void expand_and_group (Json::Value const & params, res_cb_t const & cb);
    void verify_checksum  (Json::Value const & params, res_cb_t const & cb);

    Json::Value probe_rate(Json::Value const & params);
    Json::Value query_path_dtns(Json::Value const & params);
    Json::Value get_available_best_effort_dtn(Json::Value const & params);
    Json::Value sdn_reserve_request(Json::Value const & params);
    Json::Value sdn_release_request(Json::Value const & params);
    Json::Value dtn_icmp_ping(Json::Value const & params);

    void mld_status(std::string const & task, Json::Value const & params);

private:

    void bootstrap_transfer_job(
            std::string const & id);

    bool create_raw_job_proxy_cert(
            Json::Value & job);

    bool parse_raw_job_dst(
            Json::Value & job);

    bool parse_raw_job_src(
            Json::Value & job);

    bool parse_raw_job_src_ds(
            Json::Value & job);

    void admission(
            Json::Value const & sjob, 
            Json::Value const & ssite, 
            Json::Value const & dsite, 
            bool extra );

    void schedule_clean_up(
            std::string const & rid,
            std::string const & sid,
            std::string const & error,
            //bool sdn_setup = false, 
            //bool wan_setup = false,
            Json::Value const & src_dtn = Json::Value(),
            Json::Value const & dst_dtn = Json::Value() );


    Json::Value create_per_storage_job(
            Json::Value & job);

    Json::Value find_best_effort_dtn_pair(
            std::string const & src_storage_id,
            std::string const & dst_storage_id,
            Json::Value const & src_site_info,
            Json::Value const & dst_site_info );

    Json::Value setup_wan_path(
            Json::Value const & src_dtn,
            Json::Value const & dst_dtn,
            double bandwidth,
            std::string const & wan_src_vlan_s,
            std::string const & wan_dst_vlan_s );

    Json::Value prepare_flow(
            Json::Value const & rjob,
            Json::Value const & src_dtn,
            Json::Value const & dst_dtn,
            Json::Value const & src_storage,
            Json::Value const & dst_storage,
            double max_rate );

    Json::Value network_handling(
            std::string const & sid,
            Json::Value const & src_dtn, 
            Json::Value const & dst_dtn );

    Json::Value find_dtn_stp(
            std::string const & dtn);

    bool needs_sdn_setup(
            std::string const & dtn);

private:

    enum class ld_auth_t
    {
        password,
        cert
    };

    // max number of concurrent flows per sjob
    const int max_concurrent_flows_per_job = 1;

    // asio
    asio::io_service io;
    asio::signal_set sig;

    std::vector<std::pair<asio::steady_timer, std::function<void(asio::error_code const &)>>> evs;
    std::map<std::string, asio::io_service::strand> strands;

    // confs
    std::string ld_type;
    std::string ld_queue;
    ld_auth_t   ld_auth;
    std::string ld_auth_usr;
    std::string ld_auth_pwd;
    bool        ld_file;
    bool        ld_checksum;
    std::string ld_checksum_alg;
    std::string ld_group_size_str;
    uint64_t    ld_group_size;

    std::string cmd_path;

    Json::Value net_conf;

    // timeout time before putting a job into error state due
    // to no transferring progress
    int64_t     inactive_timeout_ms;

    // components
    SiteStore & store;
    RpcClient & rpc;
    Portal      portal;
    WAN         wan;

    // job manager
    JobManager  jm;

    // Resource Manager
    ResourceManager rm;

    //
    utils::TimeSeriesDB tsdb;
    utils::TimeSeriesMeasurement<2, double> tsrate;
    utils::TimeSeriesMeasurement<0, int, int, int, int> tsjobs;
    utils::TimeSeriesMeasurement<0, int> tsschedule;
    utils::TimeSeriesMeasurement<1, int64_t, int64_t> ts_site_txrx;

    std::map<std::string, std::pair<int64_t, int64_t>> site_txrx;

    // thread pool
    std::vector<std::thread> work_threads;

    std::queue<std::exception_ptr> exceptions;
};

#endif

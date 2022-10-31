#ifndef BDE_AGENT_DTNAGENT_H
#define BDE_AGENT_DTNAGENT_H

#include <chrono>
#include <thread>
#include <mutex>

#include <linux/if_link.h>

#include "agentmanager.h"
#include "localstorageagent.h"
#include "sharedstorageagent.h"

#include "utils/mongostore.h"
#include "utils/mounts.h"
#include "utils/process.h"
#include "utils/rpcserver.h"
#include "utils/paths/path.h"
#include "utils/checksum.h"

// DTN Agent is the component that manages data transfer nodes.
//
// DTN Agent can (a) set up involved DTNs in a data transfer job, (b)
// monitor data transfer rates, (c) tear down the set up after the
// transfer job is done.
//
// See the wiki for design and requirements:
// https://cdcvs.fnal.gov/redmine/projects/bigdata-express/wiki/DTN_Agent_Design_and_Requirements

class DTNAgent : public Agent
{
public:
    DTNAgent(Json::Value const & conf);

    // Validate JSON configuration given.  Exposing as a public method
    // only for testing; otherwise, please do not call this method
    // directly.
    void check_configuration(Json::Value const & conf);

protected:
    virtual void do_bootstrap();
    virtual void do_init();
    virtual void do_registration();
    virtual Json::Value do_command(std::string const & cmd,
                                   Json::Value const & params);

private:
    // debugging aid.
    friend std::ostream& operator<<(std::ostream& out, DTNAgent const & agent);

    // methods to update the DB with DTN status.
    void register_dtn();
    void register_all_sdmaps();

    void set_dtn_properties();

    // methods to handle commands sent to DTN Agent.
    Json::Value handle_dtn_status_command(Json::Value const &message);
    Json::Value handle_expand_command(Json::Value const &message);
    Json::Value handle_expand_and_group_command(Json::Value const &message);
    Json::Value handle_expand_and_group_command_v2(Json::Value const &message);
    Json::Value handle_send_ping_command(Json::Value const &message);
    Json::Value handle_start_pong_command(Json::Value const &message);
    Json::Value handle_stop_pong_command(Json::Value const &message);
    Json::Value handle_list_command(Json::Value const &message);
    Json::Value handle_add_route_command(Json::Value const &message);
    Json::Value handle_delete_route_command(Json::Value const &message);
    Json::Value handle_add_neighbor_command(Json::Value const &message);
    Json::Value handle_delete_neighbor_command(Json::Value const &message);
    Json::Value handle_path_size_command(Json::Value const &message);
    Json::Value handle_send_icmp_ping_command(Json::Value const &message) const;
    Json::Value handle_compute_checksums_command(Json::Value const &message) const;
    Json::Value handle_verify_checksums_command(Json::Value const &message) const;
    Json::Value handle_get_gridmap_entries_command(Json::Value const &message) const;
    Json::Value handle_push_gridmap_entries_command(Json::Value const &message) const;
    Json::Value handle_get_disk_usage_command(Json::Value const &message);
    Json::Value handle_unknown_command(std::string const &cmd,
                                       Json::Value const &message) const;

    Json::Value handle_mkdir(Json::Value const& message) const;


    // To represent command parameters.
    struct expand_and_group_v2_params
    {
        expand_and_group_v2_params ()
            : result_in_db(false)
            , compute_checksum(false)
            , checksum_algorithm("sha1")
            , md(EVP_get_digestbyname(checksum_algorithm.c_str())) {}

        std::vector<std::string> src_paths;          // mandatory.
        std::string              dst_path;           // mandatory.
        size_t                   group_size;         // mandatory.
        size_t                   max_files;          // optional.
        std::string              user;               // optional.
        bool                     result_in_db;       // optional.
        bool                     compute_checksum;   // optional.
        std::string              checksum_algorithm; // optional.
        EVP_MD const            *md;
    };

    // Decode JSON.
    const expand_and_group_v2_params
    decode_expand_and_group_command_v2_params(Json::Value const &message);

    void add_file_checksum(utils::Path const &path,
                           std::string const &real_dst_path,
                           DTNAgent::expand_and_group_v2_params const &params,
                           Json::Value       &checksums);

    void add_dir_checksum(utils::Path const &path,
                          std::vector<std::string> const &path_prefixes,
                          DTNAgent::expand_and_group_v2_params const &params,
                          Json::Value       &checksums);

    // Given a directory in @path@ and parameters in @params@, compute
    // checksums of files contained in the directory, and compute the
    // checksum of checksums.  Result will be contained in the output
    // parameter @checksum@.
    void add_dir_checksum_of_checksums(
        utils::Path const &path,
        std::string const &real_dst_path,
        DTNAgent::expand_and_group_v2_params const &params,
        Json::Value &checksum);

    // method called by dtn_status handler.
    Json::Value get_interface_status(std::string const &ifname);

    // methods to manage local data folders.
    void scan_data_folder();
    void check_expand_params(std::vector<std::string> const &paths) const;
    bool data_folder_contains(std::string const &path) const;

    // methods to manage storage agents.
    void init_storage_agents();
    std::string find_storage_device(std::string const &path);
    std::string get_iozone_work_folder(std::string const &root);

    // methods to query network interfaces.
    void check_network_interfaces();
    void check_network_interface(std::string const &iface);
    void update_interface_traffic(std::string const &iface);

    // traffic control methods.
    void add_queue(std::string const & handle,
                   std::string const &device);
    void add_traffic_to_queue(std::string const & device,
                              std::string const & src,
                              std::string const & flowid,
                              int priority);
    void delete_queue(std::string const & handle);

    // "dtn_list" command helpers.
    const std::pair<uid_t, gid_t> get_system_user(std::string const & user) const;
    Json::Value list_file(utils::Path const &p, bool checksum = false) const;
    Json::Value list_directory(utils::Path const &p) const;
    Json::Value list_directory_entries(utils::Path const &p,
                                       std::string const &user,
                                       uid_t uid,
                                       gid_t gid) const;
    Json::Value list_symlink(utils::Path const &p,
                             std::string const &user,
                             uid_t uid,
                             gid_t gid,
                             std::string const &parent = std::string()) const;

    std::string make_dst_path_name(std::string const &src_path,
                                   std::string const &dst_path,
                                   std::vector<std::string> const &prefixes) const;

    std::string make_dst_dir_name(std::string const &src_path,
                                   std::string const &dst_path) const;

    // call utils::run_process(), log results/exceptions, return
    // results wrapped in a JSON object.
    Json::Value run_command(std::string const & cmd) const;

    // form the response message.
    Json::Value json_response(int code, std::string const & error) const;

    // Time series related methods.
    void update_time_series_db() const;
    void update_time_series_interface_stats(std::string const &iface) const;
    void update_time_series_sysinfo_stats() const;

    // Let ONOS know of this DTN's existence.
    void ping_with_data_interface() const;

    // Find IPv4 address from "ip-of(iface)" string
    std::string interpret_ip_of(std::string const &str) const;
    bool has_ip_of_prefix(std::string const &str) const;

    // Find IPv6 address from "ipv6-of(iface)" string
    std::string interpret_ipv6_of(std::string const &str) const;
    bool has_ipv6_of_prefix(std::string const &str) const;

    // Helpers for "ip-of(iface)" and "ipv6-of(iface)"
    std::string get_iface_name_from_ip_of(std::string const &str) const;
    std::string interpret_data_interface(std::string const &name) const;
    std::string interpret_mgmt_interface(std::string const &name) const;

private:
    std::vector<std::string> data_folders_;
    bool                     scan_data_folder_;
    std::string              mgmt_iface_;
    std::vector<std::string> data_ifaces_;
    std::vector<std::string> storage_ifaces_;
    std::string              iozone_program_path_;

    std::string                       transfer_program_path_;
    std::vector<std::string>          transfer_program_args_;
    std::map<std::string,std::string> transfer_program_env_;
    std::thread                       transfer_program_thread_;
    int                               transfer_program_port_;

    // Stats of data interfaces.  Note that values stored are
    // rates/second that we're computing for the previous polling
    // intervl; they are not cumulative.
    std::map<std::string, rtnl_link_stats> link_rates_;
    bool                                   link_rates_computed_;
    size_t rate_per_second(size_t n);

    enum TaskState {
        NotFound,
        Error,
        SetUp,
        TornDown,
        Cancelled,
        Reset
    };

    // Maximum number of threads that can run checksum at a time.
    size_t checksum_threads_;

    // The file size threshold above which checksum computation will
    // be delegated to a thread.
    size_t checksum_file_size_threshold_;

    // Table of [Storage Device, [folders]] mappings.
    std::map<std::string, std::set<std::string>> storage_map_;

    static const int expiry_interval_;
    static const int tsdb_update_interval_;
    static const int default_ping_port_;

    // JSON value used to initialize this DTN Agent.
    Json::Value conf_;

    // Configuration objects to pass on to any Local Storage Agents
    // created by DTN Agent.  Key is Storage Agent ID.
    std::map<std::string, Json::Value> storage_agent_conf_;

    struct StorageAgent {
        std::string           id_;
        std::string           name_;
        std::string           type_;
        std::string           device_;
        std::set<std::string> folders_;
    } ;

    // This field is used to update sdmap.
    std::vector<StorageAgent>   storage_agents_;

    // for "pong" handlers.
    std::thread              pong_thread_;
    asio::io_service         pong_thread_io_service_;
};

#endif

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

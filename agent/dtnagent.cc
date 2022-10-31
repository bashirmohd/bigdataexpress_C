#include <algorithm>
#include <stdexcept>
#include <future>

#include <sys/types.h>
#include <pwd.h>
#include <libgen.h>
#include <sys/sysinfo.h>
#include <dirent.h>

#include "utils/network.h"
#include "utils/mounts.h"
#include "utils/paths/dirwalker.h"
#include "utils/paths/pathgroups.h"
#include "utils/paths/dirtree.h"
#include "utils/vlan.h"
#include "utils/echo.h"
#include "utils/tsdb.h"
#include "utils/checksum.h"
#include "utils/grid-mapfile.h"
#include "utils/fsusage.h"
#include "dtnagent.h"

// deal with ancient glibc-devel on mdtm-server
#if (!defined TCP_USER_TIMEOUT)
#define TCP_USER_TIMEOUT 18
#endif

// ----------------------------------------------------------------------

// extra logging from this module.
static const bool verbose = false;

// Expiry interval is in milliseconds.
const int DTNAgent::expiry_interval_   = 2 * 60 * 1000;

// Time series DB is updated in units of seconds
const int DTNAgent::tsdb_update_interval_ = 5;

// default port for listening for "ping" messages
const int DTNAgent::default_ping_port_ = 5555;

// Ignore route/arp manipulation commands in certain situations.
static bool ignore_route_cmds_ = false;
static bool ignore_arp_cmds_   = false;

// Sometimes it is useful to register the DTN Agent using hostname
// rather than IPv4 address for control plane.
static bool use_hostname_for_control_ = false;

// We used a workaround with older versions of mdtmftp+: in results to
// "expand_and_group_v2" command's response, we would add a group of
// "parent" folders, when doing a directory transfer.  The "parent
// list" will help create a top-level directory before transfering the
// actual contents of the directory.
//
// But this parent directory list will break directory transfer in
// newer versions of mdtmftp+, so we make this a toggle-able item.
//
// Eventually we should be able to remove the workaround code entirely
// when we have new mdtmftp+ everywhere... if/when we ever get around
// to doing so. :)
static bool use_expand_v2_parent_list_ = false;

// ----------------------------------------------------------------------

// Set of networked/distributed filesystems known to us to be
// supported by recent Linux kernel versions.  We use this list to
// determine what kind of storage agent (local or shared) instance we
// need to launch.
static std::set<std::string> networked_fs_ = {
    "9p",
    "afs",
    "ceph",
    "cifs",
    "coda",
    "gfs2",
    "nfs",
    "nfs4",
    "nfsv4",
    "ocfs2",
    "orangefs",
    "lustre",
    "glusterfs"
};

// ----------------------------------------------------------------------

DTNAgent::DTNAgent(Json::Value const & conf)
    : Agent(conf["id"].asString(), conf["name"].asString(), "DTN"),
      conf_(conf),
      link_rates_computed_(false)
{
}

// ----------------------------------------------------------------------

void
DTNAgent::check_configuration(Json::Value const & conf)
{
    if (conf.empty())
    {
        throw std::runtime_error("Empty DTN Agent configuration.");
    }

    auto df = conf["data_folders"]["path"];

    if (not df.empty())
    {
        if (df.isArray())
        {
            for (auto const &d : df)
            {
                data_folders_.push_back(d.asString());
            }
        }
        else
        {
            data_folders_.push_back(df.asString());
        }
    }
    else
    {
        if (verbose)
        {
            utils::slog() << "[DTN Agent] No data_folders found.";
        }
    }

    auto data_folder_scan = conf["data_folders"]["scan"];
    scan_data_folder_ = false;
    if (data_folder_scan.isBool())
    {
        scan_data_folder_ = data_folder_scan.asBool();
    }

    mgmt_iface_ = conf["management_interface"].asString();
    if (mgmt_iface_.empty())
    {
        throw std::runtime_error("No management_interface found.");
    }

    auto di = conf["data_interfaces"];

    if (not di.empty())
    {
        if (di.isArray())
        {
            for (auto const &d : di)
            {
                data_ifaces_.push_back(d.asString());
            }
        }
        else
        {
            data_ifaces_.push_back(di.asString());
        }
    }
    else
    {
        throw std::runtime_error("No data_interfaces found.");
    }

    auto si = conf["storage_interfaces"];

    if (not si.empty())
    {
        if (si.isArray())
        {
            for (auto const &s : si)
            {
                storage_ifaces_.push_back(s.asString());
            }
        }
        else
        {
            storage_ifaces_.push_back(si.asString());
        }
    }
    else
    {
        if (verbose)
        {
            utils::slog() << "[DTN Agent] No storage_interfaces found.";
        }
    }

    auto agentconf = manager().get_config();
    iozone_program_path_ = agentconf["agent"]["programs"]["iozone"].asString();

    if (iozone_program_path_.empty())
    {
        utils::slog() << "[DTN Agent] Path to iozone program is not in configured; "
                      << "assuming iozone will be available in our PATH.";
        iozone_program_path_ = "iozone";
    }

    transfer_program_path_ = conf["data_transfer_program"]["path"].asString();
    auto args = conf["data_transfer_program"]["args"];

    if (not args.empty())
    {
        if (args.isArray())
        {
            for (auto const &arg : args)
            {
                if (has_ip_of_prefix(arg.asString()))
                {
                    auto addr = interpret_ip_of(arg.asString());
                    utils::slog() << "[DTN Agent] Translated " << arg
                                  << " to " << addr << ".";
                    transfer_program_args_.push_back(addr);
                    continue;
                }
                else if (has_ipv6_of_prefix(arg.asString()))
                {
                    auto addr = interpret_ipv6_of(arg.asString());
                    utils::slog() << "[DTN Agent] Translated " << arg
                                  << " to " << addr << ".";
                    transfer_program_args_.push_back(addr);
                    continue;
                }

                transfer_program_args_.push_back(arg.asString());
            }
        }
        else
        {
            transfer_program_args_.push_back(args.asString());
        }
    }

    // When the configured data transfer is mdtm-ftp-server, find the
    // port number from configured command line arguments.
    std::string const pattern = "mdtm-ftp-server";

    if (std::mismatch(pattern.rbegin(),
                      pattern.rend(),
                      transfer_program_path_.rbegin()).first == pattern.rend())
    {
        auto it = std::find(transfer_program_args_.begin(),
                            transfer_program_args_.end(), "-p");

        if (it != transfer_program_args_.end())
        {
            transfer_program_port_ = std::stoi(*std::next(it));
        }
        else
        {
            auto it = std::find(std::begin(transfer_program_args_),
                                std::end(transfer_program_args_), "-port");

            if (it != transfer_program_args_.end())
            {
                transfer_program_port_ = std::stoi(*std::next(it));
            }
        }
    }

    auto env = conf["data_transfer_program"]["env"];
    if (not env.empty())
    {
        if (env.isArray())
        {
            for (auto const &item : env)
            {
                auto names = item.getMemberNames();
                if (names.size() > 0)
                {
                    auto name = names.at(0);
                    auto val  = item[name].asString();
                    transfer_program_env_.emplace(name, val);
                }
            }
        }
        else
        {
            auto names = env.getMemberNames();
            if (names.size() > 0)
            {
                auto name = names.at(0);
                auto val  = env[name].asString();
                transfer_program_env_.emplace(name, val);
            }
        }
    }

    auto ir = conf["ignore_route_cmds"];
    if ((not ir.empty()) and ir.asBool())
    {
        utils::slog() << "[DTN Agent] Setting ignore_route_cmds=true";
        ignore_route_cmds_ = true;
    }

    auto ia = conf["ignore_arp_cmds"];
    if ((not ia.empty()) and ia.asBool())
    {
        utils::slog() << "[DTN Agent] Setting ignore_arp_cmds=true";
        ignore_arp_cmds_ = true;
    }

    auto use_hostname = conf["use_hostname_for_control"];
    if ((not use_hostname.empty()) and use_hostname.asBool())
    {
        utils::slog() << "[DTN Agent] Setting use_hostname_for_control=true";
        use_hostname_for_control_ = true;
    }

    auto use_head_groups = conf["use_expand_v2_parent_list"];
    if ((not use_head_groups.empty()) and use_head_groups.isBool())
    {
        utils::slog() << "[DTN Agent] Setting use_expand_v2_parent_list="
                      << (use_head_groups.asBool() ? "true" : "false");
        use_expand_v2_parent_list_ = use_head_groups.asBool();
    }

    auto checksum_threads =   conf["checksum"]["threads"];
    auto checksum_threshold = conf["checksum"]["file_size_threshold"];

    if (checksum_threads.empty())
    {
        checksum_threads_ = 256;
    }
    else
    {
        checksum_threads_ = checksum_threads.asLargestUInt();
    }

    if (checksum_threshold.empty())
    {
        checksum_file_size_threshold_ = 1 * 1024 * 1024 * 1024;
    }
    else
    {
        checksum_file_size_threshold_ = checksum_threshold.asLargestUInt();
    }

    utils::slog() << "[DTN Agent] Checksum will use up to "
                  << checksum_threads_
                  << " threads for files larger than "
                  << checksum_file_size_threshold_
                  << " bytes.";
}

// ----------------------------------------------------------------------

void
DTNAgent::do_bootstrap()
{
     check_configuration(conf_);

     if (id().empty())
     {
         update_id(get_first_mac_address());
     }
     else
     {
         utils::slog() << "[DTN Agent] Using ID " << id()
                       << " from agent configuration.";
     }

     // NOTE: Perhaps we should use name from configuration if it is
     // there.  But relying on configuration is very likely to be
     // error-prone.
     char hostname[HOST_NAME_MAX] = {0};

     if (gethostname(hostname, HOST_NAME_MAX) == 0)
     {
         update_name(hostname);
     }
     else
     {
         utils::slog() << "[DTN Agent] gethostname() failed; reason: "
                       << strerror(errno);
         update_name(id());
     }

     utils::slog() << "[DTN Agent] Bootstrapping with ID: " << id() << ", "
                   << "name: " << name();

     if (scan_data_folder_)
     {
         scan_data_folder();
     }

     init_storage_agents();

     if (verbose)
     {
         utils::slog() << "[DTN Agent] finished bootstrap step.";
     }
}

// ----------------------------------------------------------------------

void
DTNAgent::do_init()
{
    // init data interface statistics
    for (auto const &iface : data_ifaces_)
    {
        link_rates_.emplace(iface, get_link_stats(iface));
    }

    auto delay = std::chrono::milliseconds(expiry_interval_);

    // Monitor DTN status periodically.
    manager().add_event([this, delay](){
            try
            {
                check_network_interfaces();
                register_dtn();
                register_all_sdmaps();
            }
            catch(std::exception const &ex)
            {
                utils::slog() << "[DTN Agent] Error in DTN status check: "
                              << ex.what();
            }
            return delay;
        }, delay);

    auto tsdb_interval = std::chrono::seconds(tsdb_update_interval_);

    // Send data interface stats to time series DB periodically.
    manager().add_event([this, tsdb_interval](){
            update_time_series_db();
            return tsdb_interval;
        }, tsdb_interval);

    if (not data_ifaces_.empty())
    {
        // If we have data interfaces, try pinging data interface's
        // gateway periodically.  This way ONOS will know about the
        // existence of this DTN.
        //
        // See https://cdcvs.fnal.gov/redmine/issues/22166 for
        // details.

        auto ping_interval = std::chrono::minutes(1);

        manager().add_event([this, ping_interval](){
            ping_with_data_interface();
            return ping_interval;
        }, ping_interval);
    }

    // launch and monitor the data transfer process.
    if (not transfer_program_path_.empty())
    {
        if (verbose)
        {
            utils::slog() << "[DTN Agent] Launching " << transfer_program_path_;
        }

        // ~DTNAgent() will terminate this thread up by calling
        // ~std::thread().
        transfer_program_thread_ = std::thread([this]() {
                utils::supervise_process(transfer_program_path_,
                                         transfer_program_args_,
                                         transfer_program_env_);
            });
    }
}

// ----------------------------------------------------------------------

void
DTNAgent::do_registration()
{
    register_dtn();
    register_all_sdmaps();

    set_dtn_properties();

    if (verbose)
    {
        utils::slog() << "[DTN Agent] finished registration step.";
    }
}

// ----------------------------------------------------------------------

void
DTNAgent::set_dtn_properties()
{
    Json::Value ctrl;

    ctrl["name"] = mgmt_iface_;

    if (use_hostname_for_control_ and !name().empty())
    {
        ctrl["ip"] = name();
        utils::slog() << "[DTN Agent] Setting hostname property "
                      << ctrl["ip"] << " for control plane";
    }
    else
    {
        ctrl["ip"]   = utils::get_first_ipv4_address(mgmt_iface_);
        utils::slog() << "[DTN Agent] Setting IPv4 address property "
                      << ctrl["ip"] << " for control plane";
    }

    ctrl["ipv6"] = utils::get_first_ipv6_address(mgmt_iface_);
    utils::slog() << "[DTN Agent] Setting IPv6 address property "
                  << ctrl["ipv6"] << " for control plane";

    // Link speed returned by ethtool_cmd_speed() are in units of 1Mb.
    // We prefer bytes, hence the multiplication.
    auto megabyte = 1000;

    auto rate = utils::get_link_speed(mgmt_iface_) * megabyte;
    ctrl["rate"] = static_cast<Json::Value::UInt64>(rate);

    set_prop("ctrl_interface", ctrl);

    Json::Value dis;

    for (auto const &di : data_ifaces_)
    {
        Json::Value v;

        v["name"] = di;
        v["ip"]   = utils::get_first_ipv4_address(di);
        v["ipv6"] = utils::get_first_ipv6_address(di);
        v["mac"]  = utils::get_iface_mac_address(di);

        auto rate = utils::get_link_speed(di) * megabyte;
        v["rate"] = static_cast<Json::Value::UInt64>(rate);

        v["vlan_id"] = utils::get_vlan_id(di);

        dis.append(v);
    }

    set_prop("data_interfaces", dis);

    Json::Value sifs;

    for (auto const &si : storage_ifaces_)
    {
        Json::Value v;

        v["name"] = si;
        v["type"] = "local" ; // TODO: determine this.
        auto rate = utils::get_link_speed(si) * megabyte;
        v["rate"] = static_cast<Json::Value::UInt64>(rate);

        sifs.append(v);
    }

    Json::Value dfs;

    for (auto const &d : data_folders_)
    {
        dfs.append(d);
    }

    set_prop("data_folders", dfs);

    if (not transfer_program_path_.empty())
    {
        Json::Value dtp;

        dtp["path"] = transfer_program_path_;

        if (transfer_program_port_)
        {
            dtp["port"] = transfer_program_port_;
        }

        for (auto const &env : transfer_program_env_)
        {
            dtp["env"][env.first] = env.second;
        }

        set_prop("data_transfer_program", dtp);
    }

    Json::Value storages;

    for (auto const &agent : storage_agents_)
    {
        Json::Value storage;

        storage["id"]     = agent.id_;
        storage["name"]   = agent.name_;
        storage["type"]   = agent.type_;
        storage["device"] = agent.device_;

        for (auto const &folder : agent.folders_)
        {
            storage["data_folders"].append(folder);
        }

        storages.append(storage);
    }

    set_prop("storages", storages);

    if (verbose)
    {
        utils::slog() << "[DTN Agent] Properties set to: " << get_prop();
    }
}

// ----------------------------------------------------------------------

void
DTNAgent::register_dtn()
{
    Json::Value val;

    val["id"]   = id();
    val["name"] = name();

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto json_now = static_cast<Json::Value::UInt64>(now);

    val["expire_at"]  = json_now + expiry_interval_;
    val["queue_name"] = queue();

    const time_t t = (now + expiry_interval_) / 1000;
    char expiry_string[32]{0};
    strftime(expiry_string, 32, "%F %T %Z", localtime(&t));

    val["expiry_datetime"] = expiry_string;

    Json::Value ctrl;

    ctrl["name"] = mgmt_iface_;

    if (use_hostname_for_control_ and !name().empty())
    {
        ctrl["ip"] = name();
        utils::slog() << "[DTN Agent] Registering with hostname "
                      << ctrl["ip"] << " for control plane";
    }
    else
    {
        ctrl["ip"]   = utils::get_first_ipv4_address(mgmt_iface_);
        utils::slog() << "[DTN Agent] Registering with IPv4 address "
                      << ctrl["ip"] << " for control plane";
    }

    ctrl["ipv6"] = utils::get_first_ipv6_address(mgmt_iface_);
    utils::slog() << "[DTN Agent] Setting IPv6 address "
                  << ctrl["ipv6"] << " for control plane";

    // Link speed returned by ethtool_cmd_speed() are in units of 1Mb.
    // We prefer bytes, hence the multiplication.
    auto const megabyte = 1000;
    auto const rate     = utils::get_link_speed(mgmt_iface_) * megabyte;
    ctrl["rate"]        = static_cast<Json::Value::UInt64>(rate);

    val["ctrl_interface"] = ctrl;

    Json::Value dis;

    for (auto const &di : data_ifaces_)
    {
        Json::Value v;

        v["name"]    = di;
        v["ip"]      = utils::get_first_ipv4_address(di);
        v["ipv6"]    = utils::get_first_ipv6_address(di);
        v["mac"]     = utils::get_iface_mac_address(di);
        auto rate    = utils::get_link_speed(di) * megabyte;
        v["rate"]    = static_cast<Json::Value::UInt64>(rate);
        v["vlan_id"] = utils::get_vlan_id(di);

        dis.append(v);
    }

    val["data_interfaces"] = dis;

    Json::Value sifs;

    for (auto const &si : storage_ifaces_)
    {
        Json::Value v;

        v["name"] = si;
        v["type"] = "local" ; // TODO: determine this.
        auto rate = utils::get_link_speed(si) * megabyte;
        v["rate"] = static_cast<Json::Value::UInt64>(rate);

        sifs.append(v);
    }

    val["storage_interfaces"] = sifs;

    Json::Value dfs;

    for (auto const &d : data_folders_)
    {
        dfs.append(d);
    }

    val["data_folders"] = dfs;

    if (not transfer_program_path_.empty())
    {
        val["data_transfer_program"]["path"] = transfer_program_path_;

        if (transfer_program_port_)
            val["data_transfer_program"]["port"] = transfer_program_port_;

        for (auto const &env : transfer_program_env_)
        {
            val["data_transfer_program"]["env"][env.first] = env.second;
        }
    }

    store().register_dtn_agent(id(), val);
}

// ----------------------------------------------------------------------

void
DTNAgent::register_all_sdmaps()
{
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const auto expiry = static_cast<Json::Value::UInt64>(now)
        + expiry_interval_;

    const time_t t = expiry/1000;
    char expiry_string[32]{0};
    strftime(expiry_string, 32, "%F %T %Z", localtime(&t));

    auto dtn_id = id();

    // clean up the table
    store().delete_storage_dtn_maps(dtn_id);

    for (auto const &agent : storage_agents_)
    {
        Json::Value sdmap;

        sdmap["dtn"]               = dtn_id;
        sdmap["storage"]           = agent.id_;
        sdmap["storage_interface"] = agent.type_; // TODO: redundant; remove.
        sdmap["name"]              = agent.name_;
        sdmap["type"]              = agent.type_;
        sdmap["expire_at"]         = expiry;
        sdmap["name"]              = name();
        sdmap["expiry_datetime"]   = expiry_string;

        store().register_storage_dtn_map(sdmap);
    }
}

// ----------------------------------------------------------------------

void
DTNAgent::scan_data_folder()
{
    auto scan = scan_data_folder_;

    if (verbose)
    {
        utils::slog() << "[DTN Agent] Scanning data folders ...";
    }

    for (auto const &path : data_folders_)
    {
        if (verbose)
        {
            utils::slog() << "[DTN Agent] Finding storage device for path " << path
                          << ", scanning: " << (scan ? "yes" : "no");
        }

        utils::MountPointsHelper mph(path, scan);
        auto mounts = mph.get_mounts();

        // what if nothing is mounted under this path?
        if (mounts.empty())
        {
            auto mnt = mph.get_parent_mount();
            mounts.insert(mnt);
        }

        for (auto const &m : mounts)
        {
            // add {"/dev/thing", [folder list]} to our list.
            storage_map_[m.source()].insert(path);
        }
    }
}

// ----------------------------------------------------------------------

void
DTNAgent::init_storage_agents()
{
    if (verbose)
    {
        for (auto const &item : storage_map_)
        {
            auto storage = std::string();

            for (auto const &p : item.second)
            {
                storage += (p + ", ");
            }

            utils::slog() << "[DTN Agent] Adding device: " << item.first
                          << ", folders: " << storage;
        }
    }

    for (auto const &item : storage_map_)
    {
        auto device       = item.first;
        auto storage_id   = id() + "|" + device;
        auto storage_name = name() + ":" + device;
        auto storage_type =
            conf_["shared_storage_agent"].empty() ? "local" : "shared";

        Json::Value v;

        v["id"]     = storage_id;
        v["name"]   = storage_name;
        v["type"]   = storage_type;
        v["device"] = device;

        if (storage_type == "shared")
        {
            v["MGS"] = conf_["shared_storage_agent"]["Lustre MGS"];
        }

        auto paths = item.second;

        for (auto const &p : paths)
        {
            if (verbose)
            {
                utils::slog() << "[DTN Agent] Adding path " << p
                              << " to storage agent folders";
            }
            v["root-folders"].append(p);
        }

        v["iozone-executable"]  = iozone_program_path_;

        for (auto const &p : paths)
        {
            bool iozone_enable = true;

            if ((not conf_["iozone"]["enable"].empty()) and
                conf_["iozone"]["enable"].asBool() == false)
            {
                iozone_enable = false;
                utils::slog() << "[DTN Agent] "
                              << "iozone is disabled in configuration.";
                break;
            }

            std::string iozone_work_folder;

            try
            {
                iozone_work_folder = get_iozone_work_folder(p);
            }
            catch (std::exception const &ex)
            {
                utils::slog() << "[DTN Agent] "
                              << "Failed to create iozone work folder. "
                              << "Disabling iozone on data folder " << p << ".";
                iozone_enable = false;
            }

            if (iozone_enable)
            {
                // just get the first folder from the list, and make a
                // work folder for "iozone" inside it.
                v["iozone-work-folder"] = iozone_work_folder;
                break;
            }
            else
            {
                utils::slog() << "[DTN Agent] "
                              << "Skipping iozone work folder creation.";
            }
        }

        // Add to list of local storage agent configuration objects.
        storage_agent_conf_.emplace(std::make_pair(storage_id, v));

        // Get a reference to the configuration object we just stored.
        // We can't use "v" because it is going out of scope after
        // this and will be destroyed; LocalStorageAgent will need a
        // more long-lived JSON object it can hold on to.
        auto &c = storage_agent_conf_[storage_id];

        utils::slog() << "[DTN Agent] Bringing up LocalStorageAgent "
                      << "for device " << device;

        if (verbose)
        {
            utils::slog() << "[DTN Agent] LocalStorageAgent conf: " << c;
        }

        // initialize local storage agents.
        try
        {
            if (storage_type == "local")
            {
                manager().add(new LocalStorageAgent(c));
            }
            else if (storage_type == "shared")
            {
                manager().add(new SharedStorageAgent(c));
            }
            else
            {
                auto error = std::string("Unknown storage type ") +
                    storage_type + " for device " + device;
                throw std::runtime_error(error);
            }

            storage_agents_.emplace_back(
                StorageAgent{storage_id, storage_name, storage_type, device, paths});
        }
        catch (std::exception &ex)
        {
            utils::slog() << "[DTN Agent] Error bringing up"
                          << storage_type << " StorageAgent: " << ex.what();
        }

        if (verbose)
        {
            utils::slog() << "[DTN Agent] Brought up " << storage_type
                          << " StorageAgent for device " << device;
        }
    }
}

// ----------------------------------------------------------------------

std::string
DTNAgent::get_iozone_work_folder(std::string const &path)
{
    char basedir[BUFSIZ];
    snprintf(basedir, BUFSIZ, "%s/bde-iozone", path.c_str());

    DIR *basedirp = opendir(basedir);
    if (!basedirp)
    {
        if (mkdir(basedir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
        {
            auto err = "Can't create a base directory for iozone work at "
                + std::string(basedir)
                + ", error: " + std::string(strerror(errno));
            throw std::runtime_error(err);
        }
    }
    else
    {
        closedir(basedirp);
    }

    char workdir[BUFSIZ];

    snprintf(workdir, BUFSIZ, "%s/work-XXXXXX", basedir);

    if (mkdtemp(workdir) == nullptr)
    {
        auto err = "Can't create a work directory for iozone at path "
            + path + ", error: " + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    utils::slog() << "[DTN Agent] Created iozone work directory for "
                  << path << " at " << workdir << ".";

    return std::string(workdir);
}

// ----------------------------------------------------------------------

void
DTNAgent::check_network_interfaces()
{
    try
    {
        check_network_interface(mgmt_iface_);

        for (auto const &di : data_ifaces_)
        {
            check_network_interface(di);
            update_interface_traffic(di);
        }
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[DTN Agent] Error caught in network interface check: "
                      << ex.what();
    }
}

// ----------------------------------------------------------------------

void
DTNAgent::check_network_interface(std::string const &iface)
{
    auto interfaces = utils::get_interface_names();

    if (interfaces.find(iface) == interfaces.end())
    {
        utils::slog() << "[DTN Agent] No such network interface: " << iface;
        return;
    }

    if (not utils::iface_up_and_running(iface))
    {
        utils::slog() << "[DTN Agent] Network interface " << iface << " is down";
        return;
    }
}

// ----------------------------------------------------------------------

size_t
DTNAgent::rate_per_second(size_t n)
{
    // assumption is that expiry_interval_ is in milliseconds.
    return n / (expiry_interval_ / 1000);
}

void
DTNAgent::update_interface_traffic(std::string const &iface)
{
    auto & stat = link_rates_[iface];

    auto newstat = get_link_stats(iface);

    stat.rx_packets = rate_per_second(newstat.rx_packets - stat.rx_packets);
    stat.tx_packets = rate_per_second(newstat.tx_packets - stat.tx_packets);
    stat.rx_bytes   = rate_per_second(newstat.rx_bytes   - stat.rx_bytes);
    stat.tx_bytes   = rate_per_second(newstat.tx_bytes   - stat.tx_bytes);
    stat.rx_errors  = rate_per_second(newstat.rx_errors  - stat.rx_errors);
    stat.tx_errors  = rate_per_second(newstat.tx_errors  - stat.tx_errors);
    stat.rx_dropped = rate_per_second(newstat.rx_dropped - stat.rx_dropped);
    stat.tx_dropped = rate_per_second(newstat.tx_dropped - stat.tx_dropped);

    link_rates_computed_ = true;

    // utils::slog() << "[DTN Agent] link rates/sec for " << iface << ": "
    //               << link_rates_[iface];
}

void
DTNAgent::update_time_series_db() const
{
    update_time_series_sysinfo_stats();

    for (auto const &di : data_ifaces_)
    {
        update_time_series_interface_stats(di);
    }
}

// ----------------------------------------------------------------------

std::ostream&
operator<<(std::ostream& out, struct sysinfo const & info)
{
    auto l1 = info.loads[0] / (float)(1 << SI_LOAD_SHIFT);
    auto l2 = info.loads[1] / (float)(1 << SI_LOAD_SHIFT);
    auto l3 = info.loads[2] / (float)(1 << SI_LOAD_SHIFT);

    out << "uptime: "     << info.uptime    << ", "
        << "load-1m: "    << l1 << ", "
        << "load-5m: "    << l2 << ", "
        << "load-15m: "   << l3 << ", "
        << "total ram: "  << info.totalram  << ", "
        << "free ram: "   << info.freeram   << ", "
        << "buffer ram: " << info.bufferram << ", "
        << "total swap: " << info.totalswap << ", "
        << "free swap: "  << info.freeswap  << ", "
        << "processes: "  << info.procs     << ", "
        << "total high mem: " << info.totalhigh << ", "
        << "free high  mem: " << info.freehigh  << ", "
        << "mem unit: "   << info.mem_unit ;

    return out;
}

// ----------------------------------------------------------------------

void
DTNAgent::update_time_series_sysinfo_stats() const
{
    std::array<std::string, 7> field_names = { "loadavg-1m",
                                               "loadavg-5m",
                                               "loadavg-15m",
                                               "total-ram",
                                               "free-ram",
                                               "total-swap",
                                               "free-swap" };

    auto const num_tags = 2;
    std::array<std::string, num_tags> tag_names = { "name", "id" };

    using SysInfo = utils::TimeSeriesMeasurement<num_tags,
                                                 float,
                                                 float,
                                                 float,
                                                 size_t,
                                                 size_t,
                                                 size_t,
                                                 size_t>;

    SysInfo measure(tsdb(), "sysinfo", field_names, tag_names);

    struct sysinfo info{0};

    if (sysinfo(&info) < 0)
    {
        utils::slog() << "[DTN Agent] sysinfo() error: " << strerror(errno);
    }

    if (verbose)
    {
        utils::slog() << "[DTN Agent] sysinfo: " << info;
    }

    // The load average numbers returned by sysinfo() are integers
    // scaled by 2^16, presumably because the Linux kernel wants to
    // avoid floating point calculatuions; so we need to do the
    // calculations in userspace to make it consistent with what tools
    // like uptime(1) report.  Another way is to call getloadavg(),
    // which reportedly reads from /proc/loadavg.

    auto load_avg_1m  = info.loads[0] / (float)(1 << SI_LOAD_SHIFT);
    auto load_avg_5m  = info.loads[1] / (float)(1 << SI_LOAD_SHIFT);
    auto load_avg_15m = info.loads[2] / (float)(1 << SI_LOAD_SHIFT);

    try
    {
        measure.insert(load_avg_1m,
                       load_avg_5m,
                       load_avg_15m,
                       info.totalram,
                       info.freeram,
                       info.totalswap,
                       info.freeswap,
                       {name(), id()});
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[DTN Agent] time series reporting error: " << ex.what();
    }
}

// ----------------------------------------------------------------------

void
DTNAgent::update_time_series_interface_stats(std::string const &iface) const
{
    try {
        auto stat = get_link_stats(iface);

        std::array<std::string, 8> field_names =
            { "rx-bytes"   ,
              "rx-packets" ,
              "rx-dropped" ,
              "rx-errors"  ,
              "tx-bytes"   ,
              "tx-packets" ,
              "tx-dropped" ,
              "tx-errors"  };

        auto const num_tags = 3;

        std::array<std::string, num_tags> tag_names =
            { "name", "id", "interface" };

        // representation of network interface data "point" for the time
        // series database.
        using InterfaceMeasurement =
            utils::TimeSeriesMeasurement <num_tags,
                                          size_t, // rx bytes
                                          size_t, // rx packets
                                          size_t, // rx dropped
                                          size_t, // rx errors
                                          size_t, // tx bytes
                                          size_t, // tx packets
                                          size_t, // tx dropped
                                          size_t  // tx errors
                                          >;

        InterfaceMeasurement measure(tsdb(), "network", field_names, tag_names);

        measure.insert(stat.rx_bytes    ,
                       stat.rx_packets  ,
                       stat.rx_dropped  ,
                       stat.rx_errors   ,
                       stat.tx_bytes    ,
                       stat.tx_packets  ,
                       stat.tx_dropped  ,
                       stat.tx_errors   ,
                       {name(), id(), iface});
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[DTN Agent] Error on " << iface << " stats: "
                      << ex.what();
    }
}

// ----------------------------------------------------------------------

void DTNAgent::ping_with_data_interface() const
{
    if (data_ifaces_.empty())
    {
        utils::slog() << "[DTN Agent] No data interfaces, no ping";
        return;
    }

    try
    {
        // Send two or more ECHO_REQUEST packets using data
        // interfaces, with a deadline of two seconds.  We don't want
        // this process to run wild, hence this count and deadline.
        for (auto const &di : data_ifaces_)
        {
            auto cmd = "/bin/ping -I " + di + " -w 2 -c 2 fnal.gov";
            utils::run_process(cmd, false, false);
            utils::slog() << "[DTN Agent] Periodic ping on data interface: "
                          << cmd;
        }
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[DTN Agent] Error running ping using data iterface: "
                      << ex.what();
    }
}

// ----------------------------------------------------------------------

Json::Value DTNAgent::do_command(std::string const & cmd,
                                 Json::Value const & params)
{
    if (cmd == "dtn_status")
    {
        return handle_dtn_status_command(params);
    }
    else if (cmd == "dtn_expand")
    {
        return handle_expand_command(params);
    }
    else if (cmd == "dtn_expand_and_group")
    {
        return handle_expand_and_group_command(params);
    }
    else if (cmd == "dtn_expand_and_group_v2")
    {
        return handle_expand_and_group_command_v2(params);
    }
    else if (cmd == "dtn_send_icmp_ping")
    {
        return handle_send_icmp_ping_command(params);
    }
    else if (cmd == "dtn_send_ping")
    {
        return handle_send_ping_command(params);
    }
    else if (cmd == "dtn_start_pong")
    {
        return handle_start_pong_command(params);
    }
    else if (cmd == "dtn_stop_pong")
    {
        return handle_stop_pong_command(params);
    }
    else if (cmd == "dtn_list")
    {
        return handle_list_command(params);
    }
    else if (cmd == "dtn_add_route")
    {
        return handle_add_route_command(params);
    }
    else if (cmd == "dtn_delete_route")
    {
        return handle_delete_route_command(params);
    }
    else if (cmd == "dtn_add_neighbor")
    {
        return handle_add_neighbor_command(params);
    }
    else if (cmd == "dtn_delete_neighbor")
    {
        return handle_delete_neighbor_command(params);
    }
    else if (cmd == "dtn_path_size")
    {
        return handle_path_size_command(params);
    }
    else if (cmd == "dtn_compute_checksums")
    {
        return handle_compute_checksums_command(params);
    }
    else if (cmd == "dtn_verify_checksums")
    {
        return handle_verify_checksums_command(params);
    }
    else if (cmd == "dtn_get_gridmap_entries")
    {
        return handle_get_gridmap_entries_command(params);
    }
    else if (cmd == "dtn_push_gridmap_entries")
    {
        return handle_push_gridmap_entries_command(params);
    }
    else if (cmd == "dtn_get_disk_usage")
    {
        return handle_get_disk_usage_command(params);
    }
    else if (cmd == "dtn_mkdir")
    {
        return handle_mkdir(params);
    }
    else
    {
        return handle_unknown_command(cmd, params);
    }
}

// ----------------------------------------------------------------------

Json::Value DTNAgent::handle_dtn_status_command(Json::Value const &message)
{
    auto iface = message["interface"].asString();

    if (not link_rates_computed_)
    {
        return json_response(1, "link rates are not available yet");
    }

    Json::Value response;

    if (not iface.empty())
    {
        // find status of the given interface.
        auto v = get_interface_status(iface);
        response["status"] = v;
    }
    else
    {
        // if no interface name was given, find status of control,
        // data, and storage interfaces.

        auto v = get_interface_status(mgmt_iface_);
        v["role"] = "control interface";
        response["status"].append(v);

        for (auto const &di : data_ifaces_)
        {
            auto v = get_interface_status(di);
            v["role"] = "data interface";
            response["status"].append(v);
        }

        for (auto const &si : storage_ifaces_)
        {
            auto v = get_interface_status(si);
            v["role"] = "storage interface";
            response["status"].append(v);
        }
    }

    // assume that things went well.
    response["code"] = 0;

    return response;
}

Json::Value DTNAgent::get_interface_status(std::string const &ifname)
{
    Json::Value ifstat;

    ifstat["interface"] = ifname;

    auto r = link_rates_.find(ifname);

    if (r != link_rates_.end())
    {
        auto rate            = r->second;
        ifstat["rate"]["rx-bytes"]   = rate.rx_bytes;
        ifstat["rate"]["tx-bytes"]   = rate.tx_bytes;
        ifstat["rate"]["rx-packets"] = rate.rx_packets;
        ifstat["rate"]["tx-packets"] = rate.tx_packets;
        ifstat["rate"]["rx-errors"]  = rate.rx_errors;
        ifstat["rate"]["tx-errors"]  = rate.tx_errors;
        ifstat["rate"]["rx-dropped"] = rate.rx_errors;
        ifstat["rate"]["tx-dropped"] = rate.tx_errors;
    }
    else
    {
        auto error = "no stats for interface " + ifname;
        utils::slog() << "[DTN Agent] " << error;
        ifstat["error"] = error;
    }

    return ifstat;
}

// ----------------------------------------------------------------------

Json::Value DTNAgent::handle_expand_command(Json::Value const &message)
{
    Json::Value              response_files;
    size_t                   total_size = 0;

    try
    {
        auto files = message["files"];
        std::vector<std::string> paths;

        if (files.empty())
        {
            throw std::runtime_error("empty parmeter: files");
        }

        if (files.isArray())
        {
            for (auto const &p : files)
            {
                paths.push_back(p.asString());
            }
        }
        else
        {
            paths.push_back(files.asString());
        }

        check_expand_params(paths);

        auto user = message["user"].asString();
        uid_t uid;
        gid_t gid;

        if (not user.empty())
        {
            passwd  pwd;
            passwd *result         = nullptr;
            char    buffer[BUFSIZ] = {0};

            auto ret =
                getpwnam_r(user.c_str(), &pwd, buffer, BUFSIZ, &result);

            if (result == nullptr)
            {
                if (ret == 0)
                    throw std::runtime_error("no such user: " + user);

                throw std::runtime_error("getpwnam_r() failed");
            }

            uid = pwd.pw_uid;
            gid = pwd.pw_gid;

            utils::slog() << "[DTN Agent] dtn_expand for user " << user
                          << "(uid:" << uid << ", gid:" << gid << ")";
        }
        else
        {
            utils::slog() << "[DTN Agent] no user given; "
                          << "running dtn_expand with defaults";
        }

        for (auto const &path : paths)
        {
            utils::Path p(path);

            if (p.is_directory())
            {
                for (auto w : utils::DirectoryWalker(p, true))
                {
                    if ((not user.empty()) and (not w.readable_by(uid, gid)))
                    {
                        auto error = "user " + user +  " can't read " + w.name();
                        throw std::runtime_error(error);
                    }

                    auto sz = w.size();
                    total_size += sz;

                    Json::Value v;

                    v["name"] = w.name();
                    v["size"] = static_cast<Json::UInt64>(sz);

                    response_files.append(v);
                }
            }
            else
            {
                auto sz = p.size();
                total_size += sz;

                Json::Value v;
                v["name"] = p.name();
                v["size"] = static_cast<Json::UInt64>(sz);
                response_files.append(v);
            }
        }
    }
    catch (std::exception const & ex)
    {
        auto error = "error: " + std::string(ex.what());
        utils::slog() << "[DTN Agent] dtn_expand failed: " << error;
        return json_response(1, error);
    }

    Json::Value response   = json_response(0, "OK");
    response["total_size"] = static_cast<Json::UInt64>(total_size);
    response["files"]      = response_files;

    return response;
}

void DTNAgent::check_expand_params(std::vector<std::string> const &paths) const
{
    for (auto const &p : paths)
    {
        if (not data_folder_contains(p))
        {
            auto error = "path " + p + " is not under a data folder";
            throw std::runtime_error(error);
        }
    }
}

bool DTNAgent::data_folder_contains(std::string const &path) const
{
    for (auto const &df : data_folders_)
    {
        // Normalize and compare root paths: "/data", "/data/",
        // "/data//", "//data", are all the same.  If we do a simple
        // string comparison without normalizing paths, we will very
        // likely be wrong.
        //
        // We also need to special-case data folder roots below (for
        // cases when the "path" argument is the same as any of the
        // data folders) because DirectoryWalker iterator does not
        // return (top level) directory names.

        char buf1[PATH_MAX];
        char buf2[PATH_MAX];

        char *realp  = realpath(path.c_str(), buf1);
        char *realdf = realpath(df.c_str(), buf2);

        if (realp == nullptr or realdf == nullptr)
        {
            return false;
        }

        // Do a prefix match.
        std::string given_path(realp);
        std::string df_path(realdf);

        auto res = std::mismatch(df_path.begin(), df_path.end(), given_path.begin());

        if (res.first == df_path.end())
        {
            return true;
        }
    }

    return false;
}

Json::Value DTNAgent::handle_expand_and_group_command(Json::Value const &message)
{
    Json::Value              response_files;
    size_t                   total_size = 0;

    try
    {
        auto files = message["src_path"];
        std::vector<std::string> src_paths;

        if (files.empty())
        {
            throw std::runtime_error("empty parmeter: src_path");
        }

        if (files.isArray())
        {
            for (auto const &p : files)
            {
                src_paths.push_back(p.asString());
            }
        }
        else
        {
            src_paths.push_back(files.asString());
        }

        check_expand_params(src_paths);

        auto dst_path = message["dst_path"].asString();
        if (dst_path.empty())
        {
            throw std::runtime_error("empty parmeter: dst_path");
        }

        // clear designated destination folder of any trailing
        // slashes.
        while (dst_path.back() == '/')
        {
            dst_path.pop_back();
        }

        auto user = message["user"].asString();
        uid_t uid;
        gid_t gid;

        if (not user.empty())
        {
            passwd  pwd;
            passwd *result         = nullptr;
            char    buffer[BUFSIZ] = {0};

            auto ret =
                getpwnam_r(user.c_str(), &pwd, buffer, BUFSIZ, &result);

            if (result == nullptr)
            {
                if (ret == 0)
                    throw std::runtime_error("no such user: " + user);

                throw std::runtime_error("getpwnam_r() failed");
            }

            uid = pwd.pw_uid;
            gid = pwd.pw_gid;

            utils::slog() << "[DTN Agent] dtn_expand_and_group for user "
                          << user << "(uid:" << uid << ", gid:" << gid << ")";
        }
        else
        {
            utils::slog() << "[DTN Agent] no user given; "
                          << "running dtn_expand_and_group with defaults";
        }

        size_t group_size = message["group_size"].asLargestUInt();

        if (group_size == 0)
        {
            throw std::runtime_error("group_size is empty or zero");
        }

        size_t max_files = message["max_files"].asLargestUInt();

        if (max_files < 0)
        {
            throw std::runtime_error("max_files cannot be < 0");
        }

        // "dtn_expand_and_group" command need to form absolute path
        // names at source site and destination site.
        //
        // We know the absolute paths at source site when we expand
        // files under @src_paths@.  We form absolute path names at
        // destination site as follows: (1) find path prefixes for
        // paths specified in @src_paths@; (2) remove them from expanded
        // path names; (3) prefix @dst_path@.

        std::vector<std::string> path_prefixes;

        for (auto const & p : src_paths)
        {
            utils::Path path(p);
            path_prefixes.push_back(path.dir_name());
        }

        PathGroupsHelper helper(src_paths);
        auto const &path_groups = helper.group_by_size(group_size, max_files);

        Json::Value groups(Json::arrayValue);

        for (auto const &path_group : path_groups)
        {
            Json::Value  group;
            Json::UInt64 group_size = 0;

            for (auto const &path : path_group)
            {
                Json::Value pv;

                auto sz     = path.size();
                total_size += sz;
                group_size += sz;

                // TODO: permission check.
                // TODO: do real path concat.

                // get dirname; strip data dir name; append to dst_path
                auto real_dst_path = make_dst_path_name(path.name(),
                                                        dst_path,
                                                        path_prefixes);

                if (path.is_directory())
                {
                    pv.append(path.name() + "/");
                    pv.append(real_dst_path + "/");
                }
                else
                {
                    pv.append(path.name());
                    pv.append(real_dst_path);
                }

                pv.append(static_cast<Json::UInt64>(sz));
                pv.append(0);

                group["files"].append(pv);
            }

            group["size"] = group_size;
            groups.append(group);
        }

        response_files["groups"] = groups;
    }
    catch (std::exception const & ex)
    {
        auto error = "error: " + std::string(ex.what());
        utils::slog() << "[DTN Agent] dtn_expand failed: " << error;
        return json_response(1, error);
    }

    Json::Value response   = json_response(0, "OK");
    response["total_size"] = static_cast<Json::UInt64>(total_size);
    response["files"]      = response_files;

    return response;
}

const DTNAgent::expand_and_group_v2_params
DTNAgent::decode_expand_and_group_command_v2_params(Json::Value const &message)
{
    expand_and_group_v2_params params;

    auto const files = message["src_path"];

    if (files.empty())
    {
        throw std::runtime_error("empty parmeter: src_path");
    }

    if (files.isArray())
    {
        for (auto const &p : files)
        {
            params.src_paths.push_back(p.asString());
        }
    }
    else
    {
        params.src_paths.push_back(files.asString());
    }

    params.dst_path = message["dst_path"].asString();
    if (params.dst_path.empty())
    {
        throw std::runtime_error("empty parmeter: dst_path");
    }

    // clear designated destination folder of any trailing
    // slashes.
    while (params.dst_path.back() == '/')
    {
        params.dst_path.pop_back();
    }

    params.group_size = message["group_size"].asLargestUInt();
    if (params.group_size == 0)
    {
        throw std::runtime_error("group_size is empty or zero");
    }

    params.max_files  = message["max_files"].asLargestUInt();
    params.user       = message["user"].asString();

    params.result_in_db = message["result_in_db"].asBool();

    params.compute_checksum   = message["checksum"]["compute"].asBool();

    if (params.compute_checksum and !message["checksum"]["algorithm"].empty())
    {
        params.checksum_algorithm = message["checksum"]["algorithm"].asString();
        params.md = EVP_get_digestbyname(params.checksum_algorithm.c_str());
        if (!params.md)
        {
            throw std::runtime_error("Unknown message digest algorithm: " +
                                     params.checksum_algorithm);
        }
    }

    utils::slog() << "[DTN Agent] expand_and_group_v2 params: "
                  << "result_in_db: "
                  << (params.result_in_db ? "true" : "false")
                  << ", compute_checksum: "
                  << (params.compute_checksum ? "true" : "false")
                  << ", checksum_algorithm: "
                  << params.checksum_algorithm;

    return params;
}

// ----------------------------------------------------------------------

void
DTNAgent::add_file_checksum(utils::Path const &path,
                            std::string const &real_dst_path,
                            DTNAgent::expand_and_group_v2_params const &params,
                            Json::Value &checksums)
{
    Json::Value checksum;
    checksum.append(real_dst_path);
    checksum.append(utils::checksum(path.canonical_name(), params.md));
    checksums.append(checksum);
}

void
DTNAgent::add_dir_checksum(utils::Path const &path,
                           std::vector<std::string> const &path_prefixes,
                           DTNAgent::expand_and_group_v2_params const &params,
                           Json::Value &checksums)
{
    if (not path.is_directory())
    {
        return;
    }

    for (auto const &group : utils::DirectoryTree(path.canonical_name()).divide(0))
    {
        for (auto const &p : group)
        {
            if (p.is_regular_file())
            {
                auto real_dst_path = make_dst_path_name(p.canonical_name(),
                                                        params.dst_path,
                                                        path_prefixes);

                Json::Value checksum;
                checksum[0] = real_dst_path;
                checksum[1] = utils::checksum(p.canonical_name(), params.md);

                checksums.append(checksum);
            }
        }
    }
}

// ----------------------------------------------------------------------

void
DTNAgent::add_dir_checksum_of_checksums(
    utils::Path const                          &path,
    std::string const                          &real_dst_path,
    DTNAgent::expand_and_group_v2_params const &params,
    Json::Value                                &checksum)
{
    auto result =
        utils::dir_checksum_of_checksums(path,
                                         params.md,
                                         true,
                                         checksum_threads_,
                                         checksum_file_size_threshold_);

    const auto new_dst_path = utils::join_paths(real_dst_path,
                                                path.base_name()) + "/";

    Json::Value csum;
    csum.append(new_dst_path);
    csum.append(result);

    checksum.append(csum);
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_expand_and_group_command_v2(Json::Value const &message)
{
    Json::Value response_files;
    size_t      total_size = 0;

    try
    {
        auto const params = decode_expand_and_group_command_v2_params(message);

        check_expand_params(params.src_paths);

        std::vector<std::string> path_prefixes;
        for (auto const & p : params.src_paths)
        {
            path_prefixes.push_back(utils::Path(p).dir_name());
        }

        if (params.max_files < 0)
        {
            throw std::runtime_error("max_files cannot be < 0");
        }

        uid_t uid;
        gid_t gid;

        if (not params.user.empty())
        {
            passwd  pwd;
            passwd *result         = nullptr;
            char    buffer[BUFSIZ] = {0};

            auto ret =
                getpwnam_r(params.user.c_str(), &pwd, buffer, BUFSIZ, &result);

            if (result == nullptr)
            {
                if (ret == 0)
                    throw std::runtime_error("no such user: " + params.user);

                throw std::runtime_error("getpwnam_r() failed");
            }

            uid = pwd.pw_uid;
            gid = pwd.pw_gid;

            utils::slog() << "[DTN Agent] expand_and_group_v2 for user"
                          << params.user << "(uid:" << uid << ", gid:" << gid << ")";
        }
        else
        {
            utils::slog() << "[DTN Agent] no user given; "
                          << "running expand_and_group_v2 with default permissions";
        }

        std::vector<Json::Value> groups;
        std::set<utils::Path>    parent_dirs;

        // Make a unique set of input paths.  We'll need to refactor
        // this whole thing a bit to use sets, but that change will be
        // more pervasive.
        std::set<std::string> src_paths_set(params.src_paths.begin(),
                                            params.src_paths.end());

        for (auto const &path : src_paths_set)
        {
            utils::DirectoryTree tree(path);

            auto results  = tree.divide(params.group_size, params.max_files);
            total_size   += tree.size();

            auto parents = results.get_dir_list();
            parent_dirs.insert(parents.begin(), parents.end());

            for (auto const &gp : results)
            {
                Json::Value group; // {Json::arrayValue};

                for (auto const &path : gp)
                {
                    Json::Value pv;
                    Json::Value checksums;

                    // for async checksum of files.
                    std::map<std::string, std::future<std::string>> futures;

                    auto &task_counter = utils::global_task_counter();

                    if (path.is_directory())
                    {
                        // auto real_dst_path = make_dst_path_name(path.dir_name(),
                        //                                         params.dst_path,
                        //                                         path_prefixes);
                        auto real_dst_path = params.dst_path;

                        pv.append(path.canonical_name());

                        if (real_dst_path.back() != '/')
                            pv.append(real_dst_path + "/");
                        else
                            pv.append(real_dst_path);

                        if (params.compute_checksum)
                        {
                            add_dir_checksum_of_checksums(path,
                                                          real_dst_path,
                                                          params,
                                                          checksums);
                        }
                    }
                    else
                    {
                        auto real_dst_path =
                            make_dst_path_name(path.canonical_name(),
                                               params.dst_path,
                                               path_prefixes);

                        pv.append(path.canonical_name());
                        pv.append(real_dst_path);

                        if (params.compute_checksum)
                        {
                            if (gp.size() > 2 and
                                path.size() > checksum_file_size_threshold_ and
                                task_counter.get() < checksum_threads_)
                            {
                                // Use async tasks for large files.
                                utils::slog() << "[DTN Agent] Using an async task "
                                              << "to compute checksum of "
                                              << path.canonical_name()
                                              << " (size:" << path.size() << ")";

                                auto f = std::async(std::launch::async,
                                                    utils::checksum_file,
                                                    path.canonical_name(),
                                                    params.md);

                                task_counter.increment();

                                utils::slog() << "[DTN Agent] Checksum tasks: "
                                              << task_counter.get();

                                futures.emplace(real_dst_path, std::move(f));
                            }
                            else
                            {
                                add_file_checksum(path,
                                                  real_dst_path,
                                                  params,
                                                  checksums);
                            }
                        }
                    }

                    pv.append(static_cast<Json::UInt64>(path.size()));
                    pv.append(0);

                    group["files"].append(pv);

                    if (params.compute_checksum)
                    {
                        // get async computation results.
                        for (auto &f : futures)
                        {
                            auto p = f.first;
                            auto c = f.second.get();

                            task_counter.decrement();

                            Json::Value v;
                            v.append(p);
                            v.append(c);

                            utils::slog() << "[DTN Agent] Async computed "
                                          << "checksum for "
                                          << p << ": " << c;

                            checksums.append(v);
                        }

                        group["checksum"]["algorithm"] = params.checksum_algorithm;
                        group["checksum"]["checksums"] = checksums;
                    }
                }

                group["size"] = static_cast<Json::UInt64>(gp.size());
                groups.emplace_back(group);
            }

            Json::Value res_groups;

            if (use_expand_v2_parent_list_)
            {
                Json::Value head_group{Json::objectValue};

                for (auto const &d : parent_dirs)
                {
                    auto dir = make_dst_path_name(d.name(),
                                                  params.dst_path,
                                                  path_prefixes);

                    Json::Value pv;

                    pv.append(d.canonical_name());
                    pv.append(dir + (dir.back() == '/' ? "" : "/"));
                    pv.append(0);
                    pv.append(0);

                    head_group["files"].append(pv);

                    // if (parent_dirs.size() == 1)
                    // {
                    //     Json::Value pv_dup;

                    //     utils::slog() << "[DTN Agent] "
                    //                   << "Getting around "
                    //                   << "directory creation quirk: "
                    //                   << "adding duplcate entry";

                    //     pv_dup.append(d.canonical_name());
                    //     pv_dup.append(dir + (dir.back() == '/' ? "" : "/"));
                    //     pv_dup.append(0);
                    //     pv_dup.append(0);

                    //     head_group["files"].append(pv_dup);
                    // }
                }

                if (not head_group.empty())
                {
                    head_group["parent_folder_list"] = true;
                    res_groups.append(head_group);
                }
            }

            for (auto const &g : groups)
            {
                res_groups.append(g);
            }

            // If "result_in_db" is set to true, write path groups to
            // DB and add the DB IDs to the response; else append path
            // groups to the response.
            if (params.result_in_db)
            {
                Json::Value db_groups;

                // Insert results to DB "bde", collection "block".
                for (auto const &group : res_groups)
                {
                    auto id = store().add_block(group);

                    Json::Value v;
                    v["id"]   = id;
                    v["size"] = group["size"];

                    db_groups.append(v);
                }

                response_files["result_in_db"] = true;
                response_files["groups"] = db_groups;
            }
            else
            {
                response_files["groups"] = res_groups;
            }
        }
    }
    catch (std::exception const & ex)
    {
        auto error = "dtn_expand_and_group_v2 error: " + std::string(ex.what());
        utils::slog() << "[DTN Agent] " << error;
        return json_response(1, error);
    }

    Json::Value response   = json_response(0, "OK");
    response["total_size"] = static_cast<Json::UInt64>(total_size); // TODO
    response["files"]      = response_files;

    utils::slog() << "[DTN Agent] expand_and_group_v2 response: " << response;

    return response;
}

std::string
DTNAgent::make_dst_path_name(std::string const &src_path,
                             std::string const &dst_path,
                             std::vector<std::string> const &prefixes) const
{
#if 0
    // Old method of making destination path name.
    std::string src = utils::Path(src_path).canonical_name();

    // strip any data folder prefix from src_path
    for (auto const &prefix : prefixes)
    {
        auto res = std::mismatch(prefix.begin(), prefix.end(), src.begin());
        if (res.first == prefix.end())
            src.erase(0, prefix.length());
    }
#endif

    std::string basename = utils::Path(src_path).base_name();
    std::string result;

    if (dst_path.back() != '/')
        result = dst_path + "/" + basename;
    else
        result = dst_path + basename;

    utils::slog() << "[DTN Agent] " << __func__ << "():"
                  << " src: " << src_path << ","
                  << " basename: " << basename
                  << " result: " << result;

    return result;
}

std::string
DTNAgent::make_dst_dir_name(std::string const &src_path,
                            std::string const &dst_path) const
{
    std::string src(src_path);

    // strip any data folder prefix from src_path
    for (auto const &df : data_folders_)
    {
        auto res = std::mismatch(df.begin(), df.end(), src.begin());
        if (res.first == df.end())
            src.erase(0, df.length());
    }

    // find base dirname of src_path
    char buf1[PATH_MAX] = {0};
    strncpy(buf1, src.c_str(), PATH_MAX);
    std::string src_dir = dirname(buf1);

    return dst_path + src_dir;
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_send_icmp_ping_command(Json::Value const &message) const
{
    try
    {
        auto dest = message["destination"].asString();

        if (dest.empty())
        {
            auto status = "error: parameter missing: 'destination'";
            utils::slog() << "[DTN Agent] [icmp_ping] " << status;
            return json_response(1, status);
        }

        auto deadline = message["deadline"].asInt();
        auto count    = message["count"].asInt();
        auto ipv6     = message["ipv6"].asBool();

        // Use some defaults for deadline and count, or ping could
        // block for a long time.
        if (deadline == 0) { deadline = 2; }
        if (count == 0) { count = 2; }

        auto result = utils::run_ping(dest, deadline, count, ipv6);

        utils::slog() << "[DTN Agent] [icmp_ping] "
                      << "result code: " << result.code << "; "
                      << "result text: " << result.text;

        if (result.code == 0)
        {
            auto status = "Success: pinged " + dest;
            utils::slog() << "[DTN Agent] [icmp ping] " << status;
            return json_response(0, status);
        }

        auto status = "Ping failed. Error: " + result.text;
        utils::slog() << "[DTN Agent] [icmp ping] " << status;
        return json_response(result.code, status);
    }
    catch (std::exception const & ex)
    {
        auto error = "error: " + std::string(ex.what());
        utils::slog() << "[DTN Agent] [icmp_ping] " << error;
        return json_response(1, error);
    }
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_send_ping_command(Json::Value const &message)
{
    // TODO: bind sending socke to use src ip.

    auto src_ip   = message["source"]["ip"].asString();
    auto tgt_ip   = message["target"]["ip"].asString();
    auto tgt_port = message["target"]["port"].asInt();

    if (tgt_ip.empty())
    {
        auto error = "target IP is not given in ping command";
        utils::slog() << "[DTN Agent] " << error;

        return json_response(1, error);
    }

    if (tgt_port == 0)
    {
        tgt_port = default_ping_port_;
    }

    std::string ping_message = "Hello Big Data Express 1.0!";
    std::string ping_target  = tgt_ip + ":" + std::to_string(tgt_port);

    utils::slog() << "[DTN Agent] pinging " << ping_target;

    try
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            auto error = "socket() error: " + std::string(strerror(errno));
            throw std::runtime_error(error);
        }

        int arg;
        if ((arg = fcntl(sock, F_GETFL, 0)) < 0)
        {
            close(sock);
            auto error = "fcntl(F_GETFL) error: " + std::string(strerror(errno));
            throw std::runtime_error(error);
        }

        arg |= O_NONBLOCK;

        if (fcntl(sock, F_SETFL, arg) < 0)
        {
            close(sock);
            auto error = "fcntl(F_SETFL) error: " + std::string(strerror(errno));
            throw std::runtime_error(error);
        }

        struct sockaddr_in addr;
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(tgt_port);
        // TODO: resolve remote host.
        addr.sin_addr.s_addr = inet_addr(tgt_ip.c_str());

        auto result = connect(sock, (const sockaddr *) &addr, sizeof(addr));
        if (result < 0)
        {
            if (errno != EINPROGRESS)
            {
                close(sock);
                auto error = "connect() error:" + std::string(strerror(errno));
                throw std::runtime_error(error);
            }
            else
            {
                for (;;)
                {
                    timeval timeout;
                    timeout.tv_sec  = 0;
                    timeout.tv_usec = 50000;

                    fd_set fdset;
                    FD_ZERO(&fdset);
                    FD_SET(sock, &fdset);

                    result = select(sock+1, nullptr, &fdset, nullptr, &timeout);

                    if (result < 0 && errno != EINTR)
                    {
                        auto error = "select() error:" + std::string(strerror(errno));
                        throw std::runtime_error(error);
                    }

                    if (result > 0)
                    {
                        int       opt;
                        socklen_t len;

                        // do an error check.
                        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &opt, &len) < 0)
                        {
                            close(sock);
                            auto error = "getsockopt() error: " +
                                std::string(strerror(errno));
                            throw std::runtime_error(error);
                        }

                        // if errored, fail.
                        if (opt != 0)
                        {
                            close(sock);
                            auto error = "getsockopt() error, : opt: " +
                                std::to_string(opt) + ", " +
                                std::string(strerror(opt));
                            throw std::runtime_error(error);
                        }

                        // there's data on the socket.
                        break;
                    }
                    else
                    {
                        close(sock);
                        auto error = "select() timed out";
                        throw std::runtime_error(error);
                    }
                } // end for
            }
        }

        // go back to blocking mode.
        if ((arg = fcntl(sock, F_GETFL, nullptr)) < 0)
        {
            auto error = "fcntl(F_GETFL) error: " + std::string(strerror(errno));
            throw std::runtime_error(error);
        }

        arg &= (~O_NONBLOCK);

        if (fcntl(sock, F_SETFL, arg) < 0)
        {
            close(sock);
            auto error = "fcntl(F_SETFL) error: " + std::string(strerror(errno));
            throw std::runtime_error(error);
        }

        // TCP_NODELAY will disable Nagle's algorithm on this socket;
        // so packets will be sent immediately.
        int flag = 1;
        result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag,
                            sizeof(int));

        if (result < 0)
        {
            utils::slog() << "[DTN Agent] setsockopt(TCP_NODELAY) failed: "
                          << strerror(errno);
        }

        // A smaller TCP_USER_TIMEOUT will help us fail faster.
        flag = 1000;
        result = setsockopt(sock, IPPROTO_TCP, TCP_USER_TIMEOUT, (char *) &flag,
                            sizeof(flag));

        if (result < 0)
        {
            utils::slog() << "[DTN Agent] setsockopt(TCP_USER_TIMEOUT) failed: "
                          << strerror(errno);
        }

        // write & read "ping" message.
        if (write(sock, ping_message.c_str(), ping_message.length()) < 0)
        {
            close(sock);
            auto error = "write() error: " + std::string(strerror(errno));
            throw std::runtime_error(error);
        }

        char pong[32]  = {0};

        if (read(sock, pong, ping_message.length()) < 0)
        {
            close(sock);
            auto error = "read() error: " + std::string(strerror(errno));
            throw std::runtime_error(error);
        }

        close(sock);

        utils::slog() << "[DTN Agent] received response from " << ping_target
                      << ", message: " << pong;

        std::string expected(ping_message);
        std::reverse(expected.begin(), expected.end());

        if (expected == pong)
        {
            // success
            auto msg = "pinged " + ping_target + " OK";
            utils::slog() << "[DTN Agent] " << msg;
            return json_response(0, msg);
        }
        else
        {
            auto error = "unexpected response from " + ping_target + " " +
                "(expected: " + expected + ", received:" + pong + ")";
            utils::slog() << "[DTN Agent] ping error: " << error;
            return json_response(1, error);
        }

    }
    catch (std::exception const & ex)
    {
        auto error = "error pinging " + ping_target +
            " (" + std::string(ex.what()) + ")";
        utils::slog() << "[DTN Agent] " << error;
        return json_response(1, error);
    }

    // We should never get here.
    auto error = "pinging " + ping_target + ": something weird has happened";
    utils::slog() << "[DTN Agent] ping error: " << error;
    return json_response(1, error);
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_start_pong_command(Json::Value const &message)
{
    auto port     = message["port"].asInt();
    auto port_str = std::to_string(port);

    Json::Value response;

    try
    {
        auto status = "Attempting to launch pong thread on port "
            + port_str + "...";
        utils::slog() << "[DTN Agent] " << status;

        if (pong_thread_.joinable())
        {
            status = "a pong thread is already running here";
            utils::slog() << "[DTN Agent] " << status;
            return json_response(1, status);
        }

        pong_thread_ = std::thread([this, port](){
                utils::echo_server server(pong_thread_io_service_, port);
                pong_thread_io_service_.run();
            });

        utils::slog() << "[DTN Agent] created thread with id = "
                      << pong_thread_.get_id();
    }
    catch (std::exception const & ex)
    {
        auto error = "exception in pong: " + std::string(ex.what());
        utils::slog() << "[DTN Agent] " << error;
        return json_response(1, error);
    }

    auto status = "waiting for ping messages on port " + port_str;
    utils::slog() << "[DTN Agent] " << status;
    return json_response(0, status);
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_stop_pong_command(Json::Value const &message)
{
    try
    {
        utils::slog() << "[DTN Agent] attempting to stop pong thread (id="
                      << pong_thread_.get_id() << "); joinable: "
                      << (pong_thread_.joinable() == false ? "false" : "true");

        if (pong_thread_.joinable())
        {
            pong_thread_io_service_.stop();
            pong_thread_.join();
            utils::slog() << "[DTN Agent] stopped pong thread (id="
                          << pong_thread_.get_id() << ")";
        }
        else
        {
            throw std::runtime_error("no pong is running here");
        }
    }
    catch (std::exception& e)
    {
        auto error = "error: " + std::string(e.what());
        utils::slog() << "[DTN Agent] " << error;
        return json_response(1, error);
    }

    auto status = "stopped waiting for ping messages";
    utils::slog() << "[DTN Agent] " << status;
    return json_response(0, status);
}

// ----------------------------------------------------------------------

const std::pair<uid_t, gid_t>
DTNAgent::get_system_user(std::string const & user) const
{
    passwd  pwd;
    passwd *result         = nullptr;
    char    buffer[BUFSIZ] = {0};

    auto ret = getpwnam_r(user.c_str(), &pwd, buffer, BUFSIZ, &result);

    if (result == nullptr)
    {
        if (ret == 0)
        {
            throw std::runtime_error("no such user: " + user);
        }

        auto status = "getpwnam_r() failed; reason: " + std::string(strerror(errno));
        throw std::runtime_error(status);
    }

    return make_pair(pwd.pw_uid, pwd.pw_gid);
}

Json::Value
DTNAgent::handle_list_command(Json::Value const &message)
{
    // compute adler32 and md5 checksum
    auto checksum = message["checksum"].asBool();

    auto path = message["path"].asString();

    if (path.empty())
    {
        auto status = "error: empty path";
        utils::slog() << "[DTN Agent] " << status << ", message: " << message;
        return json_response(1, status);
    }

    uid_t uid = -1;
    gid_t gid = -1;

    auto user = message["user"].asString();

    if (user.empty())
    {
        utils::slog() << "[DTN Agent] No user; will not check permissions";
    }
    else
    {
        try
        {
            auto u = get_system_user(user);
            uid    = u.first;
            gid    = u.second;

            utils::slog() << "[DTN Agent] Will check permissions for user " << user
                          << " (uid: " << u.first << ", gid:" << u.second << ")";
        }
        catch (std::exception const &ex)
        {
            auto status = "Can't find user " + user + " (reason: " + ex.what() + ")";
            utils::slog() << "[DTN Agent] "<< status;
            return json_response(1, status);
        }
    }

    // Check that path is within data folder.
    if  (not data_folder_contains(path))
    {
        auto status = "Error: " + path + " is outside data folder.";
        utils::slog() << "[DTN Agent] " << status << ", message: " << message;
        return json_response(1, status);
    }

    utils::Path po(path);

    // Check that this user can read this path.
    if ((not user.empty()) and (not po.readable_by(uid, gid)))
    {
        auto status = "User " + user + " can't read " + po.name() + "; omitting.";
        utils::slog() << "[DTN Agent] " << status;
        return json_response(1, status);
    }

    Json::Value entries(Json::arrayValue);

    if (po.is_regular_file())
    {
        entries.append(list_file(po, checksum));
    }
    else if (po.is_directory())
    {
        if (checksum)
        {
            auto status = "list command requesting a folder (" + po.name() 
                + ") with checksum enabled.";
            utils::slog() << "[DTN Agent] error: " << status;
            return json_response(1, status);
        }

        entries = list_directory_entries(po, user, uid, gid);
    }
    else if (po.is_symbolic_link())
    {
        auto res = list_symlink(po, user, uid, gid);
        if (res != Json::nullValue)
        {
            entries.append(res);
        }
    }
    else
    {
        auto status = po.name() + " is not regular file, symlink, or directory.";
        utils::slog() << "[DTN Agent] Error: " << status;
        return json_response(1, status);
    }

    Json::Value response = json_response(0, "OK");
    response["entries"]  = entries;

    return response;
}

Json::Value
DTNAgent::list_file(utils::Path const &p, bool checksum) const
{
    Json::Value f;

    f.append(p.base_name());
    f.append(static_cast<Json::UInt64>(p.size()));
    f.append(false);

    if (checksum)
    {
        Json::Value cs;
        cs["adler32"] = utils::checksum(p.name(), "adler32");
        cs["md5"] = utils::checksum(p.name(), "md5");
        f.append(cs);
    }

    return f;
}

Json::Value
DTNAgent::list_directory(utils::Path const &p) const
{
    Json::Value f;

    f.append(p.base_name());
    f.append(0);
    f.append(true);

    return f;
}

Json::Value
DTNAgent::list_directory_entries(utils::Path const &path,
                                 std::string const &user,
                                 uid_t              uid,
                                 gid_t              gid) const
{
    Json::Value entries(Json::arrayValue);

    for (auto const &p : utils::DirectoryWalker(path, false))
    {
        // ignore files that are not accessible to user.
        if ((not user.empty()) and (not p.readable_by(uid, gid)))
        {
            utils::slog() << "[DTN Agent] User " << user << " can't read "
                          << p.name() << "; omitting.";
            continue;
        }

        if (p.is_regular_file())
        {
            entries.append(list_file(p));
        }
        else if (p.is_directory())
        {
            entries.append(list_directory(p));
        }
        else if (p.is_symbolic_link())
        {
            auto res = list_symlink(p, user, uid, gid, path.name());
            if (res != Json::nullValue)
            {
                // TODO: this will result in duplicate entries when
                // symlink is pointing to an entry in the same
                // directory.
                entries.append(res);
            }
        }
    }

    return entries;
}

Json::Value
DTNAgent::list_symlink(utils::Path const &p,
                       std::string const &user,
                       uid_t              uid,
                       gid_t              gid,
                       std::string const &parent) const
{
    try
    {
        auto const newp = p.follow_sybolic_link();

        utils::slog() << "[DTN Agent] Resolved symlink " << p.name()
                      << " to " << newp.name();

        // If the expanded symlink points to an entry outside
        // data folder, we ignore it.
        if  (not data_folder_contains(newp.name()))
        {
            auto status = "Symlink " + p.name() + " expanded to "
                + newp.name() + ", which is outside data folder; omitting.";
            utils::slog() << "[DTN Agent] " << status;
            throw std::runtime_error(status);
        }

        if ((not user.empty()) and (not newp.readable_by(uid, gid)))
        {
            auto status = "User " + user + " can't read "
                + newp.name() + "; omitting.";
            utils::slog() << "[DTN Agent] " << status;
            throw std::runtime_error(status);
        }

        // Check if symlink points to another entry in the "parent"
        // directory.  We want to avoid duplicates in the result, so
        // we omit symlinks that link to the same directory.
        if ((not parent.empty()) and (newp.dir_name() == parent))
        {
            auto status = "Symlink " + newp.name() + " is in " +
                parent + "; omitting to avoid duplicates";
            utils::slog() << "[DTN Agent] " << status;
            return Json::Value{};
        }

        // If the expanded symlink points to another symlink
        // or a UNIX "special" file, we ignore it.

        if (newp.is_regular_file())
        {
            return list_file(newp);
        }
        else if (newp.is_directory())
        {
            return list_directory(newp);
        }
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[DTN Agent] Error expanding symbolic link "
                      << p.name() << ", reason: " << ex.what();
    }

    return Json::Value{};
}

// ----------------------------------------------------------------------

// Equivalent of "/usr/sbin/ip route add 10.38.0.0/16 via 131.225.2.15
// dev enp11s0f0"
Json::Value
DTNAgent::handle_add_route_command(Json::Value const &message)
{
    if (ignore_route_cmds_)
    {
        utils::slog() << "[DTN Agent] Ignoring add_route command";
        return json_response(0, "OK, but ignored by configuration");
    }

    auto dst  = message["destination"].asString();
    auto via  = message["via"].asString();
    auto mask = message["mask"].asString();
    auto dev  = message["dev"].asString();

    if (dst.empty())
    {
        return json_response(1, "Error: empty destination parameter");
    }

    auto params = "{dst: " + dst + ", via: " + via
        + ", mask:" + mask + ", device: " + dev + "}";

    try
    {
        utils::slog() << "[DTN Agent] Adding route " << params;
        utils::add_route(dst, via, mask, dev);
    }
    catch (std::exception const &ex)
    {
        auto err = std::string("Error adding route: ") + ex.what();
        utils::slog() << "[DTN Agent] " << err << " " << params;
        return json_response(1, err);
    }

    return json_response(0, "OK");
}

// ----------------------------------------------------------------------

// Equivalent of "/usr/sbin/ip route del ${dst}".
Json::Value
DTNAgent::handle_delete_route_command(Json::Value const &message)
{
    if (ignore_route_cmds_)
    {
        utils::slog() << "[DTN Agent] Ignoring delete_route command";
        return json_response(0, "OK, but ignored by configuration");
    }

    auto dst = message["destination"].asString();
    auto via = message["via"].asString();
    auto dev = message["dev"].asString();

    if (dst.empty())
    {
        return json_response(1, "Error: empty destination parameter");
    }

    auto params = "{dst: " + dst + ", via: " + via + ", device:" + dev + "}";

    try
    {
        utils::slog() << "[DTN Agent] Deleting route " << params;
        utils::delete_route(dst, via, dev);
    }
    catch (std::exception const &ex)
    {
        auto err = std::string("Error deleteing route: ") + ex.what();
        utils::slog() << "[DTN Agent] " << err << " " << params;
        return json_response(1, err);
    }

    return json_response(0, "OK");
}

// ----------------------------------------------------------------------

// Equivalent of "/usr/sbin/arp -s ${ip} ${mac}".
Json::Value
DTNAgent::handle_add_neighbor_command(Json::Value const &message)
{
    if (ignore_arp_cmds_)
    {
        utils::slog() << "[DTN Agent] Ignoring add_arp command";
        return json_response(0, "OK, but ignored by configuration");
    }

    auto ip  = message["ip"].asString();
    auto mac = message["mac"].asString();

    if (ip.empty())
    {
        return json_response(1, "Error: empty ip parameter");
    }

    if (mac.empty())
    {
        return json_response(1, "Error: empty mac parameter");
    }

    std::string dev;

    if (not message["dev"].empty())
    {
        dev = message["dev"].asString();
        utils::slog() << "[DTN Agent] Using specified " << dev
                      << " for ARP route";
    }
    else
    {
        dev = data_ifaces_.at(0);
        utils::slog() << "[DTN Agent] Using data interface " << dev
                      << " for ARP route";
    }

    auto params = "{ip:" + ip + ", mac:" + mac + ", device: " + dev + "}";

    try
    {
        utils::slog() << "[DTN Agent] Adding ARP entry " << params;
        utils::add_arp(ip, mac, dev);
    }
    catch (std::exception const &ex)
    {
        auto err = std::string("Error adding arp entry: ") + ex.what();
        utils::slog() << "[DTN Agent] " << err << " " << params;
        return json_response(1, err);
    }

    return json_response(0, "OK");
}

// ----------------------------------------------------------------------

// Equivalent of "/usr/sbin/arp -d ${ip}";
Json::Value
DTNAgent::handle_delete_neighbor_command(Json::Value const &message)
{
    if (ignore_arp_cmds_)
    {
        utils::slog() << "[DTN Agent] Ignoring delete_arp command";
        return json_response(0, "OK, but ignored by configuration");
    }

    auto ip = message["ip"].asString();

    if (ip.empty())
    {
        return json_response(1, "Error: empty IP parameter");
    }

    auto params = "{ip:" + ip + "}";

    try
    {
        utils::slog() << "[DTN Agent] Deleting ARP entry " << params;
        utils::delete_arp(ip);
    }
    catch (std::exception const &ex)
    {
        auto err = std::string("Error deleting arp entry: ") + ex.what();
        utils::slog() << err << " " << params;
        return json_response(1, err);
    }

    return json_response(0, "OK");
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_path_size_command(Json::Value const &message)
{
    auto         path = message["path"].asString();
    Json::UInt64 sz   = 0;

    if (path.empty())
    {
        return json_response(1, "empty path parameter");
    }

    try
    {
        utils::Path p(path);

        if (not data_folder_contains(p.name()))
        {
            auto status = p.name() + " is not within a data folder";
            utils::slog() << "[DTN Agent] " << status;
            return json_response(1, status);
        }

        if (p.is_directory())
        {
            for (auto const &f : DirectoryWalker(p, true, true))
            {
                sz += f.size();
            }
        }
        else if (p.is_regular_file())
        {
            sz = p.size();
        }
        else if (p.is_symbolic_link())
        {
            auto const newp = p.follow_sybolic_link();

            if (not data_folder_contains(newp.name()))
            {
                auto status = newp.name() + " is not within a data folder";
                utils::slog() << "[DTN Agent] " << status;
                return json_response(1, status);
            }

            if (newp.is_directory())
            {
                for (auto const &f : DirectoryWalker(newp, true, true))
                {
                    sz += f.size();
                }
            }
            else if (newp.is_regular_file())
            {
                sz = newp.size();
            }
        }
        else
        {
            auto status = p.name() + " is not a regular file, directory or symlink";
            utils::slog() << "[DTN Agent] " << status;
            return json_response(1, status);
        }
    }
    catch (std::exception const &ex)
    {
        auto status = "Error with " + path + ": " + ex.what();
        utils::slog() << "[DTN Agent] " << status;
        return json_response(1, status);
    }

    auto resp    = json_response(0, "OK");
    resp["path"] = path;
    resp["size"] = sz;

    return resp;
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_compute_checksums_command(Json::Value const &message) const
{
    auto algorithm = message["algorithm"].asString();
    if (algorithm.empty())
        algorithm = "sha1";

    auto src_path = message["src_path"].asString();
    if (src_path.empty())
    {
        return json_response(1, "empty parameter: src_path");
    }

    std::vector<std::string> path_prefixes{src_path};

    auto dst_path = message["dst_path"].asString();
    if (dst_path.empty())
    {
        return json_response(1, "empty parmeter: dst_path");
    }

    while (dst_path.back() == '/')
    {
        dst_path.pop_back();
    }

    Json::Value checksums;
    checksums["algorithm"] = algorithm;

    try
    {
        EVP_MD const *md = EVP_get_digestbyname(algorithm.c_str());

        if (!md)
        {
            throw std::runtime_error("Unknown message digest " + algorithm);
        }

        utils:Path root(src_path);

        if (root.is_directory())
        {
            auto checksum =
                dir_checksum_of_checksums(root,
                                          md,
                                          true,
                                          checksum_threads_,
                                          checksum_file_size_threshold_);

            auto const &real_dst = make_dst_path_name(root.name(),
                                                      dst_path,
                                                      path_prefixes);

            Json::Value v;
            v.append(real_dst);
            v.append(checksum);

            checksums["checksums"].append(v);
        }
        else if (root.is_regular_file())
        {
            // Handle regular file checksum.
            auto const &name = root.canonical_name();
            auto const &csum = utils::checksum(name, algorithm);

            auto const &real_dst = dst_path + "/" + root.base_name();

            Json::Value v;
            v.append(real_dst);
            v.append(csum);
            checksums["checksums"].append(v);
        }
        else
        {
            // We don't handle other types of filesystem objects.
        }
    }
    catch (std::exception const &ex)
    {
        std::stringstream ss;
        ss << "Error when computing checksum: " << ex.what();
        utils::slog() << ss.str();
        return json_response(2, ss.str());
    }

    auto response      = json_response(0, "OK");
    response["result"] = checksums;

    return response;
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_verify_checksums_command(Json::Value const &message) const
{
    auto algorithm = message["algorithm"].asString();

    if (algorithm.empty())
        algorithm = "sha1";

    auto checksums = message["checksums"];

    if (checksums.empty())
    {
        return json_response(1, "Command contains no checksum");
    }

    try
    {
        for (auto const &c : checksums)
        {
            auto const &local_path = c[0].asString();
            if (local_path.empty())
            {
                throw std::runtime_error("No path given");
            }

            auto const &remote_csum = c[1].asString();
            if (remote_csum.empty())
            {
                throw std::runtime_error("No checksum given");
            }

            utils::Path p(local_path);
            std::string local_csum = "unknown";

            if (p.is_directory())
            {
                utils::slog() << "[DTN Agent] [dtn_verify_checksums]: \""
                              << local_path << "\" is a directory; computing "
                              << "checksum of checksums of directory contents";
                local_csum =
                    dir_checksum_of_checksums(p,
                                              algorithm,
                                              true,
                                              checksum_threads_,
                                              checksum_file_size_threshold_);
            }
            else
            {
                local_csum = utils::checksum(local_path, algorithm);
            }

            if (remote_csum != local_csum)
            {
                utils::slog() << algorithm << " mismatch for "
                              << local_path << " (reported: "
                              << remote_csum << ", computed: "
                              << local_csum << ")";

                Json::Value v;

                v["code"]  = 1;
                v["error"] = "mismatch";

                v["details"]["algorithm"] = algorithm;
                v["details"]["path"]      = local_path;
                v["details"]["computed"]  = local_csum;
                v["details"]["claimed"]   = remote_csum;

                return v;
            }
        }
    }
    catch (std::exception const &ex)
    {
        std::stringstream ss;
        ss << "Error when running checksum: " << ex.what();
        utils::slog() << ss.str();
        return json_response(2, ss.str());
    }

    return json_response(0, "OK");
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_unknown_command(std::string const &cmd,
                                 Json::Value const &message) const
{
    auto status = "Unknown command: " + cmd;
    utils::slog() << "[DTN Agent] " << status << "; message: " << message;
    return json_response(1, status);
}

// ----------------------------------------------------------------------

void
DTNAgent::add_queue(std::string const & handle,
                    std::string const & device)
{
    auto cmd = "/usr/sbin/tc qdisc add dev " + device +
        " root handle " + handle + " prio";
    utils::slog() << "[DTN Agent] Running command: \"" << cmd << "\"";
    run_process(cmd, true);
}

void
DTNAgent::add_traffic_to_queue(std::string const & device,
                               std::string const & src,
                               std::string const & flowid,
                               int priority)
{
    auto cmd = "/usr/sbin/tc filter add dev " + device +
        " parent 1:0 prio " + std::to_string(priority) +
        " protocol ip u32 match ip src " + src + " flowid " + flowid;
    utils::slog() << "[DTN Agent] Running command: \"" << cmd << "\"";
    utils::run_process(cmd, true);
}

void
DTNAgent::delete_queue(std::string const & handle)
{
    auto cmd = "/usr/sbin/tc qdisc delete handle " + handle;
    utils::slog() << "[DTN Agent] Running command: \"" << cmd << "\"";
    utils::run_process(cmd, true);
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::run_command(std::string const &cmd) const
{
    utils::slog() << "[DTN Agent] Running command: " << cmd;

    auto result = utils::run_process(cmd);

    utils::slog() << "[DTN Agent] Ran command: \"" << cmd << "\", "
                  << "process exit code: " << result.code << ", "
                  << "process output: "    << result.text;

    auto response       = json_response(result.code, result.text);
    response["command"] = cmd;

    return response;
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::json_response(int code, std::string const & error) const
{
    Json::Value v;

    v["code"]  = code;
    v["error"] = error;

    return v;
}

// ----------------------------------------------------------------------

std::ostream&
operator<<(std::ostream& out, rtnl_link_stats const & stat)
{
    out << "rx packets: " << stat.rx_packets << ", "
        << "tx packets: " << stat.tx_packets << ", "
        << "rx bytes: "   << stat.rx_bytes   << ", "
        << "tx bytes: "   << stat.tx_bytes   << ", "
        << "rx errors: "  << stat.rx_errors  << ", "
        << "tx errors: "  << stat.tx_errors  << ", "
        << "rx dropped: " << stat.rx_dropped << ", "
        << "tx dropped: " << stat.tx_dropped;

    return out;
}

std::ostream&
operator<<(std::ostream& out, DTNAgent const & agent)
{
    out << "DTN Agent id: " << agent.id() << ", "
        << "name: " << agent.name() << ", "
        << "type: " << agent.type() << ", ";

    out << "data folders: [";
    for (auto const &d : agent.data_folders_)
    {
        out << d << " ";
    }
    out << "], ";

    out << "scan data folder: "
        << std::string(agent.scan_data_folder_ ? "yes" : "no") << ", "
        << "control interface: " << agent.mgmt_iface_ << ", ";

    out << "data interfaces: [";
    for (auto const &d : agent.data_ifaces_)
    {
        out << d << " ";
    }
    out << "], ";

    out << "iozone program path: " << agent.iozone_program_path_;

    return out;
}

// ----------------------------------------------------------------------

bool
DTNAgent::has_ip_of_prefix(std::string const &str) const
{
    std::string prefix = "ip-of(";

    auto res = std::mismatch(prefix.begin(), prefix.end(), str.begin());

    return res.first == prefix.end();
}

// An awful amount of (a) ad-hoc parsing of (b) an ad-hoc spec (c) in
// C++ below; all of that is perilous territory and is certain to bite
// us some day.

std::string
DTNAgent::interpret_ip_of(std::string const &arg) const
{
    try
    {
        auto iface = get_iface_name_from_ip_of(arg);
        return utils::get_first_ipv4_address(iface);
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[DTN Agent] Error interpreting ip-of(): "
            << ex.what();
    }

    return "0.0.0.0";
}

// ----------------------------------------------------------------------

bool
DTNAgent::has_ipv6_of_prefix(std::string const &str) const
{
    std::string prefix = "ipv6-of(";

    auto res = std::mismatch(prefix.begin(), prefix.end(), str.begin());

    return res.first == prefix.end();
}

std::string
DTNAgent::interpret_ipv6_of(std::string const &arg) const
{
    try
    {
        auto iface = get_iface_name_from_ip_of(arg);
        return utils::get_first_ipv6_address(iface);
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[DTN Agent] Error interpreting ipv6-of(): "
            << ex.what();
    }

    return "::1";
}

// ----------------------------------------------------------------------

std::string
DTNAgent::get_iface_name_from_ip_of(std::string const &str) const
{
    auto v1 = utils::split(str, '(');

    if (v1.size() != 2)
    {
        auto err = "failed to parse " + str;
        utils::slog() << "[DTN Agent] " << err;
        throw std::runtime_error(err);
    }

    auto v2 = utils::split(v1[1], ')');

    if (v2.size() != 1)
    {
        auto err = "failed to parse " + str;
        utils::slog() << "[DTN Agent] " << err;
        throw std::runtime_error(err);
    }

    auto const &iface = v2[0];

    if (iface.length() > 1 and iface[0] == '&')
    {
        if (iface.find("data_interfaces") != std::string::npos)
        {
            return interpret_data_interface(iface);
        }

        if (iface.find("management_interface") != std::string::npos)
        {
            return interpret_mgmt_interface(iface);
        }
    }

    return iface;
}

std::string
DTNAgent::interpret_data_interface(std::string const &name) const
{
    auto s1 = utils::split(name, '[');

    if (s1.size() != 2)
    {
        auto err = "Failed to parse " + name;
        utils::slog() << "[DTN Agent] " << err;
        throw std::runtime_error(err);
    }

    auto s2 = utils::split(s1[1], ']');

    if (s2.size() != 1)
    {
        auto err = "Failed to parse " + name;
        utils::slog() << "[DTN Agent] " << err;
        throw std::runtime_error(err);
    }

    auto const idx = std::stoi(s2[0]);

    if (idx < data_ifaces_.size())
    {
        auto const &iface = data_ifaces_[idx];
        utils::slog() << "[DTN Agent] Will use "
                      << "data_interface[" << idx << "] ("
                      << iface << ") IPv4 address for "
                      << "mdtmftp+ data channel.";
        return iface;
    }

    auto err = std::string("Could not decode ") + name;
    // throw std::exception(err);
    return "unknown";
}

std::string DTNAgent::interpret_mgmt_interface(std::string const &name) const
{
    utils::slog() << "[DTN Agent] Will use management_interface ("
                  << mgmt_iface_ << ") IPv4 address for "
                  << "mdtmftp+ control channel.";
    return mgmt_iface_;
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_get_gridmap_entries_command(Json::Value const &message) const
{
    Json::Value respose = json_response(0, "OK");

    try
    {
        auto result = utils::grid_mapfile_get_entries();

        respose["mapfile"] = result.path;
        respose["maps"]    = Json::Value{};

        for (auto const & m : result.system_entries)
        {
            Json::Value v;
            v["dn"] = m.first;
            v["ln"] = m.second;
            respose["maps"]["system"].append(v);
        }

        for (auto const & m : result.bde_entries)
        {
            Json::Value v;
            v["dn"] = m.first;
            v["ln"] = m.second;
            respose["maps"]["bde"].append(v);
        }
    }
    catch (std::exception const & ex)
    {
        auto error = "get_gridmap_entries error: " + std::string(ex.what());
        utils::slog() << "[DTN Agent] " << error;
        return json_response(1, error);
    }

    return respose;
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_push_gridmap_entries_command(Json::Value const &message) const
{
    try
    {
        auto newmap = message["map"];

        if (not newmap.isArray())
        {
            throw std::runtime_error("params do not contain a map array");
        }

        // std::multimap<std::string, std::string> gridmap;
        utils::gridmap_entries gridmap;

        for (auto const &m : newmap)
        {
            auto dn = m["dn"].asString();
            auto ln = m["ln"].asString();

            utils::slog() << "Read map item, dn: " << dn << ", ln: " << ln;

            gridmap.insert(std::make_pair(dn, ln));
        }

        grid_mapfile_push_entries(gridmap);
    }
    catch (std::exception const & ex)
    {
        auto error = "get_gridmap_entries error: " + std::string(ex.what());
        utils::slog() << "[DTN Agent] " << error;
        return json_response(1, error);
    }

    return json_response(0, "OK");
}

// ----------------------------------------------------------------------

Json::Value
DTNAgent::handle_get_disk_usage_command(Json::Value const &message)
{
    auto const path = message["path"].asString();
    auto const user = message["user"].asString();

    // Check if path is within a data folder.
    if (not data_folder_contains(path))
    {
        auto error = "path " + path + " is not under a data folder";
        utils::slog() << "[DTN Agent] dtn_get_disk_usage failed: " << error;
        return json_response(1, error);
    }

    Json::Value response = json_response(0, "OK");

    try
    {
        auto usage = utils::get_quota_or_fs_usage(path, user);

        Json::Value result;

        result["type"]                = usage.type;

        // Indicate whethere we're using soft or hard quota to compute
        // available space or inodes.
        if (usage.type == "quota")
        {
            result["quota-type"]      = usage.quota_limit_type;
        }

        result["block_size"]          = Json::UInt64(usage.fragment_size);

        result["blocks"]["available"] = Json::UInt64(usage.available_blocks);
        result["blocks"]["usage"]     = Json::UInt64(usage.blocks_usage);

        result["size"]["available"]   = Json::UInt64(usage.available_size);
        result["size"]["usage"]       = Json::UInt64(usage.size_usage);

        result["inodes"]["available"] = Json::UInt64(usage.available_inodes);
        result["inodes"]["usage"]     = Json::UInt64(usage.inode_usage);

        response["result"] = result;
    }
    catch (std::exception const &ex)
    {
        auto error = "error: " + std::string(ex.what());
        utils::slog() << "[DTN Agent] dtn_get_disk_usage failed: " << error;
        return json_response(1, error);
    }

    utils::slog() << "[DTN Agent] dtn_get_disk_usage response: " << response;

    return response;
}

Json::Value
DTNAgent::handle_mkdir(Json::Value const& message) const
{
    std::string msg = "OK";
    int code = 0;

    auto path = message["path"].asString();
    auto user = message["user"].asString();

    struct passwd* pwd = nullptr;

    // find user first
    if (!user.empty())
    {
        pwd = getpwnam(user.c_str()); 

        if (pwd == NULL)
        {
            code = 201;
            msg = "cannot find user (" + user + ")";
            utils::slog() << "mkdir error: " << msg;
            return json_response(code, msg);
        }
    }

    // create path
    if (mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO))
    {
        code = errno;
        msg = strerror(code);
        utils::slog() << "mkdir returns error: " << code << ", " << msg;

        if (code == EEXIST) code = 0;
        else json_response(code, msg);
    }

    // change ownership
    if (pwd)
    {
        utils::slog() << "chown " << path << " to " << pwd->pw_name;
        if(chown(path.c_str(), pwd->pw_uid, pwd->pw_gid))
        {
            code = errno;
            msg = std::string("chown error: ") + strerror(code);
            utils::slog() << "mkdir error: " << msg;
            return json_response(code, msg);
        }
    }

    return json_response(code, msg);
}



// ----------------------------------------------------------------------
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

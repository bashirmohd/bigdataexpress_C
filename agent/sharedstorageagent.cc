#include <dirent.h>

#include "sharedstorageagent.h"
#include "utils/tsdb.h"

SharedStorageAgent::SharedStorageAgent(Json::Value const & conf)
    :Agent(conf["id"].asString(), conf["name"].asString(), "SharedStorage"),
    m_conf(conf)
{
}

SharedStorageAgent::~SharedStorageAgent(){
}

void SharedStorageAgent::iozone_thread_function(){
    while(iozone_thread_active){
        get_potential_bandwidth(m_device, m_iozone_test_dir);
        sleep(m_iozone_thread_interval);
    }
}

void SharedStorageAgent::do_bootstrap()
{
    check_configuration(m_conf);
}

void SharedStorageAgent::check_configuration(Json::Value const & conf)
{
    if (conf.empty())
    {
        throw std::runtime_error("Empty Shared Storage Agent configuration.");
    }

    auto df = conf["data_folders"]["path"];
    auto ost_range = conf["data_folders"]["OSTs"];

    if (not df.empty())
    {
        if (df.isArray())
        {
            for (auto d : df)
            {
                m_data_folders_.push_back(d.asString());
            }
          
        }
        else
        {
            m_data_folders_.push_back(df.asString());
        }
    }
    else
    {
        utils::slog() << "[SharedStorageAgent] No data_folders found";
    }
#if 0
    auto data_folder_scan = conf["data_folders"]["scan"];

    scan_data_folder_ = false;
    if (data_folder_scan.isBool())
    {
        scan_data_folder_ = data_folder_scan.asBool();
    }
#endif
    m_iozone_exe = conf["iozone-executable"].asString();

    if (m_iozone_exe.empty() and (not m_data_folders_.empty()))
    {
        m_iozone_exe =
            conf["local_storage_agent"]["iozone-executable"].asString();

        if (m_iozone_exe.empty())
        {
            m_iozone_exe =
                conf["shared_storage_agent"]["iozone-executable"].asString();

            if (m_iozone_exe.empty())
            {
                throw std::runtime_error("No iozone-executable configuration found");
            }
        }

    }

    m_iozone_test_dir = get_iozone_work_folder(m_data_folders_[0]);
}


void SharedStorageAgent::do_init()
{
    if (m_iozone_test_dir.empty()) {
        slog() << "[SharedStorageAgent] Fatal error: "
               << "iozone-work-folder not specified in config file!"
               << "SharedStorageAgent not functioning...";
        active = false;
        return;
    }

    if (m_iozone_exe.empty()) {
        slog() << "[SharedStorageAgent] Fatal error: iozone executable not specified in config file! SharedStorageAgent not functioning...";
        active = false;
        return;
    }

    m_device = m_conf["device"].asString();

//    auto const & folders = m_conf["root-folders"];
    for(auto const & f : m_data_folders_)
    {
        m_root_dirs.push_back(f);
    }

    vector<string> returned_devices;
    if (m_device.empty())
    {
        if (m_root_dirs.empty()){
            throw std::invalid_argument("[SharedStorageAgent] Both device and root-folders are empty");
        }
        else{
            for(auto dir : m_root_dirs){
                utils::MountPointsHelper mph(dir, false);
                auto mounts = mph.get_mounts();
                if (not mounts.empty())
                {
                    for (auto m : mounts)
                    {
                        returned_devices.push_back(m.source());
                        break;
                    }
                }
                else
                {
                    returned_devices.push_back(mph.get_parent_mount().source());
                }
            }

            if(returned_devices.empty()){
                throw std::invalid_argument("[SharedStorageAgent] Couldn't find devices associated with root folders");
            }
            else{
                m_device = returned_devices[0];
                for(auto dev : returned_devices){
                    if(m_device != dev){
                        throw std::invalid_argument("[SharedStorageAgent] Root folders do not belong to the same device");
                    }
                    m_device=dev;
                }
            }
        }
    }

    if (m_iozone_exe == "") m_iozone_exe = "iozone";

    get_potential_bandwidth(m_device, m_iozone_test_dir);
    get_realtime_bandwidth(m_device);
    get_usage(m_device);
    do_registration();

    iozone_thread_handler = make_shared<thread>(&SharedStorageAgent::iozone_thread_function, this);
    iozone_thread_active = true;
}

void SharedStorageAgent::do_registration()
{
    Json::Value v;

    v["id"] = id();
    v["name"] = name();
    v["type"] = type();
    v["device"] = m_device;
    v["root_folders"] = m_conf["root-folders"];
    v["expired_at"] = (Json::Value::UInt64)duration_cast<seconds>(steady_clock::now().time_since_epoch()).count();
    v["queue_name"] = queue();

    set_prop("sharedstorageagent", v);

    // to add a similar method for shared storage,
    // or change the method's name.
    store().reg_local_storage(v);
}

Json::Value SharedStorageAgent::do_command(string const & cmd, Json::Value const & params)
{
    if(!active){
        slog() << "[SharedStorageAgent] Fatal error: SharedStorageAgent not functioning! Command not processed!";
        return nullptr;
    }

    slog() << "[SharedStorageAgent] Command received. cmd: " << cmd << "; params: " << params;

    Json::Value v;

    v["code"] = -100;

    if (cmd == "ls_get_rate"){
        v["code"] = get_realtime_bandwidth(m_device);
        v["rate_read"] = m_jbase[m_device]["realtime_read_bandwidth"];
        v["rate_write"] = m_jbase[m_device]["realtime_write_bandwidth"];
    }
    else if (cmd == "ls_estimate_total_rate"){
        v["code"] = 0;
        v["total_rate_read"] = m_jbase[m_device]["potential_read_bandwidth"];
        v["total_rate_write"] = m_jbase[m_device]["potential_write_bandwidth"];
    }
    return v;
}

int SharedStorageAgent::get_usage(string device){
    string cmd = "df -h | grep " + device;
    string ret = run_cmd(cmd);
    if(ret == ""){
        return -1;
    }
    vector<string> result = split_string(ret, " ");
    double size = parse_size(result[1]);
    double used = parse_size(result[2]);
    double available = parse_size(result[3]);
    double percent = parse_size(result[4]);
    string mount = result[5];
    m_jbase[device]["size"] = size;
    m_jbase[device]["used"] = used;
    m_jbase[device]["available"] = available;
    m_jbase[device]["percent"] = percent;
    m_jbase[device]["mount"] = mount;

    return 0;
}

int SharedStorageAgent::get_realtime_bandwidth(string device) {
    double scan_wbw = 0.0 , scan_rbw = 0.0;

    scan_stats(scan_wbw, scan_rbw);

    slog() << "[SharedStorageAgent] device: " << device;
    slog() << "[SharedStorageAgent] Realtime write bandwidth: "
           << scan_wbw;
    slog() << "[SharedStorageAgent] Realtime read bandwidth: "
           << scan_rbw;

    m_jbase[device]["realtime_read_bandwidth"] = scan_rbw;
    m_jbase[device]["realtime_write_bandwidth"] = scan_wbw;

    array<std::string, 2> field_names = { "potential_read_bandwidth",
                                          "potential_write_bandwidth" };

    auto const num_tags = 2;
    std::array<std::string, num_tags> tag_names = { "name", "id" };

    using SSAInfo = utils::TimeSeriesMeasurement<num_tags,
                                                 float,
                                                 float>;

    SSAInfo measure(tsdb(), "ssa", field_names, tag_names);

    slog() << "[SharedStorageAgent] " << name() << id();

    try
    {
        measure.insert(scan_rbw,
                       scan_wbw,
                       {name(), id()});
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[SharedStorageAgent] time series reporting error: " << ex.what();
    }

    return 0;
}

void SharedStorageAgent::scan_stats(double & w, double & r)
{
    struct dirent * dp;
    string path = m_data_folders_[0] + "/.ssa/";

    w = r = 0.0;

    DIR * dirp = opendir(path.c_str());
    while ((dp = readdir(dirp)) != NULL)
           if (strstr(dp->d_name, "stat-")) {
               stringstream sf;
               double readbw = 0.0, writebw = 0.0;

               sf << path << dp->d_name;

               ifstream ifs (sf.str());
               ifs >> writebw >> readbw;
               ifs.close();

               w += writebw;
               r += readbw;
           }

    closedir(dirp);
}

void SharedStorageAgent::get_potential_bandwidth(string device, string dir){
    double scan_wbw = 0.0 , scan_rbw = 0.0;

    scan_stats(scan_wbw, scan_rbw);

    slog() << "[SharedStorageAgent] total write bandwidth: "
           << scan_wbw;
    slog() << "[SharedStorageAgent] total read bandwidth: "
           << scan_rbw;

    m_jbase[device]["potential_write_bandwidth"] = scan_wbw;
    m_jbase[device]["potential_read_bandwidth"] = scan_rbw;

    array<std::string, 2> field_names = { "potential_read_bandwidth",
                                          "potential_write_bandwidth" };

    auto const num_tags = 2;
    std::array<std::string, num_tags> tag_names = { "name", "id" };

    using SSAInfo = utils::TimeSeriesMeasurement<num_tags,
                                                 float,
                                                 float>;

    SSAInfo measure(tsdb(), "ssa", field_names, tag_names);

    slog() << "[SharedStorageAgent] " << name() << id();

    try
    {
        measure.insert(scan_rbw,
                       scan_wbw,
                       {name(), id()});
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[SharedStorageAgent] time series reporting error: " << ex.what();
    }
}

vector<string> SharedStorageAgent::split_string(const string& str, const string& delimiter){
    vector<string> strings;
    string::size_type pos = 0;
    string::size_type prev = 0;
    string topush;
    while ((pos = str.find(delimiter, prev)) !=  string::npos){
        topush = str.substr(prev, pos - prev);
        if(!is_empty(topush)){
            strings.push_back(topush);
        }
        prev = pos + 1;
    }
    topush = str.substr(prev);
    if(!is_empty(topush)){
        strings.push_back(topush);
    }
    return strings;
}

string SharedStorageAgent::run_cmd(string cmd){
    FILE * pipe = NULL;
    char buffer[2000];
    string result;
    pipe = popen(cmd.c_str(), "r");
    int len = 0;
    if (NULL == pipe) {
        perror("pipe");
        return "";
    }

    while (!feof(pipe)) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL)
            result = buffer;
    }
    pclose(pipe);
    return result;
}

#if 1
string SharedStorageAgent::get_iozone_work_folder(std::string const &path)
{
    char buffer[BUFSIZ];

    snprintf(buffer, BUFSIZ, "%s/bde-iozone-work-XXXXXX", path.c_str());

    if (mkdtemp(buffer) == nullptr)
    {
        auto err = "Can't create a work folder for iozone at path "
            + path + ", error: " + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    slog() << "[SharedStorageAgent] Created work folder for "
                  << path << " at " << buffer;

    return string(buffer);
}
#endif

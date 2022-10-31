#include <sys/statvfs.h>

#include "localstorageagent.h"

LocalStorageAgent::LocalStorageAgent(Json::Value const & conf)
    :Agent(conf["id"].asString(), conf["name"].asString(), "LocalStorage"),
    m_conf(conf)
{
}

LocalStorageAgent::~LocalStorageAgent(){
}

void LocalStorageAgent::iozone_thread_function(){
    while(iozone_thread_active){
        get_potential_bandwidth(m_device, m_iozone_test_dir);
        sleep(m_iozone_thread_interval);
    }
}

void LocalStorageAgent::do_init()
{
    m_iozone_exe      = m_conf["iozone-executable"].asString();
    m_iozone_test_dir = m_conf["iozone-work-folder"].asString();

    if (m_iozone_test_dir.empty())
    {
        slog() << "[LocalStorageAgent] Warning: "
               << "missing iozone-work-folder in configuration. "
               << "Disabling LocalStorageAgent.";
        active = false;
        return;
    }

    if (m_iozone_exe.empty())
    {
        slog() << "[LocalStorageAgent] Warning: "
               << "missing iozone executable in configuration. "
               << "Disabling LocalStorageAgent.";
        active = false;
        return;
    }

    m_device = m_conf["device"].asString();

    auto const & folders = m_conf["root-folders"];
    for(auto const & f : folders)
    {
        m_root_dirs.push_back(f.asString());
    }

    vector<string> returned_devices;
    if (m_device.empty())
    {
        if (m_root_dirs.empty()){
            throw std::invalid_argument("[LocalStorageAgent] Both device and root-folders are empty");
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
                throw std::invalid_argument("[LocalStorageAgent] Couldn't find devices associated with root folders");
            }
            else{
                m_device = returned_devices[0];
                for(auto dev : returned_devices){
                    if(m_device != dev){
                        throw std::invalid_argument("[LocalStorageAgent] Root folders do not belong to the same device");
                    }
                    m_device=dev;
                }
            }
        }
    }

    utils::slog() << "[LocalStorageAgent] Will try to report disk I/O stats for "
                  << m_device << " to time series database.";
    auto interval = std::chrono::seconds(5);
    manager().add_event([this, interval](){
            report_disk_io_stats();
            report_disk_usage_stats();
            return interval;
        }, interval);

    if(m_iozone_exe == "") m_iozone_exe = "iozone";

    if(get_realtime_bandwidth(m_device) < 0) {
      utils::slog() << "[LocalStorageAgent] Cannot report disk I/O stats for "
                    << m_device << " to time series database.";
      return;
    }
    get_usage(m_device);

    iozone_thread_handler = make_shared<thread>(&LocalStorageAgent::iozone_thread_function, this);
    iozone_thread_active = true;
}

void LocalStorageAgent::report_disk_io_stats() const
{
    static bool errored = false;

    // Just give up if calling this function resulted in error
    // previously.  We don't want to fill the log files with the same
    // error messages.
    if (errored)
    {
        return;
    }

    // One place that we can query for disk I/O rates is sysfs: See
    // https://www.kernel.org/doc/Documentation/block/stat.txt for
    // documentation.  Another place is /proc/diskstats.
    char sys_stat_file[PATH_MAX]{};
    char *dev = const_cast<char *>(m_device.c_str()) + strlen("/dev/");
    snprintf(sys_stat_file, PATH_MAX, "/sys/block/%s/stat", dev);

    // TODO: replace fopen() etc with C++ stream I/O.

    FILE *fp = fopen(sys_stat_file, "r");
    if (!fp) {
        errored = true;
        utils::slog() << "[LocalStorageAgent] Error opening " << sys_stat_file
                      << ", reason: " << strerror(errno) << ".";
        return;
    }

    size_t read_ios;
    size_t read_merges;
    size_t read_sectors;
    size_t read_ticks;

    size_t write_ios;
    size_t write_merges;
    size_t write_sectors;
    size_t write_ticks;

    size_t in_flight;
    size_t io_ticks;
    size_t time_in_queue;

    fscanf(fp, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
           &read_ios, &read_merges, &read_sectors, &read_ticks,
           &write_ios, &write_merges, &write_sectors, &write_ticks,
           &in_flight, &io_ticks, &time_in_queue);

    if (ferror(fp))
    {
        utils::slog() << "[LocalStorageAgent] Reading" << sys_stat_file
                      << " errored, reason: " << strerror(errno);
        fclose(fp);
        errored = true;
        return;
    }

    fclose(fp);

    // utils::slog() <<  "[LocalStorageAgent] " << dev << " stats: {"
    //               <<  "read_ios: "      << read_ios      << ", "
    //               <<  "read_merges: "   << read_merges   << ", "
    //               <<  "read_sectors: "  << read_sectors  << ", "
    //               <<  "read_ticks: "    << read_ticks    << ", "
    //               <<  "write_ios: "     << write_ios     << ", "
    //               <<  "write_merges: "  << write_merges  << ", "
    //               <<  "write_sectors: " << write_sectors << ", "
    //               <<  "write_ticks: "   << write_ticks   << ", "
    //               <<  "in_flight: "     << in_flight     << ", "
    //               <<  "io_ticks: "      << io_ticks      << ", "
    //               <<  "time_in_queue: " << time_in_queue << "}";

    auto const num_tags = 3;
    using DiskInfo = utils::TimeSeriesMeasurement<num_tags,
                                                  size_t,
                                                  size_t>;

    DiskInfo measure(tsdb(), "disk_io", {"read_ios", "write_ios"},
                     {"name", "id", "device"});

    try
    {
        measure.insert(read_ios, write_ios, {name(), id(), m_device});
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[LocalStorageAgent] Time series reporting error: "
                      << ex.what();
    }
}

void LocalStorageAgent::report_disk_usage_stats() const
{
    static bool errored = false;

    // report block size, size of fs (in blocks), free blocks,
    // total inodes, available inodes, and {name, id, folder}
    // tags.

    auto const num_tags = 3;
    using DiskUsageInfo = utils::TimeSeriesMeasurement<num_tags,
                                                       size_t,
                                                       size_t,
                                                       size_t,
                                                       size_t,
                                                       size_t>;

    DiskUsageInfo usage(tsdb(), "disk_usage",
                        {"block-size", "total-blocks", "available-blocks",
                                "total-inodes", "available-inodes" },
                        {"name", "id", "folder"});

    for (auto const &folder : m_root_dirs)
    {
        struct statvfs stat{0};

        if (statvfs(folder.c_str(), &stat) < 0)
        {
            utils::slog() << "[LocalStorageAgent] Error from statvfs(): "
                          << strerror(errno);
            continue;
        }

        try
        {
            usage.insert(stat.f_bsize,
                         stat.f_blocks, stat.f_bavail,
                         stat.f_files, stat.f_favail,
                         {name(), id(), folder});
        }
        catch (std::exception const &ex)
        {
            utils::slog() << "[LocalStorageAgent] Time series reporting error: "
                          << ex.what();
        }
    }
}

void LocalStorageAgent::do_registration()
{
    Json::Value v;

    v["id"] = id();
    v["name"] = name();
    v["type"] = type();
    v["queue_name"] = queue();
    v["expired_at"] = (Json::Value::UInt64)duration_cast<seconds>(steady_clock::now().time_since_epoch()).count();

    v["device"] = m_device;
    v["root_folders"] = m_conf["root-folders"];
    v["io_capacity_write"] = m_jbase[m_device]["potential_write_bandwidth"];
    v["io_capacity_read"] = m_jbase[m_device]["potential_read_bandwidth"];

    store().reg_local_storage(v);

    set_prop("device", m_device);
    set_prop("root_folders", m_conf["root-folders"]);
    set_prop("io_capacity_write", m_jbase[m_device]["potential_write_bandwidth"]);
    set_prop("io_capacity_read", m_jbase[m_device]["potential_read_bandwidth"]);
}

Json::Value LocalStorageAgent::do_command(string const & cmd, Json::Value const & params)
{
    slog() << "[LocalStorageAgent] Command received. cmd: "
           << cmd << "; params: " << params;


    Json::Value v;

    v["code"] = -100;

    if (!active)
    {
        if (cmd == "ls_get_rate" or cmd == "ls_estimate_total_rate")
        {
            v["message"] = "Local Storage Agent is inactive";
        }

        slog() << "[LocalStorageAgent] Warning: agent is inactive. "
               << "Sending error response to " << cmd
               << ", response: " << v;

        return v;
    }

    if (cmd == "ls_get_rate")
    {
        v["code"]       = get_realtime_bandwidth(m_device);
        v["rate_read"]  = m_jbase[m_device]["realtime_read_bandwidth"];
        v["rate_write"] = m_jbase[m_device]["realtime_write_bandwidth"];
    }
    else if (cmd == "ls_estimate_total_rate")
    {
        v["code"]             = 0;
        v["total_rate_read"]  = m_jbase[m_device]["potential_read_bandwidth"];
        v["total_rate_write"] = m_jbase[m_device]["potential_write_bandwidth"];
    }

    return v;
}

int LocalStorageAgent::get_usage(string device){
    string cmd = "df -h " + device;
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

int LocalStorageAgent::get_realtime_bandwidth(string device){
    vector<string> raw_dev = split_string(device, "/");

    // NFS has its own command. Therefore we check
    // the file system type, and determine the command
    MountPointsHelper mph(m_root_dirs[0], false);
    auto mounts = mph.get_mounts();

    // what if nothing is mounted under this path?
    if (mounts.empty())
    {
        auto mnt = mph.get_parent_mount();
        mounts.insert(mnt);
    }

    set<MountPoint>::iterator it = mounts.begin();
    string ft = it->type(), cmd;

    if (string::npos != ft.find("nfs")) {
        cmd = "nfsiostat |grep -A3 read|grep -v read|grep -v write|awk '{print $2}'|awk '{ORS=(NR%2?FS:RS)}1'";
    }
    else {
        cmd = "iostat " + device +" | grep -A1 Device | grep -v Device |awk {'print $3, $4'}";
    }

    string ret = run_cmd(cmd);
    if(ret == ""){
        return -1;
    }
    vector<string> result = split_string(ret, " ");

    if(result.size() <= 0)
	return -1;

    double readbw, writebw;

    try{
         readbw = stod(result[0]) / 1000.0;
         writebw = stod(result[1]) / 1000.0;
    } catch( ...  )
    {
        slog() << "[LocalStorageAgent] iostat returns strings in wrong format!";
    }
    m_jbase[device]["realtime_read_bandwidth"] = readbw;
    m_jbase[device]["realtime_write_bandwidth"] = writebw;
    return 0;
}

int LocalStorageAgent::get_potential_bandwidth(string device, string dir){
    stringstream cmd("mkdir -p " + dir);
    run_cmd(cmd.str());
    cmd.str("");

    std::vector<string> test_files(m_iozone_max_threads);

    double writebw = 0.0, readbw = 0.0;

    for(int i=0; i<m_iozone_max_threads; i++){

        std::stringstream filename;
        filename << dir << "/testiozone" << i;
        test_files[i] = filename.str();

        // iozone write tests
        cmd << m_iozone_exe << " -i0 -w -I -r 4M -s 128M -t ";
        cmd << i+1;
        cmd <<" -F ";
        for(auto j : test_files){
            cmd << j << " ";
        }
        cmd << " 2>&1 | grep 'Children see throughput for'";
        string ret = run_cmd(cmd.str());
        cmd.str("");
        vector<string> result = split_string(ret, " ");


        if(result.size() < 8){
            slog() << "[LocalStorageAgent] Fatal error: iozone did not return results! Please check if iozone executable is configured correctly!";
            return -1;
        }
        try{
            double bw = stod(result[7]) / 1000.0;
            if(bw > writebw){
                writebw = bw;
            }
        }
        catch(...)
        {
            slog() << "[LocalStorageAgent] iozone returned strings in wrong format!";
        }

        // iozone read tests
        cmd << m_iozone_exe << " -i1 -w -I -r 4M -s 128M -t ";
        cmd << i+1;
        cmd <<" -F ";
        for(auto j : test_files){
            cmd << j << " ";
        }
        cmd << " 2>&1 | grep 'Children see throughput for'";
        ret = run_cmd(cmd.str());
        cmd.str("");
        result = split_string(ret, " ");
        if(result.size() < 8){
            slog() << "[LocalStorageAgent] Fatal error: iozone did not return results! Please check if iozone executable is configured correctly!";
            return -1;
        }
        try{
            double bw = stod(result[7]) / 1000.0;
            if(bw > writebw){
                readbw = bw;
            }
        }
        catch(...)
        {
            slog() << "[LocalStorageAgent] iozone returned strings in wrong format!";
        }
    }


    m_jbase[device]["potential_write_bandwidth"] = writebw;
    m_jbase[device]["potential_read_bandwidth"] = readbw;

    cmd << "rm -f ";
    for(auto j : test_files){
        cmd << j << " ";
    }
    run_cmd(cmd.str());
    cmd.str("");

    return 0;
}

vector<string> LocalStorageAgent::split_string(const string& str, const string& delimiter){
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

string LocalStorageAgent::run_cmd(string cmd){
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


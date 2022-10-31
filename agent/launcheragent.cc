#include "launcheragent.h"
#include "utils/process.h"

// ----------------------------------------------------------------------

static const int expiry_interval_ = 2 * 60 * 1000;
static const bool save_messages_  = true;

// ----------------------------------------------------------------------

LauncherAgent::LauncherAgent(Json::Value const &conf)
    : Agent(conf["id"].asString(), conf["name"].asString(), "Launcher")
    , launcher_conf_(conf)
    , mdtm_report_listener_(nullptr)
{
}

LauncherAgent::~LauncherAgent()
{
    mdtm_report_listener_->stop();
}

// ----------------------------------------------------------------------

void LauncherAgent::do_bootstrap()
{
    std::string new_id   = "launcher-agent";
    std::string new_name = "Launching Agent";

    char hostname[HOST_NAME_MAX] = {0};

    if (gethostname(hostname, HOST_NAME_MAX) == 0)
    {
        // new_id   += std::string("@") + std::string(hostname);
        new_name += std::string(" on ") + std::string(hostname);
    }
    else
    {
        utils::slog() << "[DTN Agent] gethostname() failed; reason: "
                      << strerror(errno);
    }

    if (new_id != id())
        update_id(new_id);

    if (new_name != name())
        update_name(new_name);
}

void LauncherAgent::do_init()
{
    auto conf = launcher_conf_["subprocess"];

    if (conf.empty())
    {
        throw std::runtime_error("No subprocess configuration");
    }

    program_path_ = get_program_path(conf);
    program_args_ = get_program_args(conf);
    program_env_  = get_program_env(conf);

    auto program_type = get_program_type(conf);

    if (program_type == "mdtm-ftp-client" or program_type.empty())
    {
        if (conf["ipv6"].asBool())
        {
            utils::slog() << "[LauncherAgent] mdtmftp+ client will use IPV6";
            program_args_.push_back("-ipv6");
        }

        set_mdtm_queue_names(conf);
        run_mdtm_ftp_client_report_listener();
        run_subprocess();
    }
    else
    {
        run_subprocess();
    }
}

void LauncherAgent::do_registration()
{
    register_launcher();

    // Refresh registrations periodically.
    auto delay = std::chrono::milliseconds(expiry_interval_);
    manager().add_event([this, delay](){
            register_launcher();
            return delay;
        }, delay);
}

void LauncherAgent::register_launcher()
{
    Json::Value val;

    val["id"]   = id();
    val["name"] = name();
    val["type"] = type();
    val["queue_name"] = queue();

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto json_now = static_cast<Json::Value::UInt64>(now);

    val["expire_at"]  = json_now + expiry_interval_;

    Json::Value queues;
    queues["bde_server_queue"]  = bde_server_queue_;
    queues["mdtm_listen_queue"] = mdtm_client_listen_queue_;
    queues["mdtm_report_queue"] = mdtm_client_report_queue_;

    val["queues"] = queues;

    // utils::slog() << "[Launcher] Registering " << val;

    set_prop("launcher", val);

    store().register_launcher_agent(id(), val);
}

// ----------------------------------------------------------------------

Json::Value LauncherAgent::do_command(std::string const &cmd,
                                      Json::Value const &params)
{
    // Forward messages to mdtm-ftp-client.
    utils::slog() << "[Launcher -> client] Received command: " << cmd
                  << " params: " << params
                  << "; forwarding to "
                  << mdtm_client_listen_queue_ << "\n";

    Json::Value newcmd{};
    newcmd["cmd"]    = cmd;
    newcmd["params"] = params;

    try
    {
        auto resp = manager().send_message(mdtm_client_listen_queue_,
                                           newcmd);

        utils::slog() << "[client -> Launcher] Received response " << resp
                      << " to command " << cmd << "; forwarding to "
                      << bde_server_queue_ << "\n";

        return resp;
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[Launcher -> client] Exception caught: "
                      << ex.what();

        if (save_messages_)
        {
            save_message("dup-request", cmd);
        }
    }

    // Pacify the compiler.
    return {};
}

// ----------------------------------------------------------------------

std::string
LauncherAgent::get_program_type(Json::Value const &conf) const
{
    return conf["type"].asString();
}

std::string
LauncherAgent::get_program_path(Json::Value const &conf) const
{
    return conf["path"].asString();
}

std::vector<std::string>
LauncherAgent::get_program_args(Json::Value const &conf) const
{
    auto const conf_args = conf["args"];
    std::vector<std::string> args{};

    if (not conf_args.empty())
    {
        if (conf_args.isArray())
        {
            for (auto const &arg : conf_args)
            {
                args.push_back(arg.asString());
            }
        }
        else
        {
            args.push_back(conf_args.asString());
        }
    }

    return args;
}

std::map<std::string, std::string>
LauncherAgent::get_program_env(Json::Value const &conf) const
{
    auto const conf_env = conf["env"];
    std::map<std::string, std::string> env{};

    if (not conf_env.empty())
    {
        if (conf_env.isArray())
        {
            for (auto const &item : conf_env)
            {
                auto names = item.getMemberNames();
                if (names.size() > 0)
                {
                    auto name = names.at(0);
                    auto val  = item[name].asString();
                    env.emplace(name, val);
                }
            }
        }
        else
        {
            auto names = conf_env.getMemberNames();
            if (names.size() > 0)
            {
                auto name = names.at(0);
                auto val  = conf_env[name].asString();
                env.emplace(name, val);
            }
        }
    }

    return env;
}

// ----------------------------------------------------------------------

void
LauncherAgent::set_mdtm_queue_names(Json::Value const &conf)
{
    auto const &queues = conf["queues"];

    bde_server_queue_         = queues["bde_server_queue"].asString();
    mdtm_client_listen_queue_ = queues["mdtm_listen_queue"].asString();
    mdtm_client_report_queue_ = queues["mdtm_report_queue"].asString();

    // Use some defaults, in case configuration is empty.
    if (bde_server_queue_.empty())
        bde_server_queue_ = "bdeserver";

    auto pid = getpid();

    if (mdtm_client_listen_queue_.empty())
        mdtm_client_listen_queue_ = "launcher-listen-" + std::to_string(pid);

    if (mdtm_client_report_queue_.empty())
        mdtm_client_report_queue_ = "launcher-report-" + std::to_string(pid);

    auto mqconf = manager().get_mq_conf();
    auto host   = mqconf["host"].asString();
    auto port   = mqconf["port"].asInt();

    if (host.empty())
    {
        utils::slog() << "[Launcher] Empty MQTT host in config; "
                      << "using localhost";
        host = "localhost";
    }

    if (port == 0)
    {
        utils::slog() << "[Launcher] Empty/invalid MQTT port in config; "
                      << "using 1883";
        port = 1883;
    }

    auto encmode = mqconf["encryption"].asString();
    std::string cert;
    std::string psk;
    std::string psk_id;

    //
    // 1. No security
    // mdtm-ftp-client -vb -p 8 -msgq host:port:listen:report
    //
    // 2. TLS-PSK:
    // mdtm-ftp-client -vb -p 8 -msgq host:port:listen:report:1:psk:psk_id
    //
    // 3. TLS-RSA
    // mdtm-ftp-client -vb -p 8 -msgq host:port:listen:report:2:cert-path
    //

    std::stringstream mq_args;

    mq_args << host << ":" << port << ":"
            << mdtm_client_listen_queue_ << ":"
            << mdtm_client_report_queue_;

    if (encmode == "none")
    {
        utils::slog() << "[Launcher] MQTT: encryption mode is \"none\".";
    }
    else if (encmode == "psk")
    {
        psk    = mqconf["psk"].asString();
        psk_id = mqconf["psk_id"].asString();

        utils::slog() << "[Launcher] MQTT: using PSK ID: " << psk_id << ".";

        mq_args << ":" << 1 << ":" << psk << ":" << psk_id;
    }
    else if (encmode == "certificate")
    {
        cert = mqconf["cacert"].asString();

        utils::slog() << "[Launcher] MQTT: using CA certificate at "
                      << cert << ".";

        mq_args << ":" << 2 << ":" << cert;
    }
    else
    {
        utils::slog() << "[Launcher] MQTT encryption mode unknown; "
                      << "assumping none";
    }

    program_args_.push_back("-msgq");
    program_args_.push_back(mq_args.str());
}

// ----------------------------------------------------------------------

void
LauncherAgent::run_subprocess()
{
    if (!program_path_.empty())
    {
        utils::slog() << "[Launcher] Running " << program_path_;

        prog_thread_ = std::thread([this]() {
                utils::supervise_process(program_path_,
                                         program_args_,
                                         program_env_);
            });
    }
}

// ----------------------------------------------------------------------

void
LauncherAgent::run_mdtm_ftp_client_report_listener()
{
    auto const &mqconf = manager().get_mq_conf();
    auto const &host   = mqconf["host"].asString();
    auto const  port   = mqconf["port"].asInt();

    auto handler = std::bind(&LauncherAgent::handle_mdtm_report, this,
                             std::placeholders::_1,
                             std::placeholders::_2);

    mdtm_report_listener_.reset(new RpcServer(host, port,
                                              mdtm_client_report_queue_,
                                              handler));

    auto const encmode = mqconf["encryption"].asString();

    if (encmode == "psk")
    {
        auto const psk    = mqconf["psk"].asString();
        auto const psk_id = mqconf["psk_id"].asString();

        mdtm_report_listener_->set_tls_psk(psk_id, psk);

        utils::slog() << "[Launcher/ReportListener] Using PSK (ID:" << psk_id
                      << ") for " << mdtm_report_listener_->queue() << ".";
    }
    else if (encmode == "certificate")
    {
        auto const cert = mqconf["cacert"].asString();

        mdtm_report_listener_->set_tls_ca(cert);

        utils::slog() << "[Launcher/ReportListener] "
                      << "Using CA certificate at "
                      << cert << " for "
                      << mdtm_report_listener_->queue() << ".";
    }

    mdtm_report_listener_->start();

    utils::slog() << "[Launcher] Listening on "
                  << mdtm_report_listener_->queue();
}

// ----------------------------------------------------------------------

// Capture messages from mdtm-ftp-client's report queue, and forward
// them to bdeserver's queue.
void
LauncherAgent::handle_mdtm_report(Json::Value const &msg,
                                  RpcProps const &props)
{
    std::string taskid = "unknown";

    if (not msg["params"]["task"].empty())
        taskid = msg["params"]["task"].asString();
    else if (not msg["body"]["params"]["task"].empty())
        taskid = msg["body"]["params"]["task"].asString();

    std::string topic = "ld/status/" + taskid;

    utils::slog() << "[Launcher] Received a message on \""
                  << mdtm_client_report_queue_ << "\"; "
                  << "re-publishing to topic \"" << topic << "\" "
                  << "(message: " << msg << ")";

    try
    {
        mqtt_publish(topic, msg, 1, true);
    }
    catch (std::exception const &ex)
    {
        utils::slog() << "[Launcher -> server report] Exception caught: "
                      << ex.what();

        if (save_messages_)
        {
            save_message("dup-report", msg);
        }
    }
}

// ----------------------------------------------------------------------

void
LauncherAgent::save_message(std::string const &name,
                            Json::Value const &msg) const
{
    // check if directory "/tmp/$(uid)-bde-launcher-agent-$(pid)" exists;
    // make if it doesn't.

    auto uid = getuid();
    auto pid = getpid();

    auto dir = "/tmp/" + std::to_string(uid) +
        "-bde-launcher-agent-" + std::to_string(pid);

    if (mkdir(dir.c_str(), ACCESSPERMS) != 0 and errno != EEXIST)
    {
        utils::slog() << "[Launcher DEBUG] "
                      << "Error making directory \"" << dir << "\" :"
                      << std::strerror(errno) << "\n";
        return;
    }

    // Add timestamp to file names so that we can sort through them in
    // an easier manner.
    char timestamp[32]{0};
    time_t now = time(nullptr);
    strftime(timestamp, 32, "%Y%m%d-%H%M%S", localtime(&now));

    // Append a random string to filename to make it unique, because,
    // what if are logging more than one message in a second's time?
    std::srand(now);
    auto randstr = std::to_string(std::rand());

    auto path = dir + "/" + timestamp + "-" + randstr + "-" + name + ".json";

    utils::slog() << "[Launcher DEBUG] Writing message to " << path;

    std::ofstream stream(path);

    Json::StyledWriter styledWriter;
    stream << styledWriter.write(msg);

    stream.close();
}

// ----------------------------------------------------------------------

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

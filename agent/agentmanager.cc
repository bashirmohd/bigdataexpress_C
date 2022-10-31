
#include "agentmanager.h"
#include "utils/network.h"

#include <chrono>

using namespace std;
using namespace utils;

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

AgentManager::AgentManager(Conf const & conf, size_t extra_threads)
    : mid(utils::get_first_mac_address())
    , cid(utils::guid())
    , io()
    , strands()
    , sig(io, SIGINT, SIGTERM, SIGHUP)
    , mstore()
    , ss(conf)
    , ts(conf)
    , work_threads(extra_threads)

    , mq_host(conf.get<std::string>("agent.mq_server.host"))
    , mq_port(conf.get<int>("agent.mq_server.port"))
    , mq_conf(Json::objectValue)

    , rpcclient(mq_host, mq_port)
    , rpcserver(
            mq_host, 
            mq_port, 
            cid, 
            std::bind(&AgentManager::rpc_msg_handler, this, _1, _2) )

    , mqtt( mq_host,
            mq_port,
            "agent-" + cid,
            bind(&AgentManager::on_mqtt_msg, this, _1, _2),
            bind(&AgentManager::on_mqtt_conn, this, _1, _2) )

    , config(conf)
    , reg_val()
    , de_reg_val()
{
    // message queue server
    mq_conf["host"] = mq_host;
    mq_conf["port"] = mq_port;

    // ca, psk, psk_id from conf
    std::string ca     = config.get<string>("agent.mq_server.cacert", "");
    std::string psk_id = config.get<string>("agent.mq_server.psk_id", "");
    std::string psk    = config.get<string>("agent.mq_server.psk", "");

    bool use_ca  = !ca.empty();
    bool use_psk = !psk.empty() || !psk_id.empty();

    // cant have both
    if (use_ca && use_psk)
    {
        throw std::runtime_error("cant set both cafile and psk in the configuration.");
    }

    if (use_ca)
    {
        mq_conf["encryption"] = "certificate";
        mq_conf["cacert"] = ca;
    }
    else if (use_psk)
    {
        mq_conf["encryption"] = "psk";
        mq_conf["psk_id"] = psk_id;
        mq_conf["psk"] = psk;
    }
    else
    {
        mq_conf["encryption"] = "none";
    }

    // agent module storage
    mstore.reserve(100);

    // pre-defined reg objects
    reg_val["online"]  = true;
    reg_val["queue"]   = rpcserver.queue();
    reg_val["modules"] = Json::arrayValue;

    de_reg_val["online"]  = false;
    de_reg_val["queue"]   = rpcserver.queue();
    de_reg_val["modules"] = Json::arrayValue;

    // signal handler
    sig.async_wait([this](asio::error_code const & ec, int signal) {
        utils::slog() << "[AgentManager] Signal " << signal << " captured.";
        io.stop();
    });
}

void AgentManager::prepare()
{
    // module bootstrap
    for (auto const & m : mstore)
    {
        utils::slog() << "[AgentManager] Bootstrapping " << m->type() << " module.";
        m->bootstrap();
    }

    utils::slog() << "[AgentManager] bootstrapping done.";

    // encrytion method
    auto encryption = mq_conf["encryption"].asString();

    // cert
    if (encryption == "certificate")
    {
        auto ca = mq_conf["cacert"].asString();

        rpcclient.set_tls_ca(ca);
        rpcserver.set_tls_ca(ca);
        mqtt.set_tls_ca(ca);

        slog() << "[AgentManager] using CA from " << ca  << " for encrypted MQTT broker.";
    }
    // psk
    else if (encryption == "psk")
    {
        auto psk_id = mq_conf["psk_id"].asString();
        auto psk = mq_conf["psk"].asString();

        rpcclient.set_tls_psk(psk_id, psk);
        rpcserver.set_tls_psk(psk_id, psk);
        mqtt.set_tls_psk(psk_id, psk);

        slog() << "[AgentManager] using PSK with id " << psk_id  << " for encrypted MQTT broker.";
    }
    // none
    else
    {
        slog() << "[AgentManager] using unencrpyted MQTT broker.";
    }

    // rpc server
    rpcserver.start();
    utils::slog() << "[AgentManager] initialized RPC server";

    // rpc client
    rpcclient.start();
    utils::slog() << "[AgentManager] initialized RPC client";

    // module init
    for (auto const & m : mstore)
    {
        utils::slog() << "[AgentManager] agent module " << m->id()<< "  init...";
        m->init();

        utils::slog() << "[AgentManager] agent module " << m->id()<< "  registration...";
        m->registration();
    }

    // prepare for the agent (de)registration object
    for (auto const & m : mstore)
    {
        reg_val["modules"].append(m->get_prop());
        de_reg_val["modules"].append(m->get_prop());
    }

    // mqtt last will and start service
    mqtt.set_will("reg/" + cid, de_reg_val, 1 /* qos */, true /* retain */);
    mqtt.start();

    // online registration
    // mqtt.publish("reg/" + cid, reg_val, 1, true);
}

void AgentManager::run()
{
    // start io in thread pool
    for(int i=0; i<work_threads.size(); ++i)
        work_threads[i] = std::thread([this](){ io.run(); });

    // start event loop
    io.run();

    // thread join
    for (auto & t : work_threads)
        t.join();

    // at this point the main event loop is finished
    // more cleaning up work followed here
#if 0
    for (auto const &m : mstore) 
    {
        // TODO: Send shutdown message to the server from the Agent.
        Json::Value message;
        message["type"] = "agent_shutdown";
        message["id"]   = m->id();
        send_message("bdeserver", message);
    }
#endif

    // stop MQTT service
    mqtt.publish("reg/" + cid, de_reg_val, 1, true);
    mqtt.stop();

    // stop the RPC server/client
    rpcclient.stop();
    rpcserver.stop();
}

void AgentManager::add_event(
        std::function<std::chrono::milliseconds()> const & handler,
        std::chrono::milliseconds const & delay,
        std::string const & tag )
{
    // locatioin of the new event handler
    int n = evs.size();

    // save the event handler
    evs.emplace_back( std::make_pair(
        asio::steady_timer(io, delay),
        [this, n, handler, tag](asio::error_code const & ec) {
            auto intv = handler();
            auto & ev = evs[n];
            ev.first.expires_from_now(intv);

            if (tag.empty())
            {
                ev.first.async_wait(ev.second);
            }
            else
            {
                auto iter = strands.find(tag);
                if (iter != strands.end()) ev.first.async_wait(iter->second.wrap(ev.second));
            }
        }
    ));

    if (tag.empty())
    {
        // start wait without protection
        evs[n].first.async_wait(evs[n].second);
    }
    else
    {
        // insert the tag
        auto res = strands.emplace(tag, asio::io_service::strand(io));

        // start wait
        evs[n].first.async_wait(res.first->second.wrap(evs[n].second));
    }
}

void AgentManager::dispatch(std::function<void()> const & handler)
{
    io.post(handler);
}

Json::Value AgentManager::send_message(std::string const &queue,
                                       Json::Value const &message)
{
    return rpcclient.call(queue, message);
}

void AgentManager::rpc_msg_handler(Json::Value const & msg, RpcProps const & props)
{
    std::string tgt = msg["target"].isNull() ? "" : msg["target"].asString();

    if (tgt.empty() or tgt == "daemon")
    {
        // receiver is this agent daemon
        auto status = std::string("Ignoring message; ") +
            (tgt.empty() ? "no target." : "daemon target.");

        slog(s_warning) << status;

        Json::Value res;
        res["code"]    = 1;
        res["message"] = status;

        rpcserver.send_response(res, props);
    }
    else
    {
        try
        {
            auto & agent = get<Agent>(tgt);

            std::string cmd = msg["cmd"].asString();
            Json::Value arg = msg["params"];

            slog() << "[AgentManager] dispatching command to "
                   << tgt << ": " << msg;

            dispatch([this, &agent, cmd, arg, props]() {
                auto start = chrono::steady_clock::now();

                Json::Value res = agent.command(cmd, arg);
                rpcserver.send_response(res, props);

                slog() << "[AgentManager] command processed. cmd: "
                       << cmd << ", params: " << arg;

                auto dur = chrono::steady_clock::now() - start;
                report_timing(agent.id(), agent.name(), cmd, dur);
            });
        }
        catch (std::exception const & ex)
        {
            auto status = "Error: " + std::string(ex.what());

            slog(s_warning) << status;

            Json::Value res;
            res["code"]    = 1;
            res["message"] = status;

            rpcserver.send_response(res, props);
        }
        catch (...)
        {
            auto status = "Exception caught; unsure what kind";

            slog(s_warning) << status;

            Json::Value res;
            res["code"]    = 1;
            res["message"] = status;

            rpcserver.send_response(res, props);
        }
    }
}

void AgentManager::on_mqtt_msg(std::string const & topic, Json::Value const & msg)
{
    // log
    slog() << "[AgentManager] mqtt message received on topic " << topic;

    // find every subscribing module to this topic
    for (auto & entry : topics)
    {
        if (Mqtt::topic_match(entry.first, topic))
        {
            // dispatch, so the listening thread dont get blocked
            dispatch([entry, topic, msg](){
                entry.second->on_message(topic, msg);
            });

            // log
            slog() << "[AgentManager] message handler dispatched on module " 
                   << entry.second->name();
        }
    }
}

void AgentManager::on_mqtt_conn(bool conn, int rc)
{
    if (conn)
    {
        slog() << "[AgentManager] connected to mqtt server";
        mqtt.publish("reg/" + cid, reg_val, 1, true);
    }
    else
    {
        slog() << "[AgentManager] lost connection with mqtt server";
    }
}

bool AgentManager::mqtt_subscribe(Agent & module, std::string const & topic, int qos)
{
    if (!mqtt.connected())
    {
        slog() << "[AgentManager] agent module " << module.name() 
               << " trying to subscribe while the mqtt is offline.";

        return false;
    }

    // log
    slog() << "[AgentManager] agent module " << module.name() << " subscribed to " << topic;

    // actual mqtt subscribe
    mqtt.subscribe(topic, qos);

    // internal subscription, so when the message comes, it can be
    // routed to the specific module
    topics.emplace_back(topic, &module);

    return true;
}

bool AgentManager::mqtt_publish(Agent & module, std::string const & topic, Json::Value const & msg, int qos, bool retain)
{
    // log
    slog() << "[AgentManager] agent module " << module.name() << " published a message @ " << topic;

    // forward the mqtt publish
    // we have no use of the module for now, but might need it later,
    // e.g., logging who's published this message
    auto rc = mqtt.publish(topic, msg, qos, retain);

    if (rc != MOSQ_ERR_SUCCESS)
    {
        auto prefix = std::string("[MQTT publish (topic: ") + topic + ")] Error: ";

        switch (rc)
        {
        case MOSQ_ERR_INVAL:
            slog() << prefix << "input parameters were invalid.";
            break;
        case MOSQ_ERR_NOMEM:
            slog() << prefix << "an out of memory condition occurred.";
            break;
        case MOSQ_ERR_NO_CONN:
            slog() << prefix << "client isn't connected to a broker.";
            break;
        case MOSQ_ERR_PROTOCOL:
            slog() << prefix << "protocol error communicating with the broker.";
            break;
        case MOSQ_ERR_PAYLOAD_SIZE:
            slog() << prefix << "payload is too large.";
            break;
        case MOSQ_ERR_MALFORMED_UTF8:
            slog() << prefix << "the topic is not valid UTF-8.";
            break;
        default:
            slog() << prefix << "unknown error.";
            break;
        }
    }

    return (rc == MOSQ_ERR_SUCCESS);
}

void AgentManager::report_timing(std::string const & id,
                                 std::string const & name,
                                 std::string const & cmd,
                                 std::chrono::duration<double> const &diff) const
{
    std::array<std::string, 1> fields = {"duration"};

    auto const num_tags = 4;
    std::array<std::string, num_tags> tags = {"id", "name", "command", "unit"};

    using Timing = utils::TimeSeriesMeasurement<num_tags, size_t>;
    Timing timing(ts, "timing", fields, tags);

    auto dur = chrono::duration_cast<chrono::microseconds>(diff);
    slog() << "[Main] Running " << cmd << " took "
           << utils::format_number(dur.count()) << "μs.";

    timing.insert(dur.count(), {id, name, cmd, "μs"});
}





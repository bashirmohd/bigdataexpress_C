#include "main.h"

#include "conf.h"
#include "utils/utils.h"

using namespace std;
using namespace utils;

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;


namespace
{
    std::string extract_cid(Conf const & conf)
    {
        // if "cid" is defined
        if (conf.has("bde.mq_server.cid")) 
            return conf.get<string>("bde.mq_server.cid");

        // or seek to the old convention of "queue"
        if (conf.has("bde.mq_server.queue")) 
            return conf.get<string>("bde.mq_server.queue");

        // finally, use a random string
        return utils::guid();
    }
}

Main::Main(Conf const & conf)
: conf  (conf)

, st_host_ip   ( conf.get<string>("bde.store.host") )
, st_host_port ( conf.get<int>("bde.store.port") )
, mq_host_ip   ( conf.get<string>("bde.mq_server.host") )
, mq_host_port ( conf.get<int>("bde.mq_server.port") )

, store     ( st_host_ip, 
              st_host_port, 
              conf.get<string>("bde.store.db") )

, rpcserver ( mq_host_ip, 
              mq_host_port, 
              extract_cid(conf), 
              std::bind(&Main::rpc_msg_handler, this, _1, _2) )

, rpc       ( mq_host_ip, 
              mq_host_port )

, mqtt      ( mq_host_ip, 
              mq_host_port,
              "bde-" + utils::guid(),
              bind(&Main::on_msg, this, _1, _2),
              bind(&Main::on_conn, this, _1, _2) )

, scheduler (conf, store, rpc)
{
    slog() << "[BDEserver] : MQ server @ " 
           << conf.get<string>("bde.mq_server.host") << " / "
           << mq_host_ip << " : " << mq_host_port;

    slog() << "[BDEserver] : Mongo server @ " 
           << conf.get<string>("bde.store.host") << " / "
           << st_host_ip << " : " << st_host_port;
}

Main::~Main()
{
    rpc.stop();
    rpcserver.stop();
}

void Main::run()
{
    // log message
    slog(s_info) << "starting bde server.";

    // mqtt broker encryption method
    std::string ca     = conf.get<string>("bde.mq_server.cacert", "");
    std::string psk_id = conf.get<string>("bde.mq_server.psk_id", "");
    std::string psk    = conf.get<string>("bde.mq_server.psk", "");

    bool use_ca  = !ca.empty();
    bool use_psk = !psk.empty() || !psk_id.empty();

    // cant have both
    if (use_ca && use_psk)
    {
        throw std::runtime_error("cant set both cafile and psk in the configuration.");
    }

    if (use_ca)
    {
        rpc.set_tls_ca(ca);
        rpcserver.set_tls_ca(ca);
        mqtt.set_tls_ca(ca);

        slog() << "using CA from " << ca  << " for encrypted MQTT broker.";
    }
    else if (use_psk)
    {
        rpc.set_tls_psk(psk_id, psk);
        rpcserver.set_tls_psk(psk_id, psk);
        mqtt.set_tls_psk(psk_id, psk);

        slog() << "using PSK with id " << psk_id  << " for encrypted MQTT broker.";
    }
    else
    {
        slog() << "using unencrpyted MQTT broker.";
    }

    // start the RPC caller
    rpc.start();

    // start RPC server
    rpcserver.start();

    // mqtt service
    mqtt.start();
    mqtt.subscribe("reg/+", 2);
    mqtt.subscribe("ld/status/+", 1);

    // start the scheduler (block)
    scheduler.run();

    // if runs to here, meaning it is trying to exit
    rpc.stop();
    rpcserver.stop();

    slog(s_info) << "main run() finished";
}

void Main::on_msg(std::string const & topic, Json::Value const & msg)
{
    slog() << "mqtt message received, topic: " << topic;
    slog() << msg;

    if (Mqtt::topic_match("reg/+", topic))
    {
        if (!msg.isMember("online") || !msg["online"].isBool())
        {
            slog(s_error) << "missing or invalid 'online' field in the registration message";
            mqtt.clr_retain(topic, 1);
            return;
        }

        if (!msg.isMember("queue") || !msg["queue"].isString())
        {
            slog(s_error) << "missing or invalid 'queue' field in the registration message";
            mqtt.clr_retain(topic, 1);
            return;
        }

        auto tokens = Mqtt::topic_tokenise(topic);

        bool online = msg["online"].asBool();
        auto queue  = msg["queue"].asString();

        // dispatch the resource construct event
        scheduler.resource_construct(online, tokens[1], queue, msg["modules"]);

        if (!online)
        {
            // clear the offline retain message from server
            mqtt.clr_retain(topic, 1);
        }
    }
    else if (Mqtt::topic_match("ld/status/+", topic))
    {
        // job status update message
        auto tokens = Mqtt::topic_tokenise(topic);
        scheduler.mld_status(tokens[2], msg["params"]);

        // clear the retained message
        mqtt.clr_retain(topic, 1);
    }
}

void Main::on_conn(bool conn, int rc)
{
}

void Main::rpc_msg_handler(Json::Value const & msg, RpcProps const & props)
{
    auto cmd = msg["cmd"].asString();
    auto const & params = msg["params"];

    // response object
    Json::Value res;

    // response callback
    auto cb = [this, props](Json::Value const & res) {
        if (!props.reply_to.empty()) rpcserver.send_response(res, props);
    };

    // command handler
    if (cmd == "get_storage_list")
    {
        return scheduler.get_storage_list(cb);
    }
    else if (cmd == "get_dtn_list")
    {
        return scheduler.get_dtn_list(cb);
    }
    else if (cmd == "get_dtn_topology")
    {
        return scheduler.get_dtn_topology(cb);
    }
    else if (cmd == "get_file_list")
    {
        return scheduler.get_file_list(params, cb);
    }
    else if (cmd == "create_folder")
    {
        return scheduler.create_folder(params, cb);
    }
    else if (cmd == "get_dtn_info")
    {
        return scheduler.get_dtn_info(params, cb);
    }
    else if (cmd == "get_total_size")
    {
        return scheduler.get_total_size(params, cb);
    }
    else if (cmd == "submit_job")
    {
        return scheduler.add_raw_job(params["id"].asString(), cb);
    }
    else if (cmd == "file_expand_and_group")
    {
        return scheduler.expand_and_group(params, cb);
    }
    else if (cmd == "verify_checksum")
    {
        return scheduler.verify_checksum(params, cb);
    }
    else if (cmd == "publish_dtn_user_mapping")
    {
        return scheduler.publish_dtn_user_mapping(cb);
    }
    else if (cmd == "probe_rate")
    {
        res = scheduler.probe_rate(params);
    }
    else if (cmd == "query_path_dtns")
    {
        res = scheduler.query_path_dtns(params);
    }
    else if (cmd == "get_available_best_effort_dtn")
    {
        res = scheduler.get_available_best_effort_dtn(params);
    }
    else if (cmd == "sdn_reserve_request")
    {
        res = scheduler.sdn_reserve_request(params);
    }
    else if (cmd == "sdn_release_request")
    {
        res = scheduler.sdn_release_request(params);
    }
    else if (cmd == "dtn_icmp_ping")
    {
        res = scheduler.dtn_icmp_ping(params);
    }
    /*
    else if (cmd == "mld_status")
    {
        return scheduler.mld_status(params);
    }
    */
    else
    {
        res["code"] = 101;
        res["error"] = "unknown command";

        //slog() << "unknow command: " << msg;
    }

    // send back response only when the "reply_to" field is not empty
    if (!props.reply_to.empty()) rpcserver.send_response(res, props);
}



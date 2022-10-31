
#include "rpcserver.h"
#include "json/json.h"
#include "utils.h"

#include <stdexcept>
#include <iostream>

using namespace std;
using namespace utils;


RpcServer::RpcServer( std::string const & host, int port, std::string const & cid,
                      std::function<void(Json::Value const &, RpcProps const &)> const & handler )
: conf     ( nullptr )
, hostname ( host )
, port     ( port )
, cid      ( cid.empty() ? utils::guid() : cid )
, queue_   ( "rpc/" + cid )

, mqtt     ( hostname, port, "rpc-server-" + queue_,
             [this](std::string const & topic, Json::Value const & msg) { on_msg(topic, msg); }, 
             [this](bool conn, int i) { on_conn(conn, i); } )

, handlers ( )
, handler  ( handler )
, bypass   ( (bool)handler )
{
}

RpcServer::~RpcServer()
{
}

void RpcServer::start()
{
    mqtt.start();
    mqtt.subscribe(queue_, 2);
}

void RpcServer::stop()
{
    mqtt.stop();
}

void RpcServer::on_msg(std::string const & topic, Json::Value const & msg)
{
    // RPC properties
    RpcProps rpcprops = { msg["corr_id"].asString(), msg["reply_to"].asString() };

    // command handler
    if (bypass)
    {
        // bypass mode
        handler(msg["body"], rpcprops);
    }
    else
    {
        // reply object
        Json::Value reply;
        reply["corr_id"] = rpcprops.correlation_id;

        // identify the command and call the command handler
        auto iter = handlers.find(msg["body"]["cmd"].asString());

        if (iter == handlers.end())
        {
            slog(s_error) << "handler not found for command " << msg["body"]["cmd"].asString() << "\n";
            reply["body"]["error"]  = "no registered handler";
            reply["body"]["cmd"]    = msg["body"]["cmd"];
            reply["body"]["params"] = msg["body"]["params"];
        }
        else
        {
            reply["body"] = iter->second(msg["body"]["params"]);
        }

        // send the response
        mqtt.publish("rpc/" + rpcprops.reply_to, reply, 1, false);
    }
}

void RpcServer::on_conn(bool conn, int)
{
}

void RpcServer::send_response(Json::Value const & reply, RpcProps const & props)
{
    if (!bypass)
    {
        slog(s_error) << "RpcServer::send_reponse() only usable in bypass mode";
        return;
    }

    Json::Value msg;
    msg["corr_id"] = props.correlation_id;
    msg["body"] = reply;

    mqtt.publish(props.reply_to, msg, 1, false);
}


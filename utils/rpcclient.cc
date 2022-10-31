
#include "rpcclient.h"
#include "utils.h"
#include "json/json.h"

#include <stdexcept>
#include <iostream>

#include <ctime>
#include <cstdlib>
#include <sstream>
#include <iomanip>

#include <future>

using namespace utils;

RpcClient::RpcClient(std::string const & host, int port)
: hostname ( host )
, port ( port )
, cid ( utils::guid() )
, queue ( "rpc-res/" + cid )

, mqtt ( hostname, port, "rpc-client-" + cid,
         [this](std::string const & topic, Json::Value const & msg) { on_msg(topic, msg); },
         [this](bool conn, int i) { on_conn(conn, i); } )

, promises ( )
{
    std::srand(std::time(0));
}

RpcClient::~RpcClient()
{
}

void RpcClient::start()
{
    mqtt.start();
    mqtt.subscribe(queue, 2);
}

void RpcClient::stop()
{
    mqtt.stop();
}

void RpcClient::on_msg(std::string const & topic, Json::Value const & msg)
{
    // no need to check the topic since only subscribed to one topic
    // extract the correlation id
    auto corr = msg["corr_id"].asString();

    // find the promise
    auto iter = promises.find(corr);

    if (iter == promises.end())
    {
        slog(s_error) << "invalid correlation id\n";
        return;
    }

    // set the promise
    iter->second.set_value(msg["body"]);

    // erase the promise entry
    promises.erase(iter);
}

void RpcClient::on_conn(bool conn, int)
{
}

Json::Value RpcClient::call(std::string const & target, Json::Value const & params, int timeout, int verbose)
{
    // generate a correlation id for the rpc call
    std::string corrid = utils::guid();

    // create a promise associated with the correlation id
    auto r = promises.emplace(corrid, std::promise<Json::Value>{});

    if (r.second == false)
    {
        throw std::runtime_error("correlation id already exists in rpc call");
    }

    // get the future from the promise
    std::future<Json::Value> f = r.first->second.get_future();

    // prepare the message
    Json::Value msg;
    msg["corr_id"] = corrid;
    msg["reply_to"] = queue;
    msg["body"] = params;

    // decide the publish topic
    // this will put a prefix of "rpc/" to the target if not present
    std::string topic;

    if ( target.compare(0, 4, "rpc/") ) topic = "rpc/" + target;
    else topic = target;

    if (topic == "rpc/")
        throw std::runtime_error("RpcClient::call() empty target not allowed");

    // publish
    mqtt.publish(topic, msg, 1, false);

    // log
    if (verbose)
    {
        slog(s_debug) << "rpc request: " << msg;
    }

    // wait for reply
    auto res = f.wait_for(std::chrono::seconds(timeout));

    // check and return the response
    if (res != std::future_status::ready)
    {
        // no response received. delete the promise entry
        promises.erase(r.first);

        // forms a timeout reply
        Json::Value v;
        v["timed_out"] = true;
        v["error"] = "rpc call timed out";
        v["params"] = params;

        if (verbose)
        {
            slog(s_debug) << "rpc request timed out: " << v;
        }

        return v;
    }
    else
    {
        if (verbose)
        {
            slog(s_debug) << "rpc response: " << f.get();
        }

        return f.get();
    }
}


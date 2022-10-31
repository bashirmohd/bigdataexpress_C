#ifndef BDE_UTILS_RPCSERVER_H
#define BDE_UTILS_RPCSERVER_H

#include "mqtt.h"

#include "json/json.h"
#include "baseconf.h"

#include <string>
#include <map>
#include <functional>
#include <thread>

struct RpcProps
{
    std::string correlation_id;
    std::string reply_to;
};


class RpcServer
{
public:

    RpcServer( std::string const & host, int port, std::string const & cid = "",
               std::function<void(Json::Value const &, RpcProps const &)> const & handler = { } );

    ~RpcServer();

    void start();
    void stop();

    void set_tls_psk(std::string const & id, std::string const & psk)
    { mqtt.set_tls_psk(id, psk); }

    void set_tls_ca(std::string const & cafile)
    { mqtt.set_tls_ca(cafile); }

    std::string const & queue() const
    { return queue_; }

    void send_response(Json::Value const & reply, RpcProps const & pros);

    void reg_handler(std::string const & name, std::function<Json::Value(Json::Value const &)> h)
    { handlers.insert(std::make_pair(name, h)); }

private:

    void on_msg(std::string const & topic, Json::Value const & msg);
    void on_conn(bool conn, int i);

    BaseConf const & conf;

    std::string hostname;
    int port;
    std::string cid;
    std::string queue_;

    Mqtt mqtt;

    // command handlers
    std::map<std::string, std::function<Json::Value(Json::Value const &)>> handlers;
    std::function<void(Json::Value const &, RpcProps const &)> handler;

    // the RPC server works in bypass mode or not
    bool bypass;

};


#endif

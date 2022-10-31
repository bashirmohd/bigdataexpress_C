#ifndef BDE_UTILS_RPCCLIENT_H
#define BDE_UTILS_RPCCLIENT_H

#include "mqtt.h"

#include "json/json.h"

#include <string>
#include <map>
#include <functional>
#include <thread>
#include <future>


class RpcClient
{
public:

    RpcClient(std::string const & host, int port);
    ~RpcClient();

    void start();
    void stop();

    void set_tls_psk(std::string const & id, std::string const & psk)
    { mqtt.set_tls_psk(id, psk); }

    void set_tls_ca(std::string const & cafile)
    { mqtt.set_tls_ca(cafile); }

    void on_msg(std::string const & topic, Json::Value const & msg);
    void on_conn(bool conn, int i);

    Json::Value call(std::string const & queue, Json::Value const & params, int timeout=5, int verbose=0);

private:

    void consumer();

    std::string hostname;
    int port;
    std::string cid;
    std::string queue;

    Mqtt mqtt;

    // command handlers
    std::map<std::string, std::promise<Json::Value>> promises;

};


#endif

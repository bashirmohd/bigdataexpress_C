#ifndef BDE_SERVER_MAIN_H
#define BDE_SERVER_MAIN_H


#include "conf.h"
#include "scheduler.h"
#include "sitestore.h"

#include "utils/mqtt.h"
#include "utils/rpcserver.h"
#include "utils/rpcclient.h"
#include "utils/utils.h"


class Main
{
public:

    Main(Conf const & conf);
    ~Main();

    void run();

private:

    // rpc
    void rpc_msg_handler(Json::Value const & msg, RpcProps const & props);

    // MQTT
    void on_msg(std::string const & topic, Json::Value const & msg);
    void on_conn(bool conn, int rc);

    // conf
    Conf const & conf;

    std::string st_host_ip;
    int         st_host_port;

    std::string mq_host_ip;
    int         mq_host_port;

    // mongodb store
    SiteStore store;

    // RPC server
    RpcServer rpcserver;

    // RPC caller
    RpcClient rpc;

    // MQTT message handler
    Mqtt mqtt;

    // Scheduler
    Scheduler scheduler;

};

#endif

#ifndef BDE_SERVER_AGENT_AGENT_MANAGER
#define BDE_SERVER_AGENT_AGENT_MANAGER

#include "agent.h"
#include "utils/rpcserver.h"
#include "utils/rpcclient.h"

class AgentManager
{
    friend class Agent;

public:

    AgentManager(Conf const & conf, size_t extra_threads = 3);

    // prepare to run the agent manager
    void prepare();

    // start the agents
    void run();

    // register an agent module to the agent manager
    template<class T>
    T & add(T * t);

    // retrieve a registered agent module through its name
    template<class T>
    T       & get(std::string const & id);

    template<class T>
    T const & get(std::string const & id) const;

    // has a module with given id?
    bool has_module(std::string const & id) const;

    // add scheduled event
    void add_event(
        std::function<std::chrono::milliseconds()> const & handler,
        std::chrono::milliseconds const & delay,
        std::string const & tag = std::string() );

    // dispatch an immediate event
    void dispatch(std::function<void()> const & handler);

    // Send a message to the queue specified.
    Json::Value send_message(std::string const &queue, Json::Value const &message);

    // timing for command execution
    void report_timing(std::string const & id,
                       std::string const & name,
                       std::string const & cmd,
                       std::chrono::duration<double> const &diff) const;

    // accessor
    SiteStore                 &   store()       { return ss; }
    const utils::TimeSeriesDB &    tsdb() const { return ts; }

    asio::io_service const & io_service() const { return io; }
    asio::io_service       & io_service()       { return io; }

    // Expose RabbitMQ host and port, so that LauncherAgent can set
    // mdtm-ftp-client's message queue.  Other Agents should not
    // normally need this.
    std::string get_mq_host() const { return mq_host; }
    int         get_mq_port() const { return mq_port; }
    Json::Value get_mq_conf() const { return mq_conf; }

    // rpc server queue
    std::string const & queue() { return rpcserver.queue(); }

    // So that agents can start RpcServer instances if they need to.
    const Conf& get_config() { return config; }


private:

    // mqtt callbacks
    void on_mqtt_msg(std::string const & topic, Json::Value const & msg);
    void on_mqtt_conn(bool conn, int rc);

    // mqtt related methods
    bool mqtt_subscribe(Agent & module, std::string const & topic, int qos);
    bool mqtt_publish(Agent & module, std::string const & topic, Json::Value const & msg, int qos, bool retain);

    // rpc server msg handler
    void rpc_msg_handler(Json::Value const & msg, RpcProps const & props);

private:

    // machine id (first mac address)
    std::string mid;

    // client id (it is unique)
    std::string cid;

    // asio
    asio::io_service io;

    // strands
    std::map<std::string, asio::io_service::strand> strands;

    // signal handler
    asio::signal_set sig;

    // agent module storage
    std::vector<std::unique_ptr<Agent>> mstore;

    // event handler storage
    std::vector<std::pair<asio::steady_timer, std::function<void(asio::error_code const &)>>> evs;

    SiteStore ss;
    utils::TimeSeriesDB ts;

    // thread pool
    std::vector<std::thread> work_threads;

    std::string mq_host;
    int         mq_port;
    Json::Value mq_conf;

    // Rpcclient instance for messages originating from the agent.
    RpcClient rpcclient;
    RpcServer rpcserver;

    // general MQTT service
    Mqtt mqtt;

    // conf object
    Conf const &config;

    // object sent when the agent go online or offline
    Json::Value reg_val;
    Json::Value de_reg_val;

    // mqtt topics -> subscribing module
    std::vector<std::pair<std::string, Agent *>> topics;
};

template<class T>
inline T & AgentManager::add(T * t)
{
    // set manager
    t->manager(*this);

    // push into module store
    mstore.emplace_back(std::unique_ptr<T>(t));

    // return down-casted reference
    return dynamic_cast<T &>(*t);
}

template<class T>
inline T & AgentManager::get(std::string const & id)
{
    for (auto & m : mstore)
        if (m->id() == id) return dynamic_cast<T &>(*m);

    throw std::runtime_error(id + " agent module not found");
}

template<class T>
inline T const & AgentManager::get(std::string const & id) const
{
    for (auto const & m : mstore)
        if (m->id() == id) return dynamic_cast<T const &>(*m);

    throw std::runtime_error(id + " agent module not found");
}

inline bool AgentManager::has_module(std::string const & id) const
{
    for (auto const & m : mstore)
        if (m->id() == id) return true;

    return false;
}




#endif

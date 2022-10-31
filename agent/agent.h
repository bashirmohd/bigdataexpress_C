#ifndef BDE_AGENT_AGENT_H
#define BDE_AGENT_AGENT_H

#include "utils/json/json.h"
#include "utils/utils.h"
#include "utils/rpcclient.h"

#include "conf.h"
#include "sitestore.h"
#include "utils/tsdb.h"

#include <thread>
#include <chrono>
#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include <string>

class AgentManager;

class Agent
{
public:

    // ctor
    Agent(std::string const & id, std::string const & name, std::string const & type);

    // properties
    std::string const &    id() const { return id_; }
    std::string const &  name() const { return name_; }
    std::string const &  type() const { return type_; }
    std::string const & queue() const { return queue_; }

    SiteStore                 & store() const;
    const utils::TimeSeriesDB &  tsdb() const;

    // update the agent module id and name
    void update_id   (std::string const & id);
    void update_name (std::string const & name);

    // agent manager
    void           manager(AgentManager & am);
    AgentManager & manager() const { return *am_; }

    // agent module bootstrap
    void bootstrap()
    { do_bootstrap(); stage = 1; }

    // agent initialization
    void init()
    { do_init(); stage = 2; }

    // do the registration to the site storage for the agent
    void registration()
    { do_registration(); stage = 3; }

    // RPC server commands
    Json::Value command(std::string const & cmd, Json::Value const & params)
    { return do_command(cmd, params); }

    // MQTT on_message handler
    void on_message(std::string const & topic, Json::Value const & msg)
    { return do_on_message(topic, msg); }

    // modifier to the prop object
    void                set_prop(std::string const & key, Json::Value const & val);
    Json::Value const & get_prop(std::string const & key = std::string()) const;

protected:

    // overrides
    virtual void do_bootstrap() { }
    virtual void do_init() { }
    virtual void do_registration() { }
    virtual void do_on_message(std::string const & topic, Json::Value const & msg) { }
    virtual Json::Value do_command(std::string const & cmd, Json::Value const & params) {
        return {};
    }

    // for inherited class to perform the mqtt related actions
    bool mqtt_subscribe(
            std::string const & topic,
            int qos = 1 );

    bool mqtt_publish(
            std::string const & topic, 
            Json::Value const & msg, 
            int qos = 1, bool retain = false );

private:

    // basic properties
    std::string id_;
    std::string name_;
    std::string type_;
    std::string queue_;

    // holds extra properties of the agent, such as, but not limited to:
    // available bandwidth, current read/write rate, etc.
    // each property is a key value pair, where the value is a json value.
    // id, name, and type will aslo be store in the container as well.
    Json::Value prop_;

    // to access the agent manager
    AgentManager * am_;

    // stage of the agent's lifetime
    // 0: constructed, 1: done bootstrap, 2: done init, 3: done registration
    int stage;
};

#endif

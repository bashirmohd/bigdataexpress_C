#include "agent.h"
#include "agentmanager.h"


Agent::Agent(std::string const & id, std::string const & name, std::string const & type)
: id_(id)
, name_(name)
, type_(type)
, queue_()
, am_(nullptr) 
, stage(0)
{ 
    prop_["id"] = id; 
    prop_["name"] = name; 
    prop_["type"] = type; 
    prop_["queue"] = queue_;
};

SiteStore &
Agent::store() const
{
    assert(am_ != nullptr);
    return am_->store();
}

const utils::TimeSeriesDB &
Agent::tsdb() const
{
    assert(am_ != nullptr);
    return am_->tsdb();
}

void
Agent::update_id(std::string const & id)
{ 
    if ( stage >= 3 ) 
        throw std::runtime_error("Agent::update_id() called at wrong stage");

    if ( am_->has_module(id) ) 
        throw std::runtime_error("Agent::update_id() has a duplicated id");

    id_ = id; prop_["id"] = id;
}

void
Agent::update_name(std::string const & name)
{ 
    if ( stage >= 3 ) 
        throw std::runtime_error("Agent::update_name() called at wrong stage");

    name_ = name; prop_["name"] = name;
}

void
Agent::manager(AgentManager & am)
{
    if ( stage >= 1 ) 
        throw std::runtime_error("Agent::set_manager() called at wrong stage");

    am_ = &am;
    queue_ = am_->queue();
    prop_["queue"] = queue_;
}

void 
Agent::set_prop(std::string const & key, Json::Value const & val)
{ 
    if (key == "id" || key == "name" || key == "type" || key == "queue") 
        throw std::runtime_error("reserved prop key");

    prop_[key] = val; 
}

Json::Value const & 
Agent::get_prop(std::string const & key) const
{ 
    return key.empty() ? prop_ : prop_[key]; 
}

bool 
Agent::mqtt_subscribe(std::string const & topic, int qos)
{
    assert(am_ != nullptr);
    return am_->mqtt_subscribe(*this, topic, qos);
}

bool 
Agent::mqtt_publish(std::string const & topic, Json::Value const & msg, int qos, bool retain)
{
    assert(am_ != nullptr);
    return am_->mqtt_publish(*this, topic, msg, qos, retain);
}

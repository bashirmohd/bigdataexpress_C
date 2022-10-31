#include "resourcegraph.h"
#include <stdexcept>


using utils::slog;
using utils::s_warning;

// RBase
BaseJob & RBase::find_job(std::string const & id)
{
    auto iter = jobs_.find(id);
    if (iter == jobs_.end())
        throw std::runtime_error("job not found in this node/edge");

    return *(iter->second);
}

BaseJob & RBase::find_largest_re_job()
{
    if (jobs_.size() == 0)
        throw std::runtime_error("find_largest_re_job: no job on this node.");

    double largest_rate = -1.0;
    BaseJob * largest_job = nullptr;

    for (auto & p_job : jobs_)
    {
        BaseJob * job = p_job.second;

        if (job->group() == JobGroup::extra 
                && job->state() == JobState::running
                && job->rate() > largest_rate)
        {
            largest_rate = job->rate();
            largest_job = job;
        }
    }

    if (largest_job == nullptr)
        throw std::runtime_error("find_largest_re_job: all job rate < -1.0?");

    return *largest_job;
}

void RBase::add_job(BaseJob & job)
{
    jobs_.emplace(job.id(), &job);
}

void RBase::del_job(std::string const & id)
{
    jobs_.erase(id);
}

double RBase::sum_re() const
{
    double sum = 0.0;

    for(auto const & ele : jobs_)
    {
        auto const & job = *ele.second;

        if (job.state() == JobState::running 
                && job.group() == JobGroup::extra) 
        {
            sum += job.rate();
        }
    }

    return sum;
}

double RBase::sum_rb() const
{
    double sum = 0.0;

    for(auto const & ele : jobs_)
    {
        auto const & job = *ele.second;

        if (job.state() == JobState::running
                && job.group() == JobGroup::base) 
        {
            sum += job.rate();
        }
    }

    return sum;
}

// RNode
void RNode::del_in_edge(std::string const & id)
{ 
    for (int i=0; i<in_edges.size(); ++i) 
    { 
        if (in_edges[i]->rid() == id) 
        {
            in_edges.erase(in_edges.begin() + i); 
            break;
        }
    }
}

void RNode::del_out_edge(std::string const & id)
{ 
    for (int i=0; i<out_edges.size(); ++i) 
    { 
        if (out_edges[i]->rid() == id) 
        {
            out_edges.erase(out_edges.begin() + i); 
            break;
        }
    }
}



// RGraph
RGraph::RGraph(JobManager & jm)
: s_nodes()
, dtn_nodes()
, general_nodes()
, edges()
, jm(jm)
{

}

RGraph::~RGraph()
{

}

bool RGraph::has_node(std::string const & id) const
{
    return has_storage_node(id) || has_dtn_node(id) || has_general_node(id);
}

bool RGraph::has_storage_node(std::string const & id) const
{
    auto iter = s_nodes.find(id);
    return iter != s_nodes.end();
}

bool RGraph::has_dtn_node(std::string const & id) const
{
    auto iter = dtn_nodes.find(id);
    return iter != dtn_nodes.end();
}

bool RGraph::has_general_node(std::string const & id) const
{
    auto iter = general_nodes.find(id);
    return iter != general_nodes.end();
}

bool RGraph::has_gateway() const
{
    return has_general_node(gateway_id);
}

bool RGraph::has_launcher() const
{
    return has_general_node(launcher_id);
}

bool RGraph::has_sdn() const
{
    return has_general_node(sdn_id);
}

bool RGraph::has_edge(std::string const & src_node_id, std::string const & dst_node_id) const
{
    for (auto const & edge : edges)
    {
        auto const & e = edge.second;

        if (e.get_src_node().rid() == src_node_id && e.get_dst_node().rid() == dst_node_id)
        {
            return true;
        }
    }

    return false;
}

RNode & RGraph::get_node(std::string const & id)
{
    if (has_storage_node(id))
    {
        auto & node = get_storage_node(id);
        return node;
    }
    else if (has_dtn_node(id))
    {
        auto & node = get_dtn_node(id);
        return node;
    }
    else if (has_general_node(id))
    {
        auto & node = get_general_node(id);
        return node;
    }
    else
    {
        throw std::runtime_error(std::string("node not fund. id: ") + id);
    }
}

StorageNode & RGraph::get_storage_node(std::string const & sid)
{
    auto iter = s_nodes.find(sid);

    if (iter == s_nodes.end())
        throw std::runtime_error(std::string("storage node not found. id: ") + sid);

    return iter->second;
}

DTNNode & RGraph::get_dtn_node(std::string const & did)
{
    auto iter = dtn_nodes.find(did);

    if (iter == dtn_nodes.end())
        throw std::runtime_error(std::string("dtn node not found. id: ") + did);

    return iter->second;
}

RNode & RGraph::get_general_node(std::string const & gid)
{
    std::map<std::string, RNode>::iterator iter = general_nodes.find(gid);

    if (iter == general_nodes.end())
        throw std::runtime_error(std::string("general node not found. id: ") + gid);

    return iter->second;
}

RNode const & RGraph::get_general_node(std::string const & gid) const
{
    std::map<std::string, RNode>::const_iterator iter = general_nodes.find(gid);

    if (iter == general_nodes.end())
        throw std::runtime_error(std::string("general node not found. id: ") + gid);

    return iter->second;
}

RNode & RGraph::get_gateway()
{
    if (!has_gateway())
        throw std::runtime_error("gateway node not registered");

    return get_general_node(gateway_id);
}

RNode const & RGraph::get_gateway() const
{
    if (!has_gateway())
        throw std::runtime_error("gateway node not registered");

    return get_general_node(gateway_id);
}

RNode & RGraph::get_launcher()
{
    if (!has_launcher())
        throw std::runtime_error("launcher node not registered");

    return get_general_node(launcher_id);
}

RNode const & RGraph::get_launcher() const
{
    if (!has_launcher())
        throw std::runtime_error("launcher node not registered");

    return get_general_node(launcher_id);
}

RNode & RGraph::get_sdn()
{
    if (!has_sdn())
        throw std::runtime_error("sdn node not registered");

    return get_general_node(sdn_id);
}

RNode const & RGraph::get_sdn() const
{
    if (!has_sdn())
        throw std::runtime_error("sdn node not registered");

    return get_general_node(sdn_id);
}


REdge & RGraph::get_edge(std::string const & src_node_id, std::string const & dst_node_id)
{
    for (auto & edge : edges)
    {
        auto & e = edge.second;

        if (e.get_src_node().rid() == src_node_id && e.get_dst_node().rid() == dst_node_id)
        {
            return e;
        }
    }

    throw std::runtime_error(std::string("edge not found. from id: ") + src_node_id + ", to id: " + dst_node_id);
}

StorageNode & RGraph::add_storage_node(std::string const & id, std::string const & name, StorageType type, double cap)
{
    auto res = s_nodes.emplace(id, StorageNode{id, name, type, jm, cap});
    return res.first->second;
}

DTNNode & RGraph::add_dtn_node(std::string const & id, std::string const & name, double cap)
{
    auto res = dtn_nodes.emplace(id, DTNNode{id, name, jm, cap});
    return res.first->second;
}

RNode & RGraph::add_general_node(std::string const & id, std::string const & rid, std::string const & name, double cap)
{
    auto res = general_nodes.emplace(id, RNode{rid, name, jm, cap});
    return res.first->second;
}

RNode & RGraph::add_gateway()
{
    return add_general_node(gateway_id/*id*/, "gateway"/*rid*/, "Gateway"/*name*/);
}

RNode & RGraph::add_launcher(std::string const & rid, std::string const & queue)
{
    RNode & node = add_general_node(launcher_id, rid, "Launcher Agent");
    node.set_queue(queue);
    return node;
}

RNode & RGraph::add_sdn(std::string const & rid, std::string const & queue)
{
    RNode & node = add_general_node(sdn_id, rid, "SDN Agent");
    node.set_queue(queue);
    return node;
}

REdge & RGraph::add_edge(std::string const & src_node_id, std::string const & dst_node_id, double cap)
{
    if (has_edge(src_node_id, dst_node_id))
    {
        slog(s_warning) << "adding and already existed edge, from " << src_node_id << ", to " << dst_node_id;
        return get_edge(src_node_id, dst_node_id);
    }

    auto & src = get_node(src_node_id);
    auto & dst = get_node(dst_node_id);

    REdge edge(jm, cap);

    auto res = edges.emplace(edge.rid(), edge);
    res.first->second.connect(src, dst);
}

REdge & RGraph::add_edge_to_gateway(std::string const & src_node_id, double cap)
{
    if (!has_dtn_node(src_node_id))
    {
        throw std::runtime_error("node " + src_node_id + " does not exist, or is not registered as a DTN node");
    }

    return add_edge(src_node_id, gateway_id, cap);
}

void RGraph::del_node(std::string const & id)
{
    if (id == "gateway")
    {
        throw std::runtime_error("not allowed to delete the gateway node");
    }

    // node id (used in map)
    std::string nid = id;

    // launcher and sdn has special node id
    if (!has_node(id))
    {
        if (has_launcher() && id == get_launcher().rid())
        {
            nid = launcher_id;
        }
        else if (has_sdn() && id == get_sdn().rid())
        {
            nid = sdn_id;
        }
        else
        {
            slog() << "RGraph::del_node() : cannot find node id " << id;
            return;
        }
    }

    RNode & node = get_node(nid);

    auto & i_edges = node.get_in_edges();
    auto & o_edges = node.get_out_edges();

    // erase in edges
    for (auto const & e : i_edges)
    {
        e->get_src_node().del_out_edge(e->rid());
        edges.erase(e->rid());
    }

    // erase out edges
    for (auto const & e : o_edges)
    {
        e->get_dst_node().del_in_edge(e->rid());
        edges.erase(e->rid());
    }

    // erase the node
    if (has_storage_node(nid))        s_nodes.erase(nid);
    else if (has_dtn_node(nid))     dtn_nodes.erase(nid);
    else                        general_nodes.erase(nid);
}

void RGraph::del_launcher()
{
    if (!has_launcher())
    {
        slog(s_warning) << "RGraph::del_launcher(): no launcher available.";
        return;
    }

    del_node(launcher_id);
}

void RGraph::del_sdn()
{
    if (!has_sdn())
    {
        slog(s_warning) << "RGraph::del_sdn(): no SDN agent available.";
        return;
    }

    del_node(sdn_id);
}

void RGraph::reset()
{
    s_nodes.clear();
    dtn_nodes.clear();
    general_nodes.clear();
    edges.clear();
}

void RGraph::print() const
{
    slog() << "[RGraph]: general nodes = " << general_nodes.size()
           << ", dtns = " << dtn_nodes.size()
           << ", storages = " << s_nodes.size()
           << ", edges = " << edges.size();
}



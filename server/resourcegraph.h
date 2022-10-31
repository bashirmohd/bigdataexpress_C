#ifndef BDESERVER_RESOURCEGRAPH_H
#define BDESERVER_RESOURCEGRAPH_H

#include "jobmanager.h"
#include "utils/utils.h"
#include "utils/json/json.h"

#include <map>
#include <string>
#include <vector>

class RBase;
class RNode;
class REdge;

// enum types
enum class StorageType
{
    internal,
    cluster
};

// base for resource components
class RBase
{
public:

    RBase(std::string const & id, std::string const & name, JobManager & jm, double cap)
    : id_(id), name_(name), queue_(), jm_(jm), cap_(cap), connected_(false), jobs_()
    { }

    virtual ~RBase() 
    { }

    std::string const & rid() const
    { return id_; }

    std::string const & name() const
    { return name_; }

    Json::Value const & properties() const
    { return prop_; }

    void set_properties(Json::Value const & prop)
    { prop_ = prop; }

    bool connected() const
    { return connected_; }

    void set_connect(bool v)
    { connected_ = v; }

    double capacity() const
    { return cap_; }

    void set_capacity(double c)
    { cap_ = c; }

    // rabbitmq queue name
    std::string const & queue() const
    { return queue_; }

    void set_queue(std::string const & q)
    { queue_ = q; }

    // job related
    BaseJob & find_job(std::string const & id);
    BaseJob & find_largest_re_job();

    void add_job(BaseJob & job);
    void del_job(std::string const & id);

    std::map<std::string, BaseJob *> & jobs()
    { return jobs_; }

    int get_job_size() const
    { return jobs_.size(); }

    double sum_re() const;
    double sum_rb() const;

    double available_rate() const
    { return capacity() - sum_re() - sum_rb(); }

    // operator
    bool operator< (RBase const & o) const
    { return id_ < o.id_; }

protected:

    std::string id_;
    std::string name_;
    std::string queue_;

    Json::Value prop_;

    JobManager & jm_;
    double cap_;
    bool connected_;

    std::map<std::string, BaseJob *> jobs_;
};

// base class for resource nodes
class RNode : public RBase
{
public:
    RNode(std::string const & id, std::string const & name, JobManager & jm, double cap = 0.0)
    : RBase(id, name, jm, cap)
    { }

    void add_in_edge(REdge & edge)
    { in_edges.push_back(&edge); set_connect(true); }

    void add_out_edge(REdge & edge)
    { out_edges.push_back(&edge); set_connect(true); }

    void del_edge(std::string const & id)
    { del_in_edge(id); del_out_edge(id); }

    void del_in_edge(std::string const & id);
    void del_out_edge(std::string const & id);

    bool has_in_edges()  const { return in_edges.size(); }
    bool has_out_edges() const { return out_edges.size(); }

    std::vector<REdge *>       & get_in_edges()       { return in_edges; }
    std::vector<REdge *> const & get_in_edges() const { return in_edges; }

    std::vector<REdge *>       & get_out_edges()       { return out_edges; }
    std::vector<REdge *> const & get_out_edges() const { return out_edges; }

private:

    std::vector<REdge *> in_edges;
    std::vector<REdge *> out_edges;

};

// base class for resource edges
class REdge : public RBase
{
public:
    REdge(JobManager & jm, double cap = 0.0)
    : RBase(utils::guid(), "", jm, cap), src_node(nullptr), dst_node(nullptr)
    { }

    void connect(RNode & src, RNode & dst)
    {
        src_node = &src; dst_node = &dst; set_connect(true);
        src_node->add_out_edge(*this);
        dst_node->add_in_edge (*this);
    }

    RNode       & get_src_node()
    { return *src_node; }

    RNode const & get_src_node() const
    { return *src_node; }

    RNode       & get_dst_node()
    { return *dst_node; }

    RNode const & get_dst_node() const
    { return *dst_node; }

private:

    RNode * src_node;
    RNode * dst_node;
};

class DTNNode;

// storage node
class StorageNode : public RNode
{
public:
    StorageNode(std::string const & id, std::string const & name, StorageType type, JobManager & jm, double cap = 0.0)
    : RNode(id, name, jm, cap), st(type), root_folder_("/"), p_dtn(nullptr)
    { }

    std::string const & root_folder() const
    { return root_folder_; }

    void set_root_folder(std::string const & folder)
    { root_folder_ = folder; }

    DTNNode & get_one_dtn_node()
    { if (!p_dtn) throw std::runtime_error("no associated dtn node with this storage");
      return *p_dtn; }

    void set_one_dtn_node(DTNNode & dtn)
    { p_dtn = &dtn; }

private:
    StorageType st;
    std::string root_folder_;
    DTNNode * p_dtn;
};

// DTN node
class DTNNode : public RNode
{
public:
    DTNNode(std::string const & id, std::string const & name, JobManager & jm, double cap = 0.0)
    : RNode(id, name, jm, cap)
    { }

    std::string const & ctrl_ip() const
    { return ctrl_ip_; }

    void set_ctrl_ip(std::string const & ip)
    { ctrl_ip_ = ip; }

private:

    std::string ctrl_ip_;
    std::string ctrl_if_;
};

// the RGraph class
class RGraph
{
public:

    RGraph(JobManager & jm);
    ~RGraph();

    // tester
    bool has_node        (std::string const & id) const;
    bool has_storage_node(std::string const & id) const;
    bool has_dtn_node    (std::string const & id) const;
    bool has_general_node(std::string const & id) const;
    bool has_edge        (std::string const & src_node_id, std::string const & dst_node_id) const;

    bool has_gateway()  const;
    bool has_launcher() const;
    bool has_sdn()      const;

    // getter
    RNode       & get_node        (std::string const & id);
    StorageNode & get_storage_node(std::string const & sid);
    DTNNode     & get_dtn_node    (std::string const & did);
    REdge       & get_edge        (std::string const & src_node_id, std::string const & dst_node_id);

    RNode       & get_general_node(std::string const & gid);
    RNode const & get_general_node(std::string const & gid) const;

    RNode       & get_gateway();
    RNode const & get_gateway() const;

    RNode       & get_launcher();
    RNode const & get_launcher() const;

    RNode       & get_sdn();
    RNode const & get_sdn() const;

    std::map<std::string, StorageNode> const & get_storage_nodes() const { return s_nodes; }
    std::map<std::string, DTNNode>     const & get_dtn_nodes()     const { return dtn_nodes; }

    // setter
    StorageNode & add_storage_node(std::string const & id, std::string const & name, StorageType type, double cap = 0.0);
    DTNNode     & add_dtn_node    (std::string const & id, std::string const & name, double cap = 0.0);
    RNode       & add_general_node(std::string const & id, std::string const & rid, std::string const & name, double cap = 0.0);
    REdge       & add_edge        (std::string const & src_node_id, std::string const & dst_node_id, double cap = 0.0);

    RNode & add_gateway ();
    RNode & add_launcher(std::string const & rid, std::string const & queue);
    RNode & add_sdn     (std::string const & rid, std::string const & queue);

    REdge & add_edge_to_gateway(std::string const & src_node_id, double cap = 0.0);

    // helpers
    void del_node(std::string const & id);
    void del_launcher();
    void del_sdn();

    void reset();

    void print() const;

private:

    constexpr static const char * gateway_id  = "RGRAPH_ID_GATEWAY";
    constexpr static const char * launcher_id = "RGRAPH_ID_LAUNCHER";
    constexpr static const char * sdn_id      = "RGRAPH_ID_SDN";

    std::map<std::string, StorageNode> s_nodes;
    std::map<std::string, DTNNode> dtn_nodes;
    std::map<std::string, RNode> general_nodes;

    std::map<std::string, REdge> edges;

    JobManager & jm;
};


#endif

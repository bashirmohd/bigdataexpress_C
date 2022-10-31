#include <string>
#include "utils/portal.h"

struct AccToken
{
    std::string acc;
    std::string ref;
};

struct wan_create_res_t
{
    std::string uuid;
    int src_vlan;
    int dst_vlan;
    double max_bw;
};

class WAN
{

public:

    WAN(Json::Value const & sense, Portal & portal);

    wan_create_res_t create( 
            std::string const & src_stp,
            std::string const & dst_stp,
            std::string const & src_vlan_s,
            std::string const & dst_vlan_s );

    bool reserve(
            std::string const & uuid,
            std::string const & src_stp,
            std::string const & dst_stp,
            int src_vlan,
            int dst_vlan,
            double bandwidth );

    // step 3:
    bool commit(
            std::string const & uuid );

    // step 4:
    bool release(
            std::string const & uuid, 
            int max_retry = 5 );

    // step 5:
    bool terminate(
            std::string const & uuid, 
            int max_retry = 5 );

    // step 6:
    bool del(
            std::string const & uuid, 
            int max_retry = 5 );

private:

    void get_token();

private:

    AccToken token;
    Portal & p;

    std::string auth_url;
    std::string base_url;

    std::string username;
    std::string password;
    std::string client_id;
    std::string client_secret;

    bool configured;
};

#ifndef BDE_AGENT_SITE_STORE_H
#define BDE_AGENT_SITE_STORE_H

#include "conf.h"
#include "utils/mongostore.h"

class SiteStore
{
public:
    SiteStore(Conf const & conf);

    void reg_local_storage(Json::Value v);

    void register_dtn_agent(std::string const & id,
                            Json::Value const & val);

    void register_launcher_agent(std::string const & id,
                                 Json::Value const & val);

    void register_storage_dtn_map(Json::Value const & val);
    void delete_storage_dtn_maps(std::string const & dtn);

    // Called by "expand_and_group_v2" command handler when results
    // are requested in the DB.
    std::string add_block(Json::Value const &val);

private:
    MongoStore ms;
};

#endif

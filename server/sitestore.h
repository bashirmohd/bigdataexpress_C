#ifndef BDESERVER_SITESTORE_H
#define BDESERVER_SITESTORE_H

#include "bsoncxx/builder/stream/document.hpp"
#include "bsoncxx/json.hpp"

#include "mongocxx/client.hpp"
#include "mongocxx/instance.hpp"
#include "mongocxx/uri.hpp"

#include "utils/json/json.h"
#include "utils/mongostore.h"

#include "conf.h"
#include "typedefs.h"


class SiteStore
{
public:

    SiteStore(std::string const & host, int port, std::string const & db);
    ~SiteStore();

    // get site info
    Json::Value get_site_info(std::string const & id);

    // storage list in this site
    Json::Value get_storage_list();

    // DTN list in the site
    Json::Value get_dtn_list();

    // list of all user identity(CN) to DTN username mapping
    Json::Value get_cn_dtn_user_mapping();

    // get the username from mapped cn and dtnid
    std::string get_user_from_cn(std::string const & cn, std::string const & dtnid);

    // storage DTN map
    Json::Value get_storage_dtn_map();

    // dtn address of the given storage id
    std::string get_storage_dtn_addr(std::string const & storage);

    // the private key and certificate of the given user and site
    Json::Value get_user_keys(std::string const & user, std::string const & site);

    // get raw job object from db
    Json::Value get_rawjob_from_id(std::string const & id);
    Json::Value get_rawjobs_from_state(rawjob_state s);

    // get sjobs with the type "best effort", ordered by submitted time asc
    Json::Value get_best_effort_sjobs();
    Json::Value get_active_best_effort_sjobs();

    // sjobs
    Json::Value get_sjob_from_task_id(std::string const & task);
    Json::Value get_sjobs_from_rawjob_id(std::string const & id);

    // return all sites
    Json::Value get_sites();

    Json::Value get_block_files(std::string const & blockid);

    // delete sjobs which has the give rawjob id
    size_t del_sjobs(std::string const & rawjob_id);

    // updates
    void update_stream_state(std::string const & guid, int state);
    void update_rawjob_size(std::string const & id, double size);

    void insert_flow(std::string const & sjob_id, Json::Value const & flow);

    void update_rawjob_state(std::string const & rjob_id, rawjob_state state);

    void update_sjob_state  (std::string const & sjob_id, sjob_state state);
    void update_sjob_stage  (std::string const & sjob_id, std::string const& stage, sjob_stage val);

    void update_sjob_flow_state  (std::string const & flow_id, flow_state state);
    void update_sjob_block_state (std::string const & block_id, block_state state);

    void increment_rawjob_field(std::string const & id, std::string const & field, int64_t val);
    void increment_sjob_field  (std::string const & id, std::string const & field, int64_t val);
    void increment_sjob_flow_field  (std::string const & id, std::string const & field, int64_t val);
    void increment_sjob_block_field (std::string const & id, std::string const & field, int64_t val);

    Json::Value increment_site_field(std::string const & id, std::string const & field, int64_t val);

    template<class T>
    void update_sjob_flow_field(std::string const & flow_id, std::string const & field, T const & val);

    template<class T>
    void update_sjob_block_field(std::string const & block_id, std::string const & field, T const & val);


    template<class T>
    void update_collection_field(
            std::string const & col, 
            std::string const & id, 
            std::string const & field, 
            T const & val);

    // general methods
    std::string insert(std::string const & collection, Json::Value const & doc);
    void delete_all(std::string const & collection);

private:

    // helper
    Json::Value json_from_bson(
            bsoncxx::stdx::optional<bsoncxx::document::value> const & doc, 
            const char * err);

    // private members
    Json::Reader reader;
    MongoStore ms;
};

template<class T>
inline void SiteStore::update_collection_field(
        std::string const & col, 
        std::string const & id, 
        std::string const & field, 
        T const & val)
{
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::finalize;

    (*ms.client())[ms.db()][col].find_one_and_update(
            document{} << "_id" << bsoncxx::oid(id) << finalize,
            document{} << "$set" << open_document
                                 << field << val
                                 << close_document
                       << finalize
    );
}

template<class T>
inline void SiteStore::update_sjob_block_field(std::string const & block_id, std::string const & field, T const & val)
{
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::finalize;

    (*ms.client())[ms.db()]["sjob"].find_one_and_update(
            document{} << "blocks.id" << block_id << finalize,
            document{} << "$set" << open_document
                                 << std::string("blocks.$.") + field << val
                                 << close_document
                       << finalize
    );
}

template<class T>
inline void SiteStore::update_sjob_flow_field(std::string const & flow_id, std::string const & field, T const & val)
{
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::finalize;

    (*ms.client())[ms.db()]["sjob"].find_one_and_update(
            document{} << "flows.task" << flow_id << finalize,
            document{} << "$set" << open_document
                                 << std::string("flows.$.") + field << val
                                 << close_document
                       << finalize
    );
}

template<>
inline void SiteStore::update_sjob_flow_field(std::string const & flow_id, std::string const & field, Json::Value const & val)
{
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::finalize;

    using bsoncxx::builder::concatenate_doc;
    using bsoncxx::from_json;

    (*ms.client())[ms.db()]["sjob"].find_one_and_update(
            document{} << "flows.task" << flow_id << finalize,
            document{} << "$set" << open_document
                                         << std::string("flows.$.") + field 
                                         << open_document
                                         << concatenate_doc{ 
                                              bsoncxx::from_json(Json::FastWriter().write(val)).view() }
                                         << close_document
                                 << close_document
                       << finalize
    );
}

#if 0
inline void SiteStore::update_sjob_stage(
        std::string const& sid, 
        std::string const& stage, 
        T const& val )
{
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::finalize;

    using bsoncxx::builder::concatenate_doc;
    using bsoncxx::from_json;

    (*ms.client())[ms.db()]["sjob"].find_one_and_update(
            document{} << "_id" << bsoncxx::oid(sid) << finalize,
            document{} << "$set" 
                       << open_document
                           << std::string("stage.") + stage << val
                       << close_document
            << finalize
 
    );
}

template<>
inline void SiteStore::update_sjob_stage<Json::Value>(
        std::string const& sid, 
        std::string const& stage, 
        Json::Value const& val )
{
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::finalize;

    using bsoncxx::builder::concatenate_doc;
    using bsoncxx::from_json;

    (*ms.client())[ms.db()]["sjob"].find_one_and_update(
            document{} << "_id" << bsoncxx::oid(sid) << finalize,
            document{} << "$set" 
                       << open_document
                           << std::string("stage.") + stage
                           << open_document 
                               << concatenate_doc{bsoncxx::from_json(Json::FastWriter().write(val)).view()} 
                           << close_document 
                       << close_document
            << finalize
    );
}
#endif


#endif

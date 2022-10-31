
#include "sitestore.h"
#include <iostream>

#include "utils/utils.h"

using namespace utils;

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::finalize;

SiteStore::SiteStore(std::string const & host, int port, std::string const & db)
: reader ( )
, ms ( host, port, db )
{
}

SiteStore::~SiteStore()
{
}

Json::Value SiteStore::get_site_info(std::string const & id)
{
    auto doc = ms.find_one_by_id("site", id);
    return json_from_bson(doc, "site not found");
}

Json::Value SiteStore::get_storage_list()
{
    auto cursor = ms.find_all("storage");
    Json::Value res = Json::arrayValue;

    for ( auto const & doc : cursor )
    {
        //std::string name = doc["name"].get_utf8().value.to_string();
        //double d = doc["bandwidth"].get_double();
        
        Json::Value jdoc;
        Json::Reader r;

        if ( !r.parse(bsoncxx::to_json(doc), jdoc) )
        {
            throw std::runtime_error("unrecognized object from mongo query");
        }

        auto dtn = get_storage_dtn_addr(jdoc["id"].asString());

        if (!dtn.empty())
        {
            jdoc["dtn"] = dtn;
            res.append(jdoc);
        }
    }

    return res;
}


Json::Value SiteStore::get_dtn_list()
{
    // epoch
    auto exp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

    // find dtn
    // auto cursor = ms.find_all("dtn");
    auto cursor = ms.find("dtn", document{}
            << "expire_at" << open_document
                           << "$gt" << exp_ms - 10*60*1000
                           << close_document
            << finalize
    );

    Json::Value res = Json::arrayValue;

    for ( auto const & doc : cursor )
    {
        //std::string name = doc["name"].get_utf8().value.to_string();
        //double d = doc["bandwidth"].get_double();
        
        Json::Value jdoc;
        Json::Reader r;

        if ( !r.parse(bsoncxx::to_json(doc), jdoc) )
        {
            throw std::runtime_error("unrecognized object from mongo query");
        }

        res.append(jdoc);
    }

    return res;
}

Json::Value SiteStore::get_cn_dtn_user_mapping()
{
    auto cursor = ms.find_all("usercndtn");
    Json::Value res = Json::arrayValue;

    for ( auto const & doc : cursor )
    {
        Json::Value jdoc;
        Json::Reader r;

        if ( !r.parse(bsoncxx::to_json(doc), jdoc) )
        {
            throw std::runtime_error("unrecognized object from mongo query");
        }

        res.append(jdoc);
    }

    return res;
}

std::string SiteStore::get_user_from_cn(std::string const & cn, std::string const & dtnid)
{
    // find ucn
    auto ucn = ms.find_one("usercndtn", document{}
            << "cn" << cn
            << "dtnid" << dtnid
            << finalize
    );

    // ucn not found
    if (!ucn) return std::string();

    // return dtn username
    return (*ucn).view()["username"].get_utf8().value.to_string();
}

Json::Value SiteStore::get_storage_dtn_map()
{
    auto cursor = ms.find_all("sdmap");
    Json::Value res = Json::arrayValue;

    for ( auto const & doc : cursor )
    {
        //std::string name = doc["name"].get_utf8().value.to_string();
        //double d = doc["bandwidth"].get_double();
        
        Json::Value jdoc;
        Json::Reader r;

        if ( !r.parse(bsoncxx::to_json(doc), jdoc) )
        {
            throw std::runtime_error("unrecognized object from mongo query");
        }

        res.append(jdoc);
    }

    return res;
}

std::string SiteStore::get_storage_dtn_addr(std::string const & storage)
{
    // find in storage-dtn-map
    auto sd = ms.find_one_by_field("sdmap", "storage", storage);

    // storage not found
    if (!sd) return std::string();

    // epoch
    auto exp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

    // find dtn
    auto dtn = ms.find_one("dtn", document{}
            << "id" << (*sd).view()["dtn"].get_value()
            << "expire_at" << open_document
                           << "$gt" << exp_ms - 10*60*1000
                           << close_document
            << finalize
    );

    // dtn not found
    if (!dtn) return std::string();

    // return dtn host
    return (*dtn).view()["ctrl_interface"]["ip"].get_utf8().value.to_string();
}

Json::Value SiteStore::get_user_keys(std::string const & user, std::string const & site)
{
    auto doc = ms.find_one("usercert", document{} 
            << "user" << bsoncxx::oid(user)
            << "site" << bsoncxx::oid(site)
            << finalize );

    return json_from_bson(doc, "certificate not found");
}

Json::Value SiteStore::get_rawjob_from_id(std::string const & id)
{
    auto doc = ms.find_one_by_id("rawjob", id);
    return json_from_bson(doc, "raw job not found");
}

Json::Value SiteStore::get_rawjobs_from_state(rawjob_state s)
{
    auto c = ms.client();
    auto cursor = (*c)[ms.db()]["rawjob"].find(
            document{} << "state" << (int)s << finalize
    );

    Json::Reader r;
    Json::Value res = Json::arrayValue;

    for ( auto const & doc : cursor )
    {
        Json::Value sjob;

        if (!r.parse(bsoncxx::to_json(doc), sjob))
            throw std::runtime_error("unparsable rawjob object from mongo query");

        res.append(sjob);
    }

    return res;
}

Json::Value SiteStore::get_sites()
{
    auto c = ms.client();
    auto cursor = (*c)[ms.db()]["site"].find(
            document{} << finalize 
    );

    Json::Value res = Json::arrayValue;

    for ( auto const & doc : cursor )
    {
        Json::Value site;
        Json::Reader r;

        if (!r.parse(bsoncxx::to_json(doc), site))
            throw std::runtime_error("unparsable rawjob object from mongo query");

        res.append(std::move(site));
    }

    return res;
}

Json::Value SiteStore::get_block_files(std::string const & blockid)
{
    auto doc = ms.find_one_by_id("block", blockid);
    return json_from_bson(doc, "block not found");
}

Json::Value SiteStore::get_best_effort_sjobs()
{
    mongocxx::options::find opts;
    opts.sort( document{} << "submittedat" << 1 << finalize );

    auto c = ms.client();
    auto cursor = (*c)[ms.db()]["sjob"].find(
            document{} << "type" << "best_effort" << finalize,
            opts );

    Json::Value res = Json::arrayValue;

    for ( auto const & doc : cursor )
    {
        Json::Value sjob;
        std::string json_str = bsoncxx::to_json(doc);
        Json::Reader r;

        if (!r.parse(json_str, sjob))
        {
            slog(s_error) << "error parsing 'get_best_effort_sjobs' query results: " << json_str;
            throw std::runtime_error("unparsable object from mongo query");
        }

        res.append(std::move(sjob));
    }

    return res;
}

Json::Value SiteStore::get_active_best_effort_sjobs()
{
    mongocxx::options::find opts;
    opts.sort( document{} << "submittedat" << 1 << finalize );

    auto c = ms.client();
    auto cursor = (*c)[ms.db()]["sjob"].find(
            document{} 
                << "$and" << open_array 
                              << open_document 
                                  << "type" << "best_effort"
                              << close_document
                              << open_document 
                                  << "$or" << open_array 
                                               << open_document 
                                                   << "state" << (int)sjob_state::bootstrap
                                               << close_document
                                               << open_document 
                                                   << "state" << (int)sjob_state::transferring
                                               << close_document
                                           << close_array
                              << close_document
                          << close_array
            << finalize,
            opts );

    Json::Value res = Json::arrayValue;
    Json::Reader r;

    for ( auto const & doc : cursor )
    {
        Json::Value sjob;
        std::string json_str = bsoncxx::to_json(doc);

        if (!r.parse(json_str, sjob))
        {
            slog(s_error) << "error parsing 'get_best_effort_sjobs' query results: " << json_str;
            throw std::runtime_error("unparsable object from mongo query");
        }

        res.append(sjob);
    }

    return res;
}


Json::Value SiteStore::get_sjob_from_task_id(std::string const & task)
{
    auto doc = ms.find_one("sjob", 
            document{} 
            << "flows" << open_document
                       << "$elemMatch" << open_document
                                       << "task" << task
                                       << close_document
                       << close_document
            << finalize
    );

    return json_from_bson(doc, "sjob not found");
}


Json::Value SiteStore::get_sjobs_from_rawjob_id(std::string const & id)
{
    auto c = ms.client();
    auto cursor = (*c)[ms.db()]["sjob"].find(
            document{} << "rawjob" << bsoncxx::oid(id) << finalize
    );

    Json::Value res = Json::arrayValue;

    for ( auto const & doc : cursor )
    {
        Json::Value sjob;
        std::string json_str = bsoncxx::to_json(doc);
        Json::Reader r;

        if (!r.parse(json_str, sjob))
        {
            slog(s_error) << "error parsing 'get_sjobs_from_rawjob_id' query results: " << json_str;
            throw std::runtime_error("unparsable object from mongo query");
        }

        res.append(std::move(sjob));
    }

    return res;
}

size_t SiteStore::del_sjobs(std::string const & rawjob_id)
{
    return ms.del_many("sjob", document{} << "rawjob" << bsoncxx::oid(rawjob_id) << finalize);
}

std::string SiteStore::insert(std::string const & collection, Json::Value const & doc)
{
    return ms.insert_one(collection, doc);
}

void SiteStore::delete_all(std::string const & collection)
{
    ms.del_many(collection, document{} << finalize);
}

void SiteStore::insert_flow(std::string const & sjob_id, Json::Value const & flow)
{
    Json::Value update;
    update["$push"]["flows"] = flow;

    auto c = ms.client();
    auto res = (*c)[ms.db()]["sjob"].update_one(
            document{} << "_id" << bsoncxx::oid(sjob_id) << finalize,
            bsoncxx::from_json(Json::FastWriter().write(update)) );

    if (res) slog() << "insert flow: " << res->modified_count();
    else     slog() << "insert flow failed";
}

void SiteStore::update_stream_state(std::string const & guid, int state)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["stream"].update_one(
            document{} << "guid" << guid << finalize,
            document{} << "$set" << open_document
                                 << "state" << state 
                                 << close_document
                                 << finalize
    );

    if (res) slog() << "update stream state: " << res->modified_count();
    else     slog() << "update stream state failed";
}

void SiteStore::update_rawjob_size(std::string const & id, double size)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["rawjob"].update_one(
            document{} << "_id" << bsoncxx::oid(id) << finalize,
            document{} << "$set" << open_document
                                 << "size" << size
                                 << close_document
                                 << finalize
    );

    if (res) slog() << "update rawjob size: " << res->modified_count();
    else     slog() << "update rawjob size failed";
}

void SiteStore::update_rawjob_state(std::string const & rjob_id, rawjob_state state)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["rawjob"].update_one(
            document{} << "_id" << bsoncxx::oid(rjob_id) << finalize,
            document{} << "$set" << open_document
                                 << "state" << (int)state
                                 << close_document
                                 << finalize
    );

    if (res) slog() << "update rawjob state: " << res->modified_count();
    else     slog() << "update rawjob state failed";
}

void SiteStore::increment_rawjob_field(std::string const & rjob_id, std::string const & field, int64_t val)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["rawjob"].update_one(
            document{} << "_id" << bsoncxx::oid(rjob_id) << finalize,
            document{} << "$inc" << open_document
                                 << field << val
                                 << close_document
                                 << finalize
    );

    if (res) slog() << "increment rawjob field: " << res->modified_count();
    else     slog() << "increment rawjob field failed";
}


void SiteStore::update_sjob_state(std::string const & sjob_id, sjob_state state)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["sjob"].update_one(
            document{} << "_id" << bsoncxx::oid(sjob_id) << finalize,
            document{} << "$set" << open_document
                                 << "state" << (int)state
                                 << close_document
                                 << finalize
    );

    Json::Value event;
    event["sjob"]["$oid"] = sjob_id;
    event["state"] = (int)state;
    event["ts"] = (Json::UInt64)utils::epoch_ms();

    insert("sjobevent", event);

    if (res) slog() << "update sjob state: " << res->modified_count();
    else     slog() << "update sjob state failed";
}


void SiteStore::update_sjob_stage(
        std::string const& sid, 
        std::string const& stage, 
        sjob_stage val )
{
    (*ms.client())[ms.db()]["sjob"].update_one(
            document{} << "_id" << bsoncxx::oid(sid) << finalize,
            document{} << "$set" << open_document
                                 << std::string("stage.") + stage << (int)val
                                 << close_document
                                 << finalize
    );
}

void SiteStore::increment_sjob_field(std::string const & sjob_id, std::string const & field, int64_t val)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["sjob"].update_one(
            document{} << "_id" << bsoncxx::oid(sjob_id) << finalize,
            document{} << "$inc" << open_document
                                 << field << val
                                 << close_document
                                 << finalize
    );

    if (res) slog() << "increment sjob field: " << res->modified_count();
    else     slog() << "increment sjob field failed";
}

void SiteStore::update_sjob_flow_state(std::string const & flow_id, flow_state state)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["sjob"].update_one(
            document{} << "flows.task" << flow_id << finalize,
            document{} << "$set" << open_document
                                 << "flows.$.state" << (int)state
                                 << close_document
                                 << finalize
    );

    if (res) slog() << "update flow state: " << res->modified_count();
    else     slog() << "udpate flow state failed";
}

void SiteStore::increment_sjob_flow_field(std::string const & flow_id, std::string const & field, int64_t val)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["sjob"].update_one(
            document{} << "flows.task" << flow_id << finalize,
            document{} << "$inc" << open_document
                                 << std::string("flows.$.") + field << val
                                 << close_document
                                 << finalize
    );

    if (res) slog() << "increment flow field: " << res->modified_count();
    else     slog() << "increment flow field failed";
}

void SiteStore::update_sjob_block_state(std::string const & block_id, block_state state)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["sjob"].update_one(
            document{} << "blocks.id" << block_id << finalize,
            document{} << "$set" << open_document
                                 << "blocks.$.state" << (int)state
                                 << close_document
                                 << finalize
    );

    if (res) slog() << "update block state: " << res->modified_count();
    else     slog() << "update block state failed";
}

void SiteStore::increment_sjob_block_field(std::string const & block_id, std::string const & field, int64_t val)
{
    auto c = ms.client();
    auto res = (*c)[ms.db()]["sjob"].update_one(
            document{} << "blocks.id" << block_id << finalize,
            document{} << "$inc" << open_document
                                 << std::string("blocks.$.") + field << val
                                 << close_document
                                 << finalize
    );

    if (res) slog() << "increment block field: " << res->modified_count();
    else     slog() << "increment block field failed";
}


Json::Value SiteStore::increment_site_field(std::string const & id, std::string const & field, int64_t val)
{
    auto opts = mongocxx::options::find_one_and_update();
    opts.return_document(mongocxx::options::return_document::k_after);

    auto c = ms.client();
    auto doc = (*c)[ms.db()]["site"].find_one_and_update(
            document{} << "_id" << bsoncxx::oid(id) << finalize,
            document{} << "$inc" << open_document
                                 << field << val
                                 << close_document
                                 << finalize,
            opts
    );

    return json_from_bson(doc, "failed to parse at increment site field");
}



Json::Value SiteStore::json_from_bson(
        bsoncxx::stdx::optional<bsoncxx::document::value> const & doc, 
        const char * err)
{
    Json::Value jdoc;
    Json::Reader r;

    if (!doc)
    {
        jdoc["error"] = std::string(err);
    }
    else
    {
        r.parse(bsoncxx::to_json(*doc), jdoc);
    }

    return jdoc;
}



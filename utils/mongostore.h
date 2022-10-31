#ifndef BDE_UTILS_MONGOSTORE_H
#define BDE_UTILS_MONGOSTORE_H

#include "bsoncxx/builder/stream/document.hpp"
#include "bsoncxx/json.hpp"

#include "mongocxx/client.hpp"
#include "mongocxx/pool.hpp"
#include "mongocxx/instance.hpp"
#include "mongocxx/uri.hpp"

#include "json/json.h"

// ---------------------------------------------------------------
// Types
// ---------------------------------------------------------------
namespace utils
{
    using bsoncxx::stdx::optional;

    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;

    class MongoStore
    {
    public:

        MongoStore(
                std::string const & host, int port, 
                std::string const & db, 
                std::string const & username = "bde", 
                std::string const & pwd = "bde" )
        : inst { }
        //, conn { mongocxx::uri{"mongodb://" + username + ":" + pwd + "@" + host + ":" + std::to_string(port) + "/" + db} }
        , pool { mongocxx::uri{"mongodb://" + username + ":" + pwd + "@" + host + ":" + std::to_string(port) + "/" + db} }
        //, db   ( conn[db] )
        , dbname ( db )
        { }

        ~MongoStore()
        { }

        //mongocxx::database & database() { return db; }
        mongocxx::pool::entry client() { return pool.acquire(); }
        std::string const & db() const { return dbname; }

        // db operations
        //
        // db.find_one
        utils::optional<bsoncxx::document::value> 
          find_one(std::string const & collection, bsoncxx::document::view_or_value const & filter)
          { auto c = client(); return (*c)[dbname][collection].find_one(filter); }

        template<class T>
        utils::optional<bsoncxx::document::value> 
          find_one_by_field(std::string const & collection, std::string const & field, T const & val)
          { return find_one(collection, document{} << field << val << finalize); }

        utils::optional<bsoncxx::document::value> 
          find_one_by_id(std::string const & collection, std::string const & id)
          { return find_one(collection, document{} << "_id" << bsoncxx::oid(id) << finalize); }

        // db.find
        mongocxx::cursor
          find(std::string const & collection, bsoncxx::document::view_or_value const & filter)
          { auto c = client(); return (*c)[dbname][collection].find(filter); }

        mongocxx::cursor
          find_all(std::string const & collection)
          { auto c = client(); return (*c)[dbname][collection].find({}); }

        // db.delete one
        void
          del_one(std::string const & collection, bsoncxx::document::view_or_value const & filter)
          { auto c = client(); (*c)[dbname][collection].delete_one(filter); }

        void
          del_one_by_id(std::string const & collection, std::string const & id)
          { del_one(collection, document{} << "_id" << bsoncxx::oid(id) << finalize); }

        template<class T>
        void
          del_one_by_key(std::string const & collection, std::string const & key, T const & val)
          { del_one(collection, document{} << key << val << finalize); }

        // db.delete many
        size_t
          del_many(std::string const & collection, bsoncxx::document::view_or_value const & filter)
          { auto c = client(); auto res = (*c)[dbname][collection].delete_many(filter);
            return res ? (*res).deleted_count() : 0; }

        // db.insert
        std::string
          insert_one(std::string const & collection, Json::Value const & doc)
          { auto c = client();
            auto res = (*c)[dbname][collection].insert_one(std::move(bsoncxx::from_json(Json::FastWriter().write(doc)))); 
            return res ? (*res).inserted_id().get_oid().value.to_string() : ""; }

#if 0
        void
        update(std::string const & collection, Json::Value const & doc)
        { db[collection].update

        mongocxx::cursor find(std::string const & collection);
        Json::Value find(std::string const & collection);
#endif

    private:

        // private members
        mongocxx::instance inst;
        //mongocxx::client conn;
        mongocxx::pool pool;

        //mongocxx::database db;
        std::string dbname;
    };
}

// elevate the MongoStore namespace
using utils::MongoStore;

namespace utils
{
    inline Json::Value
        json_from_bson(optional<bsoncxx::document::value> const & doc, std::string const & err = "")
    {
        Json::Value jdoc;
        if (doc) Json::Reader().parse(bsoncxx::to_json(*doc), jdoc);
        else     jdoc["error"] = err;
        return jdoc;
    }
}

#endif

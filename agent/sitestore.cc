#include "sitestore.h"
#include "utils/utils.h"
#include <chrono>

using namespace std;
using namespace utils;
using namespace std::chrono;

SiteStore::SiteStore(Conf const & conf)
: ms ( conf.get<string>("agent.store.host"),
       conf.get<int>   ("agent.store.port"),
       conf.get<string>("agent.store.db") )
{
}

void SiteStore::reg_local_storage(Json::Value v)
{
    string id = v["id"].asString();
    ms.del_one_by_key("storage", "id", id);
    auto s = ms.insert_one("storage", v);
}

void SiteStore::register_dtn_agent(std::string const & id,
                                   Json::Value const & val)
{
    const auto collection = "dtn";
    ms.del_one_by_key(collection, "id", id);
    ms.insert_one(collection, val);
}

void SiteStore::register_storage_dtn_map(Json::Value const & val)
{
    const auto collection = "sdmap";
    ms.insert_one(collection, val);
}

void SiteStore::delete_storage_dtn_maps(std::string const & dtn)
{
    const auto collection = "sdmap";
    auto c = ms.del_many(collection, document{} << "dtn" << dtn << finalize);
    // utils::slog() << "[Agent/SiteStore] Deleted " << c << " sdmap entries.";
}

std::string SiteStore::add_block(Json::Value const &val)
{
    const auto collection = "block";
    return ms.insert_one(collection, val);
}

void SiteStore::register_launcher_agent(std::string const & id,
                                        Json::Value const & val)
{
    const auto collection = "launcher";
    ms.del_one_by_key(collection, "id", id);
    ms.insert_one(collection, val);
}

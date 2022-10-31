#ifndef BDESERVER_PORTAL_H
#define BDESERVER_PORTAL_H

#include "curl_easy.h"
#include "curl_header.h"
#include "curl_ios.h"
#include "utils/json/json.h"

#include <utility>  // std::pair

class Portal
{
public:

    Portal();
    ~Portal();

    std::pair<long, Json::Value> post(
            std::string const & url, 
            Json::Value const & params = Json::Value(Json::objectValue) );

    std::pair<long, Json::Value> post( 
            std::string const & url, 
            std::string const & acc_token, 
            std::string const & refresh_token,
            Json::Value const & params = Json::Value(Json::objectValue) );

    std::pair<long, Json::Value> put( 
            std::string const & url, 
            std::string const & acc_token, 
            std::string const & refresh_token );

    std::pair<long, Json::Value> del( 
            std::string const & url, 
            std::string const & acc_token, 
            std::string const & refresh_token );

    std::pair<long, Json::Value> post_form(
            std::string const & url, 
            std::string const & params );

private:

    std::pair<long, Json::Value> perform();

private:

    std::ostringstream ss;
    curl::curl_ios<std::ostringstream> ios;
    curl::curl_easy easy;
    //curl::curl_header header;

    Json::FastWriter writer;
    Json::Reader reader;

};

#endif

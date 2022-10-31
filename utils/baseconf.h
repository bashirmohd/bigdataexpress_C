#ifndef MODULATOR_UTILS_BASECONF_H
#define MODULATOR_UTILS_BASECONF_H

#include "json/json.h"

#include <fstream>
#include <string>

#if 0
#define GET_CONF(X, T)  X = pset.get(#X, X).as ## T(); pset[#X] = X;

#define GET_STR_CONF(X) GET_CONF(X, String)
#define GET_INT_CONF(X) GET_CONF(X, Int)
#define GET_BOL_CONF(X) GET_CONF(X, Bool)
#define GET_DBL_CONF(X) GET_CONF(X, Double)
#endif

#define CONF_2(A, V)          if(pset[#A].isNull()) { pset[#A] = V; need_write = true; }
#define CONF_3(A, B, V)       if(pset[#A][#B].isNull()) { pset[#A][#B] = V; need_write = true; }
#define CONF_4(A, B, C, V)    if(pset[#A][#B][#C].isNull()) { pset[#A][#B][#C] = V; need_write = true; }
#define CONF_5(A, B, C, D, V) if(pset[#A][#B][#C][#D].isNull()) { pset[#A][#B][#C][#D] = V; need_write = true; }
#define CONF_6(A, B, C, D, E, V) if(pset[#A][#B][#C][#D][#E].isNull()) { pset[#A][#B][#C][#D][#E] = V; need_write = true; }
#define CONF_7(A, B, C, D, E, F, V) if(pset[#A][#B][#C][#D][#E][#F].isNull()) { pset[#A][#B][#C][#D][#E][#F] = V; need_write = true; }
#define CONF_8(A, B, C, D, E, F, G, V) if(pset[#A][#B][#C][#D][#E][#F][#G].isNull()) { pset[#A][#B][#C][#D][#E][#F][#G] = V; need_write = true; }

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME
#define CONF(...) GET_MACRO(__VA_ARGS__, CONF_8, CONF_7, CONF_6, CONF_5, CONF_4, CONF_3, CONF_2)(__VA_ARGS__)

#include <iostream>
#include <sstream>
#include <vector>
#include <chrono>
#include <stdexcept>

class BaseConf
{
public:

    BaseConf(const char * filename)
      : file(filename != nullptr ? filename : "aaa")
    {
        if (filename != nullptr)
        {
            std::ifstream in(filename, std::ifstream::binary);

            if (in.is_open())
            {
                in >> pset;
                in.close();
            }
            else
            {
                need_write = true;
            }
        }
        else
        {
            need_write = false;
        }
    }

    virtual ~BaseConf() { }

    void write_to_file()
    {
        if ( !need_write ) return;

        std::ofstream out(file);

        if ( out.is_open() )
        {
            out << writer.write(pset) << std::endl;
            out.close();
        }
    }

    template <typename T>
    Json::Value       & operator[](T const & key)
    {  return pset[key]; }

    template <typename T>
    Json::Value const & operator[](T const & key) const
    {  return pset[key]; }

    template <typename T>
    T get(std::string const & key) const
    { 
        auto v = getv(key);

        if ( v.isNull() )
            throw std::runtime_error(std::string("conf::get() key ") + key + " not found"); 

        return convert<T>(v);
    }

    template <typename T>
    T get(std::string const & key, T const & def) const
    { 
        auto v = getv(key);

        if ( v.isNull() ) return def;
        else return convert<T>(v);
    }

    bool has(std::string const & key) const
    {
        auto v = getv(key);
        return !v.isNull();
    }

    std::vector<std::string> get_keys(std::string const & key) const
    {
        auto v = getv(key);

        if ( v.isNull() )
            throw std::runtime_error(std::string("conf::get_keys() key ") + key + " not found"); 

        return v.getMemberNames();
    }

    Json::ValueType get_type(std::string const & key) const
    {
        auto v = getv(key);
        return v.type();
    }

private:

    std::vector<std::string> split(std::string const & str, char delim) const
    {
        std::vector<std::string> elems;
        std::string item;
        std::stringstream ss(str);

        while (std::getline(ss, item, delim))
            elems.push_back(item);

        return elems;
    }

    Json::Value const & getv(std::string const & key) const
    {
        Json::Value const * v = &pset;
        auto ks = split(key, '.');

        for (auto const & k : ks)
            v = &((*v)[k]);

        return *v;
    }

    template <typename T>
    T convert(Json::Value const & v) const
    {
        throw std::runtime_error("conf::get() asked type not implemented");
    }

protected:

    Json::Value pset;
    std::string file;
    bool need_write = false;

private:

    Json::StyledWriter writer;
};

template<> inline 
int BaseConf::convert(Json::Value const & v) const
{ return v.asInt(); }

template<> inline 
bool BaseConf::convert(Json::Value const & v) const
{ return v.asBool(); }

template<> inline 
float BaseConf::convert(Json::Value const & v) const
{ return v.asFloat(); }

template<> inline 
double BaseConf::convert(Json::Value const & v) const
{ return v.asDouble(); }

template<> inline 
std::string BaseConf::convert(Json::Value const & v) const
{ return v.asString(); }

template<> inline 
std::chrono::milliseconds BaseConf::convert(Json::Value const & v) const
{ return std::chrono::milliseconds(v.asInt()); }

template<> inline 
std::chrono::seconds BaseConf::convert(Json::Value const & v) const
{ return std::chrono::seconds(v.asInt()); }

#endif

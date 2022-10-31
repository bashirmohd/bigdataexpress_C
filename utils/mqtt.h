
#ifndef MCUTILS_MQTT_H
#define MCUTILS_MQTT_H

#include "json/json.h"

#include <mosquitto.h>

#include <functional>
#include <string>
#include <tuple>
#include <thread>
#include <chrono>

#if 0
#include <msgpack.hpp>

template <class ... ARGS>
class MqttFrame
{
public:

    MqttFrame(ARGS const ... args)
    : payloads(std::make_tuple(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), args...))
    , buffer()
    { msgpack::pack(buffer, payloads); }

    MqttFrame(uint8_t const * data, size_t len)
    : payloads()
    , buffer(len)
    { msgpack::unpack((const char *)data, len).get().convert(payloads); buffer.write((const char *)data, len); }

    uint8_t * buf() const
    { return (uint8_t *)(buffer.data()); }

    size_t buf_len() const
    { return buffer.size(); }

    template <size_t I>
    auto get() const
    { return std::get<I+1>(payloads); }

    int64_t timestamp() const
    { return std::get<0>(payloads); }

private:

    std::tuple<int64_t, ARGS...> payloads;
    msgpack::sbuffer buffer;
};

typedef MqttFrame<bool>               PresenceFrame;
typedef MqttFrame<uint32_t, uint32_t> RegUpdateFrame;  // previous val, current value
typedef MqttFrame<int, int, int>      CmdFrame;        // cmd, arg1, arg2
typedef MqttFrame<bool, int>          ErrFrame;        // (true: rising, false: falling), error/warning code
typedef MqttFrame<int, int>           StateFrame;      // previous state, current state

typedef MqttFrame<int>                              IntFrame; // just one parameter with integer
typedef MqttFrame<std::vector<int>>                 ViFrame;  // vector of int
typedef MqttFrame<std::vector<float>>               VfFrame;  // vector of float
typedef MqttFrame<std::vector<std::pair<int, float>>> VifFrame; // vector of <channel, value>

typedef MqttFrame<std::string>                      StrFrame;

// enable, dual, pos, pfn, pfnsp_coeff, pfnrb_coeff
typedef MqttFrame<bool, bool, int, int, double, double, double, double, int>  CModConfFrame;

typedef VifFrame DaqFrame;
#endif

class Mqtt
{
public:

    typedef std::function<void(std::string const &, Json::Value const & msg)> msg_handler_t;
    typedef std::function<void(bool, int)>                                    con_handler_t;

    Mqtt(std::string const & host, int port, std::string const & cid, msg_handler_t mh, con_handler_t ch = { });
    ~Mqtt();


    void start();
    void stop();

    bool connected() const { return conn; }
    void subscribe(std::string const & topic, int qos);

    int publish(std::string const & topic, Json::Value const & msg, int qos, bool retain)
    { 
        std::string frame = Json::FastWriter().write(msg);
        return mosquitto_publish(mosq, NULL, topic.c_str(), frame.size(), frame.c_str(), qos, retain);
    }

    int publish(std::string const & topic, uint8_t const * data, size_t len, int qos, bool retain)
    { 
        return mosquitto_publish(mosq, NULL, topic.c_str(), len, data, qos, retain);
    }

    int clr_retain(std::string const & topic, int qos)
    {
        return mosquitto_publish(mosq, NULL, topic.c_str(), 0, NULL, qos, true);
    }

    void set_will(std::string const & topic, Json::Value const & msg, int qos, bool retain)
    { 
        if (looping) throw std::runtime_error("set_will can only be called before calling the start()");

        mosquitto_will_clear(mosq);
        std::string frame = Json::FastWriter().write(msg);
        int rc = mosquitto_will_set(mosq, topic.c_str(), frame.size(), frame.c_str(), qos, retain);
        if (rc != MOSQ_ERR_SUCCESS) throw std::runtime_error("set_psk failed");
    }

    void set_tls_psk(std::string const & id, std::string const & psk)
    {
        if (looping) throw std::runtime_error("set_tls_psk can only be called before calling the start()");
        if (use_ca)  throw std::runtime_error("either set_tls_psk() or set_tls_ca(), cannot be both");

        int rc = mosquitto_tls_psk_set(mosq, psk.c_str(), id.c_str(), NULL);
        if (rc != MOSQ_ERR_SUCCESS) throw std::runtime_error("set_tls_psk failed");

        use_psk = true;
    }

    void set_tls_ca(std::string const & cafile)
    {
        if (looping) throw std::runtime_error("set_tls_ca can only be called before calling the start()");
        if (use_psk) throw std::runtime_error("either set_tls_psk() or set_tls_ca(), cannot be both");

        int rc = 0;
        rc = mosquitto_tls_set(mosq, cafile.c_str(), NULL, NULL, NULL, NULL);
        rc = mosquitto_tls_opts_set(mosq, 1, "tlsv1.2", NULL);
        if (rc != MOSQ_ERR_SUCCESS) throw std::runtime_error("set_tls_ca failed");

        use_ca = true;
    }

    static bool topic_match(std::string const & sub, std::string const & topic)
    { 
        bool res; mosquitto_topic_matches_sub(sub.c_str(), topic.c_str(), &res); 
        return res; 
    }

    static std::vector<std::string> topic_tokenise(std::string const & topic)
    {
        int count; 
        char ** topics; 
        std::vector<std::string> vt;

        mosquitto_sub_topic_tokenise(topic.c_str(), &topics, &count);

        for (int i=0; i<count; ++i) vt.emplace_back(topics[i]);

        mosquitto_sub_topic_tokens_free(&topics, count);
        return vt;
    }

private:

    int loop();

    struct mosquitto * mosq;
    std::string host;
    int         port;
    std::string cid;

    msg_handler_t msg_handler;
    con_handler_t con_handler;

    bool looping;
    std::thread listen_thread;

    std::map<std::string, int> subscriptions;

    bool conn;
    bool use_psk;
    bool use_ca;
};

#endif

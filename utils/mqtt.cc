#include "mqtt.h"
#include "utils.h"

#include <cstring>
#include <csignal>

#include <sys/socket.h>
#include <resolv.h>

using namespace utils;

Mqtt::Mqtt(std::string const & host, int port, std::string const & cid, msg_handler_t mh, con_handler_t ch)
: mosq(nullptr)
, host(host)
, port(port)
, cid(cid)
, msg_handler(mh)
, con_handler(ch)
, looping(false)
, listen_thread()
, subscriptions()
, conn(false)
, use_psk(false)
, use_ca(false)
{
    // global init
    mosquitto_lib_init();

    // create the handler
    mosq = mosquitto_new(cid.c_str(), true, this);

    // set up the callback
    mosquitto_message_callback_set(mosq, [](struct mosquitto * mosq, void * obj, const struct mosquitto_message * message) {
        if (message->payloadlen == 0) return;

        Mqtt * inst = static_cast<Mqtt*>(obj);
        Json::Value msg;

        if (Json::Reader().parse((const char*)message->payload, (const char*)message->payload + message->payloadlen, msg)) { 
            //slog() << "[MQTT received] " << msg;
            inst->msg_handler(message->topic, msg);
        } else {
            slog(s_error) << "unable to parse the payload of the incoming MQTT message";
        }
    });

    mosquitto_connect_callback_set(mosq, [](struct mosquitto * mosq, void * obj, int rc) {
        Mqtt * inst = static_cast<Mqtt*>(obj);
        if (inst->con_handler && rc == 0) inst->con_handler(true, rc);
    });

    mosquitto_disconnect_callback_set(mosq, [](struct mosquitto * momsq, void * obj, int rc) {
        Mqtt * inst = static_cast<Mqtt*>(obj);
        if (inst->con_handler) inst->con_handler(false, rc);
    });
}

void Mqtt::start()
{
    // connect to the broker
    int rc = mosquitto_connect(mosq, host.c_str(), port, 5);

    if (rc == MOSQ_ERR_SUCCESS)
    {
        conn = true;
        slog() << "[MQTT] Connected to broker (cid: "
               << (cid.empty() ? "unknown" : cid)  << ")";
    }
    else if (rc == MOSQ_ERR_INVAL)
    {
        conn = false;
        slog(s_error) << "mqtt connect error: invalid params";
        throw std::runtime_error("mqtt connect error, invalid parameters");
    }
    else if (rc == MOSQ_ERR_ERRNO)
    {
        conn = false;
        slog(s_warning) << "mqtt connect error: " << std::strerror(errno);
    }
    else
    {
        conn = false;
        slog(s_warning) << "unknown error, rc = " << rc << ", err = " << std::strerror(errno);
        throw std::runtime_error("mqtt connect fail with an unknown error.");
    }

    // start listening thread
    listen_thread = std::thread(&Mqtt::loop, this);
}

Mqtt::~Mqtt()
{
    stop();
    mosquitto_lib_cleanup();
}

void Mqtt::subscribe(std::string const & topic, int qos)
{
    subscriptions.emplace(topic, qos);
    mosquitto_subscribe(mosq, NULL, topic.c_str(), qos);
}

void Mqtt::stop()
{
    if (looping)
    {
        looping = false; 
        listen_thread.join();
    }
}

int Mqtt::loop()
{
    looping = true;

    while(looping)
    {
        int rc = mosquitto_loop(mosq, -1, 1);

        if (looping && rc)
        {
            // mark the connection lost
            conn = false;

            // do not attempt to reconnect if the error is an TLS error
            // possibly due to the wrong PSK key or id
            if (rc == MOSQ_ERR_TLS)
            {
                slog(s_error) << "MQTT TLS error, Possible with wrong psk or id, "
                              << "or with incorrect certificate settings.";
                slog() << "sending SIGINT to terminate the application...";
                std::raise(SIGINT);
                return -1;
            }

            // for now we try to disconnect it repeatly before reconnect
            // mosquitto_disconnect(mosq);

            // sleep before trying to reconnect
            std::this_thread::sleep_for(std::chrono::seconds(10));

            // try to reconnect
            rc = mosquitto_reconnect(mosq);

            if (rc == MOSQ_ERR_SUCCESS)
            {
                // connection resumed
                conn = true;

                // subscribe again
                for(auto const & sub : subscriptions)
                {
                    mosquitto_subscribe(mosq, NULL, sub.first.c_str(), sub.second);
                }
            }
            else if (rc == MOSQ_ERR_EAI)
            {
                slog() << "hostname resolve error while reconnecting mqtt. calling res_init()";

                // force to read the conf file (resolv.conf) for default domain
                // name, hostname, and server addresses. this is to solve the
                // problem that the resolving ip was constantly failed after
                // tearing down the interface then bring it back on again. the
                // problem was preventing the client from reconnecting to the
                // broker in such case.
                res_init();
            }
        }
    }

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);

    return 0;
}

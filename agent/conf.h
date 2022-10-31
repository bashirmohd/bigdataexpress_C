#ifndef BDE_BDE_AGENT_CONF_H
#define BDE_BDE_AGENT_CONF_H

#include "utils/baseconf.h"

#include <chrono>
#include <string>

class Conf : public BaseConf
{
public:
    Conf(const char * file)
      : BaseConf(file)
    {
        if (need_write)
        {
            // construct
            construct();

            // write to a new conf file
            write_to_file();
        }
    }

protected:

    void construct()
    {
        // rabbitmq server
        CONF(agent, mq_server, host,   "yosemite.fnal.gov");
        CONF(agent, mq_server, port,   5671);
        CONF(agent, mq_server, cacert, "/home/jason/certificates/cacert.pem");

        // mongodb store
        CONF(agent, store, host, "yosemite.fnal.gov");
        CONF(agent, store, port, 27017);
        CONF(agent, store, db,   "bde");
    }

public:

};

#endif

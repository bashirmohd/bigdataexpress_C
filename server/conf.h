#ifndef BDE_BDE_SERVER_CONF_H
#define BDE_BDE_SERVER_CONF_H

#include "utils/baseconf.h"

#include <chrono>
#include <string>

class Conf : public BaseConf
{
public:
    Conf(const char * file)
      : BaseConf(file)
    {
        // construct
        construct();

        // write to a new conf file
        if (need_write) write_to_file();
    }

protected:

    void construct()
    {
        // rabbitmq server
        CONF(bde, mq_server, host,   "localhost");
        CONF(bde, mq_server, port,   1883);
        CONF(bde, mq_server, queue,  "bdeserver");

        // mongodb store
        CONF(bde, store, host, "localhost");
        CONF(bde, store, port, 27017);
        CONF(bde, store, db,   "bde");

        // launching daemon
        CONF(bde, agents, ld, authentication, "cert");  // "cert" or "password"
        CONF(bde, agents, ld, auth_username, "test");  
        CONF(bde, agents, ld, auth_password, "secret");
        CONF(bde, agents, ld, write_command_files, false);
        CONF(bde, agents, ld, checksum_enable, false);
        CONF(bde, agents, ld, checksum_algorithm, "sha1");
        CONF(bde, agents, ld, group_size, "20GB");

#if 0
        CONF(bde, agents, ld, server_port, 6001);
        CONF(bde, agents, ld, port_range_min, 5000);
        CONF(bde, agents, ld, port_range_max, 6000);
#endif

    }

public:

};

#endif

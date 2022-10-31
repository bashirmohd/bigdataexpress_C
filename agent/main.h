#ifndef BDE_AGENT_MAIN_H
#define BDE_AGENT_MAIN_H

#include "conf.h"
#include "agentmanager.h"

#include "utils/mqtt.h"
#include "utils/rpcserver.h"

class Main
{
public:

    Main(Conf const & conf);
    void run(bool daemon_mode=false);

private:

    AgentManager am;
    bool am_initialized;
};


#endif

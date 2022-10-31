
#include "main.h"
#include "localstorageagent.h"
#include "sharedstorageagent.h"
#include "dtnagent.h"
#include "launcheragent.h"
#include "bde_version.h"

#include <sys/types.h>
#include <sys/wait.h>

using namespace std;
using namespace utils;

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

namespace
{
    bool init_modules(Conf const & conf, AgentManager & am)
    {
        auto keys = conf.get_keys("modules");

        for (auto const & k : keys)
        {
            string base = string("modules.") + k;

            if (conf.get_type(base) != Json::objectValue)
                continue;

            auto id   = conf.get<string>(base + ".id",   "");  // optional
            auto name = conf.get<string>(base + ".name", "");  // optional
            auto type = conf.get<string>(base + ".type");      // must

            if (type == "LocalStorage")
            {
                am.add(new LocalStorageAgent(conf["modules"][k]));
            }
            else if (type == "SharedStorage")
            {
                am.add(new SharedStorageAgent(conf["modules"][k]));
            }
            else if (type == "DTN")
            {
                am.add(new DTNAgent(conf["modules"][k]));
            }
            else if (type == "Launcher")
            {
                am.add(new LauncherAgent(conf["modules"][k]));
            }
        }

        return true;
    }
}

Main::Main(Conf const & conf)
    : am(conf)
    , am_initialized( init_modules(conf, am) )
{
}

void Main::run(bool daemon_mode)
{
    slog() << "[Main] Starting BDE agent " << BDE_VERSION << "...";

#if defined(BDE_GIT_COMMIT_HASH) && defined(BDE_GIT_COMMIT_SUBJECT)
    slog() << "[Main] Running commit " << BDE_GIT_COMMIT_HASH << ": "
           << BDE_GIT_COMMIT_SUBJECT;
#endif

    // bootstrap agents
    am.prepare();

    slog() << "[Main] BDE agent has been started."
           << (daemon_mode ? "" : " Press Ctrl-C to quit.");

    // start main loop
    am.run();

    // exiting
    slog() << "[Main] exiting...";

    // stop supervised processes
    utils::stop_process_supervision();

    // Send SIGTERM to process group.
    if (killpg(0, SIGTERM) < 0)
    {
        slog() << "[Main] Could not send SIGTERM to subprocesses; reason: "
               << strerror(errno);
    }

    slog() << "[Main] BDE agent stopped";
}


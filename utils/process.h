#ifndef BDE_UTILS_PROCESS_H
#define BDE_UTILS_PROCESS_H

#include <string>
#include <vector>
#include <map>

namespace utils
{
    struct ProcessResult
    {
        int         code;
        std::string text;
    };

    // Function for launching a sub-process.  This is essentially a
    // wrapper around popen()/pclose().
    //
    // Both return code and stdout/stderr of the sub-process will be
    // captured in the return value.
    ProcessResult run_process(std::string const & command,
                              bool                throw_exception = false,
                              bool                verbose         = true);

    // Special casing run_process() for running /usr/bin/ping.  This
    // is a workaround for SC17 demo, because KISTI systems sometimes
    // return -1 from pclose().  The real issue is unknown, at the
    // moment, so we will have to make do.
    ProcessResult run_ping(std::string const & host,
                           size_t const deadline = 2,
                           size_t const count = 2,
                           bool ivp6 = false);

    // supervise_process() will launch and "supervise" a long-running
    // subprocess; it will try to relaunch any crashed or exited
    // programs.  Callers should use a dedicated thread.
    //
    // Note that we can't use this function to supervise programs that
    // daemonize themselves: attempting to do so will result in a fork
    // bomb.
    void
    supervise_process(std::string const                        &prog,
                      std::vector<std::string> const           &args,
                      std::map<std::string, std::string> const &env = {},
                      int                                       restart_interval = 1);

    // Toggle a flag.
    void stop_process_supervision(void);
};

#endif // BDE_UTILS_PROCESS_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

#include <stdexcept>
#include <cstring>
#include <thread>
#include <algorithm>
#include <numeric>
#include <set>
#include <iostream>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <poll.h>

#include "utils/utils.h"
#include "process.h"

// ------------------------------------------------------------------------

static bool g_supervise_process = true;

void utils::stop_process_supervision(void)
{
    g_supervise_process = false;
}

// ------------------------------------------------------------------------

// Returns true if utils::slog() is writing to stdout/stderr and if we
// have a color capable terminal.  Log files will not have ANSI color
// escape codes this way, and we can easily spot stdout/stderr of the
// subprocess.
static bool has_term_color()
{
    // bool is_log_file = true;

    // auto const &rdbuf = utils::service::console().rdbuf();
    // if (rdbuf == std::cout.rdbuf() || rdbuf == std::cerr.rdbuf())
    //     is_log_file = false;

    auto const *term = std::getenv("TERM");

    if (!term)
    {
        // utils::slog() << "[Process] No TERM set; not colorizing logs.";
        return false;
    }

    static const std::set<std::string>
        color_terms { "xterm",
                      "xterm-256",
                      "xterm-256color",
                      "screen",
                      "screen-256color",
                      "vt100",
                      "color",
                      "ansi",
                      "cygwin",
                      "linux" };

    bool has_color_term = (color_terms.find(term) != color_terms.end());

    // return ((not is_log_file) and has_color_term);
    return has_color_term;
}

enum class TermColor
{
    SubProcessStdOut,
    SubProcessStdErr,
    SubProcessLaunch,
    Reset
};

static std::string term_color(const TermColor hint)
{
    // if (not has_term_color())
    //     return "";

    // See https://en.wikipedia.org/wiki/ANSI_escape_code
    static const std::string
        FG_COLOR_RED    = "\033[0;31m",
        FG_COLOR_GREEN  = "\033[0;32m",
        FG_COLOR_YELLOW = "\033[0;33m",
        FG_COLOR_BLUE   = "\033[0;34m",
        COLOR_NONE      = "\033[0m";

    switch(hint)
    {
    case TermColor::SubProcessStdOut:
        return FG_COLOR_BLUE;

    case TermColor::SubProcessStdErr:
        return FG_COLOR_RED;

    case TermColor::SubProcessLaunch:
        return FG_COLOR_YELLOW;

    case TermColor::Reset:
    default:
        return COLOR_NONE;
    }

    return COLOR_NONE;
}

// ------------------------------------------------------------------------

// When log_output() returns false, it means that reading
// stderr/stdout of the subprocess has failed, and that is time for
// the reader thread to give up.
static bool log_output(const pid_t pid, const int fd, const TermColor type)
{
    char buffer[BUFSIZ]{0};
    auto count = read(fd, buffer, sizeof(buffer));

    std::stringstream hint;
    hint << "(pid " << pid << ", "
         << (type == TermColor::SubProcessStdOut ? "stdout" : "stderr") << ")";

    if (count == -1)
    {
        if (errno == EINTR)
        {
            utils::slog() << "[Subprocess] There may be more data: "
                          << hint.str();
        }
        else
        {
            utils::slog() << "[Subprocess] Error on read(): "
                          << strerror(errno) << ", " << hint.str();
            return false;
        }
    }
    else
    {
        utils::slog() << "[Subprocess] Logging subprocess output "
                      << hint.str() << ":\n"
                      << term_color(type)
                      << buffer
                      << term_color(TermColor::Reset);
    }

    return true;
}

// Given stdout and stderr file descriptors, poll them continously
// (until failure) for data, and log them.
static void poll_and_log_subprocess_output(const pid_t pid,
                                           const int   stdout_fd,
                                           const int   stderr_fd)
{
    auto const num_fds = 2;
    auto const timeout = 100;

    struct pollfd poll_fds[num_fds]{0};

    poll_fds[0].fd     = stdout_fd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd     = stderr_fd;
    poll_fds[1].events = POLLIN;

    while (true)
    {
        auto rc = poll(poll_fds, num_fds, timeout);

        switch (rc)
        {
        case -1:
            utils::slog() << "[Subprocess] Error on poll(): "
                          << strerror(errno)
                          << " (pid: " << pid << ")";
            break;
        case 0:
            // timeout.
            break;
        default:
            if (poll_fds[0].revents & POLLIN)
            {
                if (!log_output(pid, poll_fds[0].fd, TermColor::SubProcessStdOut))
                    return;
            }

            if (poll_fds[1].revents & POLLIN)
            {
                if (not log_output(pid, poll_fds[1].fd, TermColor::SubProcessStdErr))
                    return;
            }

            break;
        }
    }
}


// ------------------------------------------------------------------------

utils::ProcessResult utils::run_process(std::string const & command,
                                        bool                throw_exception,
                                        bool                verbose)
{
    // popen() manual does not say anything about standard error - it
    // looks like we must implement capturing standard error in terms
    // of pipe(), exec(), etc and do the plumbing ourselves. This one
    // simple tick however captures standard error in a cheap manner!
    auto real_cmd = command + " 2>&1";

    if (verbose)
    {
        utils::slog() << "[run_process] running command      : \""
                      << command << "\"";
    }

    FILE *pipe = popen(real_cmd.c_str(), "re");

    if (not pipe)
    {
        auto err = "Error running \"" + command + "\":" + strerror(errno);

        if (throw_exception)
        {
            throw std::runtime_error(err);
        }

        return ProcessResult{1, err};
    }

    ProcessResult result;
    char buffer[BUFSIZ]{0};

    while (not feof(pipe))
    {
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            result.text += buffer;
        }
    }

    auto pstat = pclose(pipe);

    if (verbose)
    {
        utils::slog() << "[run_process] pclose() return code : "
                      << pstat;
        utils::slog() << "[run_process] process exit code    : "
                      << WEXITSTATUS(pstat);
        utils::slog() << "[run_process] errno : "
                      << errno << " (" << strerror(errno) << ")";
        utils::slog() << "[run_process] process read buffer  :\n \n"
                      << result.text;
    }

    if (pstat < 0)
    {
        auto status = "pclose() error: " + std::string(strerror(errno));

        if (verbose)
        {
            utils::slog() << "[run_process] exit code: " << pstat
                          << ", status: " << status;
        }

        return ProcessResult{pstat, status};
    }

    result.code = WEXITSTATUS(pstat);

    if (result.code != 0 and throw_exception)
    {
        throw std::runtime_error(result.text);
    }

    if (result.code == 0 and result.text.empty())
    {
        result.text = "OK";
    }

    return result;
}

// ------------------------------------------------------------------------

utils::ProcessResult utils::run_ping(std::string const &dest,
                                     size_t const deadline,
                                     size_t const count,
                                     bool ipv6)
{
    auto command = "/bin/ping"
        + std::string(" -w ") + std::to_string(deadline)
        + std::string(" -c ") + std::to_string(count)
        + std::string(" ") + dest;

    if (ipv6)
        command += " -6";

    utils::slog() << "[run_ping] running command: \"" << command << "\"";

    auto real_cmd = command + " 2>&1";
    FILE *pipe = popen(real_cmd.c_str(), "re");

    if (not pipe)
    {
        auto err = "Error running \"" + command + "\":" + strerror(errno);
        utils::slog() << "[run_ping] " << err;
        return ProcessResult{1, err};
    }

    ProcessResult result;
    char buffer[BUFSIZ]{0};

    while (not feof(pipe))
    {
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            result.text += buffer;
        }
    }

    auto pstat = pclose(pipe);

    utils::slog() << "[run_ping] pclose() return code: " << pstat
                  << ", process read buffer: " << result.text;

    if (pstat < 0)
    {
        auto status = "pclose() error: " + std::string(strerror(errno));
        utils::slog() << "[run_ping] pclose() return code: " << pstat
                      << ", status: " << status;

        if (result.text.find(", 100% packet loss, ") != std::string::npos)
        {
            utils::slog() << "[run_ping] special case for ping: 100% packet loss";
            return ProcessResult{1, result.text};
        }
        else if (result.text.find(", 0% packet loss, ") != std::string::npos)
        {
            utils::slog() << "[run_ping] special case for ping: 0% packet loss";
            return ProcessResult{0, result.text};
        }
        else
        {
            utils::slog() << "[run_ping] special case for ping: some packet loss";
            return ProcessResult{1, result.text};
        }
    }

    result.code = WEXITSTATUS(pstat);
    utils::slog() << "[run_ping] ping process exit code: " << result.code;

    if (result.code == 0 and result.text.empty())
    {
        result.text = "OK";
    }

    return result;
}

// ------------------------------------------------------------------------

void
utils::supervise_process(std::string const                        &prog,
                         std::vector<std::string> const           &args,
                         std::map<std::string, std::string> const &env,
                         int                                       restart_interval)
{
    if (prctl(PR_SET_CHILD_SUBREAPER, 1) < 0)
    {
        throw std::runtime_error(strerror(errno));
    }

    if (prog.empty())
    {
        throw std::runtime_error(std::string(__func__) + " received null argument");
    }

    int stdout_fds[2]{0};

    if (pipe(stdout_fds) == -1) {
        utils::slog() << "Error on pipe(): " << strerror(errno);
        exit(1);
    }

    int stderr_fds[2]{0};

    if (pipe(stderr_fds) == -1) {
        utils::slog() << "Error on pipe(): " << strerror(errno);
        exit(1);
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        auto err = "fork() error: " + std::string(strerror(errno));
        utils::slog() << err;
        throw std::runtime_error(err);
    }

    if (pid == 0)
    {
        // Get stdin/stdout handles to subprocess.
        while (dup2(stdout_fds[1], STDOUT_FILENO) == -1)
        {
            if (errno == EINTR)
                continue;
            else
                utils::slog() << "[Child] Error on stdout dup2(): "
                              << strerror(errno);
        }

        close(stdout_fds[0]);
        close(stdout_fds[1]);

        while (dup2(stderr_fds[1], STDERR_FILENO) == -1)
        {
            if (errno == EINTR)
                continue;
            else
                utils::slog() << "[Child] Error on stderr dup2(): "
                              << strerror(errno);
        }

        close(stderr_fds[0]);
        close(stderr_fds[1]);

        // Within the fork()-ed child, prepare an argument vector for
        // execv().  Note that we can't call `delete` for the `new`
        // allocations here, because execv() will replace this process
        // with a new one.
        char **argv = new char*[args.size() + 2]();
        int    idx  = 0;

        argv[idx] = new char[prog.size()+1]();
        prog.copy(argv[idx], prog.size());

        idx++;

        for (auto const & arg : args)
        {
            argv[idx] = new char[arg.size()+1]();
            arg.copy(argv[idx], arg.size());
            idx++;
        }

        argv[idx] = nullptr;

        // form envp argument for execve()
        char **envp = new char*[env.size() + 2]();
        idx         = 0;

        for (auto const & e : env)
        {
            size_t sz = e.first.length() + e.second.length() + 2;
            envp[idx] = new char[sz]();
            snprintf(envp[idx], sz, "%s=%s", e.first.c_str(), e.second.c_str());
            idx++;
        }

        envp[idx] = nullptr;

        // Make a space-separated string, to log program + arguments.
        std::string argstr =
            std::accumulate(args.begin(), args.end(),
                            std::string{},
                            [](std::string const &a, std::string const &b) {
                                return a + ' ' + b;
                            });

        // Similarly prepare a string for logging environment variables.
        std::string envstr{};
        for (int i = 0; i < idx; ++i)
        {
            envstr += envp[i];

            if (i+1 != idx)
                envstr += ";";
        }

        // Set the parent process death signal to SIGTERM, so that the
        // children too will exit along with the parent.  This is
        // another attempt at exit cleanup, besides calling killpg()
        // at exit.  At least one of them ought to work for us, right?
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) < 0)
        {
            utils::slog() << "[process] prctl(PR_SET_PDEATHSIG, SIGTERM) failed; "
                          << "reason: " << strerror(errno);
        }

        utils::slog() << "Running \"" << prog << argstr << "\""
                      << " (pid: " << getpid() << ")"
                      << " (env: " << envstr << ")";

        if (execve(argv[0], argv, envp) < 0)
        {
            for (int i = 0; i < idx; i++)
            {
                delete argv[i];
            }

            delete argv;

            std::stringstream err;

            err  << "[child process " << getpid() << "] "
                 << "Error running \"" << prog
                 << "\": execv() error: " << strerror(errno);

            // print error both on the console and log.
            utils::slog() << err.str();
            std::cerr << err.str() << "\n";

            // exit child process with non-zero return value.
            exit(1);
        }
    }
    else
    {
        // This thread will read the sub-processes stdout and stderr
        // and log it, so that they won't be lost.
        std::thread([=](){
                        poll_and_log_subprocess_output(pid,
                                                       stdout_fds[0],
                                                       stderr_fds[0]);
                    }).detach();

        int   status  = 0;
        int   options = WUNTRACED;
        pid_t chld    = -1;

        while ((chld = waitpid(-1, &status, options)) > 0);

        utils::slog() << "[Subprocesses] closing file descriptors.";

        close(stdout_fds[0]);
        close(stdout_fds[1]);

        close(stderr_fds[0]);
        close(stderr_fds[1]);


        if (WIFEXITED(status))
        {
            if (g_supervise_process)
            {
                // Program terminated by calling _exit(2) or exit(3)
                utils::slog() << term_color(TermColor::SubProcessLaunch)
                              << prog << " (pid:" << pid << ") "
                              << "exited with " <<  WEXITSTATUS(status)
                              << "; restarting."
                              << term_color(TermColor::Reset) ;
                sleep(restart_interval);
                supervise_process(prog, args, env);
            }
            else
            {
                utils::slog() << prog << " (pid:" << pid << ") "
                              << "exited with exit code "
                              <<  WEXITSTATUS(status) << ".";
            }
        }
        else if (WIFSIGNALED(status))
        {
            // Program was terminated by a signal.
            utils::slog() << prog << " (pid:" << pid << ") was "
                          << "terminated by signal " <<  WTERMSIG(status)
                          << "; restarting.";
            sleep(restart_interval);
            supervise_process(prog, args, env);
        }
        else if (WIFSTOPPED(status))
        {
            // Program was stopped; may resume.
            utils::slog() << prog << " (pid:" << pid << ") was stopped "
                          << "by signal " << WSTOPSIG(status) << ".";
        }
        else if (WIFCONTINUED(status))
        {
            // Program has received SIGCONT.
            utils::slog() << prog << " (pid:" << pid << ") has received SIGCONT.";
        }
        else
        {
            // Error -- we don't know what to do.
            auto err = std::string(prog) + " (pid:" + std::to_string(pid) + ") " +
                "is in unknown state; giving up.\n";
            utils::slog() << err;
            throw std::runtime_error(err);
        }
    }
}


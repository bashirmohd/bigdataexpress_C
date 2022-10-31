//
// A test for reading from a sub-process' stdout and stderr.
//

#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include <thread>

#include "utils/utils.h"

static std::string term_color(const int fd)
{
    if (!isatty(fd))
        return "";

    static const std::string FG_COLOR_RED  = "\033[0;31m";
    static const std::string FG_COLOR_BLUE = "\033[0;34m";

    return fd == STDERR_FILENO ? FG_COLOR_RED : FG_COLOR_BLUE;
}

static std::string color_none(const int fd)
{
    if (!isatty(fd))
        return "";

    return "\033[0m";
}

static void log_subprocess_output(pid_t pid, int (&fds)[2], const int fd)
{
    char buffer[BUFSIZ]{0};

    while (true) {
        // TODO: polling fds is probably a better idea, rather than
        // doing blocking read() operations.
        auto count = read(fds[0], buffer, sizeof(buffer));
        if (count == -1) {
            if (errno == EINTR) {
                utils::slog() << "[Parent] Waiting for more "
                              << (fd == STDERR_FILENO ? "stderr" : "stdout") ;
                continue;
            } else {
                perror("read");
                exit(1);
            }
        } else if (count == 0) {
            break;
        } else {
            utils::slog() << "[Parent] Subprocess " << pid << " says:\n"
                          << term_color(fd)
                          << buffer
                          << color_none(fd);
            // to continue reading, do not break.
            break;
        }
    }
}

int main(int argc, char **argv)
{
    // To disable running from "ctest" or "make test", uncomment below.

    // if (getenv("CTEST_INTERACTIVE_DEBUG_MODE") != 0)
    //     return 0;

    int stdout_fds[2];

    if (pipe(stdout_fds) == -1) {
        utils::slog() << "Error on pipe(): " << strerror(errno);
        exit(1);
    }

    int stderr_fds[2];

    if (pipe(stderr_fds) == -1) {
        utils::slog() << "Error on pipe(): " << strerror(errno);
        exit(1);
    }

    auto pid = fork();

    switch (pid)
    {
    case -1:
        utils::slog() << "Error on fork(): " << strerror(errno);
        exit(1);
    case 0:
        utils::slog() << "[Child] PID: " << getpid();

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

        {
            const char* cmdpath = "/bin/ls";
            const char* cmdname = "ls";

            if (execl(cmdpath, cmdname, 0) == -1)
            {
                utils::slog() << "[Child] Error on execl(): " << strerror(errno);
            }
        }

        break;
    default:
        utils::slog() << "[Parent] PID: " << getpid();

        std::thread([&]() {
                        log_subprocess_output(pid, stdout_fds, STDOUT_FILENO);
                    }).detach();

        std::thread([&]() {
                        log_subprocess_output(pid, stderr_fds, STDERR_FILENO);
                    }).detach();

        if (wait(0) == -1)
        {
            utils::slog() << "[Parent] Error on wait():" << strerror(errno);
        }

        utils::slog() << "[Parent] closing file descriptors.";

        close(stdout_fds[0]);
        close(stdout_fds[1]);

        close(stderr_fds[0]);
        close(stderr_fds[1]);

        break;
    }

    return 0;
}

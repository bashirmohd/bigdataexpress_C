
#include "utils.h"

#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>
#include <iomanip>
#include <map>

#include <sys/stat.h> // chmod
#include <cstring>
#include <unistd.h>

std::ostream * utils::service::console_ = &std::cout;

utils::slog::slog(int level) : ss ( )
{
}

utils::slog::~slog( )
{
    char buf[64];
    time_t now = time(NULL);
    strftime(buf, 64, "%Y-%m-%d %H:%M:%S", localtime(&now));

    //std::cout << "[" << buf << "] " << ss.str() << "\n";
    service::console() << "[" << buf << "] " << ss.str() << "\n";
    service::console().flush();
}

std::string utils::exec(std::string const & cmd)
{
    char buffer[128];
    std::string result = "";
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");

    while (!feof(pipe.get()))
    {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }

    return result;
}

std::string utils::guid()
{
    std::stringstream ss;
    ss << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << (std::rand() % 65536);
    ss << "-";
    ss << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << (std::rand() % 65536);
    ss << "-";
    ss << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << (std::rand() % 65536);
    ss << "-";
    ss << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << (std::rand() % 65536);

    return ss.str();
}

bool utils::is_private_ip(std::string const & ip)
{
    // private ip address range:
    // 192.168.0.0 -- 192.168.255.255
    // 172.16.0.0  -- 172.31.255.255
    // 10.0.0.0    -- 10.255.255.255

    // TODO: follow the above ip range
    return (ip.compare(0, 4, "192.") == 0)
        || (ip.compare(0, 4, "172.") == 0)
        || (ip.compare(0, 3, "10.") == 0)
        ;
}

std::string utils::version(void)
{
#if defined(GIT_COMMIT_HASH) && defined(GIT_BRANCH)
#define xstr(s) str(s)
#define str(s) #s
    return "commit " xstr(GIT_COMMIT_HASH) ", " \
        "branch " xstr(GIT_BRANCH) ", " \
        "built on " __DATE__ " at " __TIME__;
#undef xstr
#undef str
#else
    return "BDE 1.0";
#endif
}

std::size_t utils::string2size(std::string const &str)
{
    char unit[BUFSIZ]{0};
    size_t size = 0;

    sscanf(str.c_str(), "%zu%s", &size, unit);

    // Make a table of units.
    std::map<std::string, std::size_t> const units =
        {{ "KB",  1e3   },  // Kilobyte
         { "kB",  1e3   },
         { "K",   1e3   },
         { "k",   1e3   },
         { "MB",  1e6   },  // Megabyte
         { "M",   1e6   },
         { "GB",  1e9   },  // Gigabyte
         { "G",   1e9   },
         { "TB",  1e12  },  // Terabyte
         { "T",   1e12  },
         { "P",   1e15  },  // Petabyte
         { "PB",  1e15  },
         // exabyte (EB), zettabyte (ZB), and yottabyte (YB) will overflow.
         { "KiB", 1024  },
         { "MiB", 1024 * 1024 },
         { "GiB", 1024 * 1024 * 1024 }
         // PiB and above will overflow.
        };

    // Find size corresponding to given unit
    auto const it = units.find(unit);

    if (it == units.end())
    {
        if (unit[0] != '\0')
        {
            std::stringstream ss;
            ss << "Don't know how to handle unit \"" << unit << "\"";
            throw std::runtime_error(ss.str());
        }

        return size;
    }

    return size * it->second;
}

utils::CertSSH::CertSSH(std::string const & key, std::string const & cert)
{
    char buffer[L_tmpnam];
    int keyfd, certfd;

    snprintf(buffer, L_tmpnam, "%s/bde-key-XXXXXX", P_tmpdir);

    if ((keyfd = mkstemp(buffer)) < 0)
    {
        auto err = "Can't create a temporay key file, error: " +
            std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    keyfile = buffer;

    snprintf(buffer, L_tmpnam, "%s/bde-key-XXXXXX", P_tmpdir);

    if ((certfd = mkstemp(buffer)) < 0)
    {
        auto err = "Can't create a temporay cert file, error: " +
            std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    certfile = buffer;

    close(keyfd);
    close(certfd);

    std::ofstream os_key(keyfile);
    os_key << key << "\n";
    os_key.close();

    std::ofstream os_cert(certfile);
    os_cert << cert;
    os_cert.close();

    chmod( keyfile.c_str(), S_IRUSR | S_IWUSR);
    chmod(certfile.c_str(), S_IRUSR | S_IWUSR);

    setenv("X509_USER_CERT", certfile.c_str(), 1);
    setenv("X509_USER_KEY",   keyfile.c_str(), 1);
}

utils::CertSSH::~CertSSH()
{
    //std::remove( keyfile.c_str());
    //std::remove(certfile.c_str());
}

std::string utils::CertSSH::exec(std::string const & cmd)
{
    return utils::exec(cmd);
}

std::string utils::CertSSH::exec(std::string const & host, std::string const & cmd)
{
    std::string c = "gsissh ";
    c += host + " " + cmd;

    return utils::exec(c);
}


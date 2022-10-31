#include <cstring>
#include <stdexcept>

#include <arpa/inet.h>
#include <linux/if_vlan.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils.h"

// TODO: use libcap-ng, instead of just checking euid.
// #include <cap-ng.h>

#include "vlan.h"

static bool has_vlan_module()
{
    struct stat statbuf;

    const char *procfile = "/proc/net/vlan/config";

    if (stat(procfile, &statbuf) == 0)
    {
        return true;
    }

    utils::slog() << "Could not stat " << procfile << ". "
                  << "Perhaps we need to load 8021q kernel module?\n";

    return false;
}

static bool has_cap_net_admin()
{
    if (geteuid() == 0)
    {
        return true;
    }

    // if (capng_have_capability(CAPNG_EFFECTIVE, CAP_NET_ADMIN))
    // {
    //     return true;
    // }

    utils::slog() << "We don't have CAP_NET_ADMIN\n";

    return false;
}

static bool can_control_vlan()
{
    return has_vlan_module() and has_cap_net_admin();
}

std::string utils::add_vlan(std::string const &iface,
                            unsigned int id)
{
    if (not can_control_vlan())
    {
        throw std::runtime_error("Error: can't configure vlan");
    }

    auto sock = 0;
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
    {
        auto err = "socket() failed: " + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    vlan_ioctl_args args;

    // Step 1: set name type, so that virtual interface should look
    // like "eth0.1".
    memset(&args, 0, sizeof(args));
    args.cmd         = SET_VLAN_NAME_TYPE_CMD;
    args.u.name_type = VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD;

    if (ioctl(sock, SIOCSIFVLAN, &args) < 0)
    {
        close(sock);
        auto err = "ioctl() failed: " + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    // Step 2: actually create the virtual interface.
    memset(&args, 0, sizeof(args));
    strncpy(args.device1, iface.c_str(), sizeof(args.device1));
    args.cmd   = ADD_VLAN_CMD;
    args.u.VID = id;

    if (ioctl(sock, SIOCSIFVLAN, &args) < 0)
    {
        close(sock);
        auto err = "ioctl() failed: " + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    memset(&args, 0, sizeof(args));
    snprintf(args.device1, sizeof(args.device1), "%s.%d",
            iface.c_str(), id);
    args.cmd = GET_VLAN_VID_CMD;

    if (ioctl(sock, SIOCSIFVLAN, &args) < 0)
    {
        close(sock);
        auto err = "ioctl() failed: " + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    // Step 3: verify that "args.name1", which we inferred above, now
    // exists.
    if (args.u.VID == id)
    {
        args.cmd = GET_VLAN_REALDEV_NAME_CMD;
        if (ioctl(sock, SIOCSIFVLAN, &args) < 0)
        {
            close(sock);
            auto err = "ioctl() failed: " +
                std::string(strerror(errno));
            throw std::runtime_error(err);
        }
        // utils::slog() << "noerr: dev1: " << args.device1 << ", "
        //               << "dev2: " << args.u.device2 << "\n";
    }

    close(sock);

    return std::string(args.device1);
}

void utils::delete_vlan(std::string const &name)
{
    if (not can_control_vlan())
    {
        throw std::runtime_error("Error: can't control vlan");
    }

    auto sock = 0;
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
    {
        auto err = "socket() failed: " + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    vlan_ioctl_args args;
    memset(&args, 0, sizeof(args));
    strncpy(args.device1, name.c_str(), sizeof(args.device1));
    args.cmd = DEL_VLAN_CMD;

    if (ioctl(sock, SIOCSIFVLAN, &args) < 0)
    {
        close(sock);
        auto err = "ioctl() failed: " + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    close(sock);
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

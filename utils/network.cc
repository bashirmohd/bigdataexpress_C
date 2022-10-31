#include <cstring>
#include <stdexcept>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netdb.h>
#include <linux/if_vlan.h>
#include <linux/if_packet.h>

#include "network.h"
#include "utils/utils.h"

// ------------------------------------------------------------

//
// Debugging aid that formats `struct sockaddr` to a string.
//
static std::string addr2str(const struct sockaddr * const ifa_addr)
{
    if (!ifa_addr)
    {
        return "null";
    }

    // This should be large enough.
    char addr[INET6_ADDRSTRLEN];

    if (ifa_addr->sa_family == AF_INET)
    {
        struct sockaddr_in *in = (struct sockaddr_in*) ifa_addr;
        inet_ntop(AF_INET, &in->sin_addr, addr, sizeof(addr));
        return std::string("IPv4: ") + addr;
    }
    else if (ifa_addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *in6 = (struct sockaddr_in6*) ifa_addr;
        inet_ntop(AF_INET6, &in6->sin6_addr, addr, sizeof(addr));
        return std::string("IPv6: ") + addr;
    }
    else if (ifa_addr->sa_family == AF_PACKET)
    {
        struct sockaddr_ll *ll  = (struct sockaddr_ll*) ifa_addr;
        int                 len = 0;

        for (int i = 0; i < 6; i++)
        {
            len += sprintf(addr+len,
                           "%02X%s",
                           ll->sll_addr[i],
                           i < 5 ? ":" : "");
        }

        return std::string("packet: ") + addr;
    }

    return "unknown";
}

// ------------------------------------------------------------

void utils::NetworkAddress::set(const sockaddr * const addr)
{
    if (!addr)
    {
        throw std::runtime_error("null arg to utils::NetworkAddress()");
    }

    memset(&address_, 0, sizeof(sockaddr_storage));

    switch (addr->sa_family)
    {
    case AF_PACKET:
        memcpy(&address_, addr, sizeof(sockaddr_ll));
        break;
    case AF_INET:
        memcpy(&address_, addr, sizeof(sockaddr_in));
        break;
    case AF_INET6:
        memcpy(&address_, addr, sizeof(sockaddr_in6));
        break;
    default:
        // perhaps an error?
        break;
    }
}

// ------------------------------------------------------------

const std::string utils::NetworkAddress::family_string() const
{
    switch (family())
    {
        case AF_PACKET:
            return "AF_PACKET";
        case AF_INET:
            return "AF_INET";
        case AF_INET6:
            return "AF_INET6";
        default:
            return "UNKNOWN (" + std::to_string(family()) + ")";
    }
}

const std::string utils::NetworkAddress::address_string() const
{
    switch (family())
    {
        case AF_INET:
            return format_ipv4_address();
        case AF_INET6:
            return format_ipv6_address();
        case AF_PACKET:
            return format_mac_address();
        default:
            return "UNKNOWN";
    }
}

const std::string utils::NetworkAddress::format_ipv4_address() const
{
    char str[INET_ADDRSTRLEN];
    auto sin = reinterpret_cast<const sockaddr_in *>(&address_);

    if (inet_ntop(AF_INET,
                  &sin->sin_addr,
                  str,
                  INET_ADDRSTRLEN) == nullptr)
    {
        return std::string("UNKNOWN");
    }

    return std::string(str);
}

// TODO: this doesn't seem to get the right address.
const std::string utils::NetworkAddress::format_ipv6_address() const
{
    char str[INET6_ADDRSTRLEN];
    auto sin = reinterpret_cast<const sockaddr_in6 *>(&address_);

    if (inet_ntop(AF_INET6,
                  &sin->sin6_addr,
                  str,
                  INET6_ADDRSTRLEN) == nullptr)
    {
        return std::string("UNKNOWN");
    }

    return std::string(str);
}

// TODO: this does NOT give the right address.
const std::string utils::NetworkAddress::format_mac_address() const
{
    auto ptr = reinterpret_cast<const ether_addr *>(&address_);

    char buffer[32];
    memset(buffer, 0, 32);

    ether_ntoa_zero_pad(ptr, buffer);

    return buffer;
}

std::set<std::string> utils::get_interface_names()
{
    ifaddrs *ifaddr = nullptr;

    if (getifaddrs(&ifaddr) == -1)
    {
        throw std::runtime_error("getifaddrs() failed");
    }

    std::set<std::string> results;

    for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_name != nullptr)
        {
            results.emplace(std::string(ifa->ifa_name));
        }
    }

    freeifaddrs(ifaddr);

    return results;
}

std::set<utils::NetworkInterface> utils::get_interfaces()
{
    ifaddrs *ifaddr = nullptr;

    if (getifaddrs(&ifaddr) == -1)
    {
        // TODO: better error reporting.
        throw std::runtime_error("getifaddrs() failed");
    }

    std::set<utils::NetworkInterface> results;

    for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
        {
            continue;
        }

        std::string ifname(ifa->ifa_name);

        auto key = results.find(ifname);
        utils::NetworkAddress addr(ifa->ifa_addr);

        if (key == end(results))
        {
            utils::NetworkInterface iface(ifname);
            iface.add_address(addr);
            results.emplace(iface);
        }
        else
        {
            const_cast<utils::NetworkInterface&>(*key).add_address(addr);
        }
    }

    freeifaddrs(ifaddr);

    return results;
}

std::string utils::get_first_ipv4_address(std::string const &iface)
{
    for (auto && i : get_interfaces())
    {
        if (i.name() == iface)
        {
            for (auto && a : i.addresses())
            {
                if (a.family() == AF_INET)
                {
                    return a.address_string();
                }
            }
        }
    }

    auto err = "interface " + iface + " or ipv4 address not found";
    throw std::runtime_error(err);
}

std::string utils::get_first_ipv6_address(std::string const &iface)
{
    for (auto && i : get_interfaces())
    {
        if (i.name() == iface)
        {
            for (auto && a : i.addresses())
            {
                if (a.family() == AF_INET6)
                {
                    return a.address_string();
                }
            }
        }
    }

    auto err = "interface " + iface + " or ipv4 address not found";
    throw std::runtime_error(err);
}

std::string utils::get_iface_mac_address(std::string const & iface)
{
    if (not iface_up_and_running(iface))
    {
        auto err = "Invalid interface, or the given interface is not running";
        throw std::runtime_error(err);
    }

    struct ifreq ifr;
    strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ);

    auto s = socket(AF_INET, SOCK_DGRAM, 0);

    if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0)
    {
        auto addr =
            reinterpret_cast<const ether_addr*>(ifr.ifr_hwaddr.sa_data);

        char addrbuf[BUFSIZ];
        memset(addrbuf, 0, BUFSIZ);

        ether_ntoa_zero_pad(addr, addrbuf);
        auto mac = std::string(addrbuf);

        close(s);

        if (mac.empty())
        {
            throw std::runtime_error("error getting the mac address");
        }

        static bool warned = false;

        if (mac == "00:00:00:00:00:00" and warned == false)
        {
            utils::slog(s_warning) << "[Network] Returning MAC address "
                                   << mac << " for interface " << iface << ".";
            warned = true;
        }

        return mac;
    }

    throw std::runtime_error("error at ioctl");
}


std::string utils::get_first_mac_address()
{
    for (auto const & ifname : get_interface_names())
    {
        if ((not iface_up_and_running(ifname)) or (ifname == "lo"))
        {
            continue;
        }

        struct ifreq ifr;
        strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ);

        auto s = socket(AF_INET, SOCK_DGRAM, 0);

        if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0)
        {
            auto addr =
                reinterpret_cast<const ether_addr*>(ifr.ifr_hwaddr.sa_data);

            char addrbuf[BUFSIZ];
            memset(addrbuf, 0, BUFSIZ);

            ether_ntoa_zero_pad(addr, addrbuf);
            auto mac = std::string(addrbuf);

            close(s);

            if (mac.empty() or mac == "00:00:00:00:00:00")
            {
                continue;
            }

            return mac;
        }

        close(s);
    }

    auto err = "Could not find an interface with a MAC address";
    throw std::runtime_error(err);
}

std::string utils::get_first_network_iface()
{
    for (auto const & ifname : utils::get_interface_names())
    {
        if ((not utils::iface_up_and_running(ifname)) or (ifname == "lo"))
        {
            continue;
        }

        return ifname;
    }

    return "lo";
}

int utils::get_vlan_id(std::string const &iface)
{
    int sock   = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    int result = -1;

    if (sock == -1)
    {
        auto msg = "sock() failed: " + std::string(strerror(errno));
        utils::slog() << "[utils/network] Error: " << msg;
        return result;
    }

    // VLAN related data structures are in <linux/if_vlan.h>.  ioctl()
    // commands are defined in sockios.h.

    // See if the interface is a VLAN.
    vlan_ioctl_args args{0};

    args.cmd = GET_VLAN_REALDEV_NAME_CMD;
    strncpy(args.device1, iface.c_str(), sizeof(args.device1));

    if (ioctl(sock, SIOCGIFVLAN, &args) != -1)
    {
        // Given interface is a VLAN device; get its VLAN ID.
        memset(&args, 0, sizeof(vlan_ioctl_args));
        args.cmd = GET_VLAN_VID_CMD;
        strncpy(args.device1, iface.c_str(), sizeof(args.device1));

        if (ioctl(sock, SIOCGIFVLAN, &args) == -1)
        {
            auto msg = "GET_VLAN_VID_CMD on " + iface + " failed: "
                + std::string(strerror(errno));
            utils::slog() << "[utils/network] " << msg;
        }
        else
        {
            result = args.u.VID;
        }
    }
    else if (false)
    {
        auto msg = "GET_VLAN_REALDEV_NAME_CMD on " + iface + " failed: " +
            std::string(strerror(errno)) + " (not a VLAN device?)";
        utils::slog() << "[utils/network] " << msg;
    }

    close(sock);

    return result;
}

bool utils::iface_up_and_running(std::string const &iface)
{
    ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface.c_str(), sizeof(ifr.ifr_name));

    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    bool result = false;

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1)
    {
        auto msg = "ioctl() failed: " + std::string(strerror(errno));
        throw std::runtime_error(msg);
    }
    else
    {
        result = (ifr.ifr_flags & IFF_UP) == IFF_UP and
            (ifr.ifr_flags & IFF_RUNNING) == IFF_RUNNING;
    }

    close(sock);

    return result;
}

size_t utils::get_link_speed(std::string const &iface)
{
    ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface.c_str(), sizeof(ifr.ifr_name));

    ethtool_cmd ethdata;
    memset(&ethdata, 0, sizeof(ethdata));

    ethdata.cmd  = ETHTOOL_GSET;
    ifr.ifr_data = reinterpret_cast<char *>(&ethdata);

    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock <= 0)
    {
        throw std::runtime_error("socket() failed: " +
                                 std::string(strerror(errno)));
    }

    auto speed = SPEED_UNKNOWN;

    if (ioctl(sock, SIOCETHTOOL, &ifr) >= 0)
    {
        speed = ethtool_cmd_speed(&ethdata);
    }

    close(sock);

    if (speed == SPEED_UNKNOWN)
    {
        speed = 0;
    }

    return speed;
}

// SPEED_XXXXX values are from linux/ethtool.h, which usually comes
// from Linux kernel headers (kernel-headers package in Scientific
// Linux) or glibc (Debian and friends).  Except for a handful of
// basic SPEED_XXXXX constants, what's defined in ethtool.h are also
// wildly inconsistent, depending on what we have installed, with no
// version information.
//
// Using a private copy of these constants, rather than littering our
// code with a bunch of "#if defined(SPEED_XXXX)" appears to be a
// reasonable idea.
enum LinkSpeed {
    _SPEED_10      = 10,
    _SPEED_100     = 100,
    _SPEED_1000    = 1000,
    _SPEED_2500    = 2500,
    _SPEED_5000    = 5000,
    _SPEED_10000   = 10000,
    _SPEED_20000   = 20000,
    _SPEED_25000   = 25000,
    _SPEED_40000   = 40000,
    _SPEED_50000   = 50000,
    _SPEED_56000   = 56000,
    _SPEED_100000  = 100000,
    _SPEED_UNKNOWN = -1
};

std::string utils::get_link_speed_string(std::string const &iface)
{
    auto speed = get_link_speed(iface);

    switch (speed)
    {
    case _SPEED_10:
        return "10Mbps";
    case _SPEED_100:
        return "100Mbps";
    case _SPEED_1000:
        return "1Gbps";
    case _SPEED_2500:
        return "2.5Gbps";
    case _SPEED_5000:
        return "5Gbps";
    case _SPEED_10000:
        return "10Gbps";
    case _SPEED_20000:
        return "20Gbps";
    case _SPEED_25000:
        return "25Gbps";
    case _SPEED_40000:
        return "40Gbps";
    case _SPEED_50000:
        return "50Gbps";
    case _SPEED_56000:
        return "56Gbps";
    case _SPEED_100000:
        return "100Gbps";
    case _SPEED_UNKNOWN:
    case 0:
        return "UNKNOWN";
    default:
        return std::to_string(speed);
    }
}

rtnl_link_stats utils::get_link_stats(std::string const &iface)
{
    ifaddrs *ifaddr;

    if (getifaddrs(&ifaddr) == -1)
    {
        auto error = "getifaddrs() failed on interface " + iface + ", " +
            "reason: " + std::string(strerror(errno));
        throw std::runtime_error(error);
    }

    for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
        {
            continue;
        }

        if (std::string(ifa->ifa_name) == iface)
        {
            if (ifa->ifa_data == nullptr)
            {
                throw std::runtime_error("No data on interface " + iface);
            }

            auto * stats  = static_cast<rtnl_link_stats *>(ifa->ifa_data);

            rtnl_link_stats result;
            memcpy(&result, stats, sizeof(result));

            freeifaddrs(ifaddr);
            return result;
        }
    }

    freeifaddrs(ifaddr);
    throw std::runtime_error("Could not find interface " + iface);
}

size_t utils::get_rx_packets(std::string const &iface)
{
    return get_link_stats(iface).rx_packets;
}

size_t utils::get_tx_packets(std::string const &iface)
{
    return get_link_stats(iface).tx_packets;
}

size_t utils::get_rx_bytes(std::string const &iface)
{
    return get_link_stats(iface).rx_bytes;
}

size_t utils::get_tx_bytes(std::string const &iface)
{
    return get_link_stats(iface).tx_bytes;
}

size_t utils::get_tx_errors(std::string const &iface)
{
    return get_link_stats(iface).tx_errors;
}

size_t utils::get_rx_errors(std::string const &iface)
{
    return get_link_stats(iface).rx_errors;
}

size_t utils::get_tx_dropped(std::string const &iface)
{
    return get_link_stats(iface).tx_dropped;
}

size_t utils::get_rx_dropped(std::string const &iface)
{
    return get_link_stats(iface).rx_dropped;
}

char *utils::ether_ntoa_zero_pad(const struct ether_addr *addr,
                                 char *buffer)
{
    sprintf(buffer, "%02x:%02x:%02x:%02x:%02x:%02x",
            addr->ether_addr_octet[0],
            addr->ether_addr_octet[1],
            addr->ether_addr_octet[2],
            addr->ether_addr_octet[3],
            addr->ether_addr_octet[4],
            addr->ether_addr_octet[5]);

    return buffer;
};


// ------------------------------------------------------------

// TODO: replace ioctl() commands with netlink messages.

static void mac_aton(const char * const addr, unsigned char *ptr)
{
    if (addr == nullptr or ptr == nullptr)
    {
        auto message = "invalid argument (null addr or null ptr)";
        utils::slog() << "[add_arp] " << message;
        throw std::runtime_error(message);
    }

    int v[6]{0};

    if (sscanf(addr, "%x:%x:%x:%x:%x:%x",
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
    {
        auto message = std::string("invalid ethernet address ") + addr;
        utils::slog() << "[add_arp] " << message;
        throw std::runtime_error(message);
    }

    for (auto i = 0; i < 6; i++)
    {
        ptr[i] = v[i];
    }
}

void utils::add_arp(std::string const &ip, std::string const &mac,
                    std::string const &device)
{
    auto sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0)
    {
        auto message = std::string("socket() error: ") + strerror(errno);
        utils::slog() << "[add_arp] " << message;
        throw std::runtime_error(message);
    }

    // See net/if_arp.h for 'arpreq' structure.
    arpreq req{0};

    // protocol address -- TODO: use C++ style cast.
    sockaddr_in *sin = (sockaddr_in *) &req.arp_pa;
    sin->sin_family  = AF_INET;

    // TODO: Use inet_aton()/inet_pton()/getaddrinfo()
    sin->sin_addr.s_addr    = inet_addr(ip.c_str());

    if (sin->sin_addr.s_addr == -1)
    {
        hostent *hp{nullptr};

        if ((hp = gethostbyname(ip.c_str())) == nullptr)
        {
            auto message = "Can't resolve " + ip;
            utils::slog() << "[add_arp] " << message;
            throw std::runtime_error(message);
        }

        bcopy(hp->h_addr, &sin->sin_addr, sizeof(sin->sin_addr));
    }

    // Set hardware address.
    mac_aton(mac.c_str(), (unsigned char *) req.arp_ha.sa_data);

    // Set flags.
    req.arp_flags = ATF_COM | ATF_PERM;

    // Set device.  TODO: avoid device name assumption.
    const char *dev = device.empty() ? "" : device.c_str();
    utils::slog() << "[add_arp] Using interface: "  << dev;
    strncpy(req.arp_dev, dev, sizeof(req.arp_dev));

    if (ioctl(sock, SIOCSARP, &req) < 0)
    {
        auto err = std::string("ioctl() error: ") + strerror(errno);
        throw std::runtime_error(err);
    }

    utils::slog() << "[add_arp] Added entry {ip: " << ip
                  << ", mac: " << mac
                  << ", device: " << dev
                  << " flags: ATF_COM|ATF_PERM}";

    return;
}

void utils::delete_arp(std::string const &ip)
{
    auto sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0)
    {
        auto message = std::string("socket() error: ") + strerror(errno);
        utils::slog() << "[delete_arp] " << message;
        throw std::runtime_error(message);
    }

    arpreq req{0};

    // protocol address
    sockaddr_in *sin     = (sockaddr_in *) &req.arp_pa;
    sin->sin_family      = AF_INET;
    sin->sin_addr.s_addr = inet_addr(ip.c_str());

    if (sin->sin_addr.s_addr == -1)
    {
        hostent *hp{nullptr};

        if ((hp = gethostbyname(ip.c_str())) == nullptr)
        {
            auto message = "Can't resolve " + ip;
            utils::slog() << "[delete_arp] " << message;
            throw std::runtime_error(message);
        }

        bcopy(hp->h_addr, &sin->sin_addr, sizeof(sin->sin_addr));
    }

    if (ioctl(sock, SIOCDARP, &req) < 0)
    {
        auto message = std::string("ioctl() error: ") + strerror(errno);
        utils::slog() << "[delete_arp] " << message;
        throw std::runtime_error(message);
    }

    return;
}

// ------------------------------------------------------------

void utils::add_route(std::string const &dest,
                      std::string const &via,
                      std::string const &mask,
                      std::string const &device)
{
    utils::slog() << "[add_route] Adding route {dest: " << dest
                  << ", via: " << via
                  << ", mask: " << mask
                  << ", device: " << device << "}";

    auto sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0)
    {
        auto message = std::string("socket() error: ") + strerror(errno);
        utils::slog() << "[add_route] " << message;
        throw std::runtime_error(message);
    }

    // See net/route.h for `struct rtentry`.
    rtentry entry{0};
    sockaddr_in *addr{nullptr};

    addr                  = (sockaddr_in*) &entry.rt_dst;
    addr->sin_family      = AF_INET;
    addr->sin_addr.s_addr = inet_addr(dest.c_str());

    // See net/route.h for flags.
    entry.rt_flags = RTF_UP | RTF_HOST;

    if (not via.empty())
    {
        addr                  = (sockaddr_in*) &entry.rt_gateway;
        addr->sin_family      = AF_INET;
        addr->sin_addr.s_addr = inet_addr(via.c_str());
        entry.rt_flags        |= RTF_GATEWAY;
    }

    if (not mask.empty())
    {
        addr                  = (sockaddr_in*) &entry.rt_genmask;
        addr->sin_family      = AF_INET;
        addr->sin_addr.s_addr = inet_addr(mask.c_str());
    }

    if (not device.empty())
    {
        entry.rt_dev = const_cast<char *>(device.c_str());
    }

    if (ioctl(sock, SIOCADDRT, &entry) < 0)
    {
        auto err = std::string("ioctl() error: ") + strerror(errno);

        utils::slog() << "[add_route] Error adding route {dest: " << dest
                      << ", via: " << via
                      << ", mask: " << mask
                      << ", device: " << entry.rt_dev
                      << "}; error: " << err;

        throw std::runtime_error(err);
    }

    utils::slog() << "[add_route] Added entry {dest: " << dest
                  << ", via: " << via
                  << ", mask: " << mask
                  << ", device: " << entry.rt_dev << "}";

    return;
}


void utils::delete_route(std::string const &dest,
                         std::string const &via,
                         std::string const &device)
{
    auto sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0)
    {
        auto message = std::string("socket() error: ") + strerror(errno);
        utils::slog() << "[delete_route] " << message;
        throw std::runtime_error(message);
    }

    // See net/route.h for `struct rtentry`.
    rtentry entry{0};
    sockaddr_in *addr{nullptr};

    addr                  = (sockaddr_in*) &entry.rt_dst;
    addr->sin_family      = AF_INET;
    addr->sin_addr.s_addr = inet_addr(dest.c_str());

    // See net/route.h for flags.
    entry.rt_flags = RTF_UP | RTF_HOST;

    if (not via.empty())
    {
        addr                  = (sockaddr_in*) &entry.rt_gateway;
        addr->sin_family      = AF_INET;
        addr->sin_addr.s_addr = inet_addr(via.c_str());
        entry.rt_flags        |= RTF_GATEWAY;
    }

    if (not device.empty())
    {
        entry.rt_dev = const_cast<char *>(device.c_str());
    }

    if (ioctl(sock, SIOCDELRT, &entry) < 0)
    {
        auto message = std::string("ioctl() error: ") + strerror(errno);
        utils::slog() << "[delete_route] " << message;
        throw std::runtime_error(message);
    }

    return;
}


std::string utils::host_to_ip(std::string const & host)
{
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *h;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host.c_str(), NULL, &hints , &servinfo)) != 0)
    {
        std::stringstream ss;
        ss << "getaddrinfo: " << gai_strerror(rv);

        utils::slog(s_error) << ss.str();
        throw std::runtime_error(ss.str());
    }

    std::string ip;

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        h = (struct sockaddr_in *) p->ai_addr;
        ip = inet_ntoa(h->sin_addr);
    }

    // all done with this structure
    freeaddrinfo(servinfo);

    return ip;
}

// ------------------------------------------------------------
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

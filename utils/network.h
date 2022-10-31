//
//  Helpers to query network interfaces.
//

#ifndef BDE_UTILS_NETWORK_H
#define BDE_UTILS_NETWORK_H

#include <set>
#include <vector>
#include <string>

#include <netinet/in.h>
#include <linux/if_link.h>

// ----------------------------------------------------------------------

namespace utils
{
    // This class is a wrapper around `struct sockaddr_storage`.  It
    // knows of AF_PACKET, AF_INET, and AF_INET6 addresses, for now.
    //
    // We're using `struct sockaddr_storage` since `struct sockaddr`
    // is not large enough to hold IPv6 addresses.  Turned out that
    // `struct sockaddr` is large enough to hold `struct sockaddr_in`,
    // but it is not large enough for `struct sockaddr_in6`.
    //
    // "NetworkAddress" objects are created by get_interfaces()
    // function to report "NetworkInterface" addresses; this class is
    // not designed for direct consumption.
    class NetworkAddress
    {
    public:
        NetworkAddress(const struct sockaddr * const addr) {
            set(addr);
        }

        short family() const { return address_.ss_family; }

        const struct sockaddr &address() const {
            // XXX: risky type cast and pointer indirection here.
            // There must be a better way?
            return *reinterpret_cast<const struct sockaddr *>(&address_);
        }

        const std::string family_string() const;
        const std::string address_string() const;

    private:
        void set(const sockaddr * const addr);

        const std::string format_ipv4_address() const;
        const std::string format_ipv6_address() const;
        const std::string format_mac_address() const;

    private:
        struct sockaddr_storage address_;
    };

    // "NetworkInterface" objects are created by get_interfaces()
    // function; like "NetworkAddress" this class is not designed for
    // direct consumption either.
    class NetworkInterface
    {
    public:
        NetworkInterface(std::string const &name)
            : name_(name) {}

        const std::string name() const { return name_; }
        const std::vector<NetworkAddress> addresses() const {
            return address_;
        }

        bool operator<(NetworkInterface const &rhs) const {
            return name_ < rhs.name();
        }

    private:
        friend std::set<NetworkInterface> get_interfaces();

        void add_address(NetworkAddress const &addr) {
            address_.emplace_back(addr);
        }

    private:
        std::string                 name_;
        std::vector<NetworkAddress> address_;
    };

    std::set<std::string> get_interface_names();
    std::set<NetworkInterface> get_interfaces();

    // The below functions are used in DTN Agent; they are not meant
    // to be generally useful.
    std::string get_first_ipv4_address(std::string const &iface);
    std::string get_first_ipv6_address(std::string const &iface);
    std::string get_first_mac_address();
    std::string get_first_network_iface();
    std::string get_iface_mac_address(std::string const &iface);

    // Get 802.1Q VLAN ID of the given interface.
    int get_vlan_id(std::string const &iface);

    bool   iface_up_and_running(std::string const &iface);

    size_t get_link_speed(std::string const &iface);
    std::string get_link_speed_string(std::string const &iface);

    rtnl_link_stats get_link_stats(std::string const &iface);

    size_t get_rx_packets(std::string const &iface);
    size_t get_tx_packets(std::string const &iface);

    size_t get_rx_bytes(std::string const &iface);
    size_t get_tx_bytes(std::string const &iface);

    size_t get_rx_errors(std::string const &iface);
    size_t get_tx_errors(std::string const &iface);

    size_t get_rx_dropped(std::string const &iface);
    size_t get_tx_dropped(std::string const &iface);

    // Both ether_ntoa() and ether_ntoa_r() drops leading "0" from a
    // MAC address; this is a workaround.
    char *ether_ntoa_zero_pad(const struct ether_addr *addr,
                              char *buffer);

    void add_arp(std::string const &ip,
                 std::string const &mac,
                 std::string const &device = std::string());
    void delete_arp(std::string const &ip);

    void add_route(std::string const &dest,
                   std::string const &via,
                   std::string const &mask,
                   std::string const &device);
    void delete_route(std::string const &dest,
                      std::string const &via,
                      std::string const &device);

    // get ip address from hostname
    std::string host_to_ip(std::string const & host);
};

#endif // BDE_UTILS_NETWORK_H

// ----------------------------------------------------------------------
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

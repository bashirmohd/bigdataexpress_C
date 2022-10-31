#ifndef BDE_UTILS_VLAN_H
#define BDE_UTILS_VLAN_H

#include <string>

namespace utils
{
    // Given an network interface (such as "eth0"), and a Vlan ID,
    // create a virtual interface, and return the newly created
    // virtual interface's name.
    //
    // Note that virtual interfacs name may be of four different
    // formats: "eth0.1", "eth0.0001", "vlan1", "vlan0001", or
    // "vlan1".  A sysadmin can change the name type with the command:
    //
    //   "vconfig set_name_type <name_type>".
    //
    // We use the "eth0.1" format, by using SET_VLAN_NAME_TYPE_CMD.
    //
    // To get the interface name type, we must parse
    // /proc/net/vlan/config; but meh, that is too much work.
    std::string add_vlan(std::string const & interface,
                         unsigned int id);

    // Delete the virtual interface specified by "ifname".
    void delete_vlan(std::string const & ifname);
};

#endif // BDE_UTILS_VLAN_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

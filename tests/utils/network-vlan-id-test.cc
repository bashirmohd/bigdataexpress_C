#include <iostream>
#include <iomanip>
#include <utils/network.h>

int main(int argc, char **argv)
{
    std::cout << std::left
              << std::setw(16)
              << "iface"
              << std::setw(16)
              << "vlan id"
              << std::endl;

    std::cout << std::left
              << std::setw(16)
              << "-----"
              << std::setw(16)
              << "-------"
              << std::endl;

    for (auto const &iface : utils::get_interface_names())
    {
        auto id = utils::get_vlan_id(iface);

        std::cout << std::left
                  << std::setw(16)
                  << iface
                  << std::setw(16)
                  << id
                  << std::endl;
    }
}


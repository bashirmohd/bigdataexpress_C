#include <iostream>
#include <utils/network.h>

// TODO: use Catch; test for exceptions and the such.

static void call_get_interface_names()
{
    std::cout << "network interfaces in this machine:\n";
    for (auto iface : utils::get_interface_names())
    {
        std::cout << "   " << iface << "\n";
    }
    std::cout << "\n";
}

static void call_get_interfaces()
{
    for (auto iface : utils::get_interfaces())
    {
        std::cout << "name: " << iface.name() << "\n";
        for (auto addr : iface.addresses())
        {
            std::cout << "   "
                      << "family: " << addr.family_string() << ", "
                      << "address: " << addr.address_string() << "\n";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

static void call_stats()
{
    for (auto iface : utils::get_interface_names())
    {
        try
        {
            std::cout << iface << " stats: \n"
                      << "rx packets : "
                      << utils::get_rx_packets(iface) << "\n"
                      << "tx packets : "
                      << utils::get_tx_packets(iface) << "\n"
                      << "rx bytes   : "
                      << utils::get_rx_bytes(iface) << "\n"
                      << "tx bytes   : "
                      << utils::get_tx_bytes(iface) << "\n"
                      << "tx errors  : "
                      << utils::get_tx_errors(iface) << "\n"
                      << "rx errors  : "
                      << utils::get_rx_errors(iface) << "\n"
                      << "tx dropped : "
                      << utils::get_tx_dropped(iface) << "\n"
                      << "tx dropped : "
                      << utils::get_rx_dropped(iface) << "\n\n";
        }
        catch (std::exception const &ex)
        {
            std::cerr << "Error: " << ex.what() << "\n\n";
        }
    }
}

static void call_up_down_status()
{
    for (auto iface : utils::get_interface_names())
    {
        std::cout << iface << " is up and running: "
                  << (utils::iface_up_and_running(iface) ? "yes" : "no")
                  << "\n";
    }

    std::cout << "\n";
}

static void call_link_speed()
{
    for (auto iface : utils::get_interface_names())
    {
        std::cout << "Speed of " << iface << ": "
                  << utils::get_link_speed_string(iface) << "\n";
    }

    std::cout << "\n";
}

int main(int argc, char **argv)
{
    call_get_interface_names();
    call_get_interfaces();
    call_stats();

    call_up_down_status();
    call_link_speed();

    return 0;
}

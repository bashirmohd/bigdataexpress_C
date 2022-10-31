#include <iostream>
#include "utils/process.h"

void test_process(std::string const &cmd)
{
    std::cout << "------------------------------------\n"
              << "Running command: " << cmd << "\n"
              << "------------------------------------\n";

    auto result = utils::run_process(cmd, true);
    std::cout << "result code   : " << result.code << "\n"
              << "result text   : " << result.text << "\n\n";
}

int main(int argc, char **argv)
{
    test_process("ls -l /");

    try
    {
        test_process("tc -l /");
    }
    catch (std::exception &ex)
    {
        std::cout << "Caught exception: " << ex.what() << "\n";
    }

    return 0;
}

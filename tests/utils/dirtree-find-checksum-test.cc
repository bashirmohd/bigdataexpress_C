#include <iostream>

#include <utils/paths/dirtree.h>
#include <utils/checksum.h>

int main(int argc, char *argv[])
{
    const char * dir = ".";

    if (argc >= 2)
    {
        dir = argv[1];
    }

    for (auto const &group : utils::DirectoryTree(dir).divide(0))
    {
        for (auto const &p : group)
        {
            auto const &name = p.name();
            try
            {
                if (p.is_regular_file())
                {
                    auto const md = utils::checksum(name, EVP_sha1());
                    std::cout << name << "\t" << md << "\n";
                }
            }
            catch (std::exception const &ex)
            {
                std::cout << "Error with " << name << ": " << ex.what();
            }
        }
    }

    return 0;
}

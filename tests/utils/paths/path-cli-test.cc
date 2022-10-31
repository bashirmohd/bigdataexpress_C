//
// CLI test to check that utils::Path class works as expected.
//

#include <iostream>
#include <utils/paths/path.h>

static std::string quote(std::string const &s)
{
    return "\"" + s + "\"";
}

static void print_path_info(std::string const &path)
{
    utils::Path p(path);

    std::cout << " - " << quote(path) << "";
    if (p.is_regular_file())
        std::cout << "is a regular file.\n";
    else if (p.is_directory())
        std::cout << "is a directory.\n";
    else if (p.is_symbolic_link())
        std::cout << " is a symbolic link.\n";

    std::cout << " - " << quote(path) << " has: \n"
              << " \t canonical_name(): " << p.canonical_name() << "\n"
              << " \t base_name(): " << p.base_name() << "\n"
              << " \t dir_name(): " << p.dir_name() << "\n";
}

static void print_expanded_path_info(std::string const &path)
{
    utils::Path p(path);
    auto newpath = p.canonical_name();

    utils::Path p2(newpath);

    std::cout << " - \"" << newpath << "\" has: \n"
              << " \t canonical_name(): " << p2.canonical_name() << "\n"
              << " \t base_name(): " << p2.base_name() << "\n"
              << " \t dir_name(): " << p2.dir_name() << "\n";
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            print_path_info(argv[i]);
            print_expanded_path_info(argv[i]);
        }
    }
    else
    {
        print_path_info(".");
        print_expanded_path_info(".");
    }

    return 0;
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

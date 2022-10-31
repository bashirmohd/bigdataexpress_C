#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>

#include <libgen.h>

#include "paths/path.h"
#include "grid-mapfile.h"
#include "utils.h"

// ----------------------------------------------------------------------

static const std::string BEGIN_MARKER     = "## BEGIN-BDE-SECTION";
static const std::string END_MARKER       = "## END-BDE-SECTION";
static const std::string WHITESPACE_CHARS = " \t\n";
static const std::string QUOTING_CHARS    = "\"";
static const std::string ESCAPING_CHARS   = "\\";
static const std::string COMMENT_CHARS    = "#";

// Characters seperating user ids in the gridmap file
static const std::string USERID_SEP_CHARS = ",";

// ----------------------------------------------------------------------

static utils::Path get_grid_mapfile()
{
    auto env     = getenv("GRIDMAP");
    auto mapfile = env ? env : "/etc/grid-security/grid-mapfile";

    return utils::Path(mapfile);
}

static std::string get_grid_mapfile_save_file()
{
    auto dir = utils::Path(get_grid_mapfile().dir_name());
    return dir.join("/grid-mapfile.save.bde");
}

static std::string get_grid_mapfile_working_file()
{
    auto dir = utils::Path(get_grid_mapfile().dir_name());
    return dir.join("/grid-mapfile.save.bde.work");
}

// ----------------------------------------------------------------------

static std::string& trim_left(std::string &str)
{
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(), [](int ch) {
                      return !std::isspace(ch);
                  }));

    return str;
}

static std::string& trim_right(std::string &str)
{
    str.erase(std::find_if(str.rbegin(), str.rend(), [](int ch) {
                return !std::isspace(ch);
            }).base(), str.end());

    return str;
}

static std::string& trim(std::string &str)
{
    return trim_right(trim_left(str));
}

static bool has_quoted_dn(std::string const &line)
{
    return (line.compare(0, QUOTING_CHARS.length(), QUOTING_CHARS) == 0);
}

static bool is_comment_line(std::string const &line)
{
    return (line.compare(0, COMMENT_CHARS.length(), COMMENT_CHARS) == 0);
}

static bool is_begin_marker(std::string const &line)
{
    return (line.compare(0, BEGIN_MARKER.length(), BEGIN_MARKER) == 0);
}

static bool is_end_marker(std::string const &line)
{
    return (line.compare(0, END_MARKER.length(), END_MARKER) == 0);
}

static std::string quote(std::string const &str)
{
    return QUOTING_CHARS + str + QUOTING_CHARS;
}

static std::vector<std::string> split_string(std::string const &str,
                                             std::string const &delim)
{
    std::stringstream ss(str);
    std::string       line;
    std::vector<std::string> result{};

    while (std::getline(ss, line))
    {
        size_t prev = 0;
        size_t pos  = 0;

        while ((pos = line.find_first_of(delim, prev)) != std::string::npos)
        {
            if (pos > prev)
            {
                result.push_back(line.substr(prev, pos - prev));
            }
            prev = pos + 1;
        }

        if (prev < line.length())
        {
            result.push_back(line.substr(prev, std::string::npos));
        }
    }

    return result;
}

// ----------------------------------------------------------------------

utils::grid_map utils::grid_mapfile_get_entries()
{
    auto path = get_grid_mapfile();
    auto name = path.canonical_name();

    if (not path.exists())
    {
        throw std::runtime_error(name + " does not exist");
    }

    std::ifstream instream(name);

    // utils::gridmap_entries entries{};
    bool system = true;
    utils::gridmap_entries system_entries{};
    utils::gridmap_entries bde_entries{};

    std::string line;

    while (std::getline(instream, line))
    {
        // trim leading and trailing whitepaces.
        trim(line);

        if (is_begin_marker(line))
        {
            system = false;
        }

        if (is_end_marker(line))
        {
            system = true;
        }

        if (line.empty() or is_comment_line(line))
        {
            continue;
        }

        if (has_quoted_dn(line))
        {
            // DN begins with QUOTING_CHARS; split by QUOTING_CHARS.
            auto v = split_string(line, QUOTING_CHARS);

            if (v.size() == 2)
            {
                auto pair = std::make_pair(v.at(0), trim(v.at(1)));
                if (system)
                {
                    system_entries.emplace(pair);
                }
                else
                {
                    bde_entries.emplace(pair);
                }
            }
            else
            {
                auto err = "malformed line: \"" + line + "\"";
                utils::slog() << "[read gridmap quoted] " << err;
                throw std::runtime_error(err);
            }
        }
        else
        {
            auto v = split_string(line, WHITESPACE_CHARS);

            if (v.size() == 2)
            {
                auto pair = std::make_pair(v.at(1), trim(v.at(2)));
                if (system)
                {
                    system_entries.emplace(pair);
                }
                else
                {
                    bde_entries.emplace(pair);
                }
            }
            else
            {
                auto err = "malformed line: \"" + line + "\"";
                utils::slog() << "[read gridmap unquoted] " << err;
                throw std::runtime_error(err);
            }
        }
    }

    return {name, system_entries, bde_entries};
}

// ----------------------------------------------------------------------

void utils::grid_mapfile_push_entries(gridmap_entries const &entries)
{
    auto const p = get_grid_mapfile();

    // Check that gridmap file exists, and we can write to it.
    if (not p.exists())
    {
        // Create a new gridmap file when none found.
        utils::slog() << "[grid mapfile push] "
                      << p.name() << " does not exist; will try to create.";

        std::ofstream ostream(p.name());
        ostream << BEGIN_MARKER << "\n";
        for (auto const &p : entries)
        {
            ostream << p.first << " " << p.second << "\n";
        }
        ostream << END_MARKER << "\n";
        ostream.close();

        return;
    }
    else
    {
        auto path = get_grid_mapfile();
        auto name = path.canonical_name();

        std::ifstream instream(name);
        std::string   line;

        std::vector<std::string> lines;

        while (std::getline(instream, line))
        {
             lines.emplace_back(line);
        }

        auto beginp = std::find(lines.begin(), lines.end(), BEGIN_MARKER);

        if (beginp == lines.end())
        {
            // Marker not found; append entries to end.
            lines.push_back(BEGIN_MARKER);
            for (auto const &p : entries)
            {
                auto newline = p.first + " " + p.second;
                lines.push_back(newline);
            }
            lines.push_back(END_MARKER);
        }
        else
        {
            // Clear existing "bde" entries.
            auto endp = std::find(beginp, lines.end(), END_MARKER);
            if (endp == lines.end())
            {
                auto message = std::string("Could not find end marker in ")
                    + name;
                utils::slog() << "[gridmap push] " << message;
                throw std::runtime_error(message);
            }

            lines.erase(beginp + 1, endp);
            int i = 1;

            // Add newly provided entries.
            for (auto const &p : entries)
            {
                auto newline = quote(p.first) + " " + p.second;
                lines.insert(beginp + i, newline);
                i++;
            }
        }

        utils::slog() << "[Grid mapfile] Updated entries.";
        for (auto const &l : lines)
        {
            utils::slog() << "[new gridmap] " << l;
        }

        // Write resuls to a temporary file.
        auto workfile = get_grid_mapfile_working_file();
        utils::slog() << "Writing to workfile: " << workfile;

        std::ofstream ostream(workfile);

        for (auto const &l : lines)
        {
            ostream << l << "\n";
        }

        ostream.close();

        // Save original gridmap file.
        auto backup = get_grid_mapfile_save_file();
        utils::slog() << "Backing up original gridmap to " << backup;

        if (std::rename(p.name().c_str(), backup.c_str()) != 0)
        {
            auto err = "Error renaming " + p.name() + " to "
                + backup + ":" + std::strerror(errno);
            utils::slog() << err;
            throw std::runtime_error(err);
        }

        // Rename workfile to gridmap file.
        utils::slog() << "Renaming workfile " << workfile
                      << " to " << p.name();

        if (std::rename(workfile.c_str(), p.name().c_str()) != 0)
        {
            auto err = "Error renaming " + workfile + " to "
                + p.name() + ":" + std::strerror(errno);
            utils::slog() << err;
            throw std::runtime_error(err);
        }

        utils::slog() << "Done.";
    }
}

// ----------------------------------------------------------------------

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

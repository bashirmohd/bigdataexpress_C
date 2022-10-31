#ifndef BDE_UTILS_UTILS_H
#define BDE_UTILS_UTILS_H

#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

#include <chrono>

// ---------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------
namespace utils
{
    const int s_debug   = 0;
    const int s_info    = 1;
    const int s_warning = 2;
    const int s_error   = 3;

    class slog
    {
    public:
        slog(int level = s_info);
        ~slog();

        template<class T>
        slog & operator << ( T const & t);

    private:
        std::stringstream ss;
    };

    class service
    {
        public:
        static std::ostream & console() { return *console_; }
        static void set_console(std::ostream & os) { console_ = &os; }

        private:
        static std::ostream * console_;
    };

    // utility functions
    inline std::vector<std::string> 
      split(std::string const & s, char delim) 
    {
        std::vector<std::string> elems;
        std::stringstream ss(s);
        std::string item;

        while (std::getline(ss, item, delim)) 
            elems.push_back(item);

        return elems;
    }

    inline void
      find_and_replace(std::string & str, std::string const & find, std::string const & replace)
    {
        for(std::string::size_type i = 0; (i=str.find(find, i)) != std::string::npos;)
        {
            str.replace(i, find.length(), replace);
            i += replace.length();
        }
    }
}

template<class T>
utils::slog & utils::slog::operator << ( T const & t )
{
    ss << t;
    return *this;
}

namespace utils
{
    // shell exec
    std::string exec(std::string const & cmd);

    // returns string of GUID
    std::string guid();

    // is private ip
    bool is_private_ip(std::string const & ip);

    // returns the git hash and git branch as the version
    std::string version(void);

    // This will translate strings like "1KB", "2MB", "2GiB" into the
    // corresponding number of bytes.
    std::size_t string2size(std::string const &str);

    // Format a number as string, interspersing commas where
    // necessary.  This will provide easier readability when printing
    // large numbers in logs and the such.
    template<typename T>
    std::string format_number(T val)
    {
        std::stringstream ss;

        ss.imbue(std::locale("en_US.UTF-8"));
        ss << std::fixed << val;

        return ss.str();
    }

    // certificate based SSH exec
    class CertSSH
    {
    public:
        CertSSH(std::string const & key, std::string const & cert);
        ~CertSSH();

        // exec command over the gsissh link
        std::string exec(std::string const & host, std::string const & cmd);
        std::string exec(std::string const & cmd);

    private:
        std::string keyfile;
        std::string certfile;
    };

    inline int64_t epoch_ms()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
                system_clock::now().time_since_epoch() ).count();
    }
}



#endif

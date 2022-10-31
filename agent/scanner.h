#ifndef BDE_AGENT_SCANNER_H
#define BDE_AGENT_SCANNER_H

#include <string>
#include "conf.h"
#include "agent.h"

using namespace std;

class Scanner
{
public:
    Scanner(Conf const & conf);

    void run();

    string run_cmd(string cmd);

    vector<string> split_string(const string& str, const string& delimiter);

    int get_potential_bandwidth();

    int update_stats();

private:

    inline bool is_empty(string &s) {
        auto unit=s.end()-1;
        if(*unit == '\n'){
            s.erase(s.end()-1,s.end());
        }
        for(auto i : s) {
            if(!isspace(i)) {
                    return false;
            }
        }

        return true;
    }

    string m_id;
    string m_scan_dir;
    string m_stats_file;

    int m_iozone_max_threads = 4;
    int m_sleep_mins;
    string m_iozone_exe;

    int m_ost_low;
    int m_ost_high;
    double m_writebw;
    double m_readbw;
};


#endif

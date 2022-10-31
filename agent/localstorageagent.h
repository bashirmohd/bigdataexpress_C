#ifndef BDE_AGENT_LOCAL_STORAGE_AGENT_H
#define BDE_AGENT_LOCAL_STORAGE_AGENT_H

#include "utils/utils.h"
#include "utils/mounts.h"
#include "utils/json/json.h"
#include "agentmanager.h"
#include <queue>
#include <string>
#include <thread>

using namespace std;
using namespace utils;
using namespace std::chrono;

class LocalStorageAgent : public Agent
{
    public:
        LocalStorageAgent(Json::Value const & conf);
        ~LocalStorageAgent();

    protected:
        virtual void do_init();
        virtual void do_registration();
        virtual Json::Value do_command(std::string const & cmd, Json::Value const & params);

    private:
        string run_cmd(string cmd);
        int get_usage(string device);
        int get_realtime_bandwidth(string device);
        int get_potential_bandwidth(string device, string dir);
        void iozone_thread_function();

        void report_disk_io_stats() const;
        void report_disk_usage_stats() const;

        vector<string> split_string(const string& str, const string& delimiter);
        inline bool is_empty(string &s){
            auto unit=s.end()-1;
            if(*unit == '\n'){
                s.erase(s.end()-1,s.end());
            }
            for(auto i : s){
                if(!isspace(i)){
                    return false;
                }
            }
            return true;
        }
        inline double parse_size(string s){
            auto unit=s.end()-1;
            double c = 0;
            if(*unit == 'T'){
                c = 1000000;
            }
            else if(*unit == 'G'){
                c = 1000;
            }
            else if(*unit == 'M'){
                c = 1;
            }
            else if(*unit == 'K'){
                c = 0.001;
            }
            else if(*unit == '%'){
                c = 1;
            }
            s.erase(s.end()-1,s.end());
            if (not s.empty()) {
                try {
                    c *= stof(s);
                } catch (...) {
                    // XXX: ignore whatever this throws.  We don't
                    // want to this to crash the agent as a whole.
                }
            }
            return c;
        }

        Json::Value const & m_conf;
        string m_iozone_exe;
        string m_device;
        vector<string> m_root_dirs;
        string m_iozone_test_dir;
        int m_iozone_max_threads = 8;
        Json::Value m_jbase;

        size_t m_iozone_thread_interval = 1000000;
        shared_ptr<thread> iozone_thread_handler;
        bool iozone_thread_active=false;
        bool active=true;

};

#endif

#ifndef BDE_AGENT_SHARED_STORAGE_AGENT_H
#define BDE_AGENT_SHARED_STORAGE_AGENT_H

#include "utils/utils.h"
#include "utils/mounts.h"
#include "utils/json/json.h"
#include "utils/process.h"
#include "agent.h"
#include <queue>
#include <string>
#include <thread>

using namespace std;
using namespace utils;
using namespace std::chrono;

class SharedStorageAgent : public Agent
{
    public:
        SharedStorageAgent(Json::Value const & conf);
        ~SharedStorageAgent();

    protected:
        virtual void do_bootstrap();
        virtual void do_init();
        virtual void do_registration();
        virtual Json::Value do_command(std::string const & cmd, Json::Value const & params);

    private:
        void check_configuration(Json::Value const & conf);
        void scan_stats(double & w, double & r);
        string get_iozone_work_folder(std::string const &path);
        string run_cmd(string cmd);
        int get_usage(string device);
        int get_realtime_bandwidth(string device);
        void get_potential_bandwidth(string device, string dir);
        void iozone_thread_function();
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
            double c;
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
            c *= stof(s);
            return c;
        }

        Json::Value const & m_conf;
        string m_iozone_exe;
        string m_device;
        vector<string> m_root_dirs;
        string m_iozone_test_dir;
        int m_iozone_max_threads = 4;
        int m_storage_devs;
        Json::Value m_jbase;

        size_t m_iozone_thread_interval = 1000000;
        shared_ptr<thread> iozone_thread_handler;
        bool iozone_thread_active=false;
        bool active=true;
        vector<std::string> m_data_folders_;
#if 0
        bool                scan_data_folder_;
#endif
};

#endif

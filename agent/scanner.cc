#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <errno.h>
#include <fstream>
#include <unistd.h>

#include "scanner.h"
#include "localstorageagent.h"
#include "sharedstorageagent.h"

using namespace std;
using namespace utils;

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

Scanner::Scanner(Conf const & conf)
{
     // To find out the folder to scan
    auto path = conf["data_folders"]["path"];
    if (path.empty())
    {
        throw std::runtime_error("[SSA Scanner] data_folders path is required.");
    }

    // To create .ssa directory to keep the stats files
    m_scan_dir = path.asString() + "/.ssa";

    int status = mkdir(m_scan_dir.c_str(),
                       S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (status < 0)
    {
        slog() <<"[SSA Scanner] mkdir() failed with due to the following error: "
               << strerror(errno);
    }

    // To figure out the stats file naming so that
    // if there are a number of Scanner, they won't
    // collide.
    m_id = conf["id"].asString();
    m_stats_file = m_scan_dir + "/stat-" + m_id;

    // To get the path of iozone
    auto iozone_path = conf["iozone-executable"];
    if (iozone_path.empty())
    {
        throw runtime_error("[SSA Scanner] iozone path is required.");
    }

    m_iozone_exe = iozone_path.asString();

    // To get iozone sleep time
    m_sleep_mins = stoi(conf["sleep time"].asString());

    // To get the OST range. Right now, only
    // support [OST1,OST2].
    vector<string> ost_range = split_string(conf["OSTs"].asString(), "-");
    if (ost_range.size() != 2)
    {
        throw runtime_error("[SSA Scanner] OSTs needs to be " \
                            "in the format of OST1-OST2");
    }

    try {
        m_ost_low  = stoi(ost_range[0]);
        m_ost_high = stoi(ost_range[1]);
    }

    catch (...)
    {
        throw runtime_error("[SSA Scanner] Parsing error in OST range in JSON.");
    }
}

 static void             /* Display information from inotify_event structure */
 displayInotifyEvent(struct inotify_event *i)
 {
     printf("    wd =%2d; ", i->wd);
     if (i->cookie > 0)
         printf("cookie =%4d; ", i->cookie);

     printf("mask = ");
     if (i->mask & IN_ACCESS)        printf("IN_ACCESS ");
     if (i->mask & IN_ATTRIB)        printf("IN_ATTRIB ");
     if (i->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
     if (i->mask & IN_CLOSE_WRITE)   printf("IN_CLOSE_WRITE ");
     if (i->mask & IN_CREATE)        printf("IN_CREATE ");
     if (i->mask & IN_DELETE)        printf("IN_DELETE ");
     if (i->mask & IN_DELETE_SELF)   printf("IN_DELETE_SELF ");
     if (i->mask & IN_IGNORED)       printf("IN_IGNORED ");
     if (i->mask & IN_ISDIR)         printf("IN_ISDIR ");
     if (i->mask & IN_MODIFY)        printf("IN_MODIFY ");
     if (i->mask & IN_MOVE_SELF)     printf("IN_MOVE_SELF ");
     if (i->mask & IN_MOVED_FROM)    printf("IN_MOVED_FROM ");
     if (i->mask & IN_MOVED_TO)      printf("IN_MOVED_TO ");
     if (i->mask & IN_OPEN)          printf("IN_OPEN ");
     if (i->mask & IN_Q_OVERFLOW)    printf("IN_Q_OVERFLOW ");
     if (i->mask & IN_UNMOUNT)       printf("IN_UNMOUNT ");

     printf("        name = %s\n", i->name);
 }

int Scanner::update_stats()
{
    ofstream ofs;
    ofs.open (m_stats_file.c_str(), ofstream::out | ofstream::trunc);

    ofs << m_writebw << endl << m_readbw;

    ofs.close();

    return 0;
}

void Scanner::run()
{
#if 0
    int fd, wd, length;
    char buffer[BUF_LEN];
#endif
    slog() << "[SSA Scanner] Starting SSA Scanner...";

    while (1)
    {
        get_potential_bandwidth();

        update_stats();

        sleep(m_sleep_mins * 60);
    }
#if 0
    fd = inotify_init();
    if (fd < 0) {
        slog() << "[SSA Scanner] inotify_init error.";
    }

//    string stats_file = m_scan_dir + "/stats";
    string stats_file = m_scan_dir;

    wd = inotify_add_watch(fd, stats_file.c_str(), IN_CLOSE);

    length = read(fd, buffer, BUF_LEN);

    for (char * p = buffer; p < buffer + length; ) {
        struct inotify_event * event = (struct inotify_event *) p;
        displayInotifyEvent(event);

        p += sizeof(struct inotify_event) + event->len;
    }

    inotify_rm_watch (fd, wd);
#endif
    slog() << "[SSA Scanner] exit";
}

string Scanner::run_cmd(string cmd)
{
    FILE * pipe = NULL;
    char buffer[BUFSIZ];
    string result;

    pipe = popen(cmd.c_str(), "r");

    int len = 0;
    if (NULL == pipe) {
        perror("pipe");
        return "";
    }

    while (!feof(pipe)) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL)
            result = buffer;
    }

    pclose(pipe);

    return result;
}

vector<string> Scanner::split_string(const string& str, const string& delimiter){
    vector<string> strings;
    string::size_type pos = 0;
    string::size_type prev = 0;
    string topush;
    while ((pos = str.find(delimiter, prev)) !=  string::npos){
        topush = str.substr(prev, pos - prev);
        if(!is_empty(topush)){
            strings.push_back(topush);
        }
        prev = pos + 1;
    }
    topush = str.substr(prev);
    if(!is_empty(topush)){
        strings.push_back(topush);
    }
    return strings;
}

int Scanner::get_potential_bandwidth()
{
    stringstream cmd;
    double writebw = 0.0, readbw = 0.0;

    m_writebw = m_readbw = 0.0;

    vector<string> test_files(m_iozone_max_threads);

    for (int ostid = m_ost_low; ostid <= m_ost_high; ostid++) {
        for (int i = 0; i < m_iozone_max_threads; i++) {

            stringstream filename;
            filename << m_scan_dir << "/testiozone."
                     << m_id << "." << i;
            test_files[i] = filename.str();

            // probe the bandwidth of each OST. This is done
            // via 'lfs setstripe' command.
            // Once we have the aggregated bandwidth of
            // the OSTS specified in JSON, write the results
            // to the stats file.
            cmd.str("");
            cmd << "lfs setstripe " + filename.str() + " -c 1 -i ";
            cmd << ostid;

            run_cmd(cmd.str());

            // iozone write tests
            cmd.str("");
            cmd << m_iozone_exe << " -i0 -w -I -r 16M -s 16M -t ";
            cmd << i + 1;
            cmd <<" -F ";

            for (auto j : test_files) {
                cmd << j << " ";
            }

            cmd << " 2>&1 | grep 'Children see throughput for'";
            string ret = run_cmd(cmd.str());

            cmd.str("");
            vector<string> result = split_string(ret, " ");

            if (result.size() < 8) {
                slog() << "[SSA Scanner] Fatal error 1: "
                       << "iozone did not return results! "
                       << "Please check if iozone is configured correctly!";
                return -1;
            }

            try {
                double bw = stod(result[7]) / 1024.0;
                if (bw > writebw) {
                    writebw = bw;
                }
            }

            catch(...)
            {
                slog() << "[SSA Scanner] iozone returned strings in wrong format!";
            }

            // iozone read tests
            cmd << m_iozone_exe << " -i1 -w -I -r 16M -s 16M -t ";
            cmd << i + 1;
            cmd <<" -F ";
            for (auto j : test_files) {
                cmd << j << " ";
            }

            cmd << " 2>&1 | grep 'Children see throughput for'";
            ret = run_cmd(cmd.str());
            cmd.str("");
            result = split_string(ret, " ");
            if (result.size() < 8) {
                slog() << "[SSA Scanner] Fatal error 2: iozone did not return results! Please check if iozone executable is configured correctly!";
                return -1;
             }

             try {
                 double bw = stod(result[7]) / 1024.0;
                 if (bw > writebw) {
                     readbw = bw;
                 }
             }

             catch(...)
             {
                 slog() << "[SSA Scanner] iozone returned strings in wrong format!";
            }
        }

        slog() << "[OST:" << ostid << "] writebw: " << writebw
               << " readbw: "  << readbw;

        m_writebw += writebw;
        m_readbw  += readbw;

        // delete iozone files
        cmd << "rm -f ";
        for (auto j : test_files) {
            cmd << j << " ";
        }

        run_cmd(cmd.str());
    }

    return 0;
}

#ifndef BDE_AGENT_LAUNCHER_AGENT_H
#define BDE_AGENT_LAUNCHER_AGENT_H

#include <memory>

#include "agentmanager.h"
#include "utils/rpcserver.h"

class LauncherAgent : public Agent
{
public:
    LauncherAgent(Json::Value const &conf);
    ~LauncherAgent();

protected:
    virtual void do_bootstrap();
    virtual void do_init();
    virtual void do_registration();

    virtual Json::Value do_command(std::string const &cmd,
                                   Json::Value const &params);

private:
    std::string get_program_type(Json::Value const &conf) const;
    std::string get_program_path(Json::Value const &conf) const;
    std::vector<std::string> get_program_args(Json::Value const &conf) const;
    std::map<std::string, std::string> get_program_env(Json::Value const &conf) const;

    void set_mdtm_queue_names(Json::Value const &conf);
    void run_mdtm_ftp_client_report_listener();
    void handle_mdtm_report(Json::Value const &msg, RpcProps const &props);

    void run_subprocess();

    void register_launcher();

    void save_message(std::string const &name, Json::Value const &msg) const;

private:
    const Json::Value &launcher_conf_;

    std::thread                        prog_thread_;
    std::string                        program_path_;
    std::vector<std::string>           program_args_;
    std::map<std::string, std::string> program_env_;

    std::string                bde_server_queue_;
    std::string                mdtm_client_listen_queue_;
    std::string                mdtm_client_report_queue_;
    std::unique_ptr<RpcServer> mdtm_report_listener_;
};

#endif

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

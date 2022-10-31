
#include "utils/utils.h"
#include "utils/rpcserver.h"
#include "utils/json/json.h"

#include <signal.h>
#include <iostream>
#include <future>

using namespace utils;

std::promise<int> promise;
std::future<int> f = promise.get_future();

void sig_handler(int dummy)
{
    promise.set_value(1);
}

int main()
{
    signal(SIGINT, sig_handler);

    RpcServer server("localhost", 1883, "test-server");
    server.reg_handler("cmd-test", [](Json::Value const & params) {
        slog() << Json::StyledWriter().write(params);
        Json::Value res;
        res["code"] = 0;
        res["message"] = "success";
        return res;
    });

    server.start();
    slog() << "server started";

    f.wait();

    server.stop();
    slog() << "server stopped";
}


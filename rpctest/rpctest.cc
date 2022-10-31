#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

#include <unistd.h>
#include <getopt.h>

#include "utils/rpcclient.h"

static const std::string DEFAULT_MQ_HOST    = "yosemite.fnal.gov";
static const int         DEFAULT_MQ_PORT    = 1883;
static const int         DEFAULT_MQ_TIMEOUT = 5;
static const std::string DEFAULT_MQ_TOPIC   = "daemon";
static const std::string DEFAULT_MQ_CERT    = "cacert.pem";
static const std::string DEFAULT_MQ_PSK     = "";
static const std::string DEFAULT_MQ_PSK_ID  = "";
static const int         DEFAULT_VERBOSE    = 0;

static const std::string COLOR_RED  = "\033[0;31m";
static const std::string COLOR_NONE = "\033[0m";

void print_usage(const char * const progname)
{
    std::cout << "Usage: " << progname << " [Options] [payload file]\n"
              << "\nOptions are: \n"
              << "\t -h [MQTT broker host]  "
              << "\t(default: " << DEFAULT_MQ_HOST << ")\n"
              << "\t -p [MQTT broker port]  "
              << "\t(default: " << DEFAULT_MQ_PORT << ")\n"
              << "\t -q [MQTT topic]        "
              << "\t(default: " << DEFAULT_MQ_TOPIC << ")\n"
              << "\t -t [MQTT timeout]      "
              << "\t(default: " << DEFAULT_MQ_TIMEOUT << "s)\n"
              << "\t -c [MQTT cacert]       "
              << "\t(default: " << DEFAULT_MQ_CERT << ")\n"
              << "\t -k [MQTT psk]          "
              << "\t(default: " << DEFAULT_MQ_PSK << "'')\n"
              << "\t -i [MQTT psk identity] "
              << "\t(default: " << DEFAULT_MQ_PSK_ID << "'')\n"
              << "\t -v [verbosity]         "
              << "\t(default: " << DEFAULT_VERBOSE << ")\n"
              << "Example: rpctest -h yosemite.fnal.gov -p 1883 "
              << "-i easy -k 12345 -q storage ./params.json\n\n";
}

int main(int argc, char ** argv)
{
    std::string mq_host       = DEFAULT_MQ_HOST;
    int         mq_port       = DEFAULT_MQ_PORT;
    int         mq_timeout    = DEFAULT_MQ_TIMEOUT; // seconds
    std::string mq_topic_name = DEFAULT_MQ_TOPIC;
    std::string mq_cert       = DEFAULT_MQ_CERT;
    std::string mq_psk        = DEFAULT_MQ_PSK;
    std::string mq_psk_id     = DEFAULT_MQ_PSK_ID;
    int         verbose       = DEFAULT_VERBOSE;
    std::string param_file    = "params.json";

    bool use_ca  = false;
    bool use_psk = false;

    int c;

    while ((c = getopt (argc, argv, "h:p:c:k:i:q:t:v:?")) != -1)
    {
        switch (c)
           {
           case 'h':
               mq_host = optarg;
               break;
           case 'p':
               mq_port = std::stoi(optarg);
               break;
           case 'c':
               mq_cert = optarg;
               use_ca = true;
               break;
           case 'k':
               mq_psk = optarg;
               use_psk = true;
               break;
           case 'i':
               mq_psk_id = optarg;
               use_psk = true;
               break;
           case 'q':
               mq_topic_name = optarg;
               break;
           case 't':
               mq_timeout = std::stoi(optarg);
               break;
           case 'v':
               verbose = std::stoi(optarg);
               break;
           case '?':
               print_usage(argv[0]);
               exit(1);
           default:
               break;
           }
    }

    if (optind == argc)
    {
        std::cerr << "\n"
                  << COLOR_RED
                  << "Error: No payload file given. Quitting."
                  << COLOR_NONE
                  << "\n\n";
        print_usage(argv[0]);
        exit(1);
    }


    if (use_ca && use_psk)
    {
        std::cerr << "\n"
                  << COLOR_RED
                  << "Error: Cant use both CA and PSK. Quitting."
                  << COLOR_NONE
                  << "\n\n";
        print_usage(argv[0]);
        exit(1);
    }

    if (mq_topic_name.empty())
    {
        std::cerr << "\n"
                  << COLOR_RED
                  << "Error: No topic name given. Quitting."
                  << COLOR_NONE
                  << "\n\n";
        print_usage(argv[0]);
        exit(1);
    }

    param_file = argv[optind];

    if (access(param_file.c_str(), R_OK) == -1)
    {
        std::cerr << COLOR_RED
                  << "Error: Can't open JSON param file \'"
                  << param_file << "\' "
                  << "(error: " << strerror(errno) << ")"
                  << COLOR_NONE
                  << "\n";
        exit(1);
    }

    std::ifstream json(param_file, std::ifstream::binary);
    Json::Value params;

    Json::Reader reader;
    bool b = reader.parse(json, params);

    if (!b)
    {
        std::cerr << COLOR_RED
                  << "Error: failed to parse the json file \'"
                  << param_file << "\'"
                  << COLOR_NONE
                  << "\n";
        exit(1);
    }

    RpcClient client(mq_host, mq_port);

    if (use_ca)
    {
        client.set_tls_ca(mq_cert);
    }
    else if (use_psk)
    {
        client.set_tls_psk(mq_psk_id, mq_psk);
    }

    client.start();

    auto r = client.call(mq_topic_name, params, mq_timeout, verbose);

    Json::StyledWriter writer;

    client.stop();

    if (r["error"] == "rpc call timed out")
    {
        std::cerr << COLOR_RED << writer.write(r) << COLOR_NONE << "\n";
        return 1;
    }
    else
    {
        std::cout << writer.write(r) << "\n";
        return 0;
    }
}

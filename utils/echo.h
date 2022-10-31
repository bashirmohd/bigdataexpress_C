#ifndef BDE_UTILS_ECHO_H
#define BDE_UTILS_ECHO_H

// Helpers for DTN Agents to send/receive "ping" commands.  Code is
// straight from asio "echo" example; except that we reverse the
// response string, just for fun.

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include "asio.hpp"

#include "utils/utils.h"

using asio::ip::tcp;

namespace utils
{

    class echo_session
        : public std::enable_shared_from_this<echo_session>
    {
    public:
        echo_session(tcp::socket socket)
            : socket_(std::move(socket)) {
        }

        void start() {
            do_read();
        }

    private:
        void do_read() {
            auto self(shared_from_this());
            socket_.async_read_some(
                asio::buffer(data_, max_length),
                [this, self](std::error_code ec, std::size_t length) {
                    if (!ec) {
                        utils::slog() << "echo received data: " << data_;
                        do_write(length);
                    }
                });
        }

        void do_write(std::size_t length) {
            auto self(shared_from_this());

            std::string response(data_);
            std::reverse(response.begin(), response.end());

            utils::slog() << "echo sending response: " << response;

            asio::async_write(
                socket_, asio::buffer(response, response.length()),
                [this, self](std::error_code ec, std::size_t /*length*/) {
                    if (!ec) {
                        do_read();
                    }
                });
        }

        tcp::socket socket_;
        enum { max_length = 1024 };
        char data_[max_length];
    };

    class echo_server
    {
    public:
        echo_server(asio::io_service& io_service, short port)
            : acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
              socket_(io_service) {
            do_accept();
        }

    private:
        void do_accept() {
            acceptor_.async_accept(
                socket_, [this](std::error_code ec) {
                    if (!ec) {
                        std::make_shared<echo_session>(
                            std::move(socket_))->start();
                    }

                    do_accept();
                });
        }

        tcp::acceptor acceptor_;
        tcp::socket   socket_;
    };

};

#endif // BDE_UTILS_ECHO_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:

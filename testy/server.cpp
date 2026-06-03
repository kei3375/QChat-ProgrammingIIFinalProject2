//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//

#include <array>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <asio.hpp>

using asio::ip::udp;

std::string target_ip;
int iport;
int oport;





std::string make_daytime_string()
{
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

class udp_server
{
public:
    udp_server(asio::io_context& io_context, int iport, int oport)
        : socket_(io_context, udp::endpoint(udp::v4(), iport))
    {
        udp::resolver res(io_context);
        send_endpoint = *res.resolve(udp::v4(), target_ip, std::to_string(oport)).begin();
        start_receive();
    }

    // private:
    void start_receive()
    {
        socket_.async_receive_from(
            asio::buffer(recv_buffer_), remote_endpoint_,
            std::bind(&udp_server::handle_receive, this,
                asio::placeholders::error,
                asio::placeholders::bytes_transferred));
    }

    void handle_receive(const std::error_code& error, std::size_t /*bytes_transferred*/)
    {
        if (!error)
        {
            //std::shared_ptr<std::string> message(new std::string(make_daytime_string()));
            // std::string message = "meow\n";

            socket_.async_send_to(
                asio::buffer(recv_buffer_),
                remote_endpoint_,
                std::bind(
                    &udp_server::handle_send, this,
                    asio::placeholders::error,
                    asio::placeholders::bytes_transferred
                )
            );

            std::cout << "message received\n message: ";

            std::string msg(std::begin(recv_buffer_), std::end(recv_buffer_));

            start_receive();
        }
    }

    void send(std::string msg) {
        socket_.async_send_to(
            asio::buffer(msg),
            send_endpoint,
            std::bind(
                &udp_server::handle_send, this,
                asio::placeholders::error,
                asio::placeholders::bytes_transferred
            )
        );
    }

    void handle_send(
        const std::error_code& /*error*/,
        std::size_t /*bytes_transferred*/
    ) {
        std::cout << "message sent;\n";
    }

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    udp::endpoint send_endpoint;
    std::array<char, 64> recv_buffer_;
};

int main(int argc, char const* argv[])
{
    try
    {
        if (argc != 4) {
            std::cerr << "Usage: client <host> <receiver port> <sender port>" << std::endl;
            return 1;
        }
        else {
            target_ip = argv[1];
            iport = std::stoi(argv[2]);
            oport = std::stoi(argv[3]);
        }

        asio::io_context io_context;
        udp_server server(io_context, iport, oport);
        server.send("hello\n  meow meow\n 1000000000001");
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <array>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <asio.hpp>
#include <conio.h>


using asio::ip::udp;

std::string target_ip;
int iport;
int oport;


std::string written_message;

std::string UI, UI_existing;

std::vector<std::string> messages(20);


void add_message(std::string msg) {
    for(int i{1}; i<messages.size(); ++i) {
        messages[i-1] = messages[i];
    }
    messages[messages.size()-1] = msg;
}


void updateUI() {
    // system("cls");

    UI = "";
    UI = "ip: " + target_ip + "; i_port: " + std::to_string(iport) + "; o_port" + std::to_string(oport) + "\n";

    for(int m{}; m<messages.size(); ++m) {
        UI += messages[m] + "\n";
    }

    UI += "you: " + written_message;

    if(UI != UI_existing) {
        system("cls");
        std::cout<<UI;
        UI_existing = UI;
    }
}

class udp_server
{
public:
  udp_server(asio::io_context& io_context, int iport, int oport)
    : socket_(io_context, udp::endpoint(udp::v4(), iport)), kbd_t(io_context, asio::chrono::milliseconds(1))
  {
    udp::resolver res(io_context);
    // kbd_t = asio::steady_timer(io_context, asio::chrono::milliseconds(1));
    send_endpoint = *res.resolve(udp::v4(), target_ip, std::to_string(oport)).begin();
    start_receive();
    keyboard_listen(&kbd_t);

    std::fill_n(recv_buffer_.begin(), 64, 0); // clear shit
  }

  void keyboard_listen(asio::steady_timer * t) {
    if(_kbhit()) {
        char c = _getch();
        if(c == 224 || c == 0) c = _getch();

        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '1' && c <= '9') || c == '0' || c == ' ') {
            written_message += c;
        }
        if(c == 8) { // 8 = backspace
            written_message.pop_back();
        }
        if(c == 13) { //13 = enter
            add_message("you: " + written_message);
            send(written_message);
            written_message = "";
        }

        updateUI();
    }
    t->expires_at(t->expiry() + asio::chrono::milliseconds(10));
    t->async_wait(std::bind(&udp_server::keyboard_listen, this, t));
  }

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

            // socket_.async_send_to(
            //     asio::buffer(recv_buffer_), 
            //     remote_endpoint_,
            //     std::bind(
            //         &udp_server::handle_send, this, 
            //         asio::placeholders::error,
            //         asio::placeholders::bytes_transferred
            //     )
            // );

            // std::cout<<"message received\n message: ";

            std::string msg(std::begin(recv_buffer_), std::end(recv_buffer_));
            // std::cout<<msg<<std::endl;

            add_message("friend: " + msg);

            std::fill_n(recv_buffer_.begin(), 64, 0);

            start_receive();

            updateUI();
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

    void sync_send(std::string msg) {
        socket_.send_to(
            asio::buffer(msg), 
            send_endpoint
        );
    }

    void handle_send(
        const std::error_code& /*error*/,
        std::size_t /*bytes_transferred*/
    ) {
        // std::cout<<"message sent;\n";
    }

  udp::socket socket_;
  udp::endpoint remote_endpoint_;
  udp::endpoint send_endpoint;
  std::array<char, 64> recv_buffer_;
  asio::steady_timer kbd_t;
};

int main(int argc, char const *argv[])
{
  try
  {
    if (argc != 4) {
        std::cerr << "Usage: client <host> <receiver port> <sender port>" << std::endl;
        return 1;
    } else {
        target_ip = argv[1];
        iport = std::stoi(argv[2]);
        oport = std::stoi(argv[3]);
    }

    updateUI();

    asio::io_context io_context;
    udp_server server(io_context, iport, oport);
    // server.send("hello\n  meow meow\n 1000000000001");

    std::string mssg;
    // while(1) {
    //     getline(std::cin, mssg);
    //     server.sync_send(mssg);
    // }

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
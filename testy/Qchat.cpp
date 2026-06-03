#include <iostream>
#include <array>
#define ASIO_STANDALONE

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

using asio::ip::tcp;

int main(int argc, char* argv[])
{
    

    try {
        if (argc != 2) {
            std::cerr << "usage: client <host>" << std::endl;
            return 1;
        }


        asio::io_context io;
        tcp::resolver resolver(io);

        tcp::resolver::results_type endpoints = resolver.resolve(argv[1], "daytime");

        tcp::socket socket(io);
        asio::connect(socket, endpoints);

        for (;;)
        {
            std::array<char, 128> buf;
            std::error_code error;

            size_t len = socket.read_some(asio::buffer(buf), error);


            if (error == asio::error::eof)
                break; // Connection closed cleanly by peer.
            else if (error)
                throw std::system_error(error); // Some other error.

            std::cout.write(buf.data(), len);

        }
    }
catch (std::exception& e)
{
    std::cerr << e.what() << std::endl;
}

    std::cout << "Hello World!\n";
    
}


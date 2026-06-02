#include <functional>
#include <iostream>
#include <asio.hpp>

int count;

void print(asio::steady_timer * t) {
    ++count;

    std::cout<<"n: "<<count<<"\n";

    t->expires_at(t->expiry() + asio::chrono::milliseconds(61));
    t->async_wait(std::bind(print, t));
}

int main(int argc, char const *argv[])
{
    asio::io_context io;

    asio::steady_timer t(io, asio::chrono::milliseconds(1));

    t.async_wait(std::bind(print, &t));

    io.run();

    return 0;
}

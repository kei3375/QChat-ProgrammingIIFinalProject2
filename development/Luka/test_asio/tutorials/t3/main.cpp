#include <functional>
#include <iostream>
#include <asio.hpp>


void t_print(int *count, asio::steady_timer * t) {
        if (*count<10) {
                std::cout<<"meow "<<*count<<std::endl;
                ++(*count);

                t->expires_at(t->expiry() + asio::chrono::milliseconds(250));
                t->async_wait(std::bind(t_print, count, t));
        }
}

void woof(asio::steady_timer *t, int *count) {
        if(*count<2) {
                std::cout<<"woof woof\n";
                ++(*count);

                t->expires_at(t->expiry() + asio::chrono::seconds(2));
                t->async_wait(std::bind(woof, t, count));
        }
}

int main() {
        asio::io_context io;

        int count{};
        int woof_count{};

        asio::steady_timer t(io, asio::chrono::milliseconds(200));
        asio::steady_timer t2(io, asio::chrono::milliseconds(200));
        t.async_wait(std::bind(t_print, &count, &t));
        t.async_wait(std::bind(woof, &t2, &woof_count));

        io.run();

        std::cout<<count<<" meows in total !! >:3\n";

        return 0;
}
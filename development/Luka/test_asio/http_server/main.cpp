#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <asio.hpp>

using asio::ip::tcp;


int rport = 80;


std::string readwebsite() { //ai generated function to read from ./index.html file
    // Open the file at the end (ios::ate) and in binary mode
    std::string filename = "./index.html";
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    // Get the current position (which is the file size)
    std::streamsize size = file.tellg();
    
    // Move back to the beginning of the file to start reading
    file.seekg(0, std::ios::beg);

    // Allocate the string with the correct size
    std::string contents(size, '\0');

    // Read the file data into the string's buffer
    if (!file.read(&contents[0], size)) {
        throw std::runtime_error("Error reading file: " + filename);
    }

    return contents;

    // std::string res;
    // res+= "HTTP/1.1 200 OK\r\n";
    // res+= "Content-Type: text/html\r\n";
    // res+= "Content-Length: " + std::to_string(contents.size()) + "\r\n";
    // res+= "Connection: close\r\n";
    // res+= "\r\n";
    // res+= contents;
    // return res;
}


std::string make_daytime_string()
{
  using namespace std; // For time_t, time and ctime;
  time_t now = time(0);
  return ctime(&now);
}

std::string make_http_response(const std::string& body, const std::string& content_type) {
    std::string res;
    res += "HTTP/1.1 200 OK\r\n";
    res += "Content-Type: " + content_type + "\r\n";
    res += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    res += "Access-Control-Allow-Origin: *\r\n";
    res += "Connection: close\r\n";
    res += "\r\n";
    res += body;
    return res;
}

class tcp_connection
  : public std::enable_shared_from_this<tcp_connection>
{
public:
    typedef std::shared_ptr<tcp_connection> pointer;

    tcp::socket socket_;
    std::string message_;

    std::array<char, 4096> buffer;

    static pointer create(asio::io_context& io_context)
    {
        return pointer(new tcp_connection(io_context));
    }

    tcp::socket& socket()
    {
        return socket_;
    }

    void start()
    {
        message_ = readwebsite();

        // std::cout<<"sending index.html to: "<<socket_.remote_endpoint().address().to_string()<<"\n";

        // asio::async_write(
        //     socket_, asio::buffer(message_),
        //     std::bind(
        //         &tcp_connection::handle_write, shared_from_this(),
        //         asio::placeholders::error,
        //         asio::placeholders::bytes_transferred
        //     )
        // );

        socket_.async_read_some(
            asio::buffer(buffer),
            std::bind(
                &tcp_connection::handle_read, shared_from_this(),
                asio::placeholders::error,
                asio::placeholders::bytes_transferred
            )
        );

    }

    void handle_read(const std::error_code& error, size_t bytes) {
        if (error) return;

        std::string client_ip = socket_.remote_endpoint().address().to_string();
        std::string request(buffer.data(), bytes);

        // sprawdź pierwszą linię requestu
        if (request.find("GET /time") != std::string::npos) {
            // request o godzinę
            std::cout << "GET /time from: " << client_ip << "\n";
            time_t now = time(0);
            std::string t = ctime(&now);
            t.erase(t.find_last_not_of("\n") + 1); // usuń newline
            message_ = make_http_response(t, "text/plain");
        } else {
            // domyślnie — wyślij index.html
            std::cout << "GET / from: " << client_ip << "\n";
            message_ = make_http_response(readwebsite(), "text/html");
        }

        asio::async_write(socket_, asio::buffer(message_),
            std::bind(&tcp_connection::handle_write, shared_from_this(),
                asio::placeholders::error,
                asio::placeholders::bytes_transferred)
        );
    }

    tcp_connection(asio::io_context& io_context)
    : socket_(io_context)
    {

    }

    void handle_write(const std::error_code& /*error*/,
    size_t /*bytes_transferred*/)
    {

    }
    
    
};

class tcp_server
{
public:
    asio::io_context& io_context_;
    tcp::acceptor acceptor_;

    tcp_server(asio::io_context& io_context)
    : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), 80))
    {
        start_accept();
    }


    void start_accept()
    {
        tcp_connection::pointer new_connection =
            tcp_connection::create(io_context_);
        
        acceptor_.async_accept(new_connection->socket(),
            std::bind(
                &tcp_server::handle_accept, this, new_connection,
                asio::placeholders::error)
            );
    }

    void handle_accept(
        tcp_connection::pointer new_connection,
        const std::error_code& error
    )
    {
        if (!error)
        {
            new_connection->start();
        }

        start_accept();
    }

};

int main(int argc, char const *argv[])
{
    try
    {
        asio::io_context io_context;
        tcp_server server(io_context);
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

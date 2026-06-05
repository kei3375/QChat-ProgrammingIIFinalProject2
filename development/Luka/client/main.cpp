// #include "QLib.hpp";

#include <iostream>
#include <asio.hpp>
#include <vector>
#include <fstream>
#include <array>
#include <string>




using asio::ip::tcp;
using asio::ip::udp;


std::string qindex = "./frontend/index.html";
std::string style = "./frontend/style.css";
std::string options = "./frontend/modules/options.html";
std::string input_bar = "./frontend/modules/input_bar.html";
std::string javascript = "./frontend/modules/javascript.js";
std::string messages_module = "./frontend/modules/messages.html";
std::string  chat_header = "./frontend/modules/chat_header.html";
std::string friend_list = "./frontend/modules/friend_list.html";
std::string top_bar = "./frontend/modules/top_bar.html";
// std::string  = "./frontend/modules/.html";
std::string meow = "./frontend/meow.txt";


std::string icon = "./frontend/icon.ico";

std::string uuid="meow-meow-this-is-real-uuid-trust111";


int client_port = 80;





std::string replace_string_pattern_with(std::string source, const std::string& pattern1, const std::string& string2) {//gemini generated function
    if (pattern1.empty()) return source; // Prevent infinite loop if pattern1 is empty
    
    size_t start_pos = 0;
    while ((start_pos = source.find(pattern1, start_pos)) != std::string::npos) {
        source.replace(start_pos, pattern1.length(), string2);
        // Move start_pos forward so we don't accidentally replace inside the newly inserted string
        start_pos += string2.length(); 
    }

    return source;
}

std::string make_json_messages(std::string req_uuid) {
    std::string location = "./friends/"+req_uuid+"/messages.txt";
    std::ifstream file(location);
    if (!file.is_open()) return "[]";

    std::string result = "[";
    std::string line;
    bool first = true;

    while (std::getline(file, line)) {
        std::string sender = "";
        if (line == "$friend") sender = req_uuid;
        // else if (line == "$you") sender = uuid;
        else if (line == "$you") sender = "you";
        else continue;

        std::string message, date, time_str;

        while (std::getline(file, line) && line != "$end") {
            if (line.rfind("message:", 0) == 0) message  = line.substr(8);
            else if (line.rfind("date:",   0) == 0) date = line.substr(5);
            else if (line.rfind("time:",   0) == 0) time_str = line.substr(5);
        }

        if (!first) result += ",";
        first = false;

        result += "{";
        result += "\"sender\":\"" + sender + "\",";
        result += "\"message\":\"" + message + "\",";
        result += "\"date\":\"" + date + "\",";
        result += "\"time\":\"" + time_str + "\"";
        result += "}";
    }

    result += "]";
    file.close();
    return result;
}

std::string readwebsite(std::string filename) { //ai generated function to read from ./index.html file
    // Open the file at the end (ios::ate) and in binary mode
    // std::string filename = "./index.html";
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
}

std::string prepare_html(std::string html) {
    html = replace_string_pattern_with(html, "$$messages$$", readwebsite(messages_module));
    html = replace_string_pattern_with(html, "`$`meow meow`$`", readwebsite(meow));
    html = replace_string_pattern_with(html, "$$options$$", readwebsite(options));
    html = replace_string_pattern_with(html, "$$input bar$$", readwebsite(input_bar));
    html = replace_string_pattern_with(html, "$$javascript$$", readwebsite(javascript));
    html = replace_string_pattern_with(html, "$$chat header$$", readwebsite(chat_header));
    html = replace_string_pattern_with(html, "$$friend list$$", readwebsite(friend_list));
    html = replace_string_pattern_with(html, "$$top bar$$", readwebsite(top_bar));
    return html;
}

std::string make_daytime_string()       //for debugging / testing ?? delete later
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

void save_message(std::string requested_uuid, std::string message) {
    // pobierz aktualną datę i czas
        std::time_t t = std::time(nullptr);
        std::tm* tm = std::localtime(&t);
        char date[11], time_str[9];
        std::strftime(date, sizeof(date), "%Y-%m-%d", tm);
        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", tm);
        
        // zapisz do pliku
        std::ofstream file("friends/" + requested_uuid + "/messages.txt", std::ios::app);
        file << "\n$you\n";
        file << "message:" << message << "\n";
        file << "date:" << date << "\n";
        file << "time:" << time_str << "\n";
        file << "$end\n";
        file.close();
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
        message_ = readwebsite(qindex);

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
        } else if (request.find("GET /style.css") != std::string::npos) {
            // request o css
            std::cout << "GET /style.css from: " << client_ip << "\n";
            message_ = make_http_response(readwebsite(style), "text/css");
        } else if (request.find("GET /icon.ico") != std::string::npos) {
            // request o ikone
            std::cout << "GET /icon.ico from: " << client_ip << "\n";
            message_ = make_http_response(readwebsite(icon), "image/x-icon");
        } else if (request.find("GET /messages/") != std::string::npos) {
            // request o wiadomości do specific usera
            std::string requested_uuid = request.substr(request.find("GET /messages/") + 14, 36);
            std::cout << "GET /messages/"<<requested_uuid<<" from: "<< client_ip <<"\n";
            message_ = make_json_messages(requested_uuid);
        } else if (request.find("POST /send/") != std::string::npos) {

            
            std::string requested_uuid = request.substr(request.find("POST /send/") + 11, 36);
            std::string body = request.substr(request.find("\r\n\r\n") + 4);
            
            std::cout<<"GET /send/uuid "<<client_ip<<" is sending message to "<<requested_uuid<<"\n";
            
            // wyciągnij message z JSON {"message":"treść"}
            std::string message = body.substr(body.find("\"message\":\"") + 11);
            message = message.substr(0, message.find("\""));
            
            save_message(requested_uuid, message);
            
            message_ = make_http_response("ok", "text/plain");
        } else {
            // domyślnie wyślij index.html
            std::cout << "GET / from: " << client_ip << "\n";
            message_ = make_http_response(prepare_html(readwebsite(qindex)), "text/html");
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

class http_server
{
public:
    asio::io_context& io_context_;
    tcp::acceptor acceptor_;

    http_server(asio::io_context& io_context)
    : io_context_(io_context),
    acceptor_(io_context, tcp::endpoint(
        //tcp::v4() //opened to everyone in the network
        asio::ip::make_address("127.0.0.1"), //opened only to this computer
        client_port)
    ) {
        start_accept();
    }


    void start_accept()
    {
        tcp_connection::pointer new_connection =
            tcp_connection::create(io_context_);
        
        acceptor_.async_accept(new_connection->socket(),
            std::bind(
                &http_server::handle_accept, this, new_connection,
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

    asio::io_context io;

    http_server http_server(io);

    io.run();


    return 0;
}
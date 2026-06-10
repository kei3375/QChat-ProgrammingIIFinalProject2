#pragma once

// #include <iostream>
// #include <asio.hpp>
// #include <vector>
// #include <fstream>
// #include <array>
// #include <string>
// #include <unordered_map>
// #include <ctime>
// #include <random>
// #include <unordered_set>

// #include "c_tcp_client.hpp"



// // #define C_SERVER_IP   "144.24.174.135"
// #define C_SERVER_IP   "127.0.0.1"
// #define C_SERVER_PORT 81

// bool connection_to_server_active = false;


// using asio::ip::tcp;
// using asio::ip::udp;


#include <iostream>
#include <string>

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string replace_string_pattern_with(std::string source, const std::string& pattern1, const std::string& string2) {
    if (pattern1.empty()) return source; // Prevent infinite loop if pattern1 is empty
    
    size_t start_pos = 0;
    while ((start_pos = source.find(pattern1, start_pos)) != std::string::npos) {
        source.replace(start_pos, pattern1.length(), string2);
        start_pos += string2.length(); 
    }
    return source;
}






#include <array>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <asio.hpp>
#include <conio.h>


using asio::ip::udp;
using asio::ip::tcp;

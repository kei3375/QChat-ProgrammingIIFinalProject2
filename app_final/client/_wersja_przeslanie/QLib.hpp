#pragma once

#include "crypto.hpp"


#include <iostream>
#include <asio.hpp>
#include <vector>
#include <fstream>
#include <array>
#include <string>
#include <unordered_map>
#include <ctime>
#include <random>
#include <unordered_set>
#include <stdint.h>
#include <cstdint>

#include <string>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

std::unordered_map<std::string, std::chrono::system_clock::time_point> last_packets_times;
std::unordered_map<std::string, bool> online_status;
std::unordered_map<std::string, int> notifications_count;


//zmienne lokalizacji plików
const std::string javascript = "./frontend/modules/javascript.js";
const std::string style = "./frontend/style.css";
const std::string options = "./frontend/modules/options.html";
const std::string qindex = "./frontend/index.html";
const std::string input_bar = "./frontend/modules/input_bar.html";
const std::string messages_module = "./frontend/modules/messages.html";
const std::string  chat_header = "./frontend/modules/chat_header.html";
const std::string friend_list = "./frontend/modules/friend_list.html";
const std::string top_bar = "./frontend/modules/top_bar.html";
const std::string settings_hidden = "./frontend/modules/settings_hidden.html";


const std::string icon = "./frontend/icon.ico";
const std::string logo = "./frontend/logo.jpg";
const std::string profile_picture = "./user_settings/profile_picture.jpg";
const std::string server_state_file = "./user_settings/server.txt";

std::string uuid="";

std::string username = "./user_settings/name.txt";

int client_port = 80;   //port z którego korzysta przeglądarka do komunikacji z programem, używając http

std::string selected_user;



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








std::string make_json_messages(std::string req_uuid) {              //funkcja zbierająca wiadomości w formacie json
    std::string location = "./friends/"+req_uuid+"/messages.txt";
    std::ifstream file(location);
    if (!file.is_open()) return "[]";

    std::string result = "[";
    std::string line;
    bool first = true;

    
    while (std::getline(file, line)) {
        std::string sender = "";
        if (line == "$friend") sender = req_uuid;
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
        result += "\"message\":\"" + json_escape(message) + "\",";
        result += "\"date\":\"" + date + "\",";
        result += "\"time\":\"" + time_str + "\"";
        result += "}";
    }

    result += "]";
    file.close();
    return result;
}


bool check_activity(std::string _uuid);

std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
    return out;
}


std::string make_json_friends() {           //funkcja zwracająca dane o znajomych
    std::string friends_path = "./friends";
    
    // sprawdzenie, czy katalog główny istnieje
    if (!fs::exists(friends_path) || !fs::is_directory(friends_path)) {
        return "[]";
    }

    std::string result = "[";
    bool first = true;

    // iteracja po wszystkich elementach w katalogu ./friends
    for (const auto& entry : fs::directory_iterator(friends_path)) {
        if (entry.is_directory()) {
            std::string uuid = entry.path().filename().string();
            
            std::string username_path = entry.path().string() + "/username.txt";
            std::ifstream user_file(username_path);
            std::string username = "";
            if (user_file.is_open()) {
                std::getline(user_file, username);
                user_file.close();
            } else {
                // jeżeli nie można odczytać pliku użytkownika, pomijamy ten katalog
                continue; 
            }

            // odczytanie ostatniej wiadomości z ./friends/[uuid]/messages.txt
            std::string messages_path = entry.path().string() + "/messages.txt";
            std::ifstream msg_file(messages_path);
            std::string last_message = "";
            std::string last_message_date = "";
            std::string last_message_time = "";
            if (msg_file.is_open()) {
                std::string line;
                // przechodzimy przez cały plik i nadpisujemy zmienną, 
                // dzięki czemu na końcu zostanie w niej ostatnia linijka z "message:"
                while (std::getline(msg_file, line)) {
                    if (line.rfind("message:", 0) == 0) {
                        last_message = line.substr(8);
                    }
                    if (line.rfind("date:", 0) == 0) {
                        last_message_date = line.substr(5);
                    }
                    if (line.rfind("time:", 0) == 0) {
                        last_message_time = line.substr(5);
                    }
                }
                msg_file.close();
            }

            // składanie tego w format json
            if (!first) result += ",";
            first = false;

            result += "{";
            result += "\"uuid\":\"" + uuid + "\",";
            result += "\"username\":\"" + /*html_escape*/(username) + "\",";
            result += "\"last_message\":\"" + json_escape(last_message) + "\",";
            result += "\"last_message_date\":\"" + last_message_date + "\",";
            result += "\"last_message_time\":\"" + last_message_time + "\",";
            
            if(selected_user == uuid)
                result += "\"selected\":\"true\",";
            else
                result += "\"selected\":\"false\",";
            
            if(check_activity(uuid)) {
                result += "\"status\":\"online\",";       
            } else {
                result += "\"status\":\"offline\",";
            }
            result += "\"unread\":\"false\",";       
            result += "\"notifications\":\""+std::to_string(notifications_count[uuid])+"\"";     
            result += "}";
        }
    }

    result += "]";
    return result;
}

std::string read_file(std::string filename) {       //funkcja służąca do czytania z pliku
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::streamsize size = file.tellg();
    
    file.seekg(0, std::ios::beg);

    std::string contents(size, '\0');

    if (!file.read(&contents[0], size)) {
        throw std::runtime_error("Error reading file: " + filename);
    }

    return contents;
}

std::string friend_ip(std::string _uuid) {      //funkcja zwracająca adres ip znajomego
    std::string ip_io_path = "./friends/" + _uuid + "/ip_io";
    std::ifstream file(ip_io_path);
    if (!file.is_open()) return "";
    std::string ip_addr;
    file >> ip_addr;
    return ip_addr;
}

int friend_iport(std::string _uuid) {           //funkcja zwracająca port o znajomego
    std::string ip_io_path = "./friends/" + _uuid + "/ip_io";
    std::ifstream file(ip_io_path);
    if (!file.is_open()) return -1;
    std::string ip_addr;
    int iport;
    file >> ip_addr >> iport;
    return iport;
}

int friend_oport(std::string _uuid) {           ////funkcja zwracająca port i znajomego
    std::string ip_io_path = "./friends/" + _uuid + "/ip_io";
    std::ifstream file(ip_io_path);
    if (!file.is_open()) return -1;
    std::string ip_addr;
    int iport, oport;
    file >> ip_addr >> iport >> oport;
    return oport;
}


std::string prepare_html(std::string html) {        //funckja przygotowująca html do wysłania do przeglądarki. zamienia $$nazwy specjalne$$ na moduły lub zmienne
    html = replace_string_pattern_with(html, "$$javascript$$", read_file(javascript));    //javascript u góry bo jest tam używana zmienna $$selected uuid$$
    html = replace_string_pattern_with(html, "$$messages$$", read_file(messages_module));
    html = replace_string_pattern_with(html, "$$options$$", read_file(options));
    html = replace_string_pattern_with(html, "$$input bar$$", read_file(input_bar));
    html = replace_string_pattern_with(html, "$$chat header$$", read_file(chat_header));
    html = replace_string_pattern_with(html, "$$friend list$$", read_file(friend_list));
    html = replace_string_pattern_with(html, "$$top bar$$", read_file(top_bar));
    html = replace_string_pattern_with(html, "$$settings hidden$$", read_file(settings_hidden));
    html = replace_string_pattern_with(html, "$$username$$", html_escape(read_file(username)));
    html = replace_string_pattern_with(html, "$$uuid$$", uuid);
    html = replace_string_pattern_with(html, "$$selected uuid$$", selected_user);
    html = replace_string_pattern_with(html, "$$server connection$$", read_file(server_state_file));
    std::string sel_username = "";
    try { if (!selected_user.empty()) sel_username = read_file("./friends/"+selected_user+"/username.txt"); } catch (...) {}
    html = replace_string_pattern_with(html, "$$selected user username$$", sel_username);
    html = replace_string_pattern_with(html, "$$friend ip$$", selected_user.empty() ? "" : friend_ip(selected_user));
    int sel_iport = selected_user.empty() ? -1 : friend_iport(selected_user);
    int sel_oport = selected_user.empty() ? -1 : friend_oport(selected_user);
    html = replace_string_pattern_with(html, "$$iport$$", sel_iport < 0 ? "" : std::to_string(sel_iport));
    html = replace_string_pattern_with(html, "$$oport$$", sel_oport < 0 ? "" : std::to_string(sel_oport));
    return html;
}


std::string make_http_response(const std::string& body, const std::string& content_type) {      //funkcja przygotowująca dane do wysłania zgodnie z protokołem http
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

void save_message(std::string requested_uuid, std::string message, std::string who) {       //funkcja zapisująca wiadomość
        // pobierz aktualną datę i czas
        std::time_t t = std::time(nullptr);
        std::tm* tm = std::localtime(&t);
        char date[11], time_str[9];
        std::strftime(date, sizeof(date), "%Y-%m-%d", tm);
        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", tm);
        
        // zapisz do pliku
        std::ofstream file("friends/" + requested_uuid + "/messages.txt", std::ios::app);
        file << "\n$"+who+"\n";
        file << "message:" << message << "\n";
        file << "date:" << date << "\n";
        file << "time:" << time_str << "\n";
        file << "$end\n";
        file.close();
}



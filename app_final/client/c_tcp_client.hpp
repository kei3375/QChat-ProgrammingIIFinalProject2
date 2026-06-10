#pragma once

#include "QLib.hpp"

//client for communication with coordinating server


// class tcp_c_client {
// public:
//     asio::io_context&    io_;
//     tcp::socket          socket_;
//     asio::steady_timer   timer_;

//     std::string          server_ip_;
//     uint16_t             server_port_;

//     bool                 registered_  = false;
//     std::string          c_uuid_      = "";
//     std::array<uint8_t, 32> aes_key_  {};

//     std::vector<uint8_t> write_buf_;
//     uint8_t              hdr_[5];
//     std::vector<uint8_t> payload_;

//     std::unordered_map<std::string, udp_server*>* user_connections_;

//     tcp_c_client(asio::io_context& io,
//                  const std::string& server_ip,
//                  uint16_t           server_port,
//                  std::unordered_map<std::string, udp_server*>* user_connections)
//         : io_(io), socket_(io), timer_(io),
//           server_ip_(server_ip), server_port_(server_port),
//           user_connections_(user_connections)
//     {
//         // Wczytaj UUID i klucz jeśli już mamy konto
//         try {
//             c_uuid_ = read_file(uuid_file);
//             while (!c_uuid_.empty() && (c_uuid_.back() == '\r' || c_uuid_.back() == '\n'))
//                 c_uuid_.pop_back();
//             aes_key_ = load_hex_key(c_aes_key_file);
//             registered_ = (c_uuid_.size() == 36);
//         } catch (...) {
//             registered_ = false;
//         }
//         c_server_uuid = c_uuid_;
//     }

//     void start() {
//         if (!connection_to_server_active) {
//             schedule_retry(5);
//             return;
//         }
//         do_connect();
//     }

//     void stop() {
//         asio::error_code ec;
//         timer_.cancel();
//         socket_.close(ec);
//     }

// private:

//     // ── Połączenie z serwerem ──
//     void do_connect() {
//         if (!connection_to_server_active) { schedule_retry(5); return; }

//         auto ep = tcp::endpoint(asio::ip::make_address(server_ip_), server_port_);
//         auto self = shared_from_this_workaround();
//         socket_.async_connect(ep, [this, self](const std::error_code& ec) {
//             if (ec) {
//                 std::cerr << "[c_client] Błąd połączenia z serwerem: " << ec.message() << "\n";
//                 reconnect();
//                 return;
//             }
//             std::cout << "[c_client] Połączono z serwerem koordynującym\n";

//             if (!registered_)
//                 do_register();
//             else
//                 do_routine();
//         });
//     }

//     // ── Rejestracja: wyślij klucz publiczny ECC ──
//     void do_register() {
//         std::string ecc_pub = read_file("./user_settings/ECC_PUB");
//         if (ecc_pub.size() < 32) {
//             std::cerr << "[c_client] Brak klucza ECC_PUB\n";
//             reconnect();
//             return;
//         }

//         // frame: [0x01][len 4B][32B ECC pub]
//         send_frame(0x01, std::string(ecc_pub.begin(), ecc_pub.begin() + 32),
//             [this](){ read_header_then([this](){ handle_register_response(); }); });
//     }

//     void handle_register_response() {
//         // payload: [36B uuid][reszta = ECC-encrypted AES key]
//         if (payload_.size() < 36 + 32 + 16 + 1) {
//             std::cerr << "[c_client] Za krótka odpowiedź rejestracji\n";
//             reconnect();
//             return;
//         }

//         c_uuid_ = std::string(reinterpret_cast<char*>(payload_.data()), 36);
//         std::string ecc_payload(reinterpret_cast<char*>(payload_.data() + 36),
//                                 payload_.size() - 36);

//         // odszyfruj AES key przez ECC (decrypt_ECC z main.cpp)
//         try {
//             aes_key_ = decrypt_ECC(ecc_payload);
//         } catch (const std::exception& e) {
//             std::cerr << "[c_client] Błąd deszyfrowania AES od serwera: " << e.what() << "\n";
//             reconnect();
//             return;
//         }

//         // Zapisz UUID i klucz
//         {
//             std::ofstream f(uuid_file);
//             f << c_uuid_;
//             uuid = c_uuid_;
//         }
//         save_hex_key(c_aes_key_file, aes_key_);
//         registered_ = true;
//         c_server_uuid = c_uuid_;

//         std::cout << "[c_client] Zarejestrowano, UUID: " << c_uuid_ << "\n";
//         do_routine();
//     }

//     // ── Routine (wysyłany co 2s) ──
//     void do_routine() {
//         if (!connection_to_server_active) { schedule_retry(2); return; }

//         // --- zbuduj payload (przed szyfrowaniem) ---
//         std::string local_ip = get_local_ip();
//         my_local_ip = local_ip;

//         // porty zajęte: zbieramy iport i oport każdego udp_server
//         std::unordered_set<uint16_t> busy;
//         for (auto& [u, srv] : *user_connections_) {
//             if (srv == nullptr) continue;
//             // wyciągamy lokalny port gniazda
//             try {
//                 uint16_t lp = srv->socket.local_endpoint().port();
//                 busy.insert(lp);
//             } catch (...) {}
//         }

//         // lista UUID znajomych z którymi NIE ma jeszcze aktywnego połączenia
//         std::vector<std::string> targets;
//         for (auto& [u, srv] : *user_connections_) {
//             if (!check_activity(u))   // nieaktywny = chcemy się połączyć
//                 targets.push_back(u);
//         }

//         // --- serializuj ---
//         std::string raw;

//         // local_ip
//         uint8_t ip_len = static_cast<uint8_t>(std::min<size_t>(local_ip.size(), 255));
//         raw.push_back(static_cast<char>(ip_len));
//         raw += local_ip.substr(0, ip_len);

//         // liczba zajętych portów
//         uint16_t n_ports = static_cast<uint16_t>(busy.size());
//         raw.push_back((n_ports >> 8) & 0xFF);
//         raw.push_back(n_ports & 0xFF);
//         for (uint16_t p : busy) {
//             raw.push_back((p >> 8) & 0xFF);
//             raw.push_back(p & 0xFF);
//         }

//         // liczba targetów
//         uint16_t n_targets = static_cast<uint16_t>(targets.size());
//         raw.push_back((n_targets >> 8) & 0xFF);
//         raw.push_back(n_targets & 0xFF);
//         for (const auto& t : targets)
//             raw += t;   // 36 znaków UUID

//         // --- zaszyfruj AES ---
//         std::vector<uint8_t> encrypted = aes_encrypt(raw, aes_key_);

//         // --- zbuduj wiadomość: [36B uuid][encrypted] ---
//         std::string msg = c_uuid_;
//         msg += std::string(encrypted.begin(), encrypted.end());

//         send_frame(0x03, msg, [this](){ read_header_then([this](){ handle_routine_response(); }); });
//     }

//     void handle_routine_response() {
//         // payload = AES-encrypted odpowiedź serwera
//         std::vector<uint8_t> enc(payload_.begin(), payload_.end());
//         std::string decrypted;
//         try {
//             decrypted = aes_decrypt(enc, aes_key_);
//         } catch (const std::exception& e) {
//             std::cerr << "[c_client] Błąd deszyfrowania routine response: " << e.what() << "\n";
//             schedule_retry(2);
//             return;
//         }

//         size_t offset = 0;
//         if (decrypted.size() < 2) { schedule_retry(2); return; }

//         uint16_t num_peers = (static_cast<uint8_t>(decrypted[0]) << 8)
//                            |  static_cast<uint8_t>(decrypted[1]);
//         offset = 2;

//         found_peers.clear();

//         for (uint16_t i = 0; i < num_peers && offset < decrypted.size(); ++i) {
//             PeerConnectionInfo peer;

//             if (offset + 36 > decrypted.size()) break;
//             peer.uuid = decrypted.substr(offset, 36);
//             offset += 36;

//             if (offset + 1 > decrypted.size()) break;
//             uint8_t ip_len = static_cast<uint8_t>(decrypted[offset++]);
//             if (offset + ip_len > decrypted.size()) break;
//             peer.ip = decrypted.substr(offset, ip_len);
//             offset += ip_len;

//             if (offset + 4 > decrypted.size()) break;
//             peer.iport = (static_cast<uint8_t>(decrypted[offset]) << 8)
//                        |  static_cast<uint8_t>(decrypted[offset + 1]);
//             offset += 2;
//             peer.oport = (static_cast<uint8_t>(decrypted[offset]) << 8)
//                        |  static_cast<uint8_t>(decrypted[offset + 1]);
//             offset += 2;

//             found_peers.push_back(peer);
//             std::cout << "[c_client] Znaleziono peera: " << peer.uuid
//                       << " " << peer.ip << ":" << peer.iport << "/" << peer.oport << "\n";

//             // Zaktualizuj ip_io i przeładuj udp_server jeśli znajomy jest w liście
//             if (user_connections_->count(peer.uuid)) {
//                 std::string ip_io_path = "./friends/" + peer.uuid + "/ip_io";
//                 std::ofstream fout(ip_io_path);
//                 fout << peer.ip << "\n" << peer.iport << "\n" << peer.oport << "\n";
//                 fout.close();

//                 auto& conn = (*user_connections_)[peer.uuid];
//                 if (conn == nullptr) {
//                     try {
//                         conn = new udp_server(io_, peer.iport, peer.oport, peer.ip, peer.uuid);
//                     } catch (const std::exception& e) {
//                         std::cerr << "[c_client] Błąd tworzenia udp_server: " << e.what() << "\n";
//                     }
//                 } else {
//                     conn->change_target(peer.iport, peer.oport, peer.ip);
//                 }
//             }
//         }

//         // Wyciągnij publiczne IP z nagłówka TCP (to co serwer widzi)
//         try {
//             // my_public_ip = socket_.local_endpoint().address().to_string();
//         } catch (...) {}

//         // Zaplanuj kolejne routine za 2 sekundy
//         schedule_retry(2);
//     }

//     // ── Wysyłanie ramki ──
//     void send_frame(uint8_t type, const std::string& data, std::function<void()> on_done) {
//         uint32_t len = static_cast<uint32_t>(data.size());
//         write_buf_.resize(5 + len);
//         write_buf_[0] = type;
//         write_buf_[1] = (len >> 24) & 0xFF;
//         write_buf_[2] = (len >> 16) & 0xFF;
//         write_buf_[3] = (len >> 8)  & 0xFF;
//         write_buf_[4] =  len        & 0xFF;
//         std::copy(data.begin(), data.end(), write_buf_.begin() + 5);

//         auto self = shared_from_this_workaround();
//         asio::async_write(socket_, asio::buffer(write_buf_),
//             [this, self, on_done](const std::error_code& ec, size_t) {
//                 if (ec) { reconnect(); return; }
//                 on_done();
//             });
//     }

//     // ── Odczyt nagłówka (5 bajtów) ──
//     void read_header_then(std::function<void()> on_payload) {
//         auto self = shared_from_this_workaround();
//         asio::async_read(socket_, asio::buffer(hdr_, 5),
//             [this, self, on_payload](const std::error_code& ec, size_t) {
//                 if (ec) { reconnect(); return; }
//                 uint32_t len = (static_cast<uint32_t>(hdr_[1]) << 24)
//                              | (static_cast<uint32_t>(hdr_[2]) << 16)
//                              | (static_cast<uint32_t>(hdr_[3]) << 8)
//                              |  static_cast<uint32_t>(hdr_[4]);
//                 payload_.resize(len);
//                 asio::async_read(socket_, asio::buffer(payload_),
//                     [this, self, on_payload](const std::error_code& ec2, size_t) {
//                         if (ec2) { reconnect(); return; }
//                         on_payload();
//                     });
//             });
//     }

//     // ── Reconnect / retry ──
//     void reconnect() {
//         asio::error_code ec;
//         socket_.close(ec);
//         socket_ = tcp::socket(io_);
//         schedule_retry(5);
//     }

//     void schedule_retry(int seconds) {
//         timer_.expires_after(std::chrono::seconds(seconds));
//         auto self = shared_from_this_workaround();
//         timer_.async_wait([this, self](const std::error_code& ec) {
//             if (ec) return;
//             if (!connection_to_server_active) { schedule_retry(5); return; }
//             if (!socket_.is_open())
//                 do_connect();
//             else
//                 do_routine();
//         });
//     }

//     // ── Pobierz lokalny IP ──
//     std::string get_local_ip() {
//         try {
//             tcp::socket tmp(io_);
//             tmp.connect(tcp::endpoint(asio::ip::make_address("8.8.8.8"), 80));
//             return tmp.local_endpoint().address().to_string();
//         } catch (...) {
//             return "127.0.0.1";
//         }
//     }

//     // shared_from_this workaround — tcp_c_client nie dziedziczy po enable_shared_from_this
//     std::shared_ptr<tcp_c_client> shared_from_this_workaround() {
//         return self_ptr_;
//     }

// public:
//     std::shared_ptr<tcp_c_client> self_ptr_;   // ustawiamy po konstruktorze
// };

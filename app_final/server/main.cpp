// server.cpp
#include <iostream>
#include <asio.hpp>
#include <vector>
#include <fstream>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <memory>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

using asio::ip::tcp;
namespace fs = std::filesystem;

struct ClientSessionInfo {
    std::string uuid;
    std::string public_ip;
    std::string local_ip;
    std::unordered_set<uint16_t> unavailable_ports;
    std::vector<std::string> desired_uuids;
    std::chrono::steady_clock::time_point last_seen;
    std::array<uint8_t, 32> aes_key;
};

// Globalny stan aplikacji jednowątkowej
std::unordered_map<std::string, ClientSessionInfo> active_sessions;

// Bezpieczne generowanie UUID v4 przez OpenSSL
std::string generate_uuid() {
    std::array<uint8_t, 16> bytes;
    if (RAND_bytes(bytes.data(), bytes.size()) != 1) {
        throw std::runtime_error("RAND_bytes failed during UUID generation");
    }
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    char buf[37];
    snprintf(buf, sizeof(buf),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return std::string(buf);
}

void save_aes_key(const std::string& uuid, const std::array<uint8_t, 32>& key) {
    fs::create_directories("./users/" + uuid);
    std::ofstream f("./users/" + uuid + "/AES");
    if (!f.is_open()) throw std::runtime_error("Nie udalo sie utworzyc pliku klucza AES");
    for (int i = 0; i < 32; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", key[i]);
        f << buf;
    }
}

std::array<uint8_t, 32> load_aes_key(const std::string& uuid) {
    std::ifstream f("./users/" + uuid + "/AES");
    if (!f.is_open()) throw std::runtime_error("Brak klucza AES dla uzytkownika: " + uuid);
    std::string hex;
    std::getline(f, hex);
    while (!hex.empty() && (hex.back() == '\r' || hex.back() == '\n')) {
        hex.pop_back();
    }
    if (hex.size() != 64) throw std::runtime_error("Zly format pliku klucza AES");
    std::array<uint8_t, 32> key{};
    for (int i = 0; i < 32; ++i) {
        key[i] = static_cast<uint8_t>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
    }
    return key;
}

std::vector<uint8_t> aes_encrypt(const std::string& plaintext, const std::array<uint8_t, 32>& key) {
    std::array<uint8_t, 16> iv;
    if (RAND_bytes(iv.data(), 16) != 1) throw std::runtime_error("RAND_bytes failed");

    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx || !EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key.data(), iv.data())) {
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }

    std::vector<uint8_t> ciphertext(plaintext.size() + 16);
    int len = 0, total = 0;
    if (!EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &len, reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size())) {
        throw std::runtime_error("EVP_EncryptUpdate failed");
    }
    total = len;
    if (!EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + total, &len)) {
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }
    total += len;
    ciphertext.resize(total);

    std::vector<uint8_t> result(iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    return result;
}

std::string aes_decrypt(const std::vector<uint8_t>& input, const std::array<uint8_t, 32>& key) {
    if (input.size() < 16) throw std::runtime_error("Input too short");
    std::array<uint8_t, 16> iv;
    std::copy(input.begin(), input.begin() + 16, iv.begin());

    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx || !EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key.data(), iv.data())) {
        throw std::runtime_error("EVP_DecryptInit_ex failed");
    }

    std::vector<uint8_t> plaintext(input.size() - 16);
    int len = 0, total = 0;
    if (!EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, input.data() + 16, input.size() - 16)) {
        throw std::runtime_error("EVP_DecryptUpdate failed");
    }
    total = len;
    if (!EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + total, &len)) {
        throw std::runtime_error("EVP_DecryptFinal_ex failed - wrong key or corrupted data");
    }
    total += len;
    return std::string(plaintext.begin(), plaintext.begin() + total);
}

std::string encrypt_ECC(std::array<uint8_t, 32> AES_key, std::array<uint8_t, 32> public_ecc_key) {
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> kctx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr), EVP_PKEY_CTX_free);
    if (!kctx) throw std::runtime_error("EVP_PKEY_CTX_new_id failed");

    EVP_PKEY* raw_ephemeral = nullptr;
    if (EVP_PKEY_keygen_init(kctx.get()) <= 0 || EVP_PKEY_keygen(kctx.get(), &raw_ephemeral) <= 0) {
        throw std::runtime_error("EVP_PKEY_keygen failed");
    }
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> ephemeral_key(raw_ephemeral, EVP_PKEY_free);

    std::array<uint8_t, 32> ephemeral_pub{};
    size_t pub_len = 32;
    if (EVP_PKEY_get_raw_public_key(ephemeral_key.get(), ephemeral_pub.data(), &pub_len) <= 0) {
        throw std::runtime_error("EVP_PKEY_get_raw_public_key failed");
    }

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> peer_pub(
        EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, public_ecc_key.data(), 32), EVP_PKEY_free);
    if (!peer_pub) throw std::runtime_error("EVP_PKEY_new_raw_public_key failed");

    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> dctx(EVP_PKEY_CTX_new(ephemeral_key.get(), nullptr), EVP_PKEY_CTX_free);
    if (!dctx || EVP_PKEY_derive_init(dctx.get()) <= 0 || EVP_PKEY_derive_set_peer(dctx.get(), peer_pub.get()) <= 0) {
        throw std::runtime_error("ECDH initialization failed");
    }

    size_t secret_len = 32;
    std::array<uint8_t, 32> shared_secret{};
    if (EVP_PKEY_derive(dctx.get(), shared_secret.data(), &secret_len) <= 0) {
        throw std::runtime_error("EVP_PKEY_derive failed");
    }

    std::array<uint8_t, 32> wrap_key{};
    SHA256(shared_secret.data(), 32, wrap_key.data());

    std::string aes_key_str(reinterpret_cast<char*>(AES_key.data()), 32);
    std::vector<uint8_t> encrypted_aes = aes_encrypt(aes_key_str, wrap_key);

    std::string result(reinterpret_cast<char*>(ephemeral_pub.data()), 32);
    result += std::string(reinterpret_cast<char*>(encrypted_aes.data()), encrypted_aes.size());
    return result;
}

std::pair<uint16_t, uint16_t> find_available_ports(const std::unordered_set<uint16_t>& unavailA, const std::unordered_set<uint16_t>& unavailB) {
    uint16_t p1 = 0, p2 = 0;
    for (uint32_t port = 8888; port <= 30000; ++port) {
        if (unavailA.find(port) == unavailA.end() && unavailB.find(port) == unavailB.end()) {
            if (p1 == 0) p1 = port;
            else { p2 = port; break; }
        }
    }
    return {p1, p2};
}

std::vector<uint8_t> build_routine_response(const std::string& client_uuid, const std::string& client_pub_ip, 
                                            const std::unordered_set<uint16_t>& client_unavail, 
                                            const std::vector<std::string>& client_targets, 
                                            const std::array<uint8_t, 32>& aes_key) {
    struct MatchData { std::string uuid; std::string ip; uint16_t port1; uint16_t port2; };
    std::vector<MatchData> matched_peers;

    for (const auto& target_uuid : client_targets) {
        auto it = active_sessions.find(target_uuid);
        if (it != active_sessions.end()) {
            const auto& peer = it->second;
            if (std::find(peer.desired_uuids.begin(), peer.desired_uuids.end(), client_uuid) != peer.desired_uuids.end()) {
                MatchData match;
                match.uuid = peer.uuid;
                match.ip = (client_pub_ip == peer.public_ip) ? peer.local_ip : peer.public_ip;

                auto [p1, p2] = find_available_ports(client_unavail, peer.unavailable_ports);
                if (client_uuid < peer.uuid) { match.port1 = p2; match.port2 = p1; }
                else { match.port1 = p1; match.port2 = p2; }
                matched_peers.push_back(match);
            }
        }
    }

    std::string raw_payload;
    uint16_t num_peers = static_cast<uint16_t>(matched_peers.size());
    raw_payload.push_back((num_peers >> 8) & 0xFF);
    raw_payload.push_back(num_peers & 0xFF);

    for (const auto& m : matched_peers) {
        raw_payload += m.uuid;
        uint8_t ip_len = static_cast<uint8_t>(std::min<size_t>(m.ip.size(), 255));
        raw_payload.push_back(ip_len);
        raw_payload += m.ip.substr(0, ip_len);
        raw_payload.push_back((m.port1 >> 8) & 0xFF);
        raw_payload.push_back(m.port1 & 0xFF);
        raw_payload.push_back((m.port2 >> 8) & 0xFF);
        raw_payload.push_back(m.port2 & 0xFF);
    }
    return aes_encrypt(raw_payload, aes_key);
}

void process_routine_request(const std::string& client_uuid, const std::vector<uint8_t>& encrypted_part, 
                             const std::string& remote_public_ip, std::vector<uint8_t>& out_response) {
    std::array<uint8_t, 32> aes_key;
    auto it_session = active_sessions.find(client_uuid);
    if (it_session != active_sessions.end()) {
        aes_key = it_session->second.aes_key;
    } else {
        aes_key = load_aes_key(client_uuid);
    }

    std::string decrypted = aes_decrypt(encrypted_part, aes_key);
    size_t offset = 0;
    if (decrypted.size() < 1) throw std::runtime_error("Zla dlugosc pakietu (IP len)");
    
    uint8_t ip_len = static_cast<uint8_t>(decrypted[offset++]);
    if (decrypted.size() < offset + ip_len + 2) throw std::runtime_error("Zla dlugosc pakietu (IP/Ports count)");
    
    std::string local_ip = decrypted.substr(offset, ip_len);
    offset += ip_len;

    uint16_t n_ports = (static_cast<uint8_t>(decrypted[offset]) << 8) | static_cast<uint8_t>(decrypted[offset+1]);
    offset += 2;

    if (n_ports > 100) throw std::runtime_error("Odmowa uslugi: Zbyt duza tablica portow");
    if (decrypted.size() < offset + 2 * n_ports + 2) throw std::runtime_error("Zla dlugosc pakietu (Ports array)");
    
    std::unordered_set<uint16_t> unavail_ports;
    for (uint16_t i = 0; i < n_ports; ++i) {
        uint16_t port = (static_cast<uint8_t>(decrypted[offset]) << 8) | static_cast<uint8_t>(decrypted[offset+1]);
        offset += 2;
        unavail_ports.insert(port);
    }

    uint16_t n_targets = (static_cast<uint8_t>(decrypted[offset]) << 8) | static_cast<uint8_t>(decrypted[offset+1]);
    offset += 2;

    if (n_targets > 32) throw std::runtime_error("Odmowa uslugi: Zbyt duza lista celow");
    if (decrypted.size() < offset + 36 * n_targets) throw std::runtime_error("Zla dlugosc pakietu (Targets list)");
    
    std::vector<std::string> desired_uuids;
    for (uint16_t i = 0; i < n_targets; ++i) {
        std::string t_uuid = decrypted.substr(offset, 36);
        offset += 36;
        desired_uuids.push_back(t_uuid);
    }

    // --- NOWA SEKCOJA: Logowanie intencji połączeń pomiędzy użytkownikami ---
    if (!desired_uuids.empty()) {
        std::cout << "[Żądanie Połączenia] Klient " << client_uuid << " chce połączyć się z:" << std::endl;
        for (const auto& target : desired_uuids) {
            auto it = active_sessions.find(target);
            if (it != active_sessions.end()) {
                // Sprawdzamy czy cel również chce rozmawiać z nadawcą (obustronny match)
                const auto& peer_targets = it->second.desired_uuids;
                bool mutual = (std::find(peer_targets.begin(), peer_targets.end(), client_uuid) != peer_targets.end());
                
                std::cout << "  -> " << target << (mutual ? " [DOPASOWANIE / OBUSTRONNE]" : " [ONLINE, ale cel jeszcze nie odwzajemnił żądania]") << std::endl;
            } else {
                std::cout << "  -> " << target << " [OFFLINE / BRAK AKTYWNEJ SESJI]" << std::endl;
            }
        }
    }

    ClientSessionInfo info{client_uuid, remote_public_ip, local_ip, std::move(unavail_ports), std::move(desired_uuids), std::chrono::steady_clock::now(), aes_key};
    active_sessions[client_uuid] = info;

    out_response = build_routine_response(client_uuid, remote_public_ip, active_sessions[client_uuid].unavailable_ports, active_sessions[client_uuid].desired_uuids, aes_key);
}

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}
    void start() {
        try {
            remote_ip_ = socket_.remote_endpoint().address().to_string();
            remote_port_ = socket_.remote_endpoint().port();
            std::cout << "[Połączenie] Nowy klient z adresu: " << remote_ip_ << ":" << remote_port_ << std::endl;
        } catch (...) {
            remote_ip_ = "0.0.0.0"; remote_port_ = 0;
        }
        read_header();
    }
    ~Session() {
        std::cout << "[Rozłączenie] Sesja dla " << remote_ip_ << ":" << remote_port_ << " (UUID: " << (associated_uuid_.empty() ? "Niezarejestrowany" : associated_uuid_) << ") zakończona." << std::endl;
    }
private:
    void read_header() {
        auto self = shared_from_this();
        asio::async_read(socket_, asio::buffer(header_, 5), [this, self](asio::error_code ec, size_t) {
            if (!ec) {
                uint8_t type = header_[0];
                uint32_t len = (static_cast<uint32_t>(header_[1]) << 24) | (static_cast<uint32_t>(header_[2]) << 16) | (static_cast<uint32_t>(header_[3]) << 8) | static_cast<uint32_t>(header_[4]);
                if (len > 512 * 1024) return; 
                read_payload(type, len);
            } else { close_session(); }
        });
    }
    void read_payload(uint8_t type, uint32_t len) {
        auto self = shared_from_this();
        payload_.resize(len);
        asio::async_read(socket_, asio::buffer(payload_), [this, self, type](asio::error_code ec, size_t) {
            if (!ec) {
                try { process_payload(type); }
                catch (const std::exception& e) { std::cerr << "[Sesja] Blad: " << e.what() << std::endl; close_session(); }
            } else { close_session(); }
        });
    }
    void process_payload(uint8_t type) {
        if (type == 0x01) {
            if (payload_.size() < 32) return;
            std::array<uint8_t, 32> client_ecc_pub;
            std::copy(payload_.begin(), payload_.begin() + 32, client_ecc_pub.begin());

            std::string new_uuid = generate_uuid();
            associated_uuid_ = new_uuid;
            std::cout << "[Rejestracja] Nowy profil dla: " << remote_ip_ << " -> UUID: " << new_uuid << std::endl;

            std::array<uint8_t, 32> new_aes_key;
            if (RAND_bytes(new_aes_key.data(), 32) != 1) throw std::runtime_error("RAND_bytes failed");
            save_aes_key(new_uuid, new_aes_key);

            std::string ecc_encrypted = encrypt_ECC(new_aes_key, client_ecc_pub);
            send_frame(0x02, new_uuid + ecc_encrypted);
        } else if (type == 0x03) {
            if (payload_.size() < 36) return;
            std::string client_uuid(reinterpret_cast<char*>(payload_.data()), 36);
            associated_uuid_ = client_uuid;

            std::vector<uint8_t> encrypted_part(payload_.begin() + 36, payload_.end());
            std::vector<uint8_t> encrypted_response;
            process_routine_request(client_uuid, encrypted_part, remote_ip_, encrypted_response);

            std::cout << "[Routine] Check-in od UUID: " << client_uuid << " [" << remote_ip_ << "]" << std::endl;
            send_frame(0x04, std::string(encrypted_response.begin(), encrypted_response.end()));
        } else { close_session(); }
    }
    void send_frame(uint8_t type, const std::string& data) {
        auto self = shared_from_this();
        uint32_t len = static_cast<uint32_t>(data.size());
        write_buffer_.resize(5 + len);
        write_buffer_[0] = type;
        write_buffer_[1] = (len >> 24) & 0xFF; write_buffer_[2] = (len >> 16) & 0xFF; write_buffer_[3] = (len >> 8) & 0xFF; write_buffer_[4] = len & 0xFF;
        std::copy(data.begin(), data.end(), write_buffer_.begin() + 5);

        asio::async_write(socket_, asio::buffer(write_buffer_), [this, self](asio::error_code ec, size_t) {
            if (!ec) { read_header(); } else { close_session(); }
        });
    }
    void close_session() { asio::error_code ec; socket_.close(ec); }

    tcp::socket socket_;
    std::string remote_ip_;
    uint16_t remote_port_ = 0;
    std::string associated_uuid_;
    uint8_t header_[5];
    std::vector<uint8_t> payload_;
    std::vector<uint8_t> write_buffer_;
};

class Server {
public:
    Server(asio::io_context& io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) { do_accept(); }
private:
    void do_accept() {
        acceptor_.async_accept([this](asio::error_code ec, tcp::socket socket) {
            if (!ec) { std::make_shared<Session>(std::move(socket))->start(); }
            do_accept();
        });
    }
    tcp::acceptor acceptor_;
};

void start_cleanup_timer(asio::steady_timer& timer) {
    timer.expires_after(std::chrono::seconds(5));
    timer.async_wait([&timer](const asio::error_code& ec) {
        if (!ec) {
            auto now = std::chrono::steady_clock::now();
            for (auto it = active_sessions.begin(); it != active_sessions.end();) {
                if (now - it->second.last_seen > std::chrono::seconds(30)) {
                    std::cout << "[Watchdog] Sesja WYGASŁA dla UUID: " << it->first << std::endl;
                    it = active_sessions.erase(it);
                } else { ++it; }
            }
            start_cleanup_timer(timer);
        }
    });
}

int main() {
    try {
        asio::io_context io_context;
        Server server(io_context, 81);
        std::cout << "[Serwer] Uruchomiono na porcie 81..." << std::endl;
        asio::steady_timer cleanup_timer(io_context);
        start_cleanup_timer(cleanup_timer);
        io_context.run();
    } catch (const std::exception& e) { std::cerr << "[Serwer] Krytyczny blad: " << e.what() << std::endl; }
    return 0;
}
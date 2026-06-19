//main.cpp




#include "QLib.hpp"



#define C_SERVER_IP   "144.24.174.135"
#define C_SERVER_PORT 81

bool connection_to_server_active = false;


using asio::ip::tcp;
using asio::ip::udp;





class udp_server;

std::string my_local_ip   = "";
std::string public_ip   = "";
uint16_t    my_iport      = 0;
uint16_t    my_oport      = 0;
std::string my_public_ip  = "";


const std::string uuid_file       = "./user_settings/uuid.txt";
const std::string c_aes_key_file    = "./user_settings/c_aes.txt";    //klucz AES serwera koordynującego

std::string c_server_uuid = "";   // UUID nadany przez serwer koordynujący

// ── Struktura z danymi o znalezionym peeru ──
struct PeerConnectionInfo {
    std::string uuid;
    std::string ip;
    uint16_t iport;   // port na którym mamy SŁUCHAĆ
    uint16_t oport;   // port na który mamy WYSYŁAĆ
};

// wektor globalnie dostępnych danych o ostatnio znalezionych peerach
std::vector<PeerConnectionInfo> found_peers;
// ─────────────────────────────────────────────





struct udp_packet {     //struktura ułatwiająca zarządzanie pakietami udp zgodnie z dokumentacją

    // char header; ///always F, so not needed here
    char type;
    char quantity_byte;
    uint32_t messageID;
    uint64_t packetID;          // 2 MSB are ignored
    std::array<char, 495> message;
    uint32_t checksum;

    udp_packet(char T, char Q, uint32_t _MID, uint64_t _PID, std::array<char, 495> _content)
        : type(T), quantity_byte(Q), messageID(_MID), packetID(_PID), message(_content) 
    {
        checksum = CRC32(_content);
    }
    udp_packet(char T, char Q, uint32_t _MID, uint64_t _PID, const std::string& _content) 
        : type(T), quantity_byte(Q), messageID(_MID), packetID(_PID)
    {
        message.fill(0); 
        std::copy(_content.begin(), 
                  _content.begin() + std::min<size_t>(_content.size(), 495), 
                  message.begin());
                  
        checksum = CRC32(message); 
    }

    std::array<char, 512> get_bytes() const {
        std::array<char, 512> bytes {};
        
        bytes[0] = 'F';
        bytes[1] = type;
        bytes[2] = quantity_byte;
        
        bytes[3] = static_cast<char>((messageID >> 24) & 0xFF);    
        bytes[4] = static_cast<char>((messageID >> 16) & 0xFF); 
        bytes[5] = static_cast<char>((messageID >> 8)  & 0xFF);
        bytes[6] = static_cast<char>((messageID)       & 0xFF);
        
        bytes[7]  = static_cast<char>((packetID >> 40) & 0xFF);    
        bytes[8]  = static_cast<char>((packetID >> 32) & 0xFF); 
        bytes[9]  = static_cast<char>((packetID >> 24) & 0xFF);
        bytes[10] = static_cast<char>((packetID >> 16) & 0xFF);
        bytes[11] = static_cast<char>((packetID >> 8)  & 0xFF);
        bytes[12] = static_cast<char>((packetID)       & 0xFF);
        
        for(size_t i = 0; i < message.size(); ++i) {
            bytes[13 + i] = message[i];
        }
        
        bytes[508] = static_cast<char>((checksum >> 24) & 0xFF);    
        bytes[509] = static_cast<char>((checksum >> 16) & 0xFF); 
        bytes[510] = static_cast<char>((checksum >> 8)  & 0xFF);
        bytes[511] = static_cast<char>((checksum)       & 0xFF);

        return bytes; 
    }

};


uint32_t get_messageID() {
    return (uint32_t)(rand()*2-13);
}



class udp_server            //klasa udp_server, obsługująca połączenie udp z jednym użytkownikiem na konkretnych portach. działa w trybie asynchronicznym
{
public:
    asio::io_context * io;
    udp::socket socket;
    udp::endpoint remote_endpoint;
    udp::endpoint send_endpoint;
    std::array<char, 512> recv_buffer;
    asio::steady_timer t_udp_punch;
    asio::steady_timer t_file_transmit;

    


    std::string username;
    bool stopped = false;

    std::string udp_uuid;

    udp_server(asio::io_context& io_context, int iport, int oport, std::string ip_address, std::string _uuid)
        : socket(io_context, udp::endpoint(udp::v4(), iport)), t_udp_punch(io_context, asio::chrono::milliseconds(1)), udp_uuid(_uuid), t_file_transmit(io_context, asio::chrono::milliseconds(3))
    {
        io = &io_context;
        udp::resolver res(io_context);
        // kbd_t = asio::steady_timer(io_context, asio::chrono::milliseconds(1));
        send_endpoint = *res.resolve(udp::v4(), ip_address, std::to_string(oport)).begin();
        start_receive();
        udp_punch(&t_udp_punch);

        std::fill_n(recv_buffer.begin(), 64, 0); // clear shit

        username = read_file("./friends/"+udp_uuid+"/username.txt");
    }
    
    void change_target(int iport, int oport, std::string ip_addr) {
        // udp_uuid = _uuid;
        socket.close();
        socket = udp::socket(*io, udp::endpoint(udp::v4(), iport));
        // kbd_t(io_context, asio::chrono::milliseconds(1)), 
        // t_udp_punch(io_context, asio::chrono::milliseconds(1))
        udp::resolver res(*io);
        send_endpoint = *res.resolve(udp::v4(), ip_addr, std::to_string(oport)).begin();
        // udp_punch(&t_udp_punch);
        
        std::cout<<iport<<"\n"<<oport<<"\n"<<ip_addr<<std::endl;
        
        std::fill_n(recv_buffer.begin(), 64, 0); // clear shit
        start_receive();

    }

    void udp_punch(asio::steady_timer * t) {

        if(stopped) return;

        std::string udp_punching_content = "N";
        udp_punching_content += read_file("./user_settings/name.txt");


        udp_packet packet('O', 0, (uint32_t)(rand()), (uint64_t)(rand()), udp_punching_content);



        send_arr(packet.get_bytes());

        t->expires_at(t->expiry() + asio::chrono::milliseconds(333));
        t->async_wait(std::bind(&udp_server::udp_punch, this, t));
    }

    void start_receive() {
        if (stopped) return;
        socket.async_receive_from(
            asio::buffer(recv_buffer), remote_endpoint,
            std::bind(&udp_server::handle_receive, this,
                asio::placeholders::error,
                asio::placeholders::bytes_transferred));
    }

    void handle_receive(const std::error_code& error, std::size_t /*bytes_transferred*/)    //obsłużenie odebranych pakietów
    {
        if (stopped) return;
        if (error) {
            if (error != asio::error::operation_aborted)
                std::cerr << "handle_receive error: " << error.message() << "\n";
            return;
        }
        if (!error)
        {           
            
            last_packets_times[udp_uuid] = std::chrono::system_clock::now();
            
            bool is_empty = true;
            
            for(int m{}; m<recv_buffer.size(); ++m) {
                if (recv_buffer[m] != 0 && recv_buffer[m] != ' ')   is_empty = false;
            }
            
            std::array<char, 495> actual_data{};
            for(int i = 13; i<=507; ++i) {
                actual_data[i-13] = recv_buffer[i];
            }

            uint32_t received_checksum =
                ((uint8_t)recv_buffer[508] << 24) |
                ((uint8_t)recv_buffer[509] << 16) |
                ((uint8_t)recv_buffer[510] << 8)  |
                ((uint8_t)recv_buffer[511]);

            //= [everything from recv_buffer[13] to recv_buffer[507]];
            
            bool is_checksum_correct = false;

            if(CRC32(actual_data) == received_checksum) is_checksum_correct = true;
            
            if(!is_empty && recv_buffer[0] == 'F' && is_checksum_correct) {
                if(recv_buffer[1] == 'T') {
                    
                    uint16_t enc_len = ((uint8_t)actual_data[0] << 8) | (uint8_t)actual_data[1];
                    
                    if (enc_len == 0 || enc_len > 493) {
                        std::cerr << "Nieprawidłowa długość: " << enc_len << "\n";
                        std::fill_n(recv_buffer.begin(), 512, 0);
                        start_receive();
                        return;
                    }

                    std::vector<uint8_t> encrypted(actual_data.begin() + 2, actual_data.begin() + 2 + enc_len);

                    std::string msg;
                    try {
                        auto key = load_key(udp_uuid);
                        msg = aes_decrypt(encrypted, key);
                    } catch (const std::exception& e) {
                        std::cerr << "Błąd deszyfrowania od " << udp_uuid << ": " << e.what() << "\n";
                        std::fill_n(recv_buffer.begin(), 512, 0);
                        start_receive();
                        return;
                    }

                    if (!msg.empty()) {
                        std::cout << "otrzymano pakiet tekstowy od " << remote_endpoint.address().to_string() << "\n";
                        save_message(udp_uuid, msg, "friend");
                        std::cout << "message received\n message: " << msg << "\n";
                        if(udp_uuid != selected_user)   ++notifications_count[udp_uuid];
                    }
                    
                } else if( recv_buffer[1] == 'O' ) {
                    if(actual_data[0] == 'N') {
                        std::string new_username(recv_buffer.data() + 14);
                        // przytnij zera
                        new_username = new_username.substr(0, new_username.find('\0'));
                        
                        std::string username_path = "./friends/" + udp_uuid + "/username.txt";
                        
                        
                        if (new_username != username && !new_username.empty()) {
                            if (new_username.size() > 64) new_username = new_username.substr(0, 64);
                            auto nl = new_username.find_first_of("\r\n");
                            if (nl != std::string::npos) new_username = new_username.substr(0, nl);

                            if (!new_username.empty()) {
                                fs::remove(username_path);
                                std::ofstream wf(username_path);
                                wf << html_escape(new_username);
                                wf.close();
                                username = new_username;
                            }
                                
                        }
                    } else if(actual_data[0] == 'P') {
                        std::cout<<"received public ECC key from "<<udp_uuid<<"\n";
                        //check if bytes [1-32] == [33-64] == [65-96]
                        bool are_consistent = true;
                        for(int i{}; i<32; ++i) {
                            if(actual_data[1+i] == actual_data[33+i] && actual_data[33+i] == actual_data[65+i]);
                            else are_consistent = false;
                        }

                        //check checksum of actual data
                        uint32_t received_checksum =
                            ((uint8_t)recv_buffer[508] << 24) |
                            ((uint8_t)recv_buffer[509] << 16) |
                            ((uint8_t)recv_buffer[510] << 8)  |
                            ((uint8_t)recv_buffer[511]
                        );
                        if(CRC32(actual_data) == received_checksum && are_consistent) {
                            // send_ECC_public_key();
                            std::array<uint8_t,32> received_ecc_public_key{};

                            for(int i{}; i<32; ++i) {
                                received_ecc_public_key[i] = actual_data[1+i];
                            }

                            send_encrypted_AES_key(received_ecc_public_key);
                        }
                    } else if(actual_data[0] == 'A') {
                        uint16_t enc_len = ((uint8_t)actual_data[1] << 8) | (uint8_t)actual_data[2];  // - read length
                        std::string payload(actual_data.begin() + 3, actual_data.begin() + 3 + enc_len);

                        std::cout<<"received ECC encrypted AES key from "<<udp_uuid<<"\n";
                        // odebrałeś zaszyfrowany klucz AES - odszyfruj i zapisz
                        // usuń padding zer z końca (ale uwaga: payload binarny może zawierać zera!)
                        // długość = 32 (eph_pub) + 16 (IV) + 32 (AES) + padding AES = ok. 96 bajtów
                        try {
                            auto aes_key = decrypt_ECC(payload);

                            // zapis klucza jako hex do ./friends/uuid/AES
                            std::ofstream fout("./friends/" + udp_uuid + "/AES");
                            for (int i = 0; i < 32; ++i) {
                                char buf[3];
                                snprintf(buf, sizeof(buf), "%02x", aes_key[i]);
                                fout << buf;
                            }
                            fout.close();
                            std::cout << "Zapisano nowy klucz AES dla: " << udp_uuid << "\n";
                        } catch (const std::exception& e) {
                            std::cerr << "Błąd odszyfrowywania AES key od " << udp_uuid << ": " << e.what() << "\n";
                        }
                    }
                } else if (recv_buffer[1] == 'F') {

                    uint32_t MID =
                        ((uint8_t)recv_buffer[3] << 24) |
                        ((uint8_t)recv_buffer[4] << 16) |
                        ((uint8_t)recv_buffer[5] << 8)  |
                        ((uint8_t)recv_buffer[6]);

                    uint64_t PID =
                        ((uint64_t)(uint8_t)recv_buffer[7]  << 40) |
                        ((uint64_t)(uint8_t)recv_buffer[8]  << 32) |
                        ((uint64_t)(uint8_t)recv_buffer[9]  << 24) |
                        ((uint64_t)(uint8_t)recv_buffer[10] << 16) |
                        ((uint64_t)(uint8_t)recv_buffer[11] << 8)  |
                        ((uint64_t)(uint8_t)recv_buffer[12]);

                    if (CRC32(actual_data) == received_checksum) {

                        if (recv_buffer[2] == 1) {
                            // pierwszy pakiet - odczytaj total_packets i nazwę pliku
                            uint64_t total_packets =
                                ((uint64_t)(uint8_t)actual_data[0] << 40) |
                                ((uint64_t)(uint8_t)actual_data[1] << 32) |
                                ((uint64_t)(uint8_t)actual_data[2] << 24) |
                                ((uint64_t)(uint8_t)actual_data[3] << 16) |
                                ((uint64_t)(uint8_t)actual_data[4] << 8)  |
                                ((uint64_t)(uint8_t)actual_data[5]);

                            // odczytaj nazwę pliku (null-terminated od bajtu 7)
                            std::string filename;
                            for (int i = 7; i < 495 && actual_data[i] != '\0'; ++i)
                                filename += actual_data[i];

                            // walidacja nazwy - tylko bezpieczne znaki
                            bool valid = !filename.empty();
                            for (char c : filename) {
                                if (c == '/' || c == '\\' || c == ':' || c == '*' ||
                                    c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                                    valid = false; break;
                                }
                            }
                            if (!valid) filename = std::to_string(MID);

                            // sprawdź czy nazwa się nie powtarza, dodaj suffix jeśli trzeba
                            std::string dir = "./received/" + udp_uuid + "/";
                            fs::create_directories(dir);
                            std::string final_path = dir + filename;
                            if (fs::exists(final_path)) {
                                std::string base = filename;
                                std::string ext = "";
                                size_t dot = filename.rfind('.');
                                if (dot != std::string::npos) {
                                    base = filename.substr(0, dot);
                                    ext  = filename.substr(dot);
                                }
                                int suffix = 1;
                                while (fs::exists(dir + base + "(" + std::to_string(suffix) + ")" + ext))
                                    ++suffix;
                                filename = base + "(" + std::to_string(suffix) + ")" + ext;
                            }

                            IncomingFile inf;
                            inf.total_packets = total_packets;
                            inf.filename = filename;
                            incoming_files[MID] = std::move(inf);

                            std::cout << "Rozpoczęto odbieranie pliku: " << filename
                                    << " (" << total_packets << " pakietów), MID=" << MID << "\n";

                        } else if (recv_buffer[2] == 2 || recv_buffer[2] == 0) {
                            // pakiet danych (pośredni lub ostatni)
                            auto it = incoming_files.find(MID);
                            if (it == incoming_files.end()) {
                                std::cerr << "Nieznany MID: " << MID << "\n";
                                std::fill_n(recv_buffer.begin(), 512, 0);
                                start_receive();
                                return;
                            }

                            // odszyfruj
                            uint16_t enc_len = ((uint8_t)actual_data[0] << 8) | (uint8_t)actual_data[1];
                            if (enc_len == 0 || enc_len > 493) {
                                std::cerr << "Nieprawidłowa długość pakietu pliku: " << enc_len << "\n";
                                std::fill_n(recv_buffer.begin(), 512, 0);
                                start_receive();
                                return;
                            }

                            std::vector<uint8_t> encrypted(actual_data.begin() + 2, actual_data.begin() + 2 + enc_len);
                            std::string decrypted;
                            try {
                                auto key = load_key(udp_uuid);
                                decrypted = aes_decrypt(encrypted, key);
                            } catch (const std::exception& e) {
                                std::cerr << "Błąd deszyfrowania pakietu pliku: " << e.what() << "\n";
                                std::fill_n(recv_buffer.begin(), 512, 0);
                                start_receive();
                                return;
                            }

                            it->second.chunks[PID] = std::vector<char>(decrypted.begin(), decrypted.end());

                            std::cout << "Odebrano pakiet " << PID + 1 << "/" << it->second.total_packets
                                    << " pliku " << it->second.filename << "\n";

                            // sprawdź czy mamy wszystkie pakiety
                            if (it->second.chunks.size() == it->second.total_packets) {
                                // złóż plik w kolejności packetID
                                std::string dir = "./received/" + udp_uuid + "/";
                                fs::create_directories(dir);
                                std::ofstream out(dir + it->second.filename, std::ios::binary);

                                for (uint64_t i = 0; i < it->second.total_packets; ++i) {
                                    auto chunk_it = it->second.chunks.find(i);
                                    if (chunk_it == it->second.chunks.end()) {
                                        std::cerr << "Brakujący pakiet " << i << " - plik uszkodzony!\n";
                                        break;
                                    }
                                    out.write(chunk_it->second.data(), chunk_it->second.size());
                                }
                                out.close();

                                std::cout << "Plik " << it->second.filename << " zapisany pomyślnie.\n";
                                incoming_files.erase(it);
                            }
                        }
                    }
                }
            }
            
            std::fill_n(recv_buffer.begin(), 512, 0);
            start_receive();
        }

        
    }

    void send_encrypted_AES_key(std::array<uint8_t,32> received_ecc_public_key) {
        std::string _AES_ecc_encrypted = encrypt_ECC(generate_AES(udp_uuid), received_ecc_public_key);
    
        std::array<char, 495> arr{};
        arr[0] = 'A';
        uint16_t len = static_cast<uint16_t>(_AES_ecc_encrypted.size());
        arr[1] = static_cast<char>((len >> 8) & 0xFF);  // - add length prefix
        arr[2] = static_cast<char>(len & 0xFF);
        for(size_t i = 0; i < _AES_ecc_encrypted.size(); ++i) {
            arr[3+i] = _AES_ecc_encrypted[i];
        }

        udp_packet _packet('O', 0, get_messageID(), 0, arr);

        send_arr(_packet.get_bytes());
    }

    void send_ECC_public_key() {

        std::string ECC_PUB = read_file("./user_settings/ECC_PUB");
        std::array<char, 495> arr{};
        
        arr[0] = 'P';

        for(int i{}; i<32; ++i) {
            arr[1+i] = ECC_PUB[i];
            arr[33+i] = ECC_PUB[i];
            arr[65+i] = ECC_PUB[i];
        }

        udp_packet _packet('O', 0, get_messageID(), 0, arr);

        send_arr(_packet.get_bytes());
    }

    void stop() {
        stopped = true;
        t_udp_punch.cancel();
        socket.cancel();
        socket.close();
    }

    void send(std::string msg) {

        auto msg_ptr = std::make_shared<std::string>(std::move(msg));

        socket.async_send_to(
            asio::buffer(*msg_ptr), 
            send_endpoint,
            std::bind(
                &udp_server::handle_send, this, 
                asio::placeholders::error,
                asio::placeholders::bytes_transferred
            )
        );
    }

    

    void send_text(std::string msg) {
        if (msg.size() > 477) msg = msg.substr(0, 477); // 477 + 2 bajty długości = 479

        std::string payload;
        try {
            auto key = load_key(udp_uuid);
            auto encrypted = aes_encrypt(msg, key); // max 493 bajty
            
            // pierwsze 2 bajty = długość zaszyfrowanych danych
            uint16_t len = static_cast<uint16_t>(encrypted.size());
            payload += static_cast<char>((len >> 8) & 0xFF);
            payload += static_cast<char>(len & 0xFF);
            payload += std::string(encrypted.begin(), encrypted.end());
        } catch (const std::exception& e) {
            std::cerr << "Błąd szyfrowania: " << e.what() << "\n";
            payload = msg;
        }

        udp_packet packet('T', 0, get_messageID(), 0, payload);
        auto arr = packet.get_bytes();
        auto msg_ptr = std::make_shared<std::array<char, 512>>(std::move(arr));

        socket.async_send_to(
            asio::buffer(*msg_ptr), send_endpoint,
            std::bind(&udp_server::handle_send, this,
                asio::placeholders::error,
                asio::placeholders::bytes_transferred)
        );
    }

    void send_arr(std::array<char, 512> arr) {
        auto msg_ptr = std::make_shared<std::array<char, 512>>(std::move(arr));

        socket.async_send_to(
            asio::buffer(*msg_ptr),
            send_endpoint,
            std::bind(&udp_server::handle_send, this,
                asio::placeholders::error,
                asio::placeholders::bytes_transferred)
        );
    }


    void sync_send(std::string msg) {
        socket.send_to(
            asio::buffer(msg), 
            send_endpoint
        );
    }

    void handle_send(
        const std::error_code& /*error*/,
        std::size_t /*bytes_transferred*/
    ) {
        std::cout<<"wyslano pakiet do: "<<send_endpoint.address().to_string()<<std::endl;
    }

    void send_file(uint32_t messageID, const char* data, size_t size, const std::string& filename, size_t from_here = 0) {
        if (stopped) return;

        uint64_t total_packets = (size + 460) / 461;

        std::array<char, 495> arr{};
        // [0-5] total_packets (6 bajtów)
        arr[0] = static_cast<char>((total_packets >> 40) & 0xFF);
        arr[1] = static_cast<char>((total_packets >> 32) & 0xFF);
        arr[2] = static_cast<char>((total_packets >> 24) & 0xFF);
        arr[3] = static_cast<char>((total_packets >> 16) & 0xFF);
        arr[4] = static_cast<char>((total_packets >> 8)  & 0xFF);
        arr[5] = static_cast<char>((total_packets)       & 0xFF);
        // [6] null separator
        arr[6] = '\0';
        // [7-...] nazwa pliku (max 488 znaków, null-terminated)
        size_t name_len = std::min(filename.size(), (size_t)487);
        std::copy(filename.begin(), filename.begin() + name_len, arr.begin() + 7);
        arr[7 + name_len] = '\0';

        udp_packet packet('F', 1, messageID, total_packets, arr);
        send_arr(packet.get_bytes());

        auto data_shared = std::make_shared<std::vector<char>>(data, data + size);
        t_file_transmit.expires_after(asio::chrono::milliseconds(2));
        t_file_transmit.async_wait([this, messageID, data_shared, total_packets](const std::error_code& ec) {
            if (ec || stopped) return;
            send_file_packet(messageID, data_shared, total_packets, 0);
        });
    }


    void send_file_packet(uint32_t messageID, std::shared_ptr<std::vector<char>> data, uint64_t total_packets, uint64_t packet_index) {
        if (stopped) return;

        size_t offset = packet_index * 461;
        size_t remaining = data->size() - offset;
        size_t chunk_size = std::min<size_t>(remaining, 461);

        // szyfruj chunk
        std::string chunk(data->data() + offset, chunk_size);
        std::vector<uint8_t> encrypted;
        try {
            auto key = load_key(udp_uuid);
            encrypted = aes_encrypt(chunk, key);
        } catch (const std::exception& e) {
            std::cerr << "Błąd szyfrowania pakietu pliku: " << e.what() << "\n";
            return;
        }

        // AES dodaje padding - zaszyfrowany chunk może mieć max chunk_size + 16 (padding) + 16 (IV) = chunk_size + 32
        // chunk_size musi być max 463 żeby zmieścić się w 495 bajtach z 2 bajtami długości
        // 463 + 32 = 495 - 2 (length prefix) = 493
        uint16_t enc_len = static_cast<uint16_t>(encrypted.size());

        std::array<char, 495> arr{};
        arr[0] = static_cast<char>((enc_len >> 8) & 0xFF);
        arr[1] = static_cast<char>(enc_len & 0xFF);
        std::copy(encrypted.begin(), encrypted.end(), arr.begin() + 2);

        char quantity = (packet_index + 1 == total_packets) ? 0 : 2;

        udp_packet packet('F', quantity, messageID, packet_index, arr);
        send_arr(packet.get_bytes());

        std::cout << "wysłano pakiet pliku " << packet_index + 1 << "/" << total_packets << "\n";

        if (packet_index + 1 < total_packets) {
            t_file_transmit.expires_after(asio::chrono::milliseconds(2));
            t_file_transmit.async_wait([this, messageID, data, total_packets, packet_index](const std::error_code& ec) {
                if (ec || stopped) return;
                send_file_packet(messageID, data, total_packets, packet_index + 1);
            });
        } else {
            std::cout << "transfer pliku zakończony, messageID=" << messageID << "\n";
        }
    }

    void resend_file(uint32_t messageID, std::shared_ptr<std::vector<char>> data, uint64_t packet_index) {
        if (stopped) return;

        size_t offset = packet_index * 461;
        if (offset >= data->size()) {
            std::cerr << "resend_file: packet_index poza zakresem\n";
            return;
        }

        size_t remaining = data->size() - offset;
        size_t chunk_size = std::min<size_t>(remaining, 461);

        std::string chunk(data->data() + offset, chunk_size);
        std::vector<uint8_t> encrypted;
        try {
            auto key = load_key(udp_uuid);
            encrypted = aes_encrypt(chunk, key);
        } catch (const std::exception& e) {
            std::cerr << "resend_file: błąd szyfrowania: " << e.what() << "\n";
            return;
        }

        uint16_t enc_len = static_cast<uint16_t>(encrypted.size());

        std::array<char, 495> arr{};
        arr[0] = static_cast<char>((enc_len >> 8) & 0xFF);
        arr[1] = static_cast<char>(enc_len & 0xFF);
        std::copy(encrypted.begin(), encrypted.end(), arr.begin() + 2);

        uint64_t total_packets = (data->size() + 460) / 461;
        char quantity = (packet_index + 1 == total_packets) ? 0 : 2;

        udp_packet packet('F', quantity, messageID, packet_index, arr);
        send_arr(packet.get_bytes());

        std::cout << "resend pakietu " << packet_index << " (messageID=" << messageID << ")\n";
    }

    struct IncomingFile {
        uint64_t total_packets;
        std::string filename;
        std::unordered_map<uint64_t, std::vector<char>> chunks; // packetID - odszyfrowane dane
    };
    std::unordered_map<uint32_t, IncomingFile> incoming_files; // messageID - stan

};






bool check_activity(std::string _uuid) {    //funkcja sprawdzająca aktywność użytkowników. jeżeli od 2 sekund nie otrzymano żadnych pakietów, to znaczy że użytkownik jest nieaktywny (cały czas w tle są przesyłane pakiety żeby podtrzymać działanie udp punching)
    auto it = last_packets_times.find(_uuid);
    
    // jeśli nie znaleziono uuid, nie wykryto aktywności ze strony usera
    if (it == last_packets_times.end()) {
        // online_status[_uuid] = false;
        return false;
    } else {
        // online_status[_uuid] = true;
    }

    auto last_packet_time = it->second;
    auto now = std::chrono::system_clock::now();

    return last_packet_time >= (now - std::chrono::seconds(2));
}

std::string make_json_ip_ports(std::string _uuid) {
    std::string ip_io_path = "./friends/" + _uuid + "/ip_io";
    std::ifstream file(ip_io_path);
    
    if (!file.is_open()) return "{}";

    std::string ip_addr;
    int iport, oport;
    file >> ip_addr >> iport >> oport;
    file.close();

    std::string result = "{";
    result += "\"ip\":\"" + ip_addr + "\",";
    result += "\"iport\":" + std::to_string(iport) + ",";
    result += "\"oport\":" + std::to_string(oport);
    result += "}";

    return result;
}


void user_selection();




class tcp_connection            //w tej klasie są obsłużone wszystkie requesty http
  : public std::enable_shared_from_this<tcp_connection>
{
public:
    typedef std::shared_ptr<tcp_connection> pointer;

    asio::io_context& io_context_;

    tcp::socket socket_;
    std::string message_;

    std::vector<char> buffer = std::vector<char>(65536);
    std::vector<char> full_request;

    std::unordered_map<std::string, udp_server*> * user_connections;

    tcp_connection(asio::io_context& io_context, std::unordered_map<std::string, udp_server*> * udp_c)
    : socket_(io_context), user_connections(udp_c), io_context_(io_context)
    {

    }

    static pointer create(asio::io_context& io_context, std::unordered_map<std::string, udp_server*> * udp_c)
    {
        return pointer(new tcp_connection(io_context, udp_c));
    }

    tcp::socket& socket()
    {
        return socket_;
    }

    void start()
    {
        full_request.clear();
        read_more();
    }

    void read_more() {
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

        full_request.insert(full_request.end(), buffer.begin(), buffer.begin() + bytes);

        std::string request(full_request.begin(), full_request.end());

        // sprawdź czy mamy już nagłówki
        size_t header_end = request.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            read_more();
            return;
        }

        // wyciągnij content-length
        size_t content_length = 0;
        size_t cl_pos = request.find("Content-Length: ");
        if (cl_pos != std::string::npos) {
            content_length = std::stoul(request.substr(cl_pos + 16));
        }

        size_t body_received = full_request.size() - header_end - 4;
        if (body_received < content_length) {
            read_more();
            return;
        }

        try
        {
            std::string client_ip = socket_.remote_endpoint().address().to_string();

            if (request.find("GET /time") != std::string::npos) {
                std::cout << "GET /time from: " << client_ip << "\n";
                time_t now = time(0);
                std::string t = ctime(&now);
                t.erase(t.find_last_not_of("\n") + 1);
                message_ = make_http_response(t, "text/plain");
            } else if (request.find("GET /style.css") != std::string::npos) {
                std::cout << "GET /style.css from: " << client_ip << "\n";
                message_ = make_http_response(read_file(style), "text/css");
            } else if (request.find("GET /icon.ico") != std::string::npos) {
                std::cout << "GET /icon.ico from: " << client_ip << "\n";
                message_ = make_http_response(read_file(icon), "image/x-icon");
            } else if (request.find("GET /logo.jpg") != std::string::npos) {
                std::cout << "GET /logo.jpg from: " << client_ip << "\n";
                message_ = make_http_response(read_file(logo), "image/jpeg");
            } else if (request.find("GET /notification") != std::string::npos) {
                // std::cout << "GET /notification from: " << client_ip << "\n";
                message_ = make_http_response(read_file("./user_settings/notification.mp3"), "audio/mpeg");
            } else if (request.find("GET /profile_picture.jpg") != std::string::npos) {
                std::cout << "GET /profile_picture.jpg from: " << client_ip << "\n";
                message_ = make_http_response(read_file(profile_picture), "image/jpeg");
            } else if (request.find("GET /pfps/") != std::string::npos) {
                std::string requested_uuid = request.substr(request.find("GET /pfps/") + 10, 36);
                std::cout << "GET /pfps/"<<requested_uuid<<" from: "<< client_ip <<"\n";
                std::string pfp_path = "./received/"+requested_uuid+"/profile_picture.jpg";
                std::string pfp_data;
                try { pfp_data = read_file(pfp_path); }
                catch (...) { pfp_data = read_file(logo); }
                message_ = make_http_response(pfp_data, "image/jpeg");
            } else if (request.find("GET /my_info") != std::string::npos) {
                // zwraca publiczny IP, lokalny IP, UUID koordynujący
                std::string result = "{";
                result += "\"public_ip\":\"" + public_ip + "\",";
                result += "\"local_ip\":\"" + my_local_ip + "\",";
                result += "\"c_uuid\":\"" + c_server_uuid + "\"";
                result += "}";
                message_ = make_http_response(result, "application/json");
            } else if (request.find("GET /found_peers") != std::string::npos) {
                std::string result = "[";
                bool first = true;
                for (const auto& p : found_peers) {
                    if (!first) result += ",";
                    first = false;
                    result += "{";
                    result += "\"uuid\":\"" + p.uuid + "\",";
                    result += "\"ip\":\"" + p.ip + "\",";
                    result += "\"iport\":" + std::to_string(p.iport) + ",";
                    result += "\"oport\":" + std::to_string(p.oport);
                    result += "}";
                }
                result += "]";
                message_ = make_http_response(result, "application/json");
            } else if (request.find("GET /imgs/") != std::string::npos) {       
                std::string requested_uuid = request.substr(request.find("GET /imgs/") + 10, 36);
                std::string img_name = request.substr(request.find("GET /imgs/") + 10 + 36 + 1, 
                                    request.find(" HTTP/") - (request.find("GET /imgs/") + 10 + 36 + 1));
                size_t qs = img_name.find('?');
                if (qs != std::string::npos) img_name = img_name.substr(0, qs);
                std::cout << "GET /imgs/"<<requested_uuid<<"/"<<img_name<<" from: "<< client_ip <<"\n";
                
                std::string img_path = "./received/" + requested_uuid + "/" + img_name;
                std::string img_data;
                std::string img_type = "image/jpeg";
                try { img_data = read_file(img_path); }
                catch (...) { img_data = read_file("./user_settings/loading.gif");  img_type="image/gif";}
                message_ = make_http_response(img_data, img_type);
            } else if (request.find("GET /img_exists/") != std::string::npos) {
            std::string requested_uuid = request.substr(request.find("GET /img_exists/") + 16, 36);
            std::string img_name = request.substr(request.find("GET /img_exists/") + 16 + 36 + 1,
                                request.find(" HTTP/") - (request.find("GET /img_exists/") + 16 + 36 + 1));
            size_t qs = img_name.find('?');
            if (qs != std::string::npos) img_name = img_name.substr(0, qs);
            std::string img_path = "./received/" + requested_uuid + "/" + img_name;
            message_ = make_http_response(fs::exists(img_path) ? "true" : "false", "text/plain");
            } else if (request.find("HEAD /") != std::string::npos) {
                if (request.find("HEAD /imgs/") != std::string::npos) {
                    std::string requested_uuid = request.substr(request.find("HEAD /imgs/") + 11, 36);
                    std::string img_name = request.substr(request.find("HEAD /imgs/") + 11 + 36 + 1,
                                        request.find(" HTTP/") - (request.find("HEAD /imgs/") + 11 + 36 + 1));
                    std::string img_path = "./received/" + requested_uuid + "/" + img_name;
                    std::string ct = fs::exists(img_path) ? "image/jpeg" : "image/gif";
                    std::string res = "HTTP/1.1 200 OK\r\nContent-Type: " + ct + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    message_ = res;
                } else {
                    message_ = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                }
            } else if (request.find("GET /key_status/") != std::string::npos) {
                std::string requested_uuid = request.substr(request.find("GET /key_status/") + 16, 36);
                std::cout << "GET /key_status/"<<requested_uuid<<" from: "<< client_ip <<"\n";
                std::string key_path = "./friends/" + requested_uuid + "/AES";
                if (std::filesystem::exists(key_path) && std::filesystem::file_size(key_path) > 0) {
                    message_ = "true";
                } else {
                    message_ = "false";
                }
                std::cout<<message_<<std::endl;
                message_ = make_http_response(message_, "text/plain");
            } else if (request.find("GET /select_user/") != std::string::npos) {
                std::string requested_uuid = request.substr(request.find("GET /select_user/") + 17, 36);
                std::cout << "GET /select_user/"<<requested_uuid<<" from: "<< client_ip <<"\n";
                
                notifications_count[requested_uuid]=0;
                
                selected_user = requested_uuid;
                std::string ip_addr = "";
                int iport, oport;
                std::ifstream ip_io_file("./friends/"+selected_user+"/ip_io");
                ip_io_file >> ip_addr;
                ip_io_file >> iport;
                ip_io_file >> oport;
                ip_io_file.close();


                message_ = make_http_response("ok", "text/plain");
            } else if (request.find("GET /messages/") != std::string::npos) {
                std::string requested_uuid = "";
                size_t start = request.find("GET /messages/") + 14;
                size_t end = request.find(" HTTP/", start);
                if (end != std::string::npos && end > start) {
                    requested_uuid = request.substr(start, end - start);
                }
                message_ = make_http_response(make_json_messages(requested_uuid), "application/json");
            } else if (request.find("GET /find_ip") != std::string::npos) {
                std::cout<<"connecting with server"<<std::endl;
            } else if (request.find("GET /exchange_keys/") != std::string::npos) {
                std::string requested_uuid = request.substr(request.find("GET /exchange_keys/") + 19, 36);
                std::cout << "GET /exchange_keys/"<<requested_uuid<<" from: "<< client_ip <<"\n";
                (*user_connections)[requested_uuid]->send_ECC_public_key();
                message_ = make_http_response("ok", "text/plain");
            } else if (request.find("POST /add_friend/") != std::string::npos) {
                std::string req_uuid = request.substr(request.find("POST /add_friend/") + 17, 36);
                std::string dir = "./friends/" + req_uuid;
                if (!fs::exists(dir)) fs::create_directories(dir);
                std::string uname_path = dir + "/username.txt";
                if (!fs::exists(uname_path)) { std::ofstream f(uname_path); f << "New Friend"; f.close(); }
                std::string aes_path = dir + "/AES";
                if (!fs::exists(aes_path)) { std::ofstream f(aes_path); f.close(); }
                std::string ip_io_path = dir + "/ip_io";
                if (!fs::exists(ip_io_path)) { std::ofstream f(ip_io_path); f.close(); }
                std::string msg_path = dir + "/messages.txt";
                if (!fs::exists(msg_path)) { std::ofstream f(msg_path); f.close(); }
                if (!user_connections->count(req_uuid)) (*user_connections)[req_uuid] = nullptr;
                message_ = make_http_response("ok", "text/plain");
            } else if (request.find("POST /remove_friend/") != std::string::npos) {
                std::string req_uuid = request.substr(request.find("POST /remove_friend/") + 20, 36);
                std::string dir = "./friends/" + req_uuid;
                if (user_connections->count(req_uuid) && (*user_connections)[req_uuid] != nullptr) {
                    (*user_connections)[req_uuid]->stop();
                    delete (*user_connections)[req_uuid];
                    user_connections->erase(req_uuid);
                } else {
                    user_connections->erase(req_uuid);
                }
                if (fs::exists(dir)) fs::remove_all(dir);
                if (selected_user == req_uuid) { selected_user = ""; user_selection(); }
                message_ = make_http_response("ok", "text/plain");
            } else if (request.find("GET /check_activity/") != std::string::npos) {
                std::string requested_uuid = request.substr(request.find("GET /check_activity/") + 20, 36);
                message_ = check_activity(requested_uuid) ? "true" : "false";
                message_ = make_http_response(message_, "text/plain");
            } else if (request.find("GET /ip_data/") != std::string::npos) {
                std::string requested_uuid = "";
                size_t start = request.find("GET /ip_data/") + 13;
                size_t end = request.find(" HTTP/", start);
                if (end != std::string::npos && end > start) {
                    requested_uuid = request.substr(start, end - start);
                }
                
                if (requested_uuid.empty()) {
                    message_ = make_http_response("{}", "application/json");
                } else {
                    message_ = make_http_response(make_json_ip_ports(requested_uuid), "application/json");
                }
            } else if (request.find("GET /friend_list") != std::string::npos) {
                std::cout << "GET /friend_list from: "<< client_ip <<"\n";
                message_ = make_http_response(make_json_friends(), "application/json");
            } else if (request.find("POST /send/") != std::string::npos) {

                std::string requested_uuid = request.substr(request.find("POST /send/") + 11, 36);
                std::string body = request.substr(request.find("\r\n\r\n") + 4);
                
                std::string message = "";
                size_t msg_start = body.find("\"message\":\"");
                if (msg_start != std::string::npos) {
                    msg_start += 11;
                    // czytaj znak po znaku, obsługując \" jako escaped quote
                    size_t i = msg_start;
                    while (i < body.size()) {
                        if (body[i] == '\\' && i + 1 < body.size() && body[i+1] == '"') {
                            message += '"';
                            i += 2;
                        } else if (body[i] == '"') {
                            break;
                        } else {
                            message += body[i];
                            i++;
                        }
                    }
                }
                
                save_message(requested_uuid, message, "you");
                if (user_connections->count(requested_uuid) && (*user_connections)[requested_uuid] != nullptr) {
                    (*user_connections)[requested_uuid]->send_text(message);
                }
                message_ = make_http_response("ok", "text/plain");
            } else if (request.find("POST /upload/pfp") != std::string::npos) {
                size_t body_start = request.find("\r\n\r\n");
                if (body_start != std::string::npos) {
                    // używamy surowych bajtów z full_request żeby nie niszczyć danych binarnych
                    size_t body_offset = body_start + 4;
                    std::ofstream file("./user_settings/profile_picture.jpg", std::ios::binary);
                    file.write(full_request.data() + body_offset, full_request.size() - body_offset);
                    file.close();

                    //send ./user_settings/profile_picture.jpg to all friends here
                    // wyślij zdjęcie profilowe do wszystkich znajomych
                    uint32_t mid = get_messageID();
                    for (auto& [conn_uuid, conn] : *user_connections) {
                        if (conn != nullptr) {
                            conn->send_file(mid, full_request.data() + body_offset, full_request.size() - body_offset, "profile_picture.jpg");
                        }
                    }

                }
                message_ = make_http_response("ok", "text/plain");
            } else if (request.find("POST /upload/file/") != std::string::npos) {
                std::string req_uuid = request.substr(request.find("POST /upload/file/") + 18, 36);
                size_t body_start = request.find("\r\n\r\n");
                if (body_start != std::string::npos) {
                    size_t body_offset = body_start + 4;
                    uint32_t mid = get_messageID();
                    std::string fname = std::to_string(mid);
                    size_t fn_pos = request.find("X-Filename: ");
                    if (fn_pos != std::string::npos) {
                        fname = request.substr(fn_pos + 12);
                        fname = fname.substr(0, fname.find("\r\n"));
                    }
                    if (user_connections->count(req_uuid) && (*user_connections)[req_uuid] != nullptr) {
                        (*user_connections)[req_uuid]->send_file(mid, full_request.data() + body_offset, full_request.size() - body_offset, fname);
                    }

                    std::string dir = "./received/" + uuid + "/";  
                    fs::create_directories(dir);
                    std::ofstream out(dir + fname, std::ios::binary);
                    out.write(full_request.data() + body_offset, full_request.size() - body_offset);
                    out.close();
                }
                message_ = make_http_response("ok", "text/plain");
            } else if (request.find("POST /upload/name") != std::string::npos) {
                std::string body = request.substr(request.find("\r\n\r\n") + 4);
                std::string name = body.substr(body.find("\"name\":\"") + 8);
                name = name.substr(0, name.find("\""));
                std::ofstream file("./user_settings/name.txt");
                file << name;
                file.close();
                message_ = make_http_response("ok", "text/plain");
            } else if (request.find("POST /connection") != std::string::npos) {
            std::string body = request.substr(request.find("\r\n\r\n") + 4);
            body.erase(0, body.find_first_not_of(" \t\r\n"));
            body.erase(body.find_last_not_of(" \t\r\n") + 1);

            if (body == "on" || body == "off") {
                std::ofstream f(server_state_file);
                f << body;
                f.close();
                connection_to_server_active = (body == "on");
            }
            body = read_file(server_state_file);
            message_ = make_http_response(body, "text/plain");
            } else if (request.find("POST /update_ip_data/") != std::string::npos) {
                std::string req_uuid = request.substr(request.find("POST /update_ip_data/") + 21, 36);
                std::string body = request.substr(request.find("\r\n\r\n") + 4);
                std::string field = body.substr(body.find("\"field\":\"") + 9);
                field = field.substr(0, field.find("\""));
                std::string value = body.substr(body.find("\"value\":\"") + 9);
                value = value.substr(0, value.find("\""));
                std::string ip_io_path = "./friends/" + req_uuid + "/ip_io";
                std::ifstream fin(ip_io_path);
                std::string ip; int iport, oport;
                fin >> ip >> iport >> oport;
                fin.close();
                if (field == "ip")    ip    = value;
                if (field == "iport") iport = std::stoi(value);
                if (field == "oport") oport = std::stoi(value);
                std::ofstream fout(ip_io_path);
                fout << ip << "\n" << iport << "\n" << oport << "\n";
                fout.close();
                if (user_connections->count(req_uuid)) {
                    if ((*user_connections)[req_uuid] == nullptr) {
                        try {
                            (*user_connections)[req_uuid] = new udp_server(io_context_, iport, oport, ip, req_uuid);
                        } catch (const std::exception& e) {
                            std::cerr << "Błąd tworzenia udp_server: " << e.what() << "\n";
                        }
                    } else {
                        (*user_connections)[req_uuid]->change_target(iport, oport, ip);
                    }
                }
                message_ = make_http_response("ok", "text/plain");
            } else {
                std::cout << "GET / from: " << client_ip << "\n";
                message_ = make_http_response(prepare_html(read_file(qindex)), "text/html");
            }

            asio::async_write(socket_, asio::buffer(message_),
                std::bind(&tcp_connection::handle_write, shared_from_this(),
                    asio::placeholders::error,
                    asio::placeholders::bytes_transferred)
            );
        } catch (const std::exception& e) {
            std::cerr << "handle_read error: " << e.what() << "\n";
            message_ = make_http_response("Internal Server Error", "text/plain");
            asio::async_write(socket_, asio::buffer(message_),
                std::bind(&tcp_connection::handle_write, shared_from_this(),
                    asio::placeholders::error,
                    asio::placeholders::bytes_transferred));
        }
    }

    void handle_write(const std::error_code& /*error*/,
    size_t /*bytes_transferred*/)
    {

    }
    
    
};


class http_server           //klasa oznaczająca serwer http, którego używamy do komunikacji między przeglądarką i programem
{
public:
    asio::io_context& io_context_;
    tcp::acceptor acceptor_;

    std::unordered_map<std::string, udp_server*> * user_connections;

    http_server(asio::io_context& io_context, std::unordered_map<std::string, udp_server*> * udp_s)
    : io_context_(io_context), user_connections(udp_s),
    acceptor_(io_context, tcp::endpoint(
        //tcp::v4() byłoby użyte by się otworzyć na cały lan
        asio::ip::make_address("127.0.0.1"), //opened only to this computer
        client_port)
    ) {
        start_accept();
    }


    void start_accept()
    {
        tcp_connection::pointer new_connection =
            tcp_connection::create(io_context_, user_connections);
        
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





void user_selection() { // funkcja wybierająca automatycznie użytkownika
    std::string friends_path = "./friends";
    if (!fs::exists(friends_path) || !fs::is_directory(friends_path)) return;

    std::string latest_uuid = "";
    std::string latest_combined = "";
    std::string first_uuid = "";   

    for (const auto& entry : fs::directory_iterator(friends_path)) {
        if (!entry.is_directory()) continue;

        std::string uuid = entry.path().filename().string();
        
        if (first_uuid.empty()) first_uuid = uuid;  
        
        std::string messages_path = entry.path().string() + "/messages.txt";
        std::ifstream file(messages_path);
        if (!file.is_open()) continue;

        std::string line, last_date = "", last_time = "";
        while (std::getline(file, line)) {
            if (line.rfind("date:", 0) == 0) last_date = line.substr(5);
            if (line.rfind("time:", 0) == 0) last_time = line.substr(5);
        }
        file.close();

        if (last_date.empty()) continue;

        std::string combined = last_date + " " + last_time;
        if (combined > latest_combined) {
            latest_combined = combined;
            latest_uuid = uuid;
        }
    }

    if (!latest_uuid.empty())
        selected_user = latest_uuid;
    else if (!first_uuid.empty())   
        selected_user = first_uuid;
}


std::unordered_map<std::string, udp_server*> load_connections(asio::io_context& io) {   // wczytanie połączeń udp do wszystkich użytkowników

    std::unordered_map<std::string, udp_server*> connections;
    std::string friends_path = "./friends";

    if (!fs::exists(friends_path) || !fs::is_directory(friends_path)) return connections;

    for (const auto& entry : fs::directory_iterator(friends_path)) {
        if (!entry.is_directory()) continue;

        std::string uuid = entry.path().filename().string();
        std::string ip_io_path = entry.path().string() + "/ip_io";
        std::ifstream file(ip_io_path);
        if (!file.is_open()) {
            std::cerr << "Brak pliku ip_io dla: " << uuid << "\n";
            connections[uuid] = nullptr; 
            continue;
        }

        std::string ip_addr;
        int iport = -1, oport = -1;
        file >> ip_addr >> iport >> oport;
        file.close();

        if (ip_addr.empty() || iport < 0 || oport < 0) {
            std::cerr << "Puste ip_io dla: " << uuid << " - pomijam UDP\n";
            connections[uuid] = nullptr;
            continue;
        }

        std::cout << "uuid: " << uuid << "\n";
        std::cout << "ip: " << ip_addr << "\n";
        std::cout << "iport (słucham): " << iport << "\n";
        std::cout << "oport (wysyłam): " << oport << "\n";

        try {
            connections[uuid] = new udp_server(io, iport, oport, ip_addr, uuid);
            std::cout << "Załadowano połączenie dla: " << uuid << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Błąd przy tworzeniu udp_server dla " << uuid << ": " << e.what() << "\n";
            connections[uuid] = nullptr;  
        }
    }

    return connections;
}



class tcp_c_client {        // klient do obsługi komunikacji z serwerem koordynującym
public:
    asio::io_context&    io_;
    tcp::socket          socket_;
    asio::steady_timer   timer_;

    std::string          server_ip_;
    uint16_t             server_port_;

    bool                 registered_  = false;
    std::string          c_uuid_      = "";
    std::array<uint8_t, 32> aes_key_  {};

    std::vector<uint8_t> write_buf_;
    uint8_t              hdr_[5];
    std::vector<uint8_t> payload_;

    std::unordered_map<std::string, udp_server*>* user_connections_;

    std::string          cached_local_ip_ = "127.0.0.1";

    tcp_c_client(asio::io_context& io,
                 const std::string& server_ip,
                 uint16_t           server_port,
                 std::unordered_map<std::string, udp_server*>* user_connections)
        : io_(io), socket_(io), timer_(io),
          server_ip_(server_ip), server_port_(server_port),
          user_connections_(user_connections)
    {
        try {
            c_uuid_ = read_file(uuid_file);
            while (!c_uuid_.empty() && (c_uuid_.back() == '\r' || c_uuid_.back() == '\n'))
                c_uuid_.pop_back();
            aes_key_ = load_hex_key(c_aes_key_file);
            registered_ = (c_uuid_.size() == 36);
        } catch (...) {
            registered_ = false;
        }
        c_server_uuid = c_uuid_;
    }

    void start() {
        if (!connection_to_server_active) {
            schedule_retry(5);
            return;
        }
        do_connect();
    }

    void stop() {
        asio::error_code ec;
        timer_.cancel();
        socket_.close(ec);
    }

private:

    void do_connect() {
        if (!connection_to_server_active) {
            schedule_retry(2);
            return;
        }

        auto ep = tcp::endpoint(asio::ip::make_address(server_ip_), server_port_);
        auto self = shared_from_this_workaround();
        socket_.async_connect(ep, [this, self](const std::error_code& ec) {
            if (ec) {
                std::cerr << "[c_client] Błąd połączenia z serwerem: " << ec.message() << "\n";
                reconnect();
                return;
            }
            std::cout << "[c_client] Połączono z serwerem koordynującym\n";

            try {
                cached_local_ip_ = socket_.local_endpoint().address().to_string();
            } catch (...) {
                cached_local_ip_ = "127.0.0.1";
            }

            if (!registered_)
                do_register();
            else
                do_routine();
        });
    }

    void do_register() {
        std::string ecc_pub = read_file("./user_settings/ECC_PUB");
        if (ecc_pub.size() < 32) {
            std::cerr << "[c_client] Brak klucza ECC_PUB\n";
            reconnect();
            return;
        }

        send_frame(0x01, std::string(ecc_pub.begin(), ecc_pub.begin() + 32),
            [this](){ read_header_then([this](){ handle_register_response(); }); });
    }

    void handle_register_response() {
        if (payload_.size() < 36 + 32 + 16 + 1) {
            std::cerr << "[c_client] Za krótka odpowiedź rejestracji\n";
            reconnect();
            return;
        }

        c_uuid_ = std::string(reinterpret_cast<char*>(payload_.data()), 36);
        std::string ecc_payload(reinterpret_cast<char*>(payload_.data() + 36),
                                payload_.size() - 36);

        try {
            aes_key_ = decrypt_ECC(ecc_payload);
        } catch (const std::exception& e) {
            std::cerr << "[c_client] Błąd deszyfrowania AES od serwera: " << e.what() << "\n";
            reconnect();
            return;
        }

        {
            std::ofstream f(uuid_file);
            f << c_uuid_;
            uuid = c_uuid_;
        }
        save_hex_key(c_aes_key_file, aes_key_);
        registered_ = true;
        c_server_uuid = c_uuid_;

        std::cout << "[c_client] Zarejestrowano, UUID: " << c_uuid_ << "\n";
        do_routine();
    }

    void do_routine() {     // rutynowa komunikacja z serwerem co 2s
        if (!connection_to_server_active) {
            if (socket_.is_open()) {
                std::error_code ec;
                socket_.close(ec);
                registered_ = false;
                std::cout << "[c_client] Zamykanie połączenia w rutynie (flaga inactive)\n";
            }
            schedule_retry(2);
            return;
        }

        std::string local_ip = cached_local_ip_;
        my_local_ip = local_ip;

        std::unordered_set<uint16_t> busy;
        for (auto& [u, srv] : *user_connections_) {
            if (srv == nullptr) continue;
            try {
                uint16_t lp = srv->socket.local_endpoint().port();
                busy.insert(lp);
            } catch (...) {}
        }

        std::vector<std::string> targets;
        for (auto& [u, srv] : *user_connections_) {
            if (!check_activity(u))
                targets.push_back(u);
        }

        std::string raw;

        uint8_t ip_len = static_cast<uint8_t>(std::min<size_t>(local_ip.size(), 255));
        raw.push_back(static_cast<char>(ip_len));
        raw += local_ip.substr(0, ip_len);

        uint16_t n_ports = static_cast<uint16_t>(busy.size());
        raw.push_back((n_ports >> 8) & 0xFF);
        raw.push_back(n_ports & 0xFF);
        for (uint16_t p : busy) {
            raw.push_back((p >> 8) & 0xFF);
            raw.push_back(p & 0xFF);
        }

        uint16_t n_targets = static_cast<uint16_t>(targets.size());
        raw.push_back((n_targets >> 8) & 0xFF);
        raw.push_back(n_targets & 0xFF);
        for (const auto& t : targets)
            raw += t;

        std::vector<uint8_t> encrypted = aes_encrypt(raw, aes_key_);

        std::string msg = c_uuid_;
        msg += std::string(encrypted.begin(), encrypted.end());

        send_frame(0x03, msg, [this](){ read_header_then([this](){ handle_routine_response(); }); });
    }

    void handle_routine_response() {
        std::vector<uint8_t> enc(payload_.begin(), payload_.end());
        std::string decrypted;
        try {
            decrypted = aes_decrypt(enc, aes_key_);
            std::cout<<"decrypted: "<<decrypted<<"\n";
        } catch (const std::exception& e) {
            std::cerr << "[c_client] Błąd deszyfrowania routine response: " << e.what() << "\n";
            schedule_retry(2);
            return;
        }

        size_t offset = 0;
        if (decrypted.size() < 2) { schedule_retry(2); return; }

        uint16_t num_peers = (static_cast<uint8_t>(decrypted[0]) << 8)
                           |  static_cast<uint8_t>(decrypted[1]);
        offset = 2;

        found_peers.clear();

        for (uint16_t i = 0; i < num_peers && offset < decrypted.size(); ++i) {
            PeerConnectionInfo peer;

            if (offset + 36 > decrypted.size()) break;
            peer.uuid = decrypted.substr(offset, 36);
            offset += 36;

            if (offset + 1 > decrypted.size()) break;
            uint8_t ip_len = static_cast<uint8_t>(decrypted[offset++]);
            if (offset + ip_len > decrypted.size()) break;
            peer.ip = decrypted.substr(offset, ip_len);
            offset += ip_len;

            if (offset + 4 > decrypted.size()) break;
            peer.iport = (static_cast<uint8_t>(decrypted[offset]) << 8)
                       |  static_cast<uint8_t>(decrypted[offset + 1]);
            offset += 2;
            peer.oport = (static_cast<uint8_t>(decrypted[offset]) << 8)
                       |  static_cast<uint8_t>(decrypted[offset + 1]);
            offset += 2;

            found_peers.push_back(peer);
            std::cout << "[c_client] Znaleziono peera: " << peer.uuid
                      << " " << peer.ip << ":" << peer.iport << "/" << peer.oport << "\n";

            if (user_connections_->count(peer.uuid)) {
                std::string ip_io_path = "./friends/" + peer.uuid + "/ip_io";
                std::ofstream fout(ip_io_path);
                fout << peer.ip << "\n" << peer.iport << "\n" << peer.oport << "\n";
                fout.close();

                auto& conn = (*user_connections_)[peer.uuid];
                if (conn == nullptr) {
                    try {
                        conn = new udp_server(io_, peer.iport, peer.oport, peer.ip, peer.uuid);
                    } catch (const std::exception& e) {
                        std::cerr << "[c_client] Błąd tworzenia udp_server: " << e.what() << "\n";
                    }
                } else {
                    conn->change_target(peer.iport, peer.oport, peer.ip);
                }
            }
        }

        schedule_retry(2);
    }

    void send_frame(uint8_t type, const std::string& data, std::function<void()> on_done) {
        uint32_t len = static_cast<uint32_t>(data.size());
        write_buf_.resize(5 + len);
        write_buf_[0] = type;
        write_buf_[1] = (len >> 24) & 0xFF;
        write_buf_[2] = (len >> 16) & 0xFF;
        write_buf_[3] = (len >> 8)  & 0xFF;
        write_buf_[4] =  len        & 0xFF;
        std::copy(data.begin(), data.end(), write_buf_.begin() + 5);

        auto self = shared_from_this_workaround();
        asio::async_write(socket_, asio::buffer(write_buf_),
            [this, self, on_done](const std::error_code& ec, size_t) {
                if (ec) { reconnect(); return; }
                on_done();
            });
    }

    void read_header_then(std::function<void()> on_payload) {
        auto self = shared_from_this_workaround();
        asio::async_read(socket_, asio::buffer(hdr_, 5),
            [this, self, on_payload](const std::error_code& ec, size_t) {
                if (ec) { reconnect(); return; }
                uint32_t len = (static_cast<uint32_t>(hdr_[1]) << 24)
                             | (static_cast<uint32_t>(hdr_[2]) << 16)
                             | (static_cast<uint32_t>(hdr_[3]) << 8)
                             |  static_cast<uint32_t>(hdr_[4]);
                payload_.resize(len);
                asio::async_read(socket_, asio::buffer(payload_),
                    [this, self, on_payload](const std::error_code& ec2, size_t) {
                        if (ec2) { reconnect(); return; }
                        on_payload();
                    });
            });
    }

    void reconnect() {
        asio::error_code ec;
        socket_.close(ec);
        socket_ = tcp::socket(io_);
        schedule_retry(5);
    }

    void schedule_retry(int seconds) {
        timer_.expires_after(std::chrono::seconds(seconds));
        
        if (!connection_to_server_active) {
            if (socket_.is_open()) {
                std::error_code ec;
                socket_.close(ec);
                registered_ = false;
                std::cout << "[c_client] Rozłączono z serwerem (flaga inactive)\n";
            }
        }

        auto self = shared_from_this_workaround();
        timer_.async_wait([this, self](const std::error_code& ec) {
            if (ec) return;
            
            if (!connection_to_server_active) { 
                schedule_retry(2); 
                return; 
            }
            
            if (!socket_.is_open()) {
                std::cout << "[c_client] Wykryto aktywację flagi, uruchamiam połączenie od nowa...\n";
                do_connect();
            } else {
                do_routine();
            }
        });
    }

    std::shared_ptr<tcp_c_client> shared_from_this_workaround() {
        return self_ptr_;
    }

public:
    std::shared_ptr<tcp_c_client> self_ptr_;
};


int main(int argc, char const *argv[])
{
    generate_or_load_ecc_keys();
    connection_to_server_active = read_file("./user_settings/server.txt")=="on"?true:false; //pobranie z pliku preferencji użytkownika odnośnie korzystania z serwera koordynującego
    srand(time(NULL));

    system("start http://127.0.0.1:80");    //otworzenie przeglądarki

    system("chcp 65001");   //włączenie polskich znaków w konsoli

    try
    {
        uuid = read_file("./user_settings/uuid.txt");
    } catch(...) { }
    
    try {
        asio::io_context io_context;
        udp::socket net_socket(io_context, udp::v4());
        udp::endpoint remote_endpoint(asio::ip::make_address("8.8.8.8"), 53);
        net_socket.connect(remote_endpoint);

        public_ip = net_socket.local_endpoint().address().to_string();  //public w tym przypadku oznacza zewnętrzny ale lokalny (czyli poziom dalej niż loopback)
        std::cout << "Wykryty adres sieciowy: " << public_ip << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Błąd wykrywania IP: " << e.what() << std::endl;
        public_ip = "127.0.0.1"; // Wartość awaryjna
    }

    asio::io_context io;
    
    user_selection();
    
    
    std::unordered_map<std::string, udp_server*> user_connections = load_connections(io);   //wczytanie wszystkich połączeń z użytkownikami



    
    
    http_server http_server(io, &user_connections);
    auto c_client = std::make_shared<tcp_c_client>(io, C_SERVER_IP, C_SERVER_PORT, &user_connections);
    c_client->self_ptr_ = c_client;
    c_client->start();


    io.run();


    return 0;
}
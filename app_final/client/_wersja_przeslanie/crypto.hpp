#pragma once

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


#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>
#include <stdexcept>
#include <openssl/sha.h>

#include <string>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;


uint32_t CRC32(const std::array<char, 495>& arr) {                  //funkcja licząca sumę kontrolną dla faktycznej treści zgodnie ze wzorem pakietu udp z dokumentacji
    static const auto crc_table = []() {
        std::array<uint32_t, 256> table;
        const uint32_t polynomial = 0xEDB88320;
        
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (uint32_t j = 0; j < 8; ++j) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ polynomial;
                } else {
                    crc >>= 1;
                }
            }
            table[i] = crc;
        }
        return table;
    }();

    uint32_t crc = 0xFFFFFFFF; 

    for (char byte : arr) {
        uint8_t index = static_cast<uint8_t>(crc ^ byte);
        crc = (crc >> 8) ^ crc_table[index];
    }

    return crc ^ 0xFFFFFFFF; 
}



std::vector<uint8_t> aes_encrypt(const std::string& plaintext, const std::array<uint8_t, 32>& key) {    //szyfrowanie aes
    std::array<uint8_t, 16> iv;
    RAND_bytes(iv.data(), 16); // losowy IV

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()))
        throw std::runtime_error("EVP_EncryptInit_ex failed");

    std::vector<uint8_t> ciphertext(plaintext.size() + 16); // +16 na padding
    int len = 0, total = 0;

    if (!EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
        reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size()))
        throw std::runtime_error("EVP_EncryptUpdate failed");
    total = len;

    if (!EVP_EncryptFinal_ex(ctx, ciphertext.data() + total, &len))
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    total += len;

    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(total);

    // dołącz IV na początku: [16B IV][ciphertext]
    std::vector<uint8_t> result(iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    return result;
}

std::string aes_decrypt(const std::vector<uint8_t>& input, const std::array<uint8_t, 32>& key) {    //deszyfrowanie aes
if (input.size() < 16) throw std::runtime_error("Input too short");

    // wyciągnij IV z początku
    std::array<uint8_t, 16> iv;
    std::copy(input.begin(), input.begin() + 16, iv.begin());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()))
        throw std::runtime_error("EVP_DecryptInit_ex failed");

    std::vector<uint8_t> plaintext(input.size() - 16);
    int len = 0, total = 0;

    if (!EVP_DecryptUpdate(ctx, plaintext.data(), &len,
        input.data() + 16, input.size() - 16))
        throw std::runtime_error("EVP_DecryptUpdate failed");
    total = len;

    if (!EVP_DecryptFinal_ex(ctx, plaintext.data() + total, &len))
        throw std::runtime_error("EVP_DecryptFinal_ex failed - zły klucz lub uszkodzone dane");
    total += len;

    EVP_CIPHER_CTX_free(ctx);
    return std::string(plaintext.begin(), plaintext.begin() + total);
}


std::array<uint8_t, 32> hex_to_key(const std::string& hex) {        //potrzebne do wczytywania klucza AES z pliku
    if (hex.size() != 64) throw std::runtime_error("Zły klucz - musi mieć 64 znaki hex");
    std::array<uint8_t, 32> key{};
    for (int i = 0; i < 32; ++i) {
        key[i] = static_cast<uint8_t>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
    }
    return key;
}


std::array<uint8_t, 32> load_key(const std::string& _uuid) {    //wczytywanie klucza AES z pliku
    std::ifstream f("./friends/" + _uuid + "/AES");
    if (!f.is_open()) throw std::runtime_error("Brak klucza dla: " + _uuid);
    std::string hex;
    std::getline(f, hex);
    while (!hex.empty() && (hex.back() == '\r' || hex.back() == '\n'))
        hex.pop_back();
    if (hex.empty()) throw std::runtime_error("Pusty klucz AES dla: " + _uuid);
    return hex_to_key(hex);
}



void generate_or_load_ecc_keys() {
    std::string priv_path = "./user_settings/ECC";
    std::string pub_path  = "./user_settings/ECC_PUB";

    EVP_PKEY* pkey = nullptr;

    if (fs::exists(priv_path)) {
        // wczytaj istniejący klucz prywatny
        std::ifstream f(priv_path, std::ios::binary);
        std::vector<uint8_t> priv_bytes((std::istreambuf_iterator<char>(f)),
                                         std::istreambuf_iterator<char>());

        pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr,
                                             priv_bytes.data(), priv_bytes.size());
        if (!pkey) throw std::runtime_error("Błąd wczytywania klucza ECC");
    } else {
        // generuj nowy klucz
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
        if (!ctx) throw std::runtime_error("EVP_PKEY_CTX_new_id failed");

        if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, &pkey) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            throw std::runtime_error("EVP_PKEY_keygen failed");
        }
        EVP_PKEY_CTX_free(ctx);

        // zapisz klucz prywatny (32 bajty raw)
        size_t priv_len = 32;
        std::vector<uint8_t> priv_bytes(priv_len);
        if (EVP_PKEY_get_raw_private_key(pkey, priv_bytes.data(), &priv_len) <= 0)
            throw std::runtime_error("Błąd eksportu klucza prywatnego");

        std::ofstream f(priv_path, std::ios::binary);
        f.write(reinterpret_cast<char*>(priv_bytes.data()), priv_len);
        f.close();
        std::cout << "Wygenerowano nowy klucz ECC X25519\n";
    }

    // zapisz klucz publiczny (32 bajty raw)
    size_t pub_len = 32;
    std::vector<uint8_t> pub_bytes(pub_len);
    if (EVP_PKEY_get_raw_public_key(pkey, pub_bytes.data(), &pub_len) <= 0) {
        EVP_PKEY_free(pkey);
        throw std::runtime_error("Błąd eksportu klucza publicznego");
    }

    std::ofstream pf(pub_path, std::ios::binary);
    pf.write(reinterpret_cast<char*>(pub_bytes.data()), pub_len);
    pf.close();

    EVP_PKEY_free(pkey);
    std::cout << "Klucz publiczny ECC zapisany\n";
}




std::string encrypt_ECC(std::array<uint8_t, 32> AES_key, std::array<uint8_t, 32> public_ecc_key) {      //funkcja szyfrująca ECC

    // generacja efemerycznej pary kluczy X25519
    EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
    if (!kctx) throw std::runtime_error("EVP_PKEY_CTX_new_id failed");

    EVP_PKEY* ephemeral_key = nullptr;
    if (EVP_PKEY_keygen_init(kctx) <= 0 || EVP_PKEY_keygen(kctx, &ephemeral_key) <= 0) {
        EVP_PKEY_CTX_free(kctx);
        throw std::runtime_error("EVP_PKEY_keygen failed");
    }
    EVP_PKEY_CTX_free(kctx);

    // eksportowanie efemerycznego klucza publicznego (32 bajty)
    std::array<uint8_t, 32> ephemeral_pub{};
    size_t pub_len = 32;
    if (EVP_PKEY_get_raw_public_key(ephemeral_key, ephemeral_pub.data(), &pub_len) <= 0) {
        EVP_PKEY_free(ephemeral_key);
        throw std::runtime_error("EVP_PKEY_get_raw_public_key failed");
    }

    // ładowanie publicznego klucza rozmówcy
    EVP_PKEY* peer_pub = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, nullptr,
        public_ecc_key.data(), 32
    );
    if (!peer_pub) {
        EVP_PKEY_free(ephemeral_key);
        throw std::runtime_error("EVP_PKEY_new_raw_public_key failed");
    }

    // ECDH - obliczanie shared secret
    EVP_PKEY_CTX* dctx = EVP_PKEY_CTX_new(ephemeral_key, nullptr);
    if (!dctx) {
        EVP_PKEY_free(ephemeral_key);
        EVP_PKEY_free(peer_pub);
        throw std::runtime_error("EVP_PKEY_CTX_new failed");
    }

    if (EVP_PKEY_derive_init(dctx) <= 0) {
        EVP_PKEY_CTX_free(dctx);
        EVP_PKEY_free(ephemeral_key);
        EVP_PKEY_free(peer_pub);
        throw std::runtime_error("EVP_PKEY_derive_init failed");
    }

    if (EVP_PKEY_derive_set_peer(dctx, peer_pub) <= 0) {
        EVP_PKEY_CTX_free(dctx);
        EVP_PKEY_free(ephemeral_key);
        EVP_PKEY_free(peer_pub);
        throw std::runtime_error("EVP_PKEY_derive_set_peer failed");
    }

    size_t secret_len = 32;
    std::array<uint8_t, 32> shared_secret{};
    if (EVP_PKEY_derive(dctx, shared_secret.data(), &secret_len) <= 0) {
        EVP_PKEY_CTX_free(dctx);
        EVP_PKEY_free(ephemeral_key);
        EVP_PKEY_free(peer_pub);
        throw std::runtime_error("EVP_PKEY_derive failed");
    }

    EVP_PKEY_CTX_free(dctx);
    EVP_PKEY_free(ephemeral_key);
    EVP_PKEY_free(peer_pub);

    // sha256 shared secret - klucz AES do owijania
    std::array<uint8_t, 32> wrap_key{};
    SHA256(shared_secret.data(), 32, wrap_key.data());

    // szyfrowanie AES_key (32 bajty) używając wrap_key przez aes_encrypt
    std::string aes_key_str(reinterpret_cast<char*>(AES_key.data()), 32);
    std::vector<uint8_t> encrypted_aes = aes_encrypt(aes_key_str, wrap_key);

    // złożenie wyniku: [32B ephemeral_pub] + [zaszyfrowany AES_key]
    std::string result(reinterpret_cast<char*>(ephemeral_pub.data()), 32);
    result += std::string(reinterpret_cast<char*>(encrypted_aes.data()), encrypted_aes.size());

    return result;
}

std::array<uint8_t, 32> decrypt_ECC(const std::string& encrypted_payload) {
    
    if (encrypted_payload.size() < 32 + 16 + 1)
        throw std::runtime_error("decrypt_ECC: payload za krótki");

    // wyciągnięcie efemerycznego klucza publicznego (pierwsze 32 bajty)
    std::array<uint8_t, 32> ephemeral_pub{};
    std::copy(
        encrypted_payload.begin(),
        encrypted_payload.begin() + 32,
        ephemeral_pub.begin()
    );

    // wczytanie własnego klucza prywatnego z pliku
    std::ifstream f("./user_settings/ECC", std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("decrypt_ECC: brak klucza prywatnego ECC");

    std::vector<uint8_t> priv_bytes(
        (std::istreambuf_iterator<char>(f)),
         std::istreambuf_iterator<char>()
    );
    f.close();

    if (priv_bytes.size() != 32)
        throw std::runtime_error("decrypt_ECC: zły rozmiar klucza prywatnego");

    // ładowanie własnego klucza prywatnego jako EVP_PKEY
    EVP_PKEY* own_priv = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_X25519, nullptr,
        priv_bytes.data(), 32
    );
    if (!own_priv)
        throw std::runtime_error("decrypt_ECC: EVP_PKEY_new_raw_private_key failed");

    // ładowanie efemerycznego klucza publicznego jako EVP_PKEY
    EVP_PKEY* eph_pub = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, nullptr,
        ephemeral_pub.data(), 32
    );
    if (!eph_pub) {
        EVP_PKEY_free(own_priv);
        throw std::runtime_error("decrypt_ECC: EVP_PKEY_new_raw_public_key failed");
    }

    // ECDH - obliczanie shared secret
    EVP_PKEY_CTX* dctx = EVP_PKEY_CTX_new(own_priv, nullptr);
    if (!dctx) {
        EVP_PKEY_free(own_priv);
        EVP_PKEY_free(eph_pub);
        throw std::runtime_error("decrypt_ECC: EVP_PKEY_CTX_new failed");
    }

    if (EVP_PKEY_derive_init(dctx) <= 0 ||
        EVP_PKEY_derive_set_peer(dctx, eph_pub) <= 0) {
        EVP_PKEY_CTX_free(dctx);
        EVP_PKEY_free(own_priv);
        EVP_PKEY_free(eph_pub);
        throw std::runtime_error("decrypt_ECC: ECDH init/set_peer failed");
    }

    size_t secret_len = 32;
    std::array<uint8_t, 32> shared_secret{};
    if (EVP_PKEY_derive(dctx, shared_secret.data(), &secret_len) <= 0) {
        EVP_PKEY_CTX_free(dctx);
        EVP_PKEY_free(own_priv);
        EVP_PKEY_free(eph_pub);
        throw std::runtime_error("decrypt_ECC: EVP_PKEY_derive failed");
    }

    EVP_PKEY_CTX_free(dctx);
    EVP_PKEY_free(own_priv);
    EVP_PKEY_free(eph_pub);

    // sha256 shared secret - klucz do odszyfrowania
    std::array<uint8_t, 32> wrap_key{};
    SHA256(shared_secret.data(), 32, wrap_key.data());

    // deszyfrowanie payloadu (bajty 32 do końca) - surowy klucz AES (32 bajty)
    std::vector<uint8_t> encrypted_aes(
        reinterpret_cast<const uint8_t*>(encrypted_payload.data()) + 32,
        reinterpret_cast<const uint8_t*>(encrypted_payload.data()) + encrypted_payload.size()
    );

    std::string decrypted = aes_decrypt(encrypted_aes, wrap_key);

    if (decrypted.size() != 32)
        throw std::runtime_error("decrypt_ECC: odszyfrowany klucz ma zły rozmiar: "
                                 + std::to_string(decrypted.size()));

    std::array<uint8_t, 32> aes_key{};
    std::copy(decrypted.begin(), decrypted.end(), aes_key.begin());
    return aes_key;
}


std::array<uint8_t, 32> generate_AES(std::string _uuid) {       //generowanie klucza AES
    std::array<uint8_t, 32> key{};
    RAND_bytes(key.data(), 32);

    // zapisz jako hex do pliku AES
    std::ofstream f("./friends/" + _uuid + "/AES");
    for (int i = 0; i < 32; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", key[i]);
        f << buf;
    }
    f.close();

    return key;
}

void save_hex_key(const std::string& path, const std::array<uint8_t, 32>& key) { //zapis klucza jako hex
    std::ofstream f(path);
    for (int i = 0; i < 32; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", key[i]);
        f << buf;
    }
}

std::array<uint8_t, 32> load_hex_key(const std::string& path) {         //wczytanie klucza zapisanego jako hex
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Brak pliku klucza: " + path);
    std::string hex;
    std::getline(f, hex);
    while (!hex.empty() && (hex.back() == '\r' || hex.back() == '\n')) hex.pop_back();
    if (hex.size() != 64) throw std::runtime_error("Zły format klucza hex: " + path);
    std::array<uint8_t, 32> key{};
    for (int i = 0; i < 32; ++i)
        key[i] = static_cast<uint8_t>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
    return key;
}
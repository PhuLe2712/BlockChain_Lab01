#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace crypto {
struct KeyPair {
    std::vector<unsigned char> private_key; // 32-byte Ed25519 seed
    std::vector<unsigned char> public_key;  // 32-byte public key
    std::string address() const;
};
KeyPair keyPairFromSeed(const std::vector<unsigned char>& seed);
std::string sha256(const std::string& data);
std::string sign(const KeyPair& key, const std::string& domain, const std::string& message);
bool verify(const std::string& public_key_hex, const std::string& domain,
            const std::string& message, const std::string& signature_hex);
std::string toHex(const std::vector<unsigned char>& bytes);
std::vector<unsigned char> fromHex(const std::string& hex);
}

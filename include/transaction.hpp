#pragma once
#include "crypto.hpp"
#include <cstdint>
#include <string>
struct Transaction {
    std::string sender;
    std::string key;
    std::string value;
    std::uint64_t nonce{};
    std::string signature;
    std::string payload() const;
    void sign(const crypto::KeyPair& signer);
    bool isValid() const;
    std::string id() const;
};

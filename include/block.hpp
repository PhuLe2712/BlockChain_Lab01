
#pragma once

#include "state.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct Block {
    std::uint64_t height{};
    std::string parent_hash;
    std::string state_root;
    std::string proposer;
    std::vector<Transaction> transactions;
    std::string signature;

    std::string headerPayload() const;
    std::string hash() const;

    void sign(const crypto::KeyPair& key);

    bool verifyHeader() const;

    bool validateBlock(
        const State& previousState,
        const std::string& expectedParentHash,
        std::uint64_t expectedHeight,
        std::string* reason = nullptr
    ) const;
};

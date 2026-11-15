
#pragma once

#include "crypto.hpp"

#include <cstdint>
#include <optional>
#include <string>

enum class VotePhase {
    PREVOTE,
    PRECOMMIT
};

struct Vote {
    std::string chain_id;
    std::uint64_t height{};
    std::uint64_t round{};
    VotePhase phase{VotePhase::PREVOTE};

    // Empty block_hash means NIL.
    std::string block_hash;

    // Round of the POL (proof of lock) authorizing an unlock.
    std::optional<std::uint64_t> valid_round;

    std::string validator_id;
    std::string signature;

    std::string payload() const;
    std::string domain() const;
    std::string phaseName() const;

    void sign(const crypto::KeyPair& key);
    bool verify(const std::string& publicKey) const;
};

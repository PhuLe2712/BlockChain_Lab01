
#include "vote.hpp"

#include <stdexcept>

namespace {

std::string enc(const std::string& value) {
    return std::to_string(value.size()) + ":" + value;
}

}

std::string Vote::phaseName() const {
    return phase == VotePhase::PREVOTE
        ? "PREVOTE"
        : "PRECOMMIT";
}

std::string Vote::domain() const {
    return "VOTE:" + chain_id;
}

std::string Vote::payload() const {
    const std::string validRoundValue =
        valid_round.has_value()
            ? std::to_string(*valid_round)
            : "NONE";

    return enc(chain_id) +
           std::to_string(height) + "|" +
           std::to_string(round) + "|" +
           phaseName() + "|" +
           enc(block_hash) +
           enc(validRoundValue) +
           enc(validator_id);
}

void Vote::sign(const crypto::KeyPair& key) {
    if (chain_id.empty() || validator_id.empty()) {
        throw std::invalid_argument(
            "Vote chain_id and validator_id cannot be empty"
        );
    }

    if (height == 0) {
        throw std::invalid_argument("Vote height must be positive");
    }

    if (valid_round.has_value()) {
        if (phase != VotePhase::PREVOTE ||
            *valid_round >= round) {
            throw std::invalid_argument("Invalid vote valid_round");
        }
    }

    signature = crypto::sign(
        key,
        domain(),
        payload()
    );
}

bool Vote::verify(const std::string& publicKey) const {
    if (chain_id.empty() ||
        validator_id.empty() ||
        signature.empty() ||
        height == 0) {
        return false;
    }

    if (valid_round.has_value()) {
        if (phase != VotePhase::PREVOTE ||
            *valid_round >= round) {
            return false;
        }
    }

    return crypto::verify(
        publicKey,
        domain(),
        payload(),
        signature
    );
}

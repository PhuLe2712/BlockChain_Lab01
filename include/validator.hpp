
#pragma once

#include "crypto.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct Validator {
    std::string id;
    crypto::KeyPair key;
    bool byzantine{false};
};

class ValidatorSet {
public:
    ValidatorSet(
        const std::string& chainId,
        std::size_t validatorCount,
        std::size_t maxByzantine,
        const std::string& seed = "lab01-validator-seed"
    );

    const Validator* find(const std::string& id) const;

    bool isValidator(const std::string& id) const;

    bool setByzantine(
        const std::string& id,
        bool byzantine
    );

    const Validator* proposerFor(
        std::uint64_t height,
        std::uint64_t round
    ) const;

    std::size_t validatorCount() const;

    std::size_t maxByzantine() const;

    std::size_t quorumSize() const;

    std::size_t byzantineCount() const;

    bool hasQuorum(
        const std::vector<std::string>& voterIds
    ) const;

    const std::vector<std::string>& validatorIds() const;

private:
    std::string chainId_;
    std::size_t maxByzantine_;

    std::vector<std::string> orderedIds_;
    std::map<std::string, Validator> validators_;
};
    
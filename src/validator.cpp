
#include "validator.hpp"

#include <stdexcept>
#include <set>

ValidatorSet::ValidatorSet(
    const std::string& chainId,
    std::size_t validatorCount,
    std::size_t maxByzantine,
    const std::string& seed
)
    : chainId_(chainId),
      maxByzantine_(maxByzantine) {

    if (chainId.empty()) {
        throw std::invalid_argument("chain_id cannot be empty");
    }

    if (validatorCount == 0) {
        throw std::invalid_argument("Validator count must be positive");
    }

    if (validatorCount != 3 * maxByzantine + 1) {
        throw std::invalid_argument(
            "Validator count must satisfy n = 3f + 1"
        );
    }

    for (std::size_t i = 0; i < validatorCount; ++i) {
        Validator validator;

        validator.id = "validator-" + std::to_string(i);

        // Derive a deterministic Ed25519 seed for each validator.
        const std::string keyMaterial =
            seed + "|" + chainId_ + "|" + validator.id;

        const std::string seedHex = crypto::sha256(keyMaterial);

        const std::vector<unsigned char> privateSeed =
            crypto::fromHex(seedHex);

        validator.key = crypto::keyPairFromSeed(privateSeed);
        validator.byzantine = false;

        orderedIds_.push_back(validator.id);

        validators_.emplace(
            validator.id,
            std::move(validator)
        );
    }
}

const Validator* ValidatorSet::find(
    const std::string& id
) const {
    auto it = validators_.find(id);

    if (it == validators_.end()) {
        return nullptr;
    }

    return &it->second;
}

bool ValidatorSet::isValidator(
    const std::string& id
) const {
    return validators_.find(id) != validators_.end();
}

bool ValidatorSet::setByzantine(
    const std::string& id,
    bool byzantine
) {
    auto it = validators_.find(id);

    if (it == validators_.end()) {
        return false;
    }

    if (it->second.byzantine == byzantine) {
        return true;
    }

    if (byzantine && byzantineCount() >= maxByzantine_) {
        return false;
    }

    it->second.byzantine = byzantine;

    return true;
}

const Validator* ValidatorSet::proposerFor(
    std::uint64_t height,
    std::uint64_t round
) const {
    if (orderedIds_.empty() || height == 0) {
        return nullptr;
    }

    // Deterministic round-robin proposer selection.
    const std::size_t index =
        static_cast<std::size_t>(
            ((height - 1) + round) % orderedIds_.size()
        );

    return find(orderedIds_[index]);
}

std::size_t ValidatorSet::validatorCount() const {
    return validators_.size();
}

std::size_t ValidatorSet::maxByzantine() const {
    return maxByzantine_;
}

std::size_t ValidatorSet::quorumSize() const {
    return 2 * maxByzantine_ + 1;
}

std::size_t ValidatorSet::byzantineCount() const {
    std::size_t count = 0;

    for (const auto& [id, validator] : validators_) {
        (void)id;

        if (validator.byzantine) {
            ++count;
        }
    }

    return count;
}

bool ValidatorSet::hasQuorum(
    const std::vector<std::string>& voterIds
) const {
    std::set<std::string> uniqueVoters;

    for (const auto& id : voterIds) {
        if (isValidator(id)) {
            uniqueVoters.insert(id);
        }
    }

    return uniqueVoters.size() >= quorumSize();
}

const std::vector<std::string>&
ValidatorSet::validatorIds() const {
    return orderedIds_;
}

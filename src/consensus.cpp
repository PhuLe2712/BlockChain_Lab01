
#include "consensus.hpp"

#include <stdexcept>

ConsensusEngine::ConsensusEngine(
    const std::string& chainId,
    const ValidatorSet& validators,
    std::uint64_t initialHeight
)
    : chainId_(chainId),
      validators_(validators),
      height_(initialHeight) {

    if (chainId.empty()) {
        throw std::invalid_argument("chain_id cannot be empty");
    }

    if (initialHeight == 0) {
        throw std::invalid_argument(
            "Initial height must be positive"
        );
    }
}

bool ConsensusEngine::isNil(
    const std::string& blockHash
) const {
    return blockHash.empty();
}

bool ConsensusEngine::validUnlockProof(
    std::uint64_t validRound,
    const std::string& blockHash
) const {
    if (validRound >= round_ || isNil(blockHash)) {
        return false;
    }

    return hasPrevoteQuorum(validRound, blockHash);
}

bool ConsensusEngine::canPrevote(
    const std::string& validatorId,
    std::uint64_t round,
    const std::string& blockHash,
    std::optional<std::uint64_t> validRound
) const {
    if (isNil(blockHash)) {
        return true;
    }

    auto lockIt = locks_.find(validatorId);

    if (lockIt == locks_.end()) {
        return !validRound.has_value() ||
               validUnlockProof(*validRound, blockHash);
    }

    const LockState& lock = lockIt->second;

    if (lock.blockHash == blockHash) {
        return !validRound.has_value() ||
               validUnlockProof(*validRound, blockHash);
    }

    if (!validRound.has_value()) {
        return false;
    }

    if (*validRound <= lock.round || *validRound >= round) {
        return false;
    }

    return validUnlockProof(*validRound, blockHash);
}

Vote ConsensusEngine::makeVote(
    const std::string& validatorId,
    VotePhase phase,
    const std::string& blockHash,
    std::optional<std::uint64_t> validRound
) {
    const Validator* validator = validators_.find(validatorId);

    if (!validator) {
        throw std::invalid_argument("Unknown validator");
    }

    if (finalizedBlock_) {
        throw std::logic_error("Height already finalized");
    }

    if (validRound.has_value() &&
        (phase != VotePhase::PREVOTE ||
         *validRound >= round_)) {
        throw std::logic_error("Invalid valid_round");
    }

    LocalVoteKey localKey{
        height_,
        round_,
        phase,
        validatorId
    };

    if (localVotes_.find(localKey) != localVotes_.end()) {
        throw std::logic_error(
            "Validator already voted in this phase and round"
        );
    }

    if (phase == VotePhase::PREVOTE) {
        if (!canPrevote(
                validatorId,
                round_,
                blockHash,
                validRound)) {
            throw std::logic_error(
                "Prevote violates validator locking rules"
            );
        }
    }

    if (phase == VotePhase::PRECOMMIT && !isNil(blockHash)) {
        auto lockIt = locks_.find(validatorId);

        if (!hasPrevoteQuorum(round_, blockHash)) {
            throw std::logic_error(
                "Cannot precommit without prevote quorum"
            );
        }

        if (lockIt != locks_.end() &&
            lockIt->second.blockHash != blockHash) {
            throw std::logic_error(
                "Precommit conflicts with validator lock"
            );
        }
    }

    Vote vote;

    vote.chain_id = chainId_;
    vote.height = height_;
    vote.round = round_;
    vote.phase = phase;
    vote.block_hash = blockHash;
    vote.valid_round = validRound;
    vote.validator_id = validatorId;

    vote.sign(validator->key);

    localVotes_[localKey] = blockHash;

    return vote;
}

VoteResult ConsensusEngine::submitVote(const Vote& vote) {
    if (finalizedBlock_) {
        return VoteResult::ALREADY_FINALIZED;
    }

    if (vote.chain_id != chainId_) {
        return VoteResult::INVALID_SIGNATURE;
    }

    if (vote.height != height_) {
        return VoteResult::INVALID_HEIGHT;
    }

    if (vote.round != round_) {
        return VoteResult::INVALID_ROUND;
    }

    const Validator* validator =
        validators_.find(vote.validator_id);

    if (!validator) {
        return VoteResult::INVALID_VALIDATOR;
    }

    if (!vote.verify(validator->key.address())) {
        return VoteResult::INVALID_SIGNATURE;
    }

    if (vote.valid_round.has_value()) {
        if (vote.phase != VotePhase::PREVOTE ||
            *vote.valid_round >= vote.round ||
            !validUnlockProof(
                *vote.valid_round,
                vote.block_hash)) {
            return VoteResult::INVALID_VALID_ROUND;
        }
    }

    VoteKey key{
        vote.height,
        vote.round,
        vote.phase
    };

    auto& phaseVotes = votes_[key];

    auto existing = phaseVotes.find(vote.validator_id);

    if (existing != phaseVotes.end()) {
        if (existing->second == vote.block_hash) {
            return VoteResult::DUPLICATE;
        }

        // Equivocation: remove the validator's previous vote
        // so neither conflicting vote contributes to quorum.
        phaseVotes.erase(existing);
        equivocators_[key].insert(vote.validator_id);

        return VoteResult::EQUIVOCATION;
    }

    if (equivocators_[key].count(vote.validator_id) > 0) {
        return VoteResult::EQUIVOCATION;
    }

    if (vote.phase == VotePhase::PREVOTE) {
        if (!canPrevote(
                vote.validator_id,
                vote.round,
                vote.block_hash,
                vote.valid_round)) {
            return VoteResult::INVALID_VALID_ROUND;
        }
    }

    if (vote.phase == VotePhase::PRECOMMIT &&
        !isNil(vote.block_hash)) {

        if (!hasPrevoteQuorum(
                vote.round,
                vote.block_hash)) {
            return VoteResult::INVALID_PRECOMMIT;
        }

        auto lockIt = locks_.find(vote.validator_id);

        if (lockIt != locks_.end() &&
            lockIt->second.blockHash != vote.block_hash) {
            return VoteResult::INVALID_PRECOMMIT;
        }

        // A non-NIL precommit records this validator's lock.
        updateLock(
            vote.validator_id,
            vote.round,
            vote.block_hash
        );
    }

    phaseVotes.emplace(
        vote.validator_id,
        vote.block_hash
    );

    // The current single-engine test harness has no per-node message inbox.
    // Therefore, the validator whose incoming PREVOTE completes the quorum
    // is treated as the validator that has observed that quorum.
    // A real distributed implementation must maintain a separate observed
    // vote set and lock state for each node.
    if (vote.phase == VotePhase::PREVOTE &&
        !isNil(vote.block_hash) &&
        hasPrevoteQuorum(vote.round, vote.block_hash)) {
        updateLock(
            vote.validator_id,
            vote.round,
            vote.block_hash
        );
    }

    if (vote.phase == VotePhase::PRECOMMIT &&
        hasPrecommitQuorum(
            vote.round,
            vote.block_hash) &&
        !isNil(vote.block_hash)) {

        finalizedBlock_ = vote.block_hash;
    }

    return VoteResult::ACCEPTED;
}

void ConsensusEngine::updateLock(
    const std::string& validatorId,
    std::uint64_t round,
    const std::string& blockHash
) {
    auto it = locks_.find(validatorId);

    if (it == locks_.end() || round >= it->second.round) {
        locks_[validatorId] = LockState{blockHash, round};
    }
}

bool ConsensusEngine::hasPrevoteQuorum(
    std::uint64_t round,
    const std::string& blockHash
) const {
    VoteKey key{height_, round, VotePhase::PREVOTE};

    auto it = votes_.find(key);

    if (it == votes_.end()) {
        return false;
    }

    std::size_t count = 0;

    for (const auto& [validatorId, votedBlock] : it->second) {
        if (validators_.isValidator(validatorId) &&
            equivocators_.find(key) != equivocators_.end() &&
            equivocators_.at(key).count(validatorId) == 0 &&
            votedBlock == blockHash) {
            ++count;
        } else if (
            validators_.isValidator(validatorId) &&
            equivocators_.find(key) == equivocators_.end() &&
            votedBlock == blockHash) {
            ++count;
        }
    }

    return count >= validators_.quorumSize();
}

bool ConsensusEngine::hasPrecommitQuorum(
    std::uint64_t round,
    const std::string& blockHash
) const {
    VoteKey key{height_, round, VotePhase::PRECOMMIT};

    auto it = votes_.find(key);

    if (it == votes_.end()) {
        return false;
    }

    std::size_t count = 0;

    for (const auto& [validatorId, votedBlock] : it->second) {
        if (!validators_.isValidator(validatorId) ||
            votedBlock != blockHash) {
            continue;
        }

        auto equivocationIt = equivocators_.find(key);

        if (equivocationIt != equivocators_.end() &&
            equivocationIt->second.count(validatorId) > 0) {
            continue;
        }

        ++count;
    }

    return count >= validators_.quorumSize();
}

bool ConsensusEngine::advanceRound() {
    if (finalizedBlock_) {
        return false;
    }

    ++round_;
    return true;
}

bool ConsensusEngine::isFinalized() const {
    return finalizedBlock_.has_value();
}

std::optional<std::string>
ConsensusEngine::finalizedBlock() const {
    return finalizedBlock_;
}

std::optional<std::string>
ConsensusEngine::lockedBlock(
    const std::string& validatorId
) const {
    auto it = locks_.find(validatorId);

    if (it == locks_.end()) {
        return std::nullopt;
    }

    return it->second.blockHash;
}

std::uint64_t ConsensusEngine::lockedRound(
    const std::string& validatorId
) const {
    auto it = locks_.find(validatorId);

    if (it == locks_.end()) {
        return 0;
    }

    return it->second.round;
}

std::uint64_t ConsensusEngine::height() const {
    return height_;
}

std::uint64_t ConsensusEngine::round() const {
    return round_;
}

bool ConsensusEngine::isEquivocator(
    const std::string& validatorId,
    VotePhase phase,
    std::uint64_t round
) const {
    VoteKey key{height_, round, phase};

    auto it = equivocators_.find(key);

    return it != equivocators_.end() &&
           it->second.count(validatorId) > 0;
}

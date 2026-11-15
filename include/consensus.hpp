
#pragma once

#include "validator.hpp"
#include "vote.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>

enum class VoteResult {
    ACCEPTED,
    DUPLICATE,
    INVALID_VALIDATOR,
    INVALID_SIGNATURE,
    INVALID_HEIGHT,
    INVALID_ROUND,
    INVALID_VALID_ROUND,
    EQUIVOCATION,
    INVALID_PRECOMMIT,
    ALREADY_FINALIZED
};

class ConsensusEngine {
public:
    ConsensusEngine(
        const std::string& chainId,
        const ValidatorSet& validators,
        std::uint64_t initialHeight = 1
    );

    Vote makeVote(
        const std::string& validatorId,
        VotePhase phase,
        const std::string& blockHash,
        std::optional<std::uint64_t> validRound = std::nullopt
    );

    VoteResult submitVote(const Vote& vote);

    bool hasPrevoteQuorum(
        std::uint64_t round,
        const std::string& blockHash
    ) const;

    bool hasPrecommitQuorum(
        std::uint64_t round,
        const std::string& blockHash
    ) const;

    bool advanceRound();

    bool isFinalized() const;

    std::optional<std::string> finalizedBlock() const;

    std::optional<std::string> lockedBlock(
        const std::string& validatorId
    ) const;

    std::uint64_t lockedRound(
        const std::string& validatorId
    ) const;

    std::uint64_t height() const;
    std::uint64_t round() const;

    bool isEquivocator(
        const std::string& validatorId,
        VotePhase phase,
        std::uint64_t round
    ) const;

private:
    using VoteKey = std::tuple<
        std::uint64_t,
        std::uint64_t,
        VotePhase
    >;

    using LocalVoteKey = std::tuple<
        std::uint64_t,
        std::uint64_t,
        VotePhase,
        std::string
    >;

    struct LockState {
        std::string blockHash;
        std::uint64_t round{};
    };

    std::string chainId_;
    const ValidatorSet& validators_;

    std::uint64_t height_;
    std::uint64_t round_{0};

    std::optional<std::string> finalizedBlock_;

    // Votes are counted by validator, height, round and phase.
    std::map<VoteKey, std::map<std::string, std::string>> votes_;

    // Locally generated vote records.
    std::map<LocalVoteKey, std::string> localVotes_;

    // Each validator maintains its own lock.
    std::map<std::string, LockState> locks_;

    // Validators that equivocated in a particular round and phase.
    std::map<VoteKey, std::set<std::string>> equivocators_;

    bool isNil(const std::string& blockHash) const;

    bool canPrevote(
        const std::string& validatorId,
        std::uint64_t round,
        const std::string& blockHash,
        std::optional<std::uint64_t> validRound
    ) const;

    bool validUnlockProof(
        std::uint64_t validRound,
        const std::string& blockHash
    ) const;

    void updateLock(
        const std::string& validatorId,
        std::uint64_t round,
        const std::string& blockHash
    );
};

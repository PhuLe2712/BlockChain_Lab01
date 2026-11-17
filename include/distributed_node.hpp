#pragma once

#include "network.hpp"
#include "validator.hpp"
#include "vote.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

class DistributedValidatorNode {
public:
    DistributedValidatorNode(
        const std::string& chainId,
        const std::string& validatorId,
        const ValidatorSet& validators
    );

    void startPrevote(
        const std::string& blockHash,
        std::uint64_t tick,
        Network& network
    );

    // Called by the simulator when the current round's proposer times out.
    // The next proposal hash is supplied by the round/proposer scheduler.
    void onRoundTimeout(
        const std::string& nextBlockHash,
        std::uint64_t tick,
        Network& network
    );

    void processMessages(
        std::uint64_t tick,
        Network& network
    );

    const std::string& id() const;
    bool isFinalized() const;
    std::optional<std::string> finalizedBlock() const;
    std::optional<std::string> lockedBlock() const;
    std::uint64_t lockedRound() const;
    std::size_t observedVoteCount(
        VotePhase phase,
        std::uint64_t round,
        const std::string& blockHash
    ) const;

private:
    using VoteKey = std::pair<std::uint64_t, VotePhase>;

    std::string chainId_;
    std::string validatorId_;
    const ValidatorSet& validators_;
    crypto::KeyPair localKey_;

    std::uint64_t height_{1};
    std::uint64_t round_{0};

    std::map<VoteKey, std::map<std::string, std::string>> observedVotes_;
    std::map<VoteKey, std::set<std::string>> equivocators_;
    std::set<VoteKey> locallyVoted_;

    std::optional<std::string> lockedBlock_;
    std::uint64_t lockedRound_{0};
    std::optional<std::string> finalizedBlock_;

    bool started_{false};
    bool precommitSent_{false};

    static std::string serializeVote(const Vote& vote);
    static bool deserializeVote(const std::string& payload, Vote& vote);

    void broadcastVote(
        const Vote& vote,
        std::uint64_t tick,
        Network& network
    );

    bool recordVote(const Vote& vote);
    bool hasQuorum(
        VotePhase phase,
        std::uint64_t round,
        const std::string& blockHash
    ) const;

    void maybePrecommit(
        std::uint64_t tick,
        Network& network
    );
    void maybeFinalize();
};


#include "distributed_node.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

DistributedValidatorNode::DistributedValidatorNode(
    const std::string& chainId,
    const std::string& validatorId,
    const ValidatorSet& validators
)
    : chainId_(chainId),
      validatorId_(validatorId),
      validators_(validators) {

    if (chainId_.empty()) {
        throw std::invalid_argument("chain_id cannot be empty");
    }

    const Validator* validator = validators_.find(validatorId_);

    if (!validator) {
        throw std::invalid_argument("Unknown validator: " + validatorId_);
    }

    // Cache only this node's signing key.
    localKey_ = validator->key;
}

std::string DistributedValidatorNode::serializeVote(const Vote& vote) {
    std::ostringstream out;

    out << std::quoted(vote.chain_id) << ' '
        << vote.height << ' '
        << vote.round << ' '
        << static_cast<int>(vote.phase) << ' '
        << std::quoted(vote.block_hash) << ' '
        << (vote.valid_round.has_value() ? 1 : 0) << ' '
        << vote.valid_round.value_or(0) << ' '
        << std::quoted(vote.validator_id) << ' '
        << std::quoted(vote.signature);

    return out.str();
}

bool DistributedValidatorNode::deserializeVote(
    const std::string& payload,
    Vote& vote
) {
    std::istringstream in(payload);

    int phase = -1;
    int hasValidRound = 0;
    std::uint64_t validRound = 0;

    if (!(in >> std::quoted(vote.chain_id)
             >> vote.height
             >> vote.round
             >> phase
             >> std::quoted(vote.block_hash)
             >> hasValidRound
             >> validRound
             >> std::quoted(vote.validator_id)
             >> std::quoted(vote.signature))) {
        return false;
    }

    in >> std::ws;

    if (!in.eof() ||
        (phase != 0 && phase != 1) ||
        (hasValidRound != 0 && hasValidRound != 1)) {
        return false;
    }

    vote.phase = phase == 0
        ? VotePhase::PREVOTE
        : VotePhase::PRECOMMIT;

    vote.valid_round = hasValidRound
        ? std::optional<std::uint64_t>(validRound)
        : std::nullopt;

    return true;
}

void DistributedValidatorNode::broadcastVote(
    const Vote& vote,
    std::uint64_t tick,
    Network& network
) {
    const std::string payload = serializeVote(vote);

    for (const std::string& peer : validators_.validatorIds()) {
        if (peer == validatorId_) {
            continue;
        }

        network.send(
            tick,
            validatorId_,
            peer,
            "VOTE",
            height_,
            payload
        );
    }
}

bool DistributedValidatorNode::recordVote(const Vote& vote) {
    if (vote.chain_id != chainId_ ||
        vote.height != height_ ||
        vote.round != round_ ||
        vote.validator_id.empty()) {
        return false;
    }

    const Validator* signer = validators_.find(vote.validator_id);

    if (!signer || !vote.verify(signer->key.address())) {
        return false;
    }

    const VoteKey key{vote.round, vote.phase};

    auto& votes = observedVotes_[key];

    if (equivocators_[key].count(vote.validator_id) != 0) {
        return false;
    }

    const auto existing = votes.find(vote.validator_id);

    if (existing != votes.end()) {
        if (existing->second == vote.block_hash) {
            return false;
        }

        // Equivocation: remove the validator's vote.
        votes.erase(existing);
        equivocators_[key].insert(vote.validator_id);

        return false;
    }

    votes.emplace(vote.validator_id, vote.block_hash);

    return true;
}

bool DistributedValidatorNode::hasQuorum(
    VotePhase phase,
    std::uint64_t round,
    const std::string& blockHash
) const {
    const VoteKey key{round, phase};

    const auto it = observedVotes_.find(key);

    if (it == observedVotes_.end()) {
        return false;
    }

    std::size_t count = 0;

    const auto equivIt = equivocators_.find(key);

    for (const auto& [voter, hash] : it->second) {
        if (hash != blockHash) {
            continue;
        }

        if (equivIt != equivocators_.end() &&
            equivIt->second.count(voter) != 0) {
            continue;
        }

        if (validators_.isValidator(voter)) {
            ++count;
        }
    }

    return count >= validators_.quorumSize();
}

void DistributedValidatorNode::startPrevote(
    const std::string& blockHash,
    std::uint64_t tick,
    Network& network
) {
    if (started_) {
        throw std::logic_error("Node has already started this height");
    }

    if (finalizedBlock_) {
        throw std::logic_error("Height is already finalized");
    }

    started_ = true;

    Vote vote;

    vote.chain_id = chainId_;
    vote.height = height_;
    vote.round = round_;
    vote.phase = VotePhase::PREVOTE;
    vote.block_hash = blockHash;
    vote.validator_id = validatorId_;

    vote.sign(localKey_);

    locallyVoted_.insert({round_, VotePhase::PREVOTE});

    // Record the locally created vote.
    recordVote(vote);

    // Broadcast to other validators.
    broadcastVote(vote, tick, network);
}

void DistributedValidatorNode::maybePrecommit(
    std::uint64_t tick,
    Network& network
) {
    if (precommitSent_ || finalizedBlock_) {
        return;
    }

    const auto it = observedVotes_.find({
        round_,
        VotePhase::PREVOTE
    });

    if (it == observedVotes_.end()) {
        return;
    }

    // Map structure: validator_id -> block_hash.
    for (const auto& [voter, candidate] : it->second) {
        (void)voter;

        if (candidate.empty()) {
            continue;
        }

        // Check quorum using the actual block hash.
        if (hasQuorum(
                VotePhase::PREVOTE,
                round_,
                candidate
            )) {

            // Lock the block after observing a PREVOTE quorum.
            lockedBlock_ = candidate;
            lockedRound_ = round_;

            Vote vote;

            vote.chain_id = chainId_;
            vote.height = height_;
            vote.round = round_;
            vote.phase = VotePhase::PRECOMMIT;
            vote.block_hash = candidate;
            vote.validator_id = validatorId_;

            vote.sign(localKey_);

            locallyVoted_.insert({
                round_,
                VotePhase::PRECOMMIT
            });

            precommitSent_ = true;

            // Record own PRECOMMIT.
            recordVote(vote);

            // Broadcast PRECOMMIT to other validators.
            broadcastVote(vote, tick, network);

            return;
        }
    }
}

void DistributedValidatorNode::maybeFinalize() {
    if (finalizedBlock_) {
        return;
    }

    const auto it = observedVotes_.find({
        round_,
        VotePhase::PRECOMMIT
    });

    if (it == observedVotes_.end()) {
        return;
    }

    // Map structure: validator_id -> block_hash.
    for (const auto& [voter, candidate] : it->second) {
        (void)voter;

        if (candidate.empty()) {
            continue;
        }

        // Check quorum using the actual block hash.
        if (hasQuorum(
                VotePhase::PRECOMMIT,
                round_,
                candidate
            )) {

            finalizedBlock_ = candidate;
            return;
        }
    }
}

void DistributedValidatorNode::processMessages(
    std::uint64_t tick,
    Network& network
) {
    for (const Message& message : network.receive(tick, validatorId_)) {

        if (message.type != "VOTE" ||
            message.height != height_ ||
            message.from.empty()) {
            continue;
        }

        Vote vote;

        if (!deserializeVote(message.payload, vote)) {
            continue;
        }

        // Ensure authenticated signer matches transport sender.
        if (vote.validator_id != message.from) {
            continue;
        }

        recordVote(vote);

        // Re-evaluate consensus after processing every message.
        maybePrecommit(tick, network);
        maybeFinalize();
    }
}

const std::string& DistributedValidatorNode::id() const {
    return validatorId_;
}

bool DistributedValidatorNode::isFinalized() const {
    return finalizedBlock_.has_value();
}

std::optional<std::string>
DistributedValidatorNode::finalizedBlock() const {
    return finalizedBlock_;
}

std::optional<std::string>
DistributedValidatorNode::lockedBlock() const {
    return lockedBlock_;
}

std::uint64_t DistributedValidatorNode::lockedRound() const {
    return lockedRound_;
}

std::size_t DistributedValidatorNode::observedVoteCount(
    VotePhase phase,
    std::uint64_t round,
    const std::string& blockHash
) const {
    const auto it = observedVotes_.find({round, phase});

    if (it == observedVotes_.end()) {
        return 0;
    }

    std::size_t count = 0;

    const auto equivIt = equivocators_.find({round, phase});

    for (const auto& [voter, hash] : it->second) {
        if (hash == blockHash &&
            (equivIt == equivocators_.end() ||
             equivIt->second.count(voter) == 0)) {
            ++count;
        }
    }

    return count;
}

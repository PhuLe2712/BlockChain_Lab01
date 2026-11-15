
#include "distributed_node.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

void testNetworkReordering() {
    NetworkConfig config;
    config.min_delay_ticks = 1;
    config.max_delay_ticks = 1;
    config.duplicate_probability = 0.0;
    config.drop_probability = 0.0;
    config.seed = 2026;

    // Force delivery times that differ from send order:
    // message A: tick 5, message B: tick 1, message C: tick 3.
    config.scripted_delays = {5, 1, 3};

    Network network(config);

    network.send(0, "sender", "receiver", "TEST", 1, "A");
    network.send(0, "sender", "receiver", "TEST", 1, "B");
    network.send(0, "sender", "receiver", "TEST", 1, "C");

    const auto delivered = network.receive(5, "receiver");

    assert(delivered.size() == 3);
    assert(delivered[0].payload == "B");
    assert(delivered[1].payload == "C");
    assert(delivered[2].payload == "A");

    std::cout << "[PASS] T2 network reordering: B, C, A delivered\n";
}

bool allNodesFinalized(
    const std::vector<std::unique_ptr<DistributedValidatorNode>>& nodes,
    const std::string& expectedBlock
) {
    for (const auto& node : nodes) {
        if (!node->isFinalized() ||
            !node->finalizedBlock().has_value() ||
            node->finalizedBlock().value() != expectedBlock) {
            return false;
        }
    }
    return true;
}

void testDistributedDuplicateVotes() {
    const std::string chainId = "lab01";
    const std::string blockHash = "block-t2-duplicate";

    ValidatorSet validators(chainId, 4, 1);

    NetworkConfig config;
    config.min_delay_ticks = 1;
    config.max_delay_ticks = 4;
    config.duplicate_probability = 1.0; // Every sent message is duplicated.
    config.drop_probability = 0.0;
    config.seed = 20261115;

    Network network(config);

    std::vector<std::unique_ptr<DistributedValidatorNode>> nodes;

    for (const std::string& id : validators.validatorIds()) {
        nodes.push_back(
            std::make_unique<DistributedValidatorNode>(
                chainId, id, validators
            )
        );
    }

    for (auto& node : nodes) {
        node->startPrevote(blockHash, 0, network);
    }

    bool finalized = false;

    // Continue until consensus finalizes and all duplicated messages drain.
    for (std::uint64_t tick = 0; tick < 500; ++tick) {
        for (auto& node : nodes) {
            node->processMessages(tick, network);
        }

        finalized = allNodesFinalized(nodes, blockHash);

        if (finalized && network.pendingMessages() == 0) {
            break;
        }
    }

    assert(finalized);
    assert(network.pendingMessages() == 0);

    // Each validator must be counted once, even though every message
    // was duplicated by the network.
    for (const auto& node : nodes) {
        assert(node->observedVoteCount(
            VotePhase::PREVOTE, 0, blockHash
        ) == 4);

        assert(node->observedVoteCount(
            VotePhase::PRECOMMIT, 0, blockHash
        ) == 4);

        assert(node->isFinalized());
        assert(node->finalizedBlock().value() == blockHash);
        assert(node->lockedBlock().value() == blockHash);
    }

    std::cout
        << "[PASS] T2 duplicate votes: each phase counts exactly 4 validators\n";
}

void testNormalRun() {
    const std::string chainId = "lab01";
    const std::string blockHash = "block-demo-001";

    ValidatorSet validators(chainId, 4, 1);

    NetworkConfig config;
    config.min_delay_ticks = 1;
    config.max_delay_ticks = 3;
    config.duplicate_probability = 0.0;
    config.drop_probability = 0.0;
    config.seed = 2026;

    Network network(config);

    std::vector<std::unique_ptr<DistributedValidatorNode>> nodes;

    for (const std::string& id : validators.validatorIds()) {
        nodes.push_back(
            std::make_unique<DistributedValidatorNode>(
                chainId, id, validators
            )
        );
    }

    for (auto& node : nodes) {
        node->startPrevote(blockHash, 0, network);
    }

    // T1 completes only when every node has finalized AND
    // observed all 4 PRECOMMIT votes.
    bool completed = false;

    for (std::uint64_t tick = 0; tick < 100; ++tick) {
        for (auto& node : nodes) {
            node->processMessages(tick, network);
        }

        bool allPrecommitsObserved = true;

        for (const auto& node : nodes) {
            if (node->observedVoteCount(
                    VotePhase::PRECOMMIT, 0, blockHash
                ) != 4) {
                allPrecommitsObserved = false;
                break;
            }
        }

        completed =
            allNodesFinalized(nodes, blockHash) &&
            allPrecommitsObserved;

        if (completed) {
            break;
        }
    }

    assert(completed);

    for (const auto& node : nodes) {
        assert(node->observedVoteCount(
            VotePhase::PREVOTE, 0, blockHash
        ) == 4);

        assert(node->observedVoteCount(
            VotePhase::PRECOMMIT, 0, blockHash
        ) == 4);

        assert(node->isFinalized());
        assert(node->finalizedBlock().value() == blockHash);
        assert(node->lockedBlock().value() == blockHash);
    }

    std::cout << "[PASS] T1 normal run: all validators finalized same block\n";
}

std::string serializeVoteForNetwork(const Vote& vote) {
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

void testRejectInvalidSignature() {
    const std::string chainId = "lab01";
    const std::string blockHash = "block-invalid-signature";

    ValidatorSet validators(chainId, 4, 1);
    const auto ids = validators.validatorIds();
    const std::string senderId = ids.at(1);
    const std::string receiverId = ids.at(0);

    NetworkConfig config;
    config.min_delay_ticks = 1;
    config.max_delay_ticks = 1;
    config.duplicate_probability = 0.0;
    config.drop_probability = 0.0;
    config.seed = 20261115;

    Network network(config);
    DistributedValidatorNode receiver(
        chainId, receiverId, validators
    );

    const Validator* signer = validators.find(senderId);
    assert(signer != nullptr);

    Vote forged;
    forged.chain_id = chainId;
    forged.height = 1;
    forged.round = 0;
    forged.phase = VotePhase::PREVOTE;
    forged.block_hash = blockHash;
    forged.validator_id = senderId;
    forged.sign(signer->key);

    // Corrupt the signature after signing.
    assert(!forged.signature.empty());
    forged.signature[0] = forged.signature[0] == '0' ? '1' : '0';

    network.send(
        0, senderId, receiverId, "VOTE", 1,
        serializeVoteForNetwork(forged)
    );

    receiver.processMessages(1, network);

    assert(receiver.observedVoteCount(
        VotePhase::PREVOTE, 0, blockHash
    ) == 0);
    assert(!receiver.lockedBlock().has_value());
    assert(!receiver.isFinalized());

    std::cout << "[PASS] T3 invalid signature rejected\n";
}

void testRejectWrongDomain() {
    const std::string expectedChainId = "lab01";
    const std::string wrongChainId = "another-chain";
    const std::string blockHash = "block-wrong-domain";

    ValidatorSet validators(expectedChainId, 4, 1);
    const auto ids = validators.validatorIds();
    const std::string senderId = ids.at(1);
    const std::string receiverId = ids.at(0);

    NetworkConfig config;
    config.min_delay_ticks = 1;
    config.max_delay_ticks = 1;
    config.duplicate_probability = 0.0;
    config.drop_probability = 0.0;
    config.seed = 20261115;

    Network network(config);
    DistributedValidatorNode receiver(
        expectedChainId, receiverId, validators
    );

    const Validator* signer = validators.find(senderId);
    assert(signer != nullptr);

    Vote wrongDomainVote;
    wrongDomainVote.chain_id = wrongChainId;
    wrongDomainVote.height = 1;
    wrongDomainVote.round = 0;
    wrongDomainVote.phase = VotePhase::PREVOTE;
    wrongDomainVote.block_hash = blockHash;
    wrongDomainVote.validator_id = senderId;

    // The signature is valid under VOTE:another-chain only.
    wrongDomainVote.sign(signer->key);
    assert(wrongDomainVote.verify(signer->key.address()));

    network.send(
        0, senderId, receiverId, "VOTE", 1,
        serializeVoteForNetwork(wrongDomainVote)
    );

    receiver.processMessages(1, network);

    assert(receiver.observedVoteCount(
        VotePhase::PREVOTE, 0, blockHash
    ) == 0);
    assert(!receiver.lockedBlock().has_value());
    assert(!receiver.isFinalized());

    std::cout << "[PASS] T3 wrong chain/domain vote rejected\n";
}

} // namespace

int main() {
    testNormalRun();
    testNetworkReordering();
    testDistributedDuplicateVotes();
    testRejectInvalidSignature();
    testRejectWrongDomain();

    std::cout << "\nAll T1, T2 and T3 distributed tests passed.\n";
    return 0;
}

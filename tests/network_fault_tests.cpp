#include "distributed_node.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<std::unique_ptr<DistributedValidatorNode>> makeNodes(
    const ValidatorSet& validators
) {
    std::vector<std::unique_ptr<DistributedValidatorNode>> nodes;

    for (const std::string& id : validators.validatorIds()) {
        nodes.push_back(
            std::make_unique<DistributedValidatorNode>(
                "lab01", id, validators
            )
        );
    }

    return nodes;
}

void processUntil(
    std::vector<std::unique_ptr<DistributedValidatorNode>>& nodes,
    Network& network,
    std::uint64_t lastTick
) {
    for (std::uint64_t tick = 0; tick <= lastTick; ++tick) {
        for (auto& node : nodes) {
            node->processMessages(tick, network);
        }
    }
}

void assertNoConflictingFinalization(
    const std::vector<std::unique_ptr<DistributedValidatorNode>>& nodes
) {
    std::string finalizedHash;

    for (const auto& node : nodes) {
        if (!node->isFinalized()) {
            continue;
        }

        assert(node->finalizedBlock().has_value());

        if (finalizedHash.empty()) {
            finalizedHash = node->finalizedBlock().value();
        } else {
            assert(finalizedHash == node->finalizedBlock().value());
        }
    }
}

void testDroppedMessagesDoNotFinalizeConflictingBlocks() {
    ValidatorSet validators("lab01", 4, 1);

    NetworkConfig config;
    config.min_delay_ticks = 1;
    config.max_delay_ticks = 5;
    config.duplicate_probability = 0.0;
    config.drop_probability = 1.0; // All outbound messages are dropped.
    config.seed = 5001;

    Network network(config);
    auto nodes = makeNodes(validators);

    // Two validators support A and two support B.
    nodes[0]->startPrevote("block-A", 0, network);
    nodes[1]->startPrevote("block-A", 0, network);
    nodes[2]->startPrevote("block-B", 0, network);
    nodes[3]->startPrevote("block-B", 0, network);

    processUntil(nodes, network, 30);

    for (const auto& node : nodes) {
        assert(!node->isFinalized());
    }
    assertNoConflictingFinalization(nodes);

    std::cout
        << "[PASS] T5: dropped messages cannot create conflicting finalization\n";
}

void testDelayedConflictingVotesDoNotFinalize() {
    ValidatorSet validators("lab01", 4, 1);

    NetworkConfig config;
    config.min_delay_ticks = 20;
    config.max_delay_ticks = 20; // Deterministic long delay.
    config.duplicate_probability = 0.0;
    config.drop_probability = 0.0;
    config.seed = 5002;

    Network network(config);
    auto nodes = makeNodes(validators);

    nodes[0]->startPrevote("block-A", 0, network);
    nodes[1]->startPrevote("block-A", 0, network);
    nodes[2]->startPrevote("block-B", 0, network);
    nodes[3]->startPrevote("block-B", 0, network);

    // Before delivery, each node has only its own PREVOTE.
    processUntil(nodes, network, 19);

    for (const auto& node : nodes) {
        assert(!node->isFinalized());
        assert(!node->lockedBlock().has_value());
    }

    // After delivery, the view is 2 votes for A and 2 for B.
    // With quorum 3, neither block can be precommitted or finalized.
    processUntil(nodes, network, 100);

    for (const auto& node : nodes) {
        assert(!node->isFinalized());
    }
    assertNoConflictingFinalization(nodes);

    std::cout
        << "[PASS] T5: delayed conflicting votes do not reach quorum\n";
}

void testDelayedMessagesEventuallyFinalizeOneBlock() {
    ValidatorSet validators("lab01", 4, 1);

    NetworkConfig config;
    config.min_delay_ticks = 10;
    config.max_delay_ticks = 10;
    config.duplicate_probability = 0.0;
    config.drop_probability = 0.0;
    config.seed = 5003;

    Network network(config);
    auto nodes = makeNodes(validators);

    for (auto& node : nodes) {
        node->startPrevote("block-common", 0, network);
    }

    // The initial PREVOTEs have not arrived yet.
    processUntil(nodes, network, 9);

    for (const auto& node : nodes) {
        assert(!node->isFinalized());
    }

    // Once bounded-delay delivery occurs, the same block reaches quorum
    // in PREVOTE and PRECOMMIT.
    processUntil(nodes, network, 100);

    for (const auto& node : nodes) {
        assert(node->isFinalized());
        assert(node->finalizedBlock().value() == "block-common");
    }

    assertNoConflictingFinalization(nodes);

    std::cout
        << "[PASS] T5: delayed messages are eventually delivered and nodes converge\n";
}

} // namespace

int main() {
    testDroppedMessagesDoNotFinalizeConflictingBlocks();
    testDelayedConflictingVotesDoNotFinalize();
    testDelayedMessagesEventuallyFinalizeOneBlock();

    std::cout << "All T5 network drop/delay tests passed.\n";
    return 0;
}

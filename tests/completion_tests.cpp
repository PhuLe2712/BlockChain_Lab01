#include "distributed_node.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string serialize(const Vote& vote) {
    std::ostringstream out;
    out << std::quoted(vote.chain_id) << ' '
        << vote.height << ' ' << vote.round << ' '
        << static_cast<int>(vote.phase) << ' '
        << std::quoted(vote.block_hash) << ' '
        << (vote.valid_round.has_value() ? 1 : 0) << ' '
        << vote.valid_round.value_or(0) << ' '
        << std::quoted(vote.validator_id) << ' '
        << std::quoted(vote.signature);
    return out.str();
}

std::vector<std::unique_ptr<DistributedValidatorNode>> makeNodes(
    const std::string& chainId, const ValidatorSet& validators
) {
    std::vector<std::unique_ptr<DistributedValidatorNode>> nodes;
    for (const auto& id : validators.validatorIds()) {
        nodes.push_back(std::make_unique<DistributedValidatorNode>(
            chainId, id, validators));
    }
    return nodes;
}

void testSilentProposerTimeoutAndRoundRecovery() {
    const std::string chain = "lab01";
    const std::string nextBlock = "block-after-timeout";
    ValidatorSet validators(chain, 4, 1);
    NetworkConfig config;
    config.min_delay_ticks = config.max_delay_ticks = 1;
    config.seed = 6001;
    Network network(config);
    auto nodes = makeNodes(chain, validators);

    // validator-0 is the silent proposer. The scheduler times out round 0
    // and assigns the next proposer to propose this candidate in round 1.
    for (std::size_t i = 1; i < nodes.size(); ++i) {
        nodes[i]->onRoundTimeout(nextBlock, 10, network);
    }
    for (std::uint64_t tick = 10; tick < 100; ++tick) {
        for (std::size_t i = 1; i < nodes.size(); ++i) {
            nodes[i]->processMessages(tick, network);
        }
    }

    for (std::size_t i = 1; i < nodes.size(); ++i) {
        assert(nodes[i]->isFinalized());
        assert(nodes[i]->finalizedBlock().value() == nextBlock);
    }
    std::cout << "[PASS] T6 silent proposer timeout advances round and finalizes\n";
}

void testByzantineEquivocationCannotCauseConflictingFinality() {
    const std::string chain = "lab01";
    const std::string blockA = "honest-block-A";
    const std::string blockB = "byzantine-block-B";
    ValidatorSet validators(chain, 4, 1);
    NetworkConfig config;
    config.min_delay_ticks = config.max_delay_ticks = 1;
    config.seed = 7001;
    Network network(config);
    auto nodes = makeNodes(chain, validators);

    // Three correct validators support A; the Byzantine validator signs
    // conflicting PREVOTEs for A and B in the same height/round.
    for (std::size_t i = 0; i < 3; ++i) {
        nodes[i]->startPrevote(blockA, 0, network);
    }
    const Validator* byzantine = validators.find("validator-3");
    assert(byzantine);
    for (const auto& receiver : validators.validatorIds()) {
        for (const auto& hash : {blockA, blockB}) {
            Vote vote;
            vote.chain_id = chain;
            vote.height = 1;
            vote.round = 0;
            vote.phase = VotePhase::PREVOTE;
            vote.block_hash = hash;
            vote.validator_id = byzantine->id;
            vote.sign(byzantine->key);
            network.send(0, byzantine->id, receiver, "VOTE", 1,
                         serialize(vote));
        }
    }

    for (std::uint64_t tick = 0; tick < 100; ++tick) {
        for (auto& node : nodes) {
            node->processMessages(tick, network);
        }
    }
    for (std::size_t i = 0; i < 3; ++i) {
        assert(nodes[i]->isFinalized());
        assert(nodes[i]->finalizedBlock().value() == blockA);
    }
    assert(!nodes[3]->isFinalized() ||
           nodes[3]->finalizedBlock().value() == blockA);
    std::cout << "[PASS] T7 Byzantine equivocation does not split finality\n";
}

} // namespace

int main() {
    testSilentProposerTimeoutAndRoundRecovery();
    testByzantineEquivocationCannotCauseConflictingFinality();
    return 0;
}


#include "block.hpp"
#include "network.hpp"
#include "validator.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <stdexcept>
int main() {
    // ==========================================
    // TEST 1: Cryptographic signature
    // ==========================================

    std::vector<unsigned char> seed(32, 0x42);
    auto key = crypto::keyPairFromSeed(seed);

    const std::string message = "hello";

    auto signature = crypto::sign(
        key,
        "TEST:domain",
        message
    );

    assert(
        crypto::verify(
            key.address(),
            "TEST:domain",
            message,
            signature
        )
    );

    // Wrong domain must fail.
    assert(
        !crypto::verify(
            key.address(),
            "OTHER:domain",
            message,
            signature
        )
    );

    // ==========================================
    // TEST 2: Transaction validation
    // ==========================================

    Transaction tx;

    tx.key = key.address() + "/message";
    tx.value = "one";
    tx.nonce = 0;
    tx.sign(key);

    assert(tx.isValid());

    // ==========================================
    // TEST 3: State transition
    // ==========================================

    State state;
    std::string reason;

    assert(state.apply(tx, &reason));

    // Replayed transaction must be rejected.
    assert(!state.apply(tx, &reason));
    assert(reason == "invalid_or_replayed_nonce");

    // ==========================================
    // TEST 4: Transaction ownership
    // ==========================================

    Transaction forged;

    forged.sender = key.address();
    forged.key = "someone-else/message";
    forged.value = "bad";
    forged.nonce = 1;
    forged.sign(key);

    assert(!forged.isValid());

    // ==========================================
    // TEST 5: Block header signature
    // ==========================================

    Block b;

    b.height = 1;
    b.parent_hash = "GENESIS";
    b.state_root = state.root();
    b.sign(key);

    assert(b.verifyHeader());

    // ==========================================
    // TEST 6: Valid block validation
    // ==========================================

    State previousState;

    Transaction blockTx;

    blockTx.key = key.address() + "/profile";
    blockTx.value = "blockchain";
    blockTx.nonce = 0;
    blockTx.sign(key);

    State expectedState = previousState;
    assert(expectedState.apply(blockTx));

    Block validBlock;

    validBlock.height = 1;
    validBlock.parent_hash = "GENESIS";
    validBlock.transactions.push_back(blockTx);
    validBlock.state_root = expectedState.root();
    validBlock.sign(key);

    assert(
        validBlock.validateBlock(
            previousState,
            "GENESIS",
            1,
            &reason
        )
    );

    assert(reason.empty());

    // ==========================================
    // TEST 7: Invalid parent hash
    // ==========================================

    assert(
        !validBlock.validateBlock(
            previousState,
            "WRONG_PARENT",
            1,
            &reason
        )
    );

    assert(reason == "invalid_parent_hash");

    // ==========================================
    // TEST 8: Invalid block height
    // ==========================================

    assert(
        !validBlock.validateBlock(
            previousState,
            "GENESIS",
            2,
            &reason
        )
    );

    assert(reason == "invalid_block_height");

    // ==========================================
    // TEST 9: Invalid state root
    // ==========================================

    Block invalidStateBlock = validBlock;

    invalidStateBlock.state_root = "invalid_state_root";
    invalidStateBlock.sign(key);

    assert(
        !invalidStateBlock.validateBlock(
            previousState,
            "GENESIS",
            1,
            &reason
        )
    );

    assert(reason == "state_root_mismatch");

    // ==========================================
    // TEST 10: Duplicate transactions
    // ==========================================

    Block duplicateBlock;

    duplicateBlock.height = 1;
    duplicateBlock.parent_hash = "GENESIS";
    duplicateBlock.transactions.push_back(blockTx);
    duplicateBlock.transactions.push_back(blockTx);
    duplicateBlock.state_root = expectedState.root();
    duplicateBlock.sign(key);

    assert(
        !duplicateBlock.validateBlock(
            previousState,
            "GENESIS",
            1,
            &reason
        )
    );

    assert(reason == "duplicate_transaction");

    // ==========================================
    // TEST 11: Transaction tampering
    // ==========================================

    Block tamperedBlock = validBlock;

    tamperedBlock.transactions[0].value = "tampered";

    assert(!tamperedBlock.verifyHeader());

    // ==========================================
    // TEST 12: Network delay and delivery
    // ==========================================

    NetworkConfig networkConfig;

    networkConfig.min_delay_ticks = 2;
    networkConfig.max_delay_ticks = 2;
    networkConfig.duplicate_probability = 0.0;
    networkConfig.drop_probability = 0.0;
    networkConfig.seed = 42;

    Network network(networkConfig);

    network.send(
        0,
        "node-0",
        "node-1",
        "TEST_MESSAGE",
        1,
        "hello"
    );

    // Message should not arrive before tick 2.
    assert(network.receive(1, "node-1").empty());

    // Message should arrive at tick 2.
    auto received = network.receive(2, "node-1");

    assert(received.size() == 1);
    assert(received[0].payload == "hello");

    // ==========================================
    // TEST 13: Network message drop
    // ==========================================

    NetworkConfig dropConfig;

    dropConfig.min_delay_ticks = 1;
    dropConfig.max_delay_ticks = 1;
    dropConfig.drop_probability = 1.0;
    dropConfig.duplicate_probability = 0.0;
    dropConfig.seed = 42;

    Network dropNetwork(dropConfig);

    dropNetwork.send(
        0,
        "node-0",
        "node-1",
        "TEST_MESSAGE",
        1,
        "dropped"
    );

    assert(dropNetwork.pendingMessages() == 0);
    assert(dropNetwork.receive(10, "node-1").empty());

    // ==========================================
    // TEST 14: Network message duplication
    // ==========================================

    NetworkConfig duplicateConfig;

    duplicateConfig.min_delay_ticks = 1;
    duplicateConfig.max_delay_ticks = 1;
    duplicateConfig.duplicate_probability = 1.0;
    duplicateConfig.drop_probability = 0.0;
    duplicateConfig.seed = 42;

    Network duplicateNetwork(duplicateConfig);

    duplicateNetwork.send(
        0,
        "node-0",
        "node-1",
        "TEST_MESSAGE",
        1,
        "duplicated"
    );

    assert(duplicateNetwork.pendingMessages() == 2);

    auto duplicatedMessages =
        duplicateNetwork.receive(10, "node-1");

    assert(duplicatedMessages.size() == 2);

    // ==========================================
    // TEST 15: Peer blocking
    // ==========================================

    Network blockedNetwork(networkConfig);

    blockedNetwork.setPeerBlocked(
        "node-0",
        "node-1",
        true
    );

    blockedNetwork.send(
        0,
        "node-0",
        "node-1",
        "TEST_MESSAGE",
        1,
        "blocked"
    );

    assert(blockedNetwork.pendingMessages() == 0);

    // Unblock peer and send again.
    blockedNetwork.setPeerBlocked(
        "node-0",
        "node-1",
        false
    );

    blockedNetwork.send(
        0,
        "node-0",
        "node-1",
        "TEST_MESSAGE",
        1,
        "unblocked"
    );

    assert(blockedNetwork.pendingMessages() == 1);

    // ==========================================
    // TEST 16: Validator set initialization
    // ==========================================

    ValidatorSet validators("lab01", 4, 1);

    assert(validators.validatorCount() == 4);
    assert(validators.maxByzantine() == 1);
    assert(validators.quorumSize() == 3);
    assert(validators.byzantineCount() == 0);

    // ==========================================
    // TEST 17: Validator membership
    // ==========================================

    assert(validators.isValidator("validator-0"));
    assert(validators.isValidator("validator-1"));
    assert(!validators.isValidator("validator-99"));

    assert(validators.find("validator-0") != nullptr);
    assert(validators.find("validator-99") == nullptr);

    // ==========================================
    // TEST 18: Validator key uniqueness
    // ==========================================

    const Validator* validator0 =
        validators.find("validator-0");

    const Validator* validator1 =
        validators.find("validator-1");

    assert(validator0 != nullptr);
    assert(validator1 != nullptr);

    assert(
        validator0->key.address() !=
        validator1->key.address()
    );

    // ==========================================
    // TEST 19: Byzantine limit
    // ==========================================

    assert(
        validators.setByzantine("validator-0", true)
    );

    assert(validators.byzantineCount() == 1);

    // Cannot exceed maxByzantine = 1.
    assert(
        !validators.setByzantine("validator-1", true)
    );

    assert(validators.byzantineCount() == 1);

    // Unknown validator cannot be modified.
    assert(
        !validators.setByzantine("validator-99", true)
    );

    // ==========================================
    // TEST 20: Byzantine validator recovery
    // ==========================================

    assert(
        validators.setByzantine("validator-0", false)
    );

    assert(validators.byzantineCount() == 0);

    // ==========================================
    // TEST 21: Proposer selection
    // ==========================================

    const Validator* proposer1 =
        validators.proposerFor(1, 0);

    const Validator* proposer2 =
        validators.proposerFor(1, 1);

    assert(proposer1 != nullptr);
    assert(proposer2 != nullptr);

    assert(proposer1->id == "validator-0");
    assert(proposer2->id == "validator-1");

    assert(validators.proposerFor(0, 0) == nullptr);

    // ==========================================
    // TEST 22: Quorum validation
    // ==========================================

    std::vector<std::string> threeVotes = {
        "validator-0",
        "validator-1",
        "validator-2"
    };

    assert(validators.hasQuorum(threeVotes));

    std::vector<std::string> twoVotes = {
        "validator-0",
        "validator-1"
    };

    assert(!validators.hasQuorum(twoVotes));

    // Duplicate votes must not count twice.
    std::vector<std::string> duplicateVotes = {
        "validator-0",
        "validator-0",
        "validator-1"
    };

    assert(!validators.hasQuorum(duplicateVotes));

    // Unknown validators must not count.
    std::vector<std::string> invalidVotes = {
        "validator-0",
        "validator-1",
        "validator-99"
    };

    assert(!validators.hasQuorum(invalidVotes));

    // ==========================================
    // TEST 23: Invalid validator configuration
    // ==========================================

    bool invalidConfigRejected = false;

    try {
        ValidatorSet invalidValidators("lab01", 5, 1);
    } catch (const std::invalid_argument&) {
        invalidConfigRejected = true;
    }

    assert(invalidConfigRejected);

    std::cout << "All core and network unit tests passed\n";

    return 0;
}

#include "crypto.hpp"
#include "state.hpp"
#include "transaction.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

crypto::KeyPair makeTestKey() {
    // Deterministic key for reproducible tests only.
    return crypto::keyPairFromSeed(
        std::vector<unsigned char>(32, 0x42)
    );
}

Transaction makeTransaction(
    const crypto::KeyPair& signer,
    std::uint64_t nonce,
    const std::string& value
) {
    Transaction tx;
    tx.sender = signer.address();
    tx.key = signer.address() + "/message";
    tx.value = value;
    tx.nonce = nonce;
    tx.sign(signer);
    return tx;
}

void testExactTransactionReplayIsRejected() {
    const auto signer = makeTestKey();
    const Transaction tx = makeTransaction(signer, 0, "first-value");

    State state;
    std::string reason;

    assert(tx.isValid());
    assert(state.apply(tx, &reason));
    assert(reason.empty());

    const std::string rootAfterFirstApply = state.root();
    const auto dataAfterFirstApply = state.data();

    // Submit the exact same signed transaction a second time.
    reason.clear();
    assert(!state.apply(tx, &reason));
    assert(reason == "invalid_or_replayed_nonce");

    // A rejected replay must not mutate either application data or state root.
    assert(state.data() == dataAfterFirstApply);
    assert(state.root() == rootAfterFirstApply);

    std::cout << "[PASS] T4 exact transaction replay rejected\n";
}

void testDifferentTransactionWithUsedNonceIsRejected() {
    const auto signer = makeTestKey();

    const Transaction first = makeTransaction(signer, 0, "value-A");
    const Transaction conflictingReplay =
        makeTransaction(signer, 0, "value-B");

    assert(first.id() != conflictingReplay.id());

    State state;
    std::string reason;

    assert(state.apply(first, &reason));
    const std::string rootAfterFirstApply = state.root();

    // Even a different, correctly signed transaction cannot reuse nonce 0.
    reason.clear();
    assert(!state.apply(conflictingReplay, &reason));
    assert(reason == "invalid_or_replayed_nonce");

    assert(state.data().at(first.key) == "value-A");
    assert(state.root() == rootAfterFirstApply);

    std::cout
        << "[PASS] T4 different transaction with reused nonce rejected\n";
}

void testSequentialNonceAcceptedAndOldNonceRejected() {
    const auto signer = makeTestKey();

    const Transaction tx0 = makeTransaction(signer, 0, "value-0");
    const Transaction tx1 = makeTransaction(signer, 1, "value-1");

    State state;
    std::string reason;

    assert(state.apply(tx0, &reason));
    assert(state.apply(tx1, &reason));
    assert(state.data().at(tx1.key) == "value-1");

    const std::string rootAfterSequentialTransactions = state.root();

    // A previously used nonce remains invalid after a later nonce is accepted.
    reason.clear();
    assert(!state.apply(tx0, &reason));
    assert(reason == "invalid_or_replayed_nonce");

    assert(state.data().at(tx1.key) == "value-1");
    assert(state.root() == rootAfterSequentialTransactions);

    std::cout
        << "[PASS] T4 sequential nonce accepted; old nonce replay rejected\n";
}

} // namespace

int main() {
    testExactTransactionReplayIsRejected();
    testDifferentTransactionWithUsedNonceIsRejected();
    testSequentialNonceAcceptedAndOldNonceRejected();

    std::cout << "\nAll T4 transaction replay tests passed.\n";
    return 0;
}
